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

/*
 * mod-guild-level — the guild talents.
 *
 * From level GuildLevel.Talents.FromLevel, every level and Paragon level gives
 * the guild GuildLevel.Talents.PointsPerLevel points. The guild master and the
 * officers (ranks up to GuildLevel.Talents.Rank) spend them in a tree of
 * nodes, drawn by a GM in the talent window's edit mode and kept in the world
 * database. A node holds one effect, an amount per point, and as many points
 * as its maximum; it can be bought when it is a start node or linked to a node
 * that holds a point. Resetting gives every point back and costs gold from the
 * guild bank.
 *
 * Every member of the guild gets the effects, computed here: statistics as
 * flat modifiers, damage and healing through the unit hooks, the rest on the
 * guild's aura (spell 92026), whose tooltip lists them all (the window's
 * client file adds the lines).
 */

#ifndef MOD_GUILD_TALENTS_H_
#define MOD_GUILD_TALENTS_H_

#include "DataMap.h"
#include "Define.h"
#include "GuildPerks.h"
#include "ObjectGuid.h"

#include <array>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class ChatHandler;
class Player;
class SpellInfo;
class Unit;

// The guild's aura, visible, with the effects that need one: run speed (0)
// and maximum health (1). Its tooltip lists every effect.
constexpr uint32 GUILD_TALENT_AURA = 92026;

// THE EFFECTS. The number is what the world table keeps: never renumber.
//   1-9    main statistics          10-39  secondary statistics
//   40-79  special effects
enum GuildTalentEffect : uint8
{
    GT_NONE = 0,

    GT_STAMINA = 1,
    GT_STRENGTH = 2,
    GT_AGILITY = 3,
    GT_INTELLECT = 4,
    GT_SPIRIT = 5,

    GT_CRIT = 10,               // rating, melee, ranged and spells
    GT_HASTE = 11,              // rating, melee, ranged and spells
    GT_HIT = 12,                // rating, melee, ranged and spells
    GT_EXPERTISE = 13,          // rating
    GT_ARMOR_PEN = 14,          // rating
    GT_ATTACK_POWER = 15,       // melee and ranged
    GT_SPELL_POWER = 16,        // damage and healing
    GT_MP5 = 17,                // mana every 5 seconds
    GT_DEFENSE = 18,            // rating
    GT_DODGE = 19,              // rating
    GT_PARRY = 20,              // rating
    GT_BLOCK = 21,              // rating
    GT_RESILIENCE = 22,         // rating
    GT_ARMOR = 23,
    GT_SPELL_PEN = 24,
    GT_RESISTANCE = 25,         // fire, nature, frost, shadow and arcane
    GT_HEALTH = 26,
    GT_MANA = 27,

    GT_DAMAGE_PCT = 40,         // % damage done
    GT_HEALING_PCT = 41,        // % healing done
    GT_DAMAGE_TAKEN_PCT = 42,   // % less damage taken
    GT_HEALTH_PCT = 43,         // % maximum health (the aura)
    GT_RUN_SPEED_PCT = 44,      // % run speed (the aura)
    GT_XP_PCT = 45,             // added to the level perks
    GT_HONOR_PCT = 46,
    GT_REPUTATION_PCT = 47,
    GT_MOUNT_SPEED_PCT = 48,
    GT_VENDOR_PCT = 49,
    GT_GOLD_TO_BANK_PCT = 50,
    GT_GATHERING_PCT = 51,      // % chance of a lucky gathering
    GT_CONSUMABLE_MIN = 52,     // minutes on flasks, elixirs and food buffs
    GT_HEARTHSTONE_MIN = 53,    // minutes off the Hearthstone

    GT_EFFECT_END = 54
};

enum GuildTalentKind : uint8
{
    GT_KIND_MAIN = 0,
    GT_KIND_SECONDARY = 1,
    GT_KIND_SPECIAL = 2
};

// The effects that exist, in the order the editor lists them.
std::vector<GuildTalentEffect> const& GuildTalentEffects();
bool IsGuildTalentEffect(uint32 effect);
GuildTalentKind KindOf(GuildTalentEffect effect);

struct GuildTalentNode
{
    uint32 id = 0;
    int32 x = 0;
    int32 y = 0;
    GuildTalentEffect effect = GT_NONE;
    float amount = 0.0f;                // per point
    uint8 maxPoints = 1;
    bool start = false;
    std::string icon;                   // empty: the effect's own
    std::set<uint32> links;
};

struct GuildTalentConfig
{
    bool enable = true;
    uint8 fromLevel = 10;
    uint32 pointsPerLevel = 1;
    uint8 rank = 1;                     // the highest guild rank id that spends points
    uint32 resetCost = 5000;            // gold
    float resetIncrease = 10.0f;        // % of resetCost added by each reset
    float resetDecrease = 10.0f;        // % of resetCost taken off each week
};

// What a guild bought, and its reset price.
struct GuildTalentState
{
    std::map<uint32, uint8> points;     // node -> points
    uint32 nextPrice = 0;               // gold, as of lastReset; 0: never reset
    uint32 lastReset = 0;               // unix time
};

// What the talents put on a member online, to be taken off as such.
struct GuildTalentPlayerData : public DataMap::Base
{
    std::array<float, GT_EFFECT_END> applied{};
};

enum class GuildTalentResult
{
    Ok,
    Disabled,
    NoGuild,
    NotAllowed,     // rank
    NoNode,
    Full,           // the node holds its maximum
    NotLinked,
    NoPoints,
    NothingToReset,
    NoMoney         // the guild bank
};

class GuildTalentMgr
{
public:
    static GuildTalentMgr* instance();

    void LoadConfig();
    void LoadTree();                    // world database, startup and `.gtalent reload`
    void LoadGuilds();                  // characters database, startup

    [[nodiscard]] GuildTalentConfig const& Config() const { return _config; }

    // The tree, for the commands and the window.
    [[nodiscard]] std::map<uint32, GuildTalentNode> Tree() const;

    // Points a guild has earned, spent, and the price of its next reset.
    [[nodiscard]] uint32 Earned(uint32 guildId) const;
    [[nodiscard]] uint32 Spent(uint32 guildId) const;
    [[nodiscard]] uint32 ResetPrice(uint32 guildId) const;
    [[nodiscard]] GuildTalentState State(uint32 guildId) const;
    [[nodiscard]] bool CanSpend(Player const* player) const;

    // A point into a node, by a member. Effects follow for the guild online.
    GuildTalentResult Buy(Player* player, uint32 nodeId);
    // Every point back; the price comes out of the guild bank.
    GuildTalentResult Reset(Player* player, uint32& price);

    // The amount of an effect for a member; 0 out of a guild.
    [[nodiscard]] float Amount(Player const* player, GuildTalentEffect effect) const;
    // The same for a guild.
    [[nodiscard]] float Amount(uint32 guildId, GuildTalentEffect effect) const;

    // The effects on a member online: statistics, the aura. World or map thread
    // of the player.
    void Apply(Player* player);
    void RefreshGuild(uint32 guildId);

    // The hooks.
    void ModifyDamage(Unit* target, Unit* attacker, uint32& damage);
    void ModifyHeal(Unit* healer, uint32& heal);

    // GM edits of the tree, saved at once. Each returns false when the node is
    // unknown or the values do not hold.
    uint32 AddNode(int32 x, int32 y, GuildTalentEffect effect, float amount, uint8 maxPoints);
    bool SetNode(uint32 id, GuildTalentEffect effect, float amount, uint8 maxPoints);
    bool MoveNode(uint32 id, int32 x, int32 y);
    bool SetStart(uint32 id, bool start);
    bool SetIcon(uint32 id, std::string const& icon);
    bool DeleteNode(uint32 id);
    // Links or unlinks two nodes; returns false when one is unknown. linked
    // tells which.
    bool ToggleLink(uint32 a, uint32 b, bool& linked);

    void DeleteGuild(uint32 guildId);
    // Every point of a guild back, for nothing (GM).
    void ClearGuild(uint32 guildId);

    // .gtalent, in the chat.
    void Show(ChatHandler* handler, uint32 guildId) const;

    // Tells the window of every member online that the guild's talents moved.
    void NotifyGuild(uint32 guildId) const;
    void NotifyEveryone() const;

private:
    GuildTalentMgr() = default;

    float AmountLocked(uint32 guildId, GuildTalentEffect effect) const;
    void SaveNode(GuildTalentNode const& node) const;

    GuildTalentConfig _config;

    mutable std::mutex _lock;
    std::map<uint32, GuildTalentNode> _tree;
    std::unordered_map<uint32, GuildTalentState> _guilds;
    // Per guild, the effects summed from its points: read on every hit.
    std::unordered_map<uint32, std::array<float, GT_EFFECT_END>> _totals;

    void Total(uint32 guildId);         // under _lock
};

#define sGuildTalentMgr GuildTalentMgr::instance()

#endif
