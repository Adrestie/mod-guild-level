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
 * From Paragon level GuildLevel.Talents.FromParagon, every Paragon level gives
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
class ProcEventInfo;
class SpellInfo;
class Unit;

// The guild's aura, visible, with the effects that need one: run speed (0),
// maximum health (1) and critical damage (2). Its tooltip lists every effect.
constexpr uint32 GUILD_TALENT_AURA = 92026;
// Hidden, passive: the procs of the special effects (spell_guild_talent_procs).
constexpr uint32 GUILD_TALENT_PROC_AURA = 92027;

// What the special effects cast (data/sql/world/guild_level_09_perk_spells.sql).
enum GuildTalentSpells : uint32
{
    GUILD_TALENT_SPELL_FERVOR       = 92030,    // % damage done, 10 s
    GUILD_TALENT_SPELL_SHIELD       = 92031,    // absorbs, 10 s
    GUILD_TALENT_SPELL_EVASION      = 92032,    // % less damage taken, 5 s, 5 stacks
    GUILD_TALENT_SPELL_RIPOSTE      = 92033,    // physical damage to the attacker
    GUILD_TALENT_SPELL_SUNDER       = 92034,    // % armor ignored, 3 s
    GUILD_TALENT_SPELL_FREE_BASE    = 92040,    // + class: next spell costs nothing, 15 s
    GUILD_TALENT_SPELL_INSTANT_BASE = 92060     // + class: next spell is instant, 15 s
};

// THE EFFECTS. The number is what the world table keeps: never renumber.
//   1-9    main statistics          10-39  secondary statistics
//   40-59  special effects          60     the origin of the tree
//   61-79  special effects with a chance (and an internal cooldown)
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
    GT_HEALING_POWER = 28,      // healing only
    GT_SPELL_DAMAGE = 29,       // damage only

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
    GT_CRIT_DAMAGE_PCT = 54,    // % critical damage (the aura)

    GT_ORIGIN = 60,             // no effect: always held, links the tree's start

    GT_PROC_SUNDER = 61,        // a hit: % of the target's armor ignored, 3 s
    GT_PROC_FERVOR = 62,        // a hit: % damage done, 10 s
    GT_PROC_EXTRA_HIT = 63,     // a melee hit: as many extra attacks
    GT_PROC_SHIELD = 64,        // a block: a shield of % of maximum health, 10 s
    GT_PROC_RIPOSTE = 65,       // a parry: % of maximum health as damage to the attacker
    GT_PROC_EVASION = 66,       // a dodge: % less damage taken, 5 s, up to 5 times
    GT_PROC_FREE_SPELL = 67,    // a spell: the next one costs nothing
    GT_PROC_INSTANT_SPELL = 68, // a spell: the next one is instant
    GT_PROC_RESOURCE = 69,      // a hit or a spell: rage, energy or runic power (or a fifth as % of mana)

    GT_EFFECT_END = 70
};

enum GuildTalentKind : uint8
{
    GT_KIND_MAIN = 0,
    GT_KIND_SECONDARY = 1,
    GT_KIND_SPECIAL = 2,
    GT_KIND_ORIGIN = 3
};

constexpr uint8 GUILD_TALENT_TIERS = 3;

// The effects that exist, in the order the editor lists them.
std::vector<GuildTalentEffect> const& GuildTalentEffects();
bool IsGuildTalentEffect(uint32 effect);
GuildTalentKind KindOf(GuildTalentEffect effect);
bool IsProc(GuildTalentEffect effect);

struct GuildTalentNode
{
    uint32 id = 0;
    int32 x = 0;
    int32 y = 0;
    GuildTalentEffect effect = GT_NONE;
    float amount = 0.0f;                // per point
    uint8 maxPoints = 1;
    float chance = 0.0f;                // %, the special effects with a chance
    uint32 cooldown = 0;                // seconds between two procs
    uint8 tier = 1;                     // 1 to GUILD_TALENT_TIERS
    bool start = false;
    std::string icon;                   // empty: the effect's own
    std::set<uint32> links;
};

struct GuildTalentConfig
{
    bool enable = true;
    uint8 fromParagon = 10;             // the first Paragon level that gives points
    uint32 pointsPerLevel = 1;
    uint8 rank = 1;                     // the highest guild rank id that spends points
    uint32 resetCost = 5000;            // gold
    float resetIncrease = 10.0f;        // % of resetCost added by each reset
    float resetDecrease = 10.0f;        // % of resetCost taken off each week
    // Points the guild must have spent before a node of each tier is bought.
    std::array<uint32, GUILD_TALENT_TIERS> tierPoints{ 0, 10, 25 };
};

// What a guild's points give: amounts, and the chance and cooldown of the
// effects that proc.
struct GuildTalentTotals
{
    std::array<float, GT_EFFECT_END> amount{};
    std::array<float, GT_EFFECT_END> chance{};
    std::array<uint32, GT_EFFECT_END> cooldown{};
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
    GuildTalentTotals totals;
    std::array<uint32, GT_EFFECT_END> nextProc{};     // game time, ms
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
    Tier,           // not enough points spent for the node's tier
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
    // A member of a guild whose points were given back at startup logs in: told so (each member
    // once, until the next restart).
    void OnLogin(Player* player);

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
    // The hidden proc aura of a member: rolls the special effects.
    bool CheckProc(Player* player, ProcEventInfo& eventInfo) const;
    void OnProc(Player* player, ProcEventInfo& eventInfo);

    // GM edits of the tree, saved at once. Each returns false when the node is
    // unknown or the values do not hold.
    uint32 AddNode(int32 x, int32 y, GuildTalentEffect effect, float amount, uint8 maxPoints,
        float chance, uint32 cooldown, uint8 tier);
    bool SetNode(uint32 id, GuildTalentEffect effect, float amount, uint8 maxPoints,
        float chance, uint32 cooldown, uint8 tier);
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
    // The guilds that had spent more points than they earn, given back free at startup (the rules
    // changed): the members told so are kept beside them.
    std::map<uint32, std::set<ObjectGuid>> _givenBack;
    // Per guild, the effects summed from its points.
    std::unordered_map<uint32, GuildTalentTotals> _totals;

    void Total(uint32 guildId);         // under _lock
};

#define sGuildTalentMgr GuildTalentMgr::instance()

#endif
