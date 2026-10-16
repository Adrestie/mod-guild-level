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

#include "GuildActivePerks.h"
#include "GuildLevel.h"
#include "GuildPerks.h"

#include "Chat.h"
#include "Common.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Group.h"
#include "Player.h"
#include "QueryResult.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>

namespace
{
    constexpr char DATA_KEY[] = "mod-guild-level-active";

    // The subcommand of each perk, in GuildActivePerk order (.gperk <name>).
    constexpr std::array<char const*, GUILD_ACTIVE_COUNT> SUBCOMMANDS = { "raid", "rez", "bank", "vendor", "home" };

    // Option prefix of each perk, in the same order.
    constexpr std::array<char const*, GUILD_ACTIVE_COUNT> OPTIONS =
        { "GuildLevel.Active.Raid", "GuildLevel.Active.Rez", "GuildLevel.Active.Bank", "GuildLevel.Active.Vendor", "GuildLevel.Active.Home" };

    template<typename... Args>
    void Say(ChatHandler* handler, uint32 id, Args&&... args)
    {
        handler->SendSysMessage(handler->PGetParseModuleString(GUILD_LEVEL_MODULE, id, std::forward<Args>(args)...));
    }

    int64 Now()
    {
        return int64(GameTime::GetGameTime().count());
    }

    // A spot two yards in front of the player, the summoned thing facing him.
    void InFront(Player* player, float& x, float& y, float& z, float& o)
    {
        player->GetNearPoint(player, x, y, z, 0.0f, 2.0f, player->GetOrientation());
        o = Position::NormalizeOrientation(player->GetOrientation() + float(M_PI));
    }
}

GuildActivePerkMgr* GuildActivePerkMgr::instance()
{
    static GuildActivePerkMgr instance;
    return &instance;
}

void GuildActivePerkMgr::LoadConfig()
{
    GuildActivePerkConfig c;
    for (uint8 i = 0; i < GUILD_ACTIVE_COUNT; ++i)
    {
        std::string const prefix = OPTIONS[i];
        c.level[i] = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>(prefix + ".Level", c.level[i]), 1, GUILD_LEVEL_MAX));
        c.cooldown[i] = sConfigMgr->GetOption<uint32>(prefix + ".Cooldown", c.cooldown[i]);
    }
    c.rezPercent = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.Active.Rez.Percent", 35), 1, 100);
    c.bankEntry = sConfigMgr->GetOption<uint32>("GuildLevel.Active.Bank.Entry", GUILD_VAULT_ENTRY);
    c.bankDuration = std::max<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.Active.Bank.Duration", 300), 10);
    c.vendorEntry = sConfigMgr->GetOption<uint32>("GuildLevel.Active.Vendor.Entry", GUILD_SUPPLIER_ENTRY);
    c.vendorDuration = std::max<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.Active.Vendor.Duration", 300), 10);
    _config = c;
}

void GuildActivePerkMgr::Load(Player* player)
{
    auto* data = new GuildActivePerkPlayerData();
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT `perk`, `ready_at` FROM `mod_guild_level_perk_cooldown` WHERE `guid` = {}",
            player->GetGUID().GetCounter()))
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const perk = fields[0].Get<uint8>();
            if (perk < GUILD_ACTIVE_COUNT)
                data->readyAt[perk] = int64(fields[1].Get<uint32>());
        } while (result->NextRow());
    }
    player->CustomData.Set(DATA_KEY, data);
}

void GuildActivePerkMgr::DeleteCharacter(CharacterDatabaseTransaction trans, uint32 guid)
{
    trans->Append("DELETE FROM `mod_guild_level_perk_cooldown` WHERE `guid` = {}", guid);
}

uint32 GuildActivePerkMgr::Remaining(Player* player, GuildActivePerk perk) const
{
    auto const* data = player ? player->CustomData.Get<GuildActivePerkPlayerData>(DATA_KEY) : nullptr;
    if (!data)
        return 0;
    int64 const left = data->readyAt[perk] - Now();
    return left > 0 ? uint32(left) : 0;
}

void GuildActivePerkMgr::Use(ChatHandler* handler, GuildActivePerk perk)
{
    Player* player = handler->GetPlayer();
    if (!player)
        return;

    uint32 const guildId = player->GetGuildId();
    if (!guildId)
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_NO_GUILD);
        return;
    }
    if (!sGuildPerkMgr->Config().enable)
    {
        Say(handler, GUILD_LEVEL_STR_PERKS_DISABLED);
        return;
    }
    if (sGuildLevelMgr->Get(guildId).level < _config.level[perk])
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_LEVEL, uint32(_config.level[perk]));
        return;
    }
    if (uint32 const left = Remaining(player, perk))
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_COOLDOWN, left / MINUTE, left % MINUTE);
        return;
    }

    bool done = false;
    switch (perk)
    {
        case GUILD_ACTIVE_RAID:   done = SummonGroup(handler, player);    break;
        case GUILD_ACTIVE_REZ:    done = ResurrectGroup(handler, player); break;
        case GUILD_ACTIVE_BANK:   done = SummonVault(handler, player);    break;
        case GUILD_ACTIVE_VENDOR: done = SummonSupplier(handler, player); break;
        case GUILD_ACTIVE_HOME:   done = GoHome(handler, player);         break;
        default: break;
    }
    if (!done || !_config.cooldown[perk])
        return;

    // The cooldown starts once the perk did something; it survives restarts.
    auto* data = player->CustomData.GetDefault<GuildActivePerkPlayerData>(DATA_KEY);
    data->readyAt[perk] = Now() + _config.cooldown[perk];
    CharacterDatabase.Execute(
        "REPLACE INTO `mod_guild_level_perk_cooldown` (`guid`, `perk`, `ready_at`) VALUES ({}, {}, {})",
        player->GetGUID().GetCounter(), uint32(perk), uint32(data->readyAt[perk]));
}

bool GuildActivePerkMgr::SummonGroup(ChatHandler* handler, Player* player)
{
    if (player->InBattleground() || player->InArena())
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_BG);
        return false;
    }
    if (player->IsInCombat())
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_COMBAT);
        return false;
    }
    Group* group = player->GetGroup();
    if (!group)
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_NO_GROUP);
        return false;
    }

    // THE SUMMONING STONE'S REQUEST (Spell::EffectSummonPlayer): each member
    // gets the usual window, and the core checks his answer the usual way.
    float x, y, z;
    player->GetPosition(x, y, z);
    uint32 count = 0;
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || member == player || !member->IsAlive() || member->InBattleground() || member->InArena())
            continue;

        member->SetSummonPoint(player->GetMapId(), x, y, z);
        WorldPacket data(SMSG_SUMMON_REQUEST, 8 + 4 + 4);
        data << player->GetGUID();
        data << uint32(player->GetZoneId());
        data << uint32(MAX_PLAYER_SUMMON_DELAY * IN_MILLISECONDS);
        member->SendDirectMessage(&data);
        ++count;
    }

    if (!count)
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_NO_ONE);
        return false;
    }
    Say(handler, GUILD_LEVEL_STR_ACTIVE_SUMMONED, count);
    return true;
}

bool GuildActivePerkMgr::ResurrectGroup(ChatHandler* handler, Player* player)
{
    if (player->IsInCombat())
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_COMBAT);
        return false;
    }
    Group* group = player->GetGroup();
    if (!group)
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_NO_GROUP);
        return false;
    }

    // A RESURRECTION SPELL'S REQUEST (Spell::EffectResurrect): the dead of the
    // group on this map, in this instance, come back where the player stands
    // once they accept. Resurrected by a guildmate, the level 10 perk brings
    // them back with full health and mana.
    uint32 count = 0;
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || member == player || member->IsAlive() || !member->IsInWorld() ||
            member->GetMapId() != player->GetMapId() || member->GetInstanceId() != player->GetInstanceId() ||
            member->isResurrectRequested())
            continue;

        member->setResurrectRequestData(player->GetGUID(), player->GetMapId(),
            player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(),
            member->CountPctFromMaxHealth(_config.rezPercent),
            CalculatePct(member->GetMaxPower(POWER_MANA), _config.rezPercent));

        // The resurrector's name is looked up by GUID for a player: empty.
        WorldPacket data(SMSG_RESURRECT_REQUEST, 8 + 4 + 1 + 1);
        data << player->GetGUID();
        data << uint32(1);
        data << uint8(0);
        data << uint8(0);
        member->SendDirectMessage(&data);
        ++count;
    }

    if (!count)
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_NO_DEAD);
        return false;
    }
    Say(handler, GUILD_LEVEL_STR_ACTIVE_REZZED, count);
    return true;
}

bool GuildActivePerkMgr::SummonVault(ChatHandler* handler, Player* player)
{
    if (player->InBattleground() || player->InArena())
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_BG);
        return false;
    }

    float x, y, z, o;
    InFront(player, x, y, z, o);
    if (!player->SummonGameObject(_config.bankEntry, x, y, z, o, 0.0f, 0.0f, 0.0f, 0.0f, _config.bankDuration))
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_FAILED);
        return false;
    }
    Say(handler, GUILD_LEVEL_STR_ACTIVE_VAULT, std::max<uint32>(_config.bankDuration / MINUTE, 1));
    return true;
}

bool GuildActivePerkMgr::SummonSupplier(ChatHandler* handler, Player* player)
{
    if (player->InBattleground() || player->InArena())
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_BG);
        return false;
    }

    float x, y, z, o;
    InFront(player, x, y, z, o);
    if (!player->SummonCreature(_config.vendorEntry, x, y, z, o, TEMPSUMMON_TIMED_DESPAWN, _config.vendorDuration * IN_MILLISECONDS))
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_FAILED);
        return false;
    }
    Say(handler, GUILD_LEVEL_STR_ACTIVE_SUPPLIER, std::max<uint32>(_config.vendorDuration / MINUTE, 1));
    return true;
}

bool GuildActivePerkMgr::GoHome(ChatHandler* handler, Player* /*player*/)
{
    // THE HOOK OF THE GUILD HOUSE: it comes with the Paragon levels. Until
    // then the perk answers, and starts no cooldown.
    Say(handler, GUILD_LEVEL_STR_ACTIVE_NO_HOME);
    return false;
}

void GuildActivePerkMgr::List(ChatHandler* handler, uint8 guildLevel, Player* player) const
{
    if (!sGuildPerkMgr->Config().enable)
        return;

    Say(handler, GUILD_LEVEL_STR_ACTIVE_TITLE);
    for (uint8 i = 0; i < GUILD_ACTIVE_COUNT; ++i)
    {
        GuildActivePerk const perk = GuildActivePerk(i);
        std::string state;
        if (guildLevel < _config.level[i])
            state = handler->PGetParseModuleString(GUILD_LEVEL_MODULE, GUILD_LEVEL_STR_ACTIVE_LOCKED);
        else if (uint32 const left = Remaining(player, perk))
            state = handler->PGetParseModuleString(GUILD_LEVEL_MODULE, GUILD_LEVEL_STR_ACTIVE_LEFT, left / MINUTE, left % MINUTE);
        else
            state = handler->PGetParseModuleString(GUILD_LEVEL_MODULE, GUILD_LEVEL_STR_ACTIVE_READY);

        std::string const name = handler->PGetParseModuleString(GUILD_LEVEL_MODULE, GUILD_LEVEL_STR_ACTIVE_NAME + i);
        Say(handler, GUILD_LEVEL_STR_ACTIVE_LINE, name, SUBCOMMANDS[i], uint32(_config.level[i]), state);
    }
}
