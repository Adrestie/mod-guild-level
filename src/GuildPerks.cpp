/*
 * This file is part of mod-guild-level.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "GuildPerks.h"
#include "GuildText.h"
#include "GuildTalents.h"
#include "GuildActivePerks.h"
#include "GuildLevel.h"

#include "Chat.h"
#include "Common.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Item.h"
#include "Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
    constexpr char DATA_KEY[] = "mod-guild-level-perks";

    // Option name and built-in steps of each perk, in the order of GuildPerk.
    struct PerkOption
    {
        char const* option;
        char const* steps;
    };

    constexpr std::array<PerkOption, GUILD_PERK_COUNT> PERK_OPTIONS =
    { {
        { "GuildLevel.Perk.Experience",   "1:5 11:10" },
        { "GuildLevel.Perk.Durability",   "2:1 12:1" },
        { "GuildLevel.Perk.Honor",        "3:5 13:10" },
        { "GuildLevel.Perk.Hearthstone",  "4:5 14:10" },
        { "GuildLevel.Perk.Gathering",    "5:1 15:2" },
        { "GuildLevel.Perk.Vendor",       "6:5 16:5" },
        { "GuildLevel.Perk.GoldToBank",   "7:5 17:5" },
        { "GuildLevel.Perk.MountSpeed",   "8:5 18:5" },
        { "GuildLevel.Perk.Reputation",   "9:5 19:5" },
        { "GuildLevel.Perk.Resurrection", "10:1" },
        { "GuildLevel.Perk.Consumables",  "20:30" },
    } };

    // "level:amount level:amount ...", spaces or commas between the steps. A
    // step that cannot be read is skipped with an error in the log.
    std::vector<std::pair<uint8, float>> ReadSteps(char const* option, std::string text)
    {
        std::vector<std::pair<uint8, float>> steps;
        std::replace(text.begin(), text.end(), ',', ' ');
        std::istringstream in(text);
        std::string token;
        while (in >> token)
        {
            std::size_t const colon = token.find(':');
            Optional<uint32> level = colon == std::string::npos ? Optional<uint32>() : Acore::StringTo<uint32>(token.substr(0, colon));
            Optional<float> amount = colon == std::string::npos ? Optional<float>() : Acore::StringTo<float>(token.substr(colon + 1));
            if (!level || !amount || *level < 1 || *level > GUILD_LEVEL_MAX)
            {
                LOG_ERROR("server.loading", "mod-guild-level: {}: \"{}\" is not a level:amount step (level 1 to {}), skipped.",
                    option, token, GUILD_LEVEL_MAX);
                continue;
            }
            steps.emplace_back(uint8(*level), *amount);
        }
        std::sort(steps.begin(), steps.end());
        return steps;
    }

    // THE SKILL A LOOT COMES FROM: a mining or herbalism node by its lock, a
    // skinned corpse by its loot type. SKILL_NONE for anything else.
    uint32 GatheringSkill(Player* player, ObjectGuid lootGuid)
    {
        if (lootGuid.IsGameObject())
        {
            GameObject* go = ObjectAccessor::GetGameObject(*player, lootGuid);
            if (!go)
                return SKILL_NONE;

            if (LockEntry const* lock = sLockStore.LookupEntry(go->GetGOInfo()->GetLockId()))
                for (uint8 i = 0; i < MAX_LOCK_CASE; ++i)
                    if (lock->Type[i] == LOCK_KEY_SKILL)
                    {
                        SkillType const skill = SkillByLockType(LockType(lock->Index[i]));
                        if (skill == SKILL_MINING || skill == SKILL_HERBALISM)
                            return skill;
                    }
            return SKILL_NONE;
        }

        if (lootGuid.IsCreatureOrVehicle())
            if (Creature* creature = ObjectAccessor::GetCreature(*player, lootGuid))
                if (creature->loot.loot_type == LOOT_SKINNING)
                    return creature->GetCreatureTemplate()->GetRequiredLootSkill();
        return SKILL_NONE;
    }

    // A flask, an elixir or a food buff: the consumables the level 20 perk
    // lengthens. Flasks and elixirs carry the item that cast them; a food buff
    // comes later from the eating aura, without an item, and is told by its
    // name, "Well Fed" in every game file.
    bool IsLengthenedConsumable(Aura const* aura)
    {
        if (uint32 const entry = aura->GetCastItemEntry())
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
            return proto && proto->Class == ITEM_CLASS_CONSUMABLE &&
                (proto->SubClass == ITEM_SUBCLASS_FLASK || proto->SubClass == ITEM_SUBCLASS_ELIXIR);
        }

        char const* name = aura->GetSpellInfo()->SpellName[LOCALE_enUS];
        return name && std::string_view(name) == "Well Fed";
    }
}

GuildPerkMgr* GuildPerkMgr::instance()
{
    static GuildPerkMgr instance;
    return &instance;
}

void GuildPerkMgr::LoadConfig()
{
    GuildPerkConfig c;
    c.enable = sConfigMgr->GetOption<bool>("GuildLevel.Perks.Enable", true);
    for (uint8 i = 0; i < GUILD_PERK_COUNT; ++i)
        c.steps[i] = ReadSteps(PERK_OPTIONS[i].option,
            sConfigMgr->GetOption<std::string>(PERK_OPTIONS[i].option, PERK_OPTIONS[i].steps));
    c.gatheringChance = std::clamp(sConfigMgr->GetOption<float>("GuildLevel.Perk.Gathering.Chance", 10.0f), 0.0f, 100.0f);
    c.durabilityInterval = std::max<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.Perk.Durability.Interval", 60), 1);
    _config = c;
}

float GuildPerkMgr::Amount(GuildPerk perk, uint8 level) const
{
    if (!_config.enable)
        return 0.0f;

    float amount = 0.0f;
    for (auto const& [stepLevel, stepAmount] : _config.steps[perk])
        if (stepLevel <= level)
            amount += stepAmount;
    return amount;
}

float GuildPerkMgr::Amount(GuildPerk perk, Player const* player) const
{
    if (!_config.enable || !player)
        return 0.0f;

    uint32 const guildId = player->GetGuildId();
    if (!guildId)
        return 0.0f;

    // The guild talents add to some of the perks.
    float talent = 0.0f;
    switch (perk)
    {
        case GUILD_PERK_XP:          talent = sGuildTalentMgr->Amount(player, GT_XP_PCT); break;
        case GUILD_PERK_HONOR:       talent = sGuildTalentMgr->Amount(player, GT_HONOR_PCT); break;
        case GUILD_PERK_REPUTATION:  talent = sGuildTalentMgr->Amount(player, GT_REPUTATION_PCT); break;
        case GUILD_PERK_MOUNT_SPEED: talent = sGuildTalentMgr->Amount(player, GT_MOUNT_SPEED_PCT); break;
        case GUILD_PERK_VENDOR:      talent = sGuildTalentMgr->Amount(player, GT_VENDOR_PCT); break;
        case GUILD_PERK_GOLD_TO_BANK: talent = sGuildTalentMgr->Amount(player, GT_GOLD_TO_BANK_PCT); break;
        case GUILD_PERK_CONSUMABLES: talent = sGuildTalentMgr->Amount(player, GT_CONSUMABLE_MIN); break;
        case GUILD_PERK_HEARTHSTONE: talent = sGuildTalentMgr->Amount(player, GT_HEARTHSTONE_MIN); break;
        default: break;
    }
    return Amount(perk, sGuildLevelMgr->Get(guildId).level) + talent;
}

void GuildPerkMgr::Apply(Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    // The guild talents first: some perks read them.
    sGuildTalentMgr->Apply(player);
    // The spells of the active perks follow the same changes.
    sGuildActivePerkMgr->SyncSpells(player);
    ApplyAura(player);
}

void GuildPerkMgr::ApplyAura(Player* player)
{
    int32 const honor = int32(Amount(GUILD_PERK_HONOR, player));
    int32 const mount = int32(Amount(GUILD_PERK_MOUNT_SPEED, player));
    if (honor <= 0 && mount <= 0)
    {
        player->RemoveAurasDueToSpell(GUILD_PERK_AURA);
        return;
    }

    if (!sSpellMgr->GetSpellInfo(GUILD_PERK_AURA))
    {
        LOG_ERROR("module", "mod-guild-level: spell {} is missing (data/sql/world/guild_level_03_perks.sql): no honor and mount speed perks.",
            GUILD_PERK_AURA);
        return;
    }

    Aura* aura = player->GetAura(GUILD_PERK_AURA);
    if (!aura)
        aura = player->AddAura(GUILD_PERK_AURA, player);
    if (!aura)
        return;

    // Effect 0: honor, 1: ground mounts, 2: flying mounts.
    std::array<int32, 3> const amounts = { std::max(honor, 0), std::max(mount, 0), std::max(mount, 0) };
    for (uint8 i = 0; i < amounts.size(); ++i)
        if (AuraEffect* effect = aura->GetEffect(i))
            if (effect->GetAmount() != amounts[i])
                effect->ChangeAmount(amounts[i]);
}

void GuildPerkMgr::RefreshGuild(uint32 guildId)
{
    Guild* guild = sGuildMgr->GetGuildById(guildId);
    if (!guild)
        return;

    auto apply = [this](Player* member) { Apply(member); };
    guild->BroadcastWorker(apply);
}

void GuildPerkMgr::QueueRefresh(ObjectGuid guid)
{
    std::lock_guard<std::mutex> guard(_lock);
    _refresh.insert(guid);
}

void GuildPerkMgr::Update()
{
    std::set<ObjectGuid> pending;
    {
        std::lock_guard<std::mutex> guard(_lock);
        pending.swap(_refresh);
    }
    for (ObjectGuid guid : pending)
        if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
            Apply(player);
}

void GuildPerkMgr::OnGiveXP(Player* player, uint32& amount)
{
    float const percent = Amount(GUILD_PERK_XP, player);
    if (percent > 0.0f && amount)
        amount = uint32(std::lround(double(amount) * (100.0 + percent) / 100.0));
}

void GuildPerkMgr::OnGiveReputation(Player* player, float& amount)
{
    // Gains only: a loss stays a loss.
    float const percent = Amount(GUILD_PERK_REPUTATION, player);
    if (percent > 0.0f && amount > 0.0f)
        amount *= (100.0f + percent) / 100.0f;
}

void GuildPerkMgr::OnPriceDiscount(Player const* player, Creature const* creature, float& discount)
{
    // Vendors and repairers only: trainers and flight masters ask the same
    // question.
    if (!creature || (!creature->IsVendor() && !creature->IsArmorer()))
        return;

    float const percent = Amount(GUILD_PERK_VENDOR, player);
    if (percent > 0.0f)
        discount *= std::max(0.0f, (100.0f - percent) / 100.0f);
}

void GuildPerkMgr::OnLootMoney(Player* player, Loot* loot)
{
    // On the world thread (CMSG_LOOT_MONEY is thread-unsafe): the guild bank
    // can be written here. The gold is ADDED to the bank: the looters keep
    // all of theirs.
    if (!loot || !loot->gold)
        return;

    float const percent = Amount(GUILD_PERK_GOLD_TO_BANK, player);
    if (percent <= 0.0f)
        return;

    Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
    uint64 const bonus = uint64(std::floor(double(loot->gold) * percent / 100.0));
    if (!guild || !bonus)
        return;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    if (guild->ModifyBankMoney(trans, bonus, true))
        CharacterDatabase.CommitTransaction(trans);
}

void GuildPerkMgr::OnLootItem(Player* player, Item* item, ObjectGuid lootGuid)
{
    if (!item || !lootGuid)
        return;

    uint32 const extra = uint32(Amount(GUILD_PERK_GATHERING, player));
    if (!extra)
        return;

    uint32 const skill = GatheringSkill(player, lootGuid);
    if (skill != SKILL_MINING && skill != SKILL_HERBALISM && skill != SKILL_SKINNING)
        return;

    // ONE ROLL PER GATHERING, on the first item taken from the node or the
    // corpse (the ore, the herb, the leather); the stones and gems after it
    // are not rolled again.
    auto* data = player->CustomData.GetDefault<GuildPerkPlayerData>(DATA_KEY);
    if (data->lastGatherLoot == lootGuid)
        return;
    data->lastGatherLoot = lootGuid;

    if (!roll_chance_f(_config.gatheringChance + sGuildTalentMgr->Amount(player, GT_GATHERING_PCT)))
        return;

    uint32 const entry = item->GetEntry();
    if (!player->AddItem(entry, extra))
        return;

    WorldSession* session = player->GetSession();
    std::string name;
    if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry))
    {
        name = proto->Name1;
        if (ItemLocale const* locale = sObjectMgr->GetItemLocale(entry))
            ObjectMgr::GetLocaleString(locale->Name, session->GetSessionDbLocaleIndex(), name);
    }
    if (std::string const* text = session->GetModuleString(GUILD_LEVEL_MODULE, GUILD_LEVEL_STR_GATHER_BONUS))
        ChatHandler(session).SendSysMessage("|cff40ff40" + Acore::StringFormat(*text, extra, name) + "|r");
}

void GuildPerkMgr::OnResurrect(Player* player, float restorePercent)
{
    // Only the resurrection a player offered and this one accepted
    // (Player::ResurectUsingRequestData): it resurrects at 0% and sets health
    // and mana from the request right after this hook. Whoever made the
    // request is a player of this map, for whom the request was registered.
    if (restorePercent != 0.0f || !player->isResurrectRequested())
        return;

    uint32 const guildId = player->GetGuildId();
    if (!guildId || Amount(GUILD_PERK_RESURRECTION, player) <= 0.0f)
        return;

    for (auto const& ref : player->GetMap()->GetPlayers())
    {
        Player* resurrector = ref.GetSource();
        if (!resurrector || resurrector == player || resurrector->GetGuildId() != guildId ||
            !player->isResurrectRequestedBy(resurrector->GetGUID()))
            continue;

        // The request is rewritten with full health and mana; the teleport it
        // held is already done.
        player->setResurrectRequestData(resurrector->GetGUID(), player->GetMapId(),
            player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(),
            player->GetMaxHealth(), player->GetMaxPower(POWER_MANA));
        return;
    }
}

void GuildPerkMgr::OnAuraApply(Player* player, Aura* aura)
{
    if (!aura || aura->GetMaxDuration() <= 0)
        return;

    if (Amount(GUILD_PERK_CONSUMABLES, player) <= 0.0f || !IsLengthenedConsumable(aura))
        return;

    // Not here: a cast applies its aura, then sets its duration again
    // (Spell::DoSpellHitOnUnit). Lengthened on the next tick of the player.
    auto* data = player->CustomData.GetDefault<GuildPerkPlayerData>(DATA_KEY);
    if (data->pendingConsumables.size() < 16)
        data->pendingConsumables.push_back(aura->GetId());
}

void GuildPerkMgr::LengthenConsumables(Player* player, GuildPerkPlayerData* data)
{
    std::vector<uint32> pending;
    pending.swap(data->pendingConsumables);

    float const minutes = Amount(GUILD_PERK_CONSUMABLES, player);
    if (minutes <= 0.0f)
        return;

    for (uint32 spellId : pending)
    {
        Aura* aura = player->GetAura(spellId);
        // ONCE PER APPLICATION: an aura still at its own full time is new or
        // refreshed by a new flask. One already lengthened, or loaded at
        // login with the time it had, has a longer maximum than its own.
        if (!aura || aura->GetDuration() <= 0 || aura->GetMaxDuration() > aura->CalcMaxDuration())
            continue;

        int32 const bonus = int32(minutes * MINUTE * IN_MILLISECONDS);
        aura->SetMaxDuration(aura->GetMaxDuration() + bonus);
        aura->SetDuration(aura->GetDuration() + bonus);
    }
}

void GuildPerkMgr::OnHearthstone(Player* player)
{
    float const minutes = Amount(GUILD_PERK_HEARTHSTONE, player);
    if (minutes > 0.0f)
        player->ModifySpellCooldown(SPELL_HEARTHSTONE, -int32(minutes * MINUTE * IN_MILLISECONDS));
}

void GuildPerkMgr::OnUpdate(Player* player, uint32 diff)
{
    // Every period, as many damaged pieces worn as the perk says, each a
    // different one drawn at random, get 1 point of durability back. The way
    // mod-stellar-tarot mends: the field is written (the client follows), and
    // a piece that was broken gets its stats back.
    auto* data = player->CustomData.GetDefault<GuildPerkPlayerData>(DATA_KEY);
    if (!data->pendingConsumables.empty())
        LengthenConsumables(player, data);

    data->mendTimer += diff;
    if (data->mendTimer < _config.durabilityInterval * IN_MILLISECONDS)
        return;
    data->mendTimer = 0;

    uint32 count = uint32(Amount(GUILD_PERK_DURABILITY, player));
    if (!count || !player->IsAlive())
        return;

    std::vector<uint8> damaged;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            uint32 const max = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
            if (max && item->GetUInt32Value(ITEM_FIELD_DURABILITY) < max)
                damaged.push_back(slot);
        }

    while (count-- && !damaged.empty())
    {
        std::size_t const pick = urand(0, uint32(damaged.size() - 1));
        uint8 const slot = damaged[pick];
        damaged.erase(damaged.begin() + pick);

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        uint32 const current = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
        item->SetUInt32Value(ITEM_FIELD_DURABILITY, current + 1);
        item->SetState(ITEM_CHANGED, player);
        if (!current)
            player->_ApplyItemMods(item, slot, true);
    }
}

void GuildPerkMgr::Show(ChatHandler* handler, uint8 level) const
{
    auto say = [handler](uint32 id, auto&&... args)
    {
        GuildSay(handler, id, args...);
    };

    if (!_config.enable)
    {
        say(GUILD_LEVEL_STR_PERKS_DISABLED);
        return;
    }

    std::string const on = GuildText(handler, GUILD_LEVEL_STR_PERKS_ON);
    std::string const off = GuildText(handler, GUILD_LEVEL_STR_PERKS_OFF);

    say(GUILD_LEVEL_STR_PERKS_TITLE, uint32(level));
    say(GUILD_LEVEL_STR_PERKS_GAINS, Amount(GUILD_PERK_XP, level), Amount(GUILD_PERK_HONOR, level),
        Amount(GUILD_PERK_REPUTATION, level), Amount(GUILD_PERK_MOUNT_SPEED, level));
    say(GUILD_LEVEL_STR_PERKS_GOLD, Amount(GUILD_PERK_VENDOR, level), Amount(GUILD_PERK_GOLD_TO_BANK, level),
        Amount(GUILD_PERK_HEARTHSTONE, level));
    uint32 const extra = uint32(Amount(GUILD_PERK_GATHERING, level));
    say(GUILD_LEVEL_STR_PERKS_MISC, extra ? _config.gatheringChance : 0.0f, extra,
        Amount(GUILD_PERK_RESURRECTION, level) > 0.0f ? on : off, Amount(GUILD_PERK_CONSUMABLES, level));
    say(GUILD_LEVEL_STR_PERKS_DURABILITY, Amount(GUILD_PERK_DURABILITY, level), _config.durabilityInterval);
}
