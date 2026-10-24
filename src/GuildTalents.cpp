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

#include "GuildTalents.h"
#include "GuildText.h"
#include "GuildLevel.h"

#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QueryResult.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr char PLAYER_DATA[] = "mod-guild-level-talents";
    // The window's client file listens for it (CHAT_MSG_ADDON).
    constexpr char ADDON_PREFIX[] = "GuildTalents";

    struct EffectInfo
    {
        GuildTalentEffect effect;
        char const* name;               // the chat's, in English
    };

    std::vector<EffectInfo> const EFFECTS =
    {
        { GT_STAMINA, "Stamina" }, { GT_STRENGTH, "Strength" }, { GT_AGILITY, "Agility" },
        { GT_INTELLECT, "Intellect" }, { GT_SPIRIT, "Spirit" },
        { GT_CRIT, "Critical strike rating" }, { GT_HASTE, "Haste rating" }, { GT_HIT, "Hit rating" },
        { GT_EXPERTISE, "Expertise rating" }, { GT_ARMOR_PEN, "Armor penetration rating" },
        { GT_ATTACK_POWER, "Attack power" }, { GT_SPELL_POWER, "Spell power" }, { GT_MP5, "Mana per 5 sec." },
        { GT_DEFENSE, "Defense rating" }, { GT_DODGE, "Dodge rating" }, { GT_PARRY, "Parry rating" },
        { GT_BLOCK, "Block rating" }, { GT_RESILIENCE, "Resilience rating" }, { GT_ARMOR, "Armor" },
        { GT_SPELL_PEN, "Spell penetration" }, { GT_RESISTANCE, "Resistance to all schools" },
        { GT_HEALTH, "Health" }, { GT_MANA, "Mana" }, { GT_HEALING_POWER, "Healing power" },
        { GT_SPELL_DAMAGE, "Spell damage" },
        { GT_DAMAGE_PCT, "% damage done" }, { GT_HEALING_PCT, "% healing done" },
        { GT_DAMAGE_TAKEN_PCT, "% less damage taken" }, { GT_HEALTH_PCT, "% maximum health" },
        { GT_RUN_SPEED_PCT, "% run speed" }, { GT_XP_PCT, "% experience" }, { GT_HONOR_PCT, "% honor" },
        { GT_REPUTATION_PCT, "% reputation" }, { GT_MOUNT_SPEED_PCT, "% mounted speed" },
        { GT_VENDOR_PCT, "% off vendor prices and repairs" }, { GT_GOLD_TO_BANK_PCT, "% of looted gold to the guild bank" },
        { GT_GATHERING_PCT, "% chance of a lucky gathering" }, { GT_CONSUMABLE_MIN, "minutes on flasks, elixirs and food" },
        { GT_HEARTHSTONE_MIN, "minutes off the Hearthstone" }, { GT_CRIT_DAMAGE_PCT, "% critical damage" },
        { GT_ORIGIN, "the origin" },
        { GT_PROC_SUNDER, "% armor ignored after a hit" }, { GT_PROC_FERVOR, "% damage after a hit" },
        { GT_PROC_EXTRA_HIT, "extra attacks after a melee hit" }, { GT_PROC_SHIELD, "% health shield after a block" },
        { GT_PROC_RIPOSTE, "% health riposte after a parry" }, { GT_PROC_EVASION, "% less damage after a dodge" },
        { GT_PROC_FREE_SPELL, "free spell after a spell" }, { GT_PROC_INSTANT_SPELL, "instant spell after a spell" },
        { GT_PROC_RESOURCE, "resource after a hit or a spell" },
    };

    char const* NameOf(GuildTalentEffect effect)
    {
        for (EffectInfo const& info : EFFECTS)
            if (info.effect == effect)
                return info.name;
        return "?";
    }

    template<typename... Args>
    void Say(ChatHandler* handler, uint32 id, Args&&... args)
    {
        GuildSay(handler, id, std::forward<Args>(args)...);
    }

    void TellAddon(Player* player, std::string const& message)
    {
        if (!player || !player->GetSession())
            return;
        std::string const full = std::string(ADDON_PREFIX) + "\t" + message;
        WorldPacket data(SMSG_MESSAGECHAT, 100);
        data << uint8(CHAT_MSG_WHISPER);
        data << int32(LANG_ADDON);
        data << player->GetGUID();
        data << uint32(0);
        data << player->GetGUID();
        data << uint32(full.length() + 1);
        data << full;
        data << uint8(0);
        player->GetSession()->SendPacket(&data);
    }

    // A statistic on a player, put on or taken off.
    void ApplyStat(Player* p, GuildTalentEffect effect, float amount, bool apply)
    {
        int32 const v = int32(std::lround(amount));
        switch (effect)
        {
            case GT_STAMINA:   p->HandleStatFlatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, amount, apply); break;
            case GT_STRENGTH:  p->HandleStatFlatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, amount, apply); break;
            case GT_AGILITY:   p->HandleStatFlatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, amount, apply); break;
            case GT_INTELLECT: p->HandleStatFlatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, amount, apply); break;
            case GT_SPIRIT:    p->HandleStatFlatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, amount, apply); break;
            case GT_CRIT:
                p->ApplyRatingMod(CR_CRIT_MELEE, v, apply);
                p->ApplyRatingMod(CR_CRIT_RANGED, v, apply);
                p->ApplyRatingMod(CR_CRIT_SPELL, v, apply);
                break;
            case GT_HASTE:
                p->ApplyRatingMod(CR_HASTE_MELEE, v, apply);
                p->ApplyRatingMod(CR_HASTE_RANGED, v, apply);
                p->ApplyRatingMod(CR_HASTE_SPELL, v, apply);
                break;
            case GT_HIT:
                p->ApplyRatingMod(CR_HIT_MELEE, v, apply);
                p->ApplyRatingMod(CR_HIT_RANGED, v, apply);
                p->ApplyRatingMod(CR_HIT_SPELL, v, apply);
                break;
            case GT_EXPERTISE:  p->ApplyRatingMod(CR_EXPERTISE, v, apply); break;
            case GT_ARMOR_PEN:  p->ApplyRatingMod(CR_ARMOR_PENETRATION, v, apply); break;
            case GT_ATTACK_POWER:
                p->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, amount, apply);
                p->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, amount, apply);
                break;
            case GT_SPELL_POWER: p->ApplySpellPowerBonus(v, apply); break;
            case GT_MP5:         p->ApplyManaRegenBonus(v, apply); break;
            case GT_DEFENSE:     p->ApplyRatingMod(CR_DEFENSE_SKILL, v, apply); break;
            case GT_DODGE:       p->ApplyRatingMod(CR_DODGE, v, apply); break;
            case GT_PARRY:       p->ApplyRatingMod(CR_PARRY, v, apply); break;
            case GT_BLOCK:       p->ApplyRatingMod(CR_BLOCK, v, apply); break;
            case GT_RESILIENCE:
                p->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, v, apply);
                p->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, v, apply);
                p->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, v, apply);
                break;
            case GT_ARMOR:     p->HandleStatFlatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, amount, apply); break;
            case GT_SPELL_PEN: p->ApplySpellPenetrationBonus(v, apply); break;
            case GT_RESISTANCE:
                for (UnitMods mod : { UNIT_MOD_RESISTANCE_FIRE, UNIT_MOD_RESISTANCE_NATURE, UNIT_MOD_RESISTANCE_FROST,
                                      UNIT_MOD_RESISTANCE_SHADOW, UNIT_MOD_RESISTANCE_ARCANE })
                    p->HandleStatFlatModifier(mod, TOTAL_VALUE, amount, apply);
                break;
            case GT_HEALTH: p->HandleStatFlatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, amount, apply); break;
            case GT_MANA:   p->HandleStatFlatModifier(UNIT_MOD_MANA, TOTAL_VALUE, amount, apply); break;
            case GT_HEALING_POWER: p->ApplySpellHealingBonus(v, apply); break;
            case GT_SPELL_DAMAGE:  p->ApplySpellDamageBonus(v, apply); break;
            default: break;
        }
    }

    bool IsStat(GuildTalentEffect effect)
    {
        return effect >= GT_STAMINA && effect <= GT_SPELL_DAMAGE;
    }

    GuildTalentPlayerData const* DataOf(Unit const* unit)
    {
        Player const* player = unit ? unit->ToPlayer() : nullptr;
        return player ? player->CustomData.Get<GuildTalentPlayerData>(PLAYER_DATA) : nullptr;
    }
}

std::vector<GuildTalentEffect> const& GuildTalentEffects()
{
    static std::vector<GuildTalentEffect> const list = []
    {
        std::vector<GuildTalentEffect> out;
        for (EffectInfo const& info : EFFECTS)
            out.push_back(info.effect);
        return out;
    }();
    return list;
}

bool IsGuildTalentEffect(uint32 effect)
{
    for (EffectInfo const& info : EFFECTS)
        if (uint32(info.effect) == effect)
            return true;
    return false;
}

bool IsProc(GuildTalentEffect effect)
{
    return effect > GT_ORIGIN;
}

GuildTalentKind KindOf(GuildTalentEffect effect)
{
    if (effect == GT_ORIGIN)
        return GT_KIND_ORIGIN;
    if (effect < GT_CRIT)
        return GT_KIND_MAIN;
    if (effect < GT_DAMAGE_PCT)
        return GT_KIND_SECONDARY;
    return GT_KIND_SPECIAL;
}

GuildTalentMgr* GuildTalentMgr::instance()
{
    static GuildTalentMgr instance;
    return &instance;
}

void GuildTalentMgr::LoadConfig()
{
    GuildTalentConfig c;
    c.enable = sConfigMgr->GetOption<bool>("GuildLevel.Talents.Enable", true);
    c.fromParagon = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.Talents.FromParagon", 10), 1, GUILD_PARAGON_LIMIT));
    c.pointsPerLevel = sConfigMgr->GetOption<uint32>("GuildLevel.Talents.PointsPerLevel", 1);
    c.rank = uint8(std::min<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.Talents.Rank", 1), 9));
    c.resetCost = sConfigMgr->GetOption<uint32>("GuildLevel.Talents.Reset.Cost", 5000);
    c.resetIncrease = std::max(sConfigMgr->GetOption<float>("GuildLevel.Talents.Reset.Increase", 10.0f), 0.0f);
    c.resetDecrease = std::max(sConfigMgr->GetOption<float>("GuildLevel.Talents.Reset.Decrease", 10.0f), 0.0f);
    c.tierPoints[0] = 0;
    c.tierPoints[1] = sConfigMgr->GetOption<uint32>("GuildLevel.Talents.Tier2.Points", 10);
    c.tierPoints[2] = std::max(sConfigMgr->GetOption<uint32>("GuildLevel.Talents.Tier3.Points", 25), c.tierPoints[1]);

    std::lock_guard<std::mutex> guard(_lock);
    _config = c;
}

void GuildTalentMgr::LoadTree()
{
    std::map<uint32, GuildTalentNode> tree;
    if (QueryResult result = WorldDatabase.Query(
            "SELECT `id`, `x`, `y`, `effect`, `amount`, `max_points`, `start`, `icon`, `chance`, `cooldown`, `tier` "
            "FROM `mod_guild_level_talent_node`"))
    {
        do
        {
            Field* f = result->Fetch();
            GuildTalentNode node;
            node.id = f[0].Get<uint32>();
            node.x = f[1].Get<int32>();
            node.y = f[2].Get<int32>();
            uint32 const effect = f[3].Get<uint8>();
            if (!IsGuildTalentEffect(effect))
            {
                LOG_ERROR("sql.sql", "mod-guild-level: talent node {} has an unknown effect {}, skipped.", node.id, effect);
                continue;
            }
            node.effect = GuildTalentEffect(effect);
            node.amount = f[4].Get<float>();
            node.maxPoints = std::max<uint8>(f[5].Get<uint8>(), 1);
            node.start = f[6].Get<uint8>() != 0;
            node.icon = f[7].Get<std::string>();
            node.chance = f[8].Get<float>();
            node.cooldown = f[9].Get<uint32>();
            node.tier = uint8(std::clamp<uint32>(f[10].Get<uint8>(), 1, GUILD_TALENT_TIERS));
            if (node.effect == GT_ORIGIN)
                node.maxPoints = 1;
            tree[node.id] = node;
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT `node_a`, `node_b` FROM `mod_guild_level_talent_link`"))
    {
        do
        {
            uint32 const a = result->Fetch()[0].Get<uint32>();
            uint32 const b = result->Fetch()[1].Get<uint32>();
            auto ia = tree.find(a);
            auto ib = tree.find(b);
            if (ia == tree.end() || ib == tree.end() || a == b)
                continue;
            ia->second.links.insert(b);
            ib->second.links.insert(a);
        } while (result->NextRow());
    }

    std::lock_guard<std::mutex> guard(_lock);
    _tree = std::move(tree);
    for (auto& [guildId, state] : _guilds)
        Total(guildId);
    LOG_INFO("server.loading", ">> mod-guild-level: loaded {} guild talent nodes", _tree.size());
}

void GuildTalentMgr::LoadGuilds()
{
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mod_guild_level_talent` WHERE `guildid` NOT IN (SELECT `guildid` FROM `guild`)");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mod_guild_level_talent_reset` WHERE `guildid` NOT IN (SELECT `guildid` FROM `guild`)");

    std::unordered_map<uint32, GuildTalentState> guilds;
    if (QueryResult result = CharacterDatabase.Query("SELECT `guildid`, `node`, `points` FROM `mod_guild_level_talent`"))
    {
        do
        {
            Field* f = result->Fetch();
            if (uint8 const points = f[2].Get<uint8>())
                guilds[f[0].Get<uint32>()].points[f[1].Get<uint32>()] = points;
        } while (result->NextRow());
    }
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT `guildid`, `next_price`, `last_reset` FROM `mod_guild_level_talent_reset`"))
    {
        do
        {
            Field* f = result->Fetch();
            GuildTalentState& state = guilds[f[0].Get<uint32>()];
            state.nextPrice = f[1].Get<uint32>();
            state.lastReset = f[2].Get<uint32>();
        } while (result->NextRow());
    }

    std::lock_guard<std::mutex> guard(_lock);
    _guilds = std::move(guilds);
    _totals.clear();
    for (auto& [guildId, state] : _guilds)
        Total(guildId);
}

void GuildTalentMgr::Total(uint32 guildId)
{
    GuildTalentTotals totals;
    auto itr = _guilds.find(guildId);
    if (itr != _guilds.end())
        for (auto const& [nodeId, points] : itr->second.points)
        {
            auto node = _tree.find(nodeId);
            if (node == _tree.end() || node->second.effect == GT_ORIGIN)
                continue;
            GuildTalentNode const& n = node->second;
            uint8 const held = std::min(points, n.maxPoints);
            if (!held)
                continue;
            totals.amount[n.effect] += n.amount * held;
            // Two nodes of one effect that procs: the better chance, the shorter cooldown.
            if (IsProc(n.effect))
            {
                bool const first = totals.chance[n.effect] == 0.0f;
                totals.chance[n.effect] = std::max(totals.chance[n.effect], n.chance);
                totals.cooldown[n.effect] = first ? n.cooldown : std::min(totals.cooldown[n.effect], n.cooldown);
            }
        }
    _totals[guildId] = totals;
}

std::map<uint32, GuildTalentNode> GuildTalentMgr::Tree() const
{
    std::lock_guard<std::mutex> guard(_lock);
    return _tree;
}

uint32 GuildTalentMgr::Earned(uint32 guildId) const
{
    GuildLevelState const level = sGuildLevelMgr->Get(guildId);
    if (level.level < GUILD_LEVEL_MAX || level.paragon < _config.fromParagon)
        return 0;
    return (uint32(level.paragon) - _config.fromParagon + 1) * _config.pointsPerLevel;
}

uint32 GuildTalentMgr::Spent(uint32 guildId) const
{
    std::lock_guard<std::mutex> guard(_lock);
    uint32 spent = 0;
    auto itr = _guilds.find(guildId);
    if (itr != _guilds.end())
        for (auto const& [nodeId, points] : itr->second.points)
            spent += points;
    return spent;
}

GuildTalentState GuildTalentMgr::State(uint32 guildId) const
{
    std::lock_guard<std::mutex> guard(_lock);
    auto itr = _guilds.find(guildId);
    return itr != _guilds.end() ? itr->second : GuildTalentState();
}

uint32 GuildTalentMgr::ResetPrice(uint32 guildId) const
{
    GuildTalentState const state = State(guildId);
    uint32 const base = _config.resetCost;
    if (!state.nextPrice)
        return base;

    // Each full week since the last reset takes Decrease % of the base cost
    // off, down to the base cost.
    uint32 const now = uint32(GameTime::GetGameTime().count());
    uint32 const weeks = now > state.lastReset ? (now - state.lastReset) / WEEK : 0;
    double const off = double(base) * _config.resetDecrease / 100.0 * weeks;
    double const price = std::max(double(base), double(state.nextPrice) - off);
    return uint32(std::lround(price));
}

bool GuildTalentMgr::CanSpend(Player const* player) const
{
    if (!player || !player->GetGuildId())
        return false;
    Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
    if (!guild)
        return false;
    Guild::Member const* member = guild->GetMember(player->GetGUID());
    return member && member->GetRankId() <= _config.rank;
}

GuildTalentResult GuildTalentMgr::Buy(Player* player, uint32 nodeId)
{
    if (!_config.enable)
        return GuildTalentResult::Disabled;
    uint32 const guildId = player->GetGuildId();
    if (!guildId)
        return GuildTalentResult::NoGuild;
    if (!CanSpend(player))
        return GuildTalentResult::NotAllowed;
    uint32 const earned = Earned(guildId);

    uint8 now = 0;
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto node = _tree.find(nodeId);
        if (node == _tree.end())
            return GuildTalentResult::NoNode;

        if (node->second.effect == GT_ORIGIN)
            return GuildTalentResult::Full;

        GuildTalentState& state = _guilds[guildId];
        uint8& points = state.points[nodeId];
        if (points >= node->second.maxPoints)
        {
            if (!points)
                state.points.erase(nodeId);
            return GuildTalentResult::Full;
        }

        // A start node, a node already holding a point, or one linked to a
        // node that holds one.
        // The origin always holds its point.
        bool reachable = node->second.start || points > 0;
        for (uint32 other : node->second.links)
        {
            auto held = state.points.find(other);
            auto def = _tree.find(other);
            if ((held != state.points.end() && held->second > 0) || (def != _tree.end() && def->second.effect == GT_ORIGIN))
                reachable = true;
        }
        if (!reachable)
        {
            if (!points)
                state.points.erase(nodeId);
            return GuildTalentResult::NotLinked;
        }

        uint32 spent = 0;
        for (auto const& [id, p] : state.points)
            spent += p;
        if (spent < _config.tierPoints[node->second.tier - 1])
        {
            if (!points)
                state.points.erase(nodeId);
            return GuildTalentResult::Tier;
        }
        if (spent >= earned)
        {
            if (!points)
                state.points.erase(nodeId);
            return GuildTalentResult::NoPoints;
        }

        now = ++points;
        Total(guildId);
    }

    CharacterDatabase.DirectExecute(
        "REPLACE INTO `mod_guild_level_talent` (`guildid`, `node`, `points`) VALUES ({}, {}, {})", guildId, nodeId, uint32(now));
    RefreshGuild(guildId);
    return GuildTalentResult::Ok;
}

GuildTalentResult GuildTalentMgr::Reset(Player* player, uint32& price)
{
    if (!_config.enable)
        return GuildTalentResult::Disabled;
    uint32 const guildId = player->GetGuildId();
    Guild* guild = guildId ? sGuildMgr->GetGuildById(guildId) : nullptr;
    if (!guild)
        return GuildTalentResult::NoGuild;
    if (!CanSpend(player))
        return GuildTalentResult::NotAllowed;
    if (!Spent(guildId))
        return GuildTalentResult::NothingToReset;

    price = ResetPrice(guildId);
    uint64 const copper = uint64(price) * GOLD;
    if (guild->GetTotalBankMoney() < copper)
        return GuildTalentResult::NoMoney;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    if (copper && !guild->ModifyBankMoney(trans, copper, false))
        return GuildTalentResult::NoMoney;

    uint32 const now = uint32(GameTime::GetGameTime().count());
    uint32 const next = uint32(std::lround(double(price) + double(_config.resetCost) * _config.resetIncrease / 100.0));
    trans->Append("DELETE FROM `mod_guild_level_talent` WHERE `guildid` = {}", guildId);
    trans->Append("REPLACE INTO `mod_guild_level_talent_reset` (`guildid`, `next_price`, `last_reset`) VALUES ({}, {}, {})",
        guildId, next, now);
    // The bank money is a prepared statement of the async connections only:
    // never DirectCommitTransaction here (it asserts).
    CharacterDatabase.CommitTransaction(trans);

    {
        std::lock_guard<std::mutex> guard(_lock);
        GuildTalentState& state = _guilds[guildId];
        state.points.clear();
        state.nextPrice = next;
        state.lastReset = now;
        Total(guildId);
    }
    RefreshGuild(guildId);
    return GuildTalentResult::Ok;
}

void GuildTalentMgr::ClearGuild(uint32 guildId)
{
    CharacterDatabase.DirectExecute("DELETE FROM `mod_guild_level_talent` WHERE `guildid` = {}", guildId);
    {
        std::lock_guard<std::mutex> guard(_lock);
        _guilds[guildId].points.clear();
        Total(guildId);
    }
    RefreshGuild(guildId);
}

void GuildTalentMgr::DeleteGuild(uint32 guildId)
{
    {
        std::lock_guard<std::mutex> guard(_lock);
        _guilds.erase(guildId);
        _totals.erase(guildId);
    }
    CharacterDatabase.Execute("DELETE FROM `mod_guild_level_talent` WHERE `guildid` = {}", guildId);
    CharacterDatabase.Execute("DELETE FROM `mod_guild_level_talent_reset` WHERE `guildid` = {}", guildId);
}

float GuildTalentMgr::AmountLocked(uint32 guildId, GuildTalentEffect effect) const
{
    auto itr = _totals.find(guildId);
    return itr != _totals.end() ? itr->second.amount[effect] : 0.0f;
}

float GuildTalentMgr::Amount(uint32 guildId, GuildTalentEffect effect) const
{
    if (!_config.enable || !guildId)
        return 0.0f;
    std::lock_guard<std::mutex> guard(_lock);
    return AmountLocked(guildId, effect);
}

float GuildTalentMgr::Amount(Player const* player, GuildTalentEffect effect) const
{
    // What was applied to him, without the lock: read on every hit.
    GuildTalentPlayerData const* data = DataOf(player);
    return data ? data->applied[effect] : 0.0f;
}

void GuildTalentMgr::Apply(Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    GuildTalentTotals all;
    if (_config.enable && player->GetGuildId())
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto itr = _totals.find(player->GetGuildId());
        if (itr != _totals.end())
            all = itr->second;
    }
    std::array<float, GT_EFFECT_END> const& totals = all.amount;

    GuildTalentPlayerData* data = player->CustomData.GetDefault<GuildTalentPlayerData>(PLAYER_DATA);
    data->totals = all;
    bool changed = false;
    for (GuildTalentEffect effect : GuildTalentEffects())
    {
        float& applied = data->applied[effect];
        if (applied == totals[effect])
            continue;
        if (IsStat(effect))
        {
            if (applied)
                ApplyStat(player, effect, applied, false);
            if (totals[effect])
                ApplyStat(player, effect, totals[effect], true);
            changed = true;
        }
        applied = totals[effect];
    }
    if (changed)
        player->UpdateAllStats();

    // The aura: shown while the guild has any point, with its run speed and
    // maximum health.
    bool any = false;
    bool procs = false;
    for (GuildTalentEffect effect : GuildTalentEffects())
    {
        any = any || totals[effect] != 0.0f;
        procs = procs || (IsProc(effect) && totals[effect] != 0.0f && all.chance[effect] > 0.0f);
    }

    // The hidden aura that rolls the special effects.
    if (!procs)
        player->RemoveAurasDueToSpell(GUILD_TALENT_PROC_AURA);
    else if (!player->HasAura(GUILD_TALENT_PROC_AURA) && sSpellMgr->GetSpellInfo(GUILD_TALENT_PROC_AURA))
        player->AddAura(GUILD_TALENT_PROC_AURA, player);

    if (!any)
    {
        player->RemoveAurasDueToSpell(GUILD_TALENT_AURA);
        return;
    }
    if (!sSpellMgr->GetSpellInfo(GUILD_TALENT_AURA))
    {
        LOG_ERROR("module", "mod-guild-level: spell {} is missing (data/sql/world/guild_level_10_talents.sql): no talent aura.",
            GUILD_TALENT_AURA);
        return;
    }
    Aura* aura = player->GetAura(GUILD_TALENT_AURA);
    if (!aura)
        aura = player->AddAura(GUILD_TALENT_AURA, player);
    if (!aura)
        return;
    std::array<int32, 3> const amounts = { int32(std::lround(totals[GT_RUN_SPEED_PCT])),
                                           int32(std::lround(totals[GT_HEALTH_PCT])),
                                           int32(std::lround(totals[GT_CRIT_DAMAGE_PCT])) };
    for (uint8 i = 0; i < amounts.size(); ++i)
        if (AuraEffect* effect = aura->GetEffect(i))
            if (effect->GetAmount() != amounts[i])
                effect->ChangeAmount(amounts[i]);
}

void GuildTalentMgr::RefreshGuild(uint32 guildId)
{
    Guild* guild = sGuildMgr->GetGuildById(guildId);
    if (!guild)
        return;
    // The level perks read the talents too: their aura is set again with them.
    auto apply = [](Player* member) { sGuildPerkMgr->Apply(member); };
    guild->BroadcastWorker(apply);
    NotifyGuild(guildId);
}

void GuildTalentMgr::NotifyGuild(uint32 guildId) const
{
    Guild* guild = sGuildMgr->GetGuildById(guildId);
    if (!guild)
        return;
    auto tell = [](Player* member) { TellAddon(member, "changed"); };
    guild->BroadcastWorker(tell);
}

void GuildTalentMgr::NotifyEveryone() const
{
    for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
        TellAddon(player, "changed");
}

void GuildTalentMgr::ModifyDamage(Unit* target, Unit* attacker, uint32& damage)
{
    if (!damage)
        return;
    float pct = 0.0f;
    if (attacker)
        if (Player const* owner = attacker->GetCharmerOrOwnerPlayerOrPlayerItself())
            if (GuildTalentPlayerData const* data = DataOf(owner))
                pct += data->applied[GT_DAMAGE_PCT];
    if (GuildTalentPlayerData const* data = DataOf(target))
        pct -= data->applied[GT_DAMAGE_TAKEN_PCT];
    if (pct != 0.0f)
        damage = uint32(std::max(0.0, std::round(double(damage) * (100.0 + pct) / 100.0)));
}

void GuildTalentMgr::ModifyHeal(Unit* healer, uint32& heal)
{
    if (!heal || !healer)
        return;
    if (Player const* owner = healer->GetCharmerOrOwnerPlayerOrPlayerItself())
        if (GuildTalentPlayerData const* data = DataOf(owner))
            if (float const pct = data->applied[GT_HEALING_PCT])
                heal = uint32(std::max(0.0, std::round(double(heal) * (100.0 + pct) / 100.0)));
}

void GuildTalentMgr::SaveNode(GuildTalentNode const& node) const
{
    WorldDatabase.DirectExecute(
        "REPLACE INTO `mod_guild_level_talent_node` (`id`, `x`, `y`, `effect`, `amount`, `max_points`, `start`, `icon`, "
        "`chance`, `cooldown`, `tier`) VALUES ({}, {}, {}, {}, {}, {}, {}, '{}', {}, {}, {})",
        node.id, node.x, node.y, uint32(node.effect), node.amount, uint32(node.maxPoints), node.start ? 1 : 0, node.icon,
        node.chance, node.cooldown, uint32(node.tier));
}

uint32 GuildTalentMgr::AddNode(int32 x, int32 y, GuildTalentEffect effect, float amount, uint8 maxPoints,
    float chance, uint32 cooldown, uint8 tier)
{
    if (!IsGuildTalentEffect(effect) || !maxPoints)
        return 0;
    GuildTalentNode node;
    {
        std::lock_guard<std::mutex> guard(_lock);
        for (auto const& [id, other] : _tree)
            if (other.x == x && other.y == y)
                return 0;
        node.id = _tree.empty() ? 1 : _tree.rbegin()->first + 1;
        node.x = x;
        node.y = y;
        node.effect = effect;
        node.amount = amount;
        node.maxPoints = effect == GT_ORIGIN ? 1 : maxPoints;
        node.chance = std::clamp(chance, 0.0f, 100.0f);
        node.cooldown = cooldown;
        node.tier = uint8(std::clamp<uint32>(tier, 1, GUILD_TALENT_TIERS));
        // The first node of a tree is where it starts.
        node.start = _tree.empty() && effect != GT_ORIGIN;
        _tree[node.id] = node;
    }
    SaveNode(node);
    NotifyEveryone();
    return node.id;
}

bool GuildTalentMgr::SetNode(uint32 id, GuildTalentEffect effect, float amount, uint8 maxPoints,
    float chance, uint32 cooldown, uint8 tier)
{
    if (!IsGuildTalentEffect(effect) || !maxPoints)
        return false;
    GuildTalentNode node;
    std::vector<uint32> guilds;
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto itr = _tree.find(id);
        if (itr == _tree.end())
            return false;
        if (effect == GT_ORIGIN)
            maxPoints = 1;
        itr->second.effect = effect;
        itr->second.amount = amount;
        itr->second.maxPoints = maxPoints;
        itr->second.chance = std::clamp(chance, 0.0f, 100.0f);
        itr->second.cooldown = cooldown;
        itr->second.tier = uint8(std::clamp<uint32>(tier, 1, GUILD_TALENT_TIERS));
        node = itr->second;
        // Points above a lowered maximum go back to their guild.
        for (auto& [guildId, state] : _guilds)
        {
            auto held = state.points.find(id);
            if (held == state.points.end())
                continue;
            if (held->second > maxPoints)
                held->second = maxPoints;
            guilds.push_back(guildId);
            Total(guildId);
        }
    }
    SaveNode(node);
    CharacterDatabase.DirectExecute("UPDATE `mod_guild_level_talent` SET `points` = {} WHERE `node` = {} AND `points` > {}",
        uint32(maxPoints), id, uint32(maxPoints));
    for (uint32 guildId : guilds)
        RefreshGuild(guildId);
    NotifyEveryone();
    return true;
}

bool GuildTalentMgr::MoveNode(uint32 id, int32 x, int32 y)
{
    GuildTalentNode node;
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto itr = _tree.find(id);
        if (itr == _tree.end())
            return false;
        for (auto const& [other, n] : _tree)
            if (other != id && n.x == x && n.y == y)
                return false;
        itr->second.x = x;
        itr->second.y = y;
        node = itr->second;
    }
    SaveNode(node);
    NotifyEveryone();
    return true;
}

bool GuildTalentMgr::SetStart(uint32 id, bool start)
{
    GuildTalentNode node;
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto itr = _tree.find(id);
        if (itr == _tree.end())
            return false;
        itr->second.start = start;
        node = itr->second;
    }
    SaveNode(node);
    NotifyEveryone();
    return true;
}

bool GuildTalentMgr::SetIcon(uint32 id, std::string const& icon)
{
    // A texture path: letters, digits and the separators, nothing to break the query.
    for (char c : icon)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '\\' && c != '/' && c != '-' && c != '.')
            return false;
    GuildTalentNode node;
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto itr = _tree.find(id);
        if (itr == _tree.end())
            return false;
        itr->second.icon = icon;
        node = itr->second;
    }
    SaveNode(node);
    NotifyEveryone();
    return true;
}

bool GuildTalentMgr::DeleteNode(uint32 id)
{
    std::vector<uint32> guilds;
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto itr = _tree.find(id);
        if (itr == _tree.end())
            return false;
        for (uint32 other : itr->second.links)
            if (auto o = _tree.find(other); o != _tree.end())
                o->second.links.erase(id);
        _tree.erase(itr);
        // Its points go back to their guild.
        for (auto& [guildId, state] : _guilds)
            if (state.points.erase(id))
            {
                guilds.push_back(guildId);
                Total(guildId);
            }
    }
    WorldDatabase.DirectExecute("DELETE FROM `mod_guild_level_talent_node` WHERE `id` = {}", id);
    WorldDatabase.DirectExecute("DELETE FROM `mod_guild_level_talent_link` WHERE `node_a` = {} OR `node_b` = {}", id, id);
    CharacterDatabase.DirectExecute("DELETE FROM `mod_guild_level_talent` WHERE `node` = {}", id);
    for (uint32 guildId : guilds)
        RefreshGuild(guildId);
    NotifyEveryone();
    return true;
}

bool GuildTalentMgr::ToggleLink(uint32 a, uint32 b, bool& linked)
{
    if (a == b)
        return false;
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto ia = _tree.find(a);
        auto ib = _tree.find(b);
        if (ia == _tree.end() || ib == _tree.end())
            return false;
        linked = !ia->second.links.count(b);
        if (linked)
        {
            ia->second.links.insert(b);
            ib->second.links.insert(a);
        }
        else
        {
            ia->second.links.erase(b);
            ib->second.links.erase(a);
        }
    }
    uint32 const lo = std::min(a, b);
    uint32 const hi = std::max(a, b);
    if (linked)
        WorldDatabase.DirectExecute("REPLACE INTO `mod_guild_level_talent_link` (`node_a`, `node_b`) VALUES ({}, {})", lo, hi);
    else
        WorldDatabase.DirectExecute("DELETE FROM `mod_guild_level_talent_link` WHERE (`node_a` = {} AND `node_b` = {}) "
            "OR (`node_a` = {} AND `node_b` = {})", lo, hi, hi, lo);
    NotifyEveryone();
    return true;
}

void GuildTalentMgr::Show(ChatHandler* handler, uint32 guildId) const
{
    uint32 const earned = Earned(guildId);
    uint32 const spent = Spent(guildId);
    Say(handler, GUILD_LEVEL_STR_TALENT_SUMMARY, earned, spent, earned > spent ? earned - spent : 0);

    GuildTalentState const state = State(guildId);
    std::map<uint32, GuildTalentNode> const tree = Tree();
    for (auto const& [nodeId, points] : state.points)
    {
        auto node = tree.find(nodeId);
        if (node == tree.end())
            continue;
        Say(handler, GUILD_LEVEL_STR_TALENT_LINE, nodeId, uint32(points), uint32(node->second.maxPoints),
            Acore::StringFormat("{:g}", node->second.amount * points), NameOf(node->second.effect));
    }
    Say(handler, GUILD_LEVEL_STR_TALENT_PRICE, ResetPrice(guildId));
}

bool GuildTalentMgr::CheckProc(Player* player, ProcEventInfo& /*eventInfo*/) const
{
    return player && _config.enable && player->CustomData.Get<GuildTalentPlayerData>(PLAYER_DATA);
}

void GuildTalentMgr::OnProc(Player* player, ProcEventInfo& eventInfo)
{
    GuildTalentPlayerData* data = player ? player->CustomData.Get<GuildTalentPlayerData>(PLAYER_DATA) : nullptr;
    if (!data)
        return;

    uint32 const type = eventInfo.GetTypeMask();
    uint32 const hit = eventInfo.GetHitMask();
    uint32 const now = uint32(GameTime::GetGameTimeMS().count());

    // The amount of an effect when it procs now (its chance, past its
    // cooldown), else 0; a proc starts the cooldown.
    auto roll = [&](GuildTalentEffect effect) -> float
    {
        float const amount = data->totals.amount[effect];
        float const chance = data->totals.chance[effect];
        if (amount <= 0.0f || chance <= 0.0f || now < data->nextProc[effect] || !roll_chance_f(chance))
            return 0.0f;
        data->nextProc[effect] = now + data->totals.cooldown[effect] * IN_MILLISECONDS;
        return amount;
    };
    auto self = [player](uint32 spellId, int32 amount)
    {
        player->CastCustomSpell(player, spellId, &amount, nullptr, nullptr, true);
    };
    auto resource = [&]()
    {
        float const a = roll(GT_PROC_RESOURCE);
        if (a <= 0.0f)
            return;
        switch (player->getPowerType())
        {
            case POWER_RAGE:        player->ModifyPower(POWER_RAGE, int32(a * 10)); break;
            case POWER_RUNIC_POWER: player->ModifyPower(POWER_RUNIC_POWER, int32(a * 10)); break;
            case POWER_ENERGY:      player->ModifyPower(POWER_ENERGY, int32(a)); break;
            case POWER_MANA:        player->ModifyPower(POWER_MANA, int32(player->GetMaxPower(POWER_MANA) * a / 5.0f / 100.0f)); break;
            default: break;
        }
    };

    constexpr uint32 TAKEN_MELEE = PROC_FLAG_TAKEN_MELEE_AUTO_ATTACK | PROC_FLAG_TAKEN_SPELL_MELEE_DMG_CLASS;
    constexpr uint32 DONE_MELEE = PROC_FLAG_DONE_MELEE_AUTO_ATTACK | PROC_FLAG_DONE_SPELL_MELEE_DMG_CLASS;
    constexpr uint32 DONE_ATTACK = DONE_MELEE | PROC_FLAG_DONE_RANGED_AUTO_ATTACK | PROC_FLAG_DONE_SPELL_RANGED_DMG_CLASS;
    constexpr uint32 DONE_SPELL = PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_POS | PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG |
        PROC_FLAG_DONE_SPELL_NONE_DMG_CLASS_POS | PROC_FLAG_DONE_SPELL_NONE_DMG_CLASS_NEG;

    // STRUCK: a block, a parry, a dodge.
    if (type & TAKEN_MELEE)
    {
        if (hit & (PROC_HIT_BLOCK | PROC_HIT_FULL_BLOCK))
            if (float const a = roll(GT_PROC_SHIELD))
                self(GUILD_TALENT_SPELL_SHIELD, int32(player->GetMaxHealth() * a / 100.0f));
        if (hit & PROC_HIT_PARRY)
        {
            Unit* attacker = eventInfo.GetActor();
            if (attacker && attacker != player && attacker->IsAlive())
                if (float const a = roll(GT_PROC_RIPOSTE))
                {
                    int32 damage = int32(player->GetMaxHealth() * a / 100.0f);
                    player->CastCustomSpell(attacker, GUILD_TALENT_SPELL_RIPOSTE, &damage, nullptr, nullptr, true);
                }
        }
        if (hit & PROC_HIT_DODGE)
            if (float const a = roll(GT_PROC_EVASION))
                self(GUILD_TALENT_SPELL_EVASION, -int32(std::lround(a)));
        return;
    }

    // STRIKING: a hit that landed.
    if ((type & DONE_ATTACK) && (hit & (PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB)))
    {
        if (float const a = roll(GT_PROC_SUNDER))
            self(GUILD_TALENT_SPELL_SUNDER, int32(std::lround(a)));
        if (float const a = roll(GT_PROC_FERVOR))
            self(GUILD_TALENT_SPELL_FERVOR, int32(std::lround(a)));
        if (type & DONE_MELEE)
            if (float const a = roll(GT_PROC_EXTRA_HIT))
                player->AddExtraAttacks(std::max<uint32>(uint32(std::lround(a)), 1));
        resource();
        return;
    }

    // CASTING: a spell of the player's own (triggered spells do not proc).
    if ((type & DONE_SPELL) && eventInfo.GetSpellInfo())
    {
        uint8 const playerClass = player->getClass();
        if (roll(GT_PROC_FREE_SPELL) > 0.0f)
            player->CastSpell(player, GUILD_TALENT_SPELL_FREE_BASE + playerClass, true);
        if (roll(GT_PROC_INSTANT_SPELL) > 0.0f)
            player->CastSpell(player, GUILD_TALENT_SPELL_INSTANT_BASE + playerClass, true);
        resource();
    }
}
