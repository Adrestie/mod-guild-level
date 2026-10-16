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
 * mod-guild-level — the passive perks of levels 1 to 20.
 *
 * Every member of a guild gets the perks of its current level, alone or in a
 * group. Each perk is a list of steps in the configuration ("1:5 11:10": +5
 * from level 1, +10 more from level 11); the steps reached add up.
 *
 * Most perks are read in core hooks when they are needed. Honor and mounted
 * speed have no hook: they ride on a hidden server aura, GUILD_PERK_AURA,
 * whose amounts are set from the level and refreshed when it changes.
 */

#ifndef MOD_GUILD_PERKS_H_
#define MOD_GUILD_PERKS_H_

#include "DataMap.h"
#include "Define.h"
#include "ObjectGuid.h"
#include <array>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

class Aura;
class ChatHandler;
class Creature;
class Item;
struct Loot;
class Player;

// The hidden aura (data/sql/world, spell_dbc): honor, ground and flying
// mounted speed, in percent. Tranche 92 of the WoW-mods registry.
constexpr uint32 GUILD_PERK_AURA = 92000;
constexpr uint32 SPELL_HEARTHSTONE = 8690;

enum GuildPerk : uint8
{
    GUILD_PERK_XP = 0,          // % experience
    GUILD_PERK_DURABILITY,      // damaged equipped items mended by 1 point per period
    GUILD_PERK_HONOR,           // % honor
    GUILD_PERK_HEARTHSTONE,     // minutes off the Hearthstone cooldown
    GUILD_PERK_GATHERING,       // extra items on a lucky gathering
    GUILD_PERK_VENDOR,          // % off vendor prices and repairs
    GUILD_PERK_GOLD_TO_BANK,    // % of looted gold added to the guild bank
    GUILD_PERK_MOUNT_SPEED,     // % mounted speed, ground and flying
    GUILD_PERK_REPUTATION,      // % reputation
    GUILD_PERK_RESURRECTION,    // 1 = full health and mana when a guildmate resurrects
    GUILD_PERK_CONSUMABLES,     // minutes added to flasks, elixirs and food buffs
    GUILD_PERK_COUNT
};

struct GuildPerkConfig
{
    bool enable = true;
    // Per perk: (guild level, amount) steps, sorted by level.
    std::array<std::vector<std::pair<uint8, float>>, GUILD_PERK_COUNT> steps;
    float gatheringChance = 20.0f;      // % per gathering
    uint32 durabilityInterval = 60;     // seconds between two mendings
};

// What the perks keep on a character, on Player::CustomData.
struct GuildPerkPlayerData : public DataMap::Base
{
    ObjectGuid lastGatherLoot;          // the node or corpse already rolled
    uint32 mendTimer = 0;               // ms since the last mending
    // Consumable auras just applied, lengthened on the next player tick: the
    // cast sets their duration again after applying them.
    std::vector<uint32> pendingConsumables;
};

class GuildPerkMgr
{
public:
    static GuildPerkMgr* instance();

    void LoadConfig();
    [[nodiscard]] GuildPerkConfig const& Config() const { return _config; }

    // The amount of a perk at a guild level, steps added up.
    [[nodiscard]] float Amount(GuildPerk perk, uint8 level) const;
    // The same for a player, from his guild's current level; 0 out of a guild.
    [[nodiscard]] float Amount(GuildPerk perk, Player const* player) const;

    // The hidden aura of a member online, set to his guild's level. World thread.
    void Apply(Player* player);
    void RefreshGuild(uint32 guildId);
    // A member joined or left: refreshed on the next world tick, when his
    // guild id is settled.
    void QueueRefresh(ObjectGuid guid);
    void Update();

    // The hooks.
    void OnGiveXP(Player* player, uint32& amount);
    void OnGiveReputation(Player* player, float& amount);
    void OnPriceDiscount(Player const* player, Creature const* creature, float& discount);
    void OnLootMoney(Player* player, Loot* loot);
    void OnLootItem(Player* player, Item* item, ObjectGuid lootGuid);
    void OnResurrect(Player* player, float restorePercent);
    void OnAuraApply(Player* player, Aura* aura);
    void OnHearthstone(Player* player);
    void OnUpdate(Player* player, uint32 diff);

    // .guildlevel perks
    void Show(ChatHandler* handler, uint8 level) const;

private:
    GuildPerkMgr() = default;

    void LengthenConsumables(Player* player, GuildPerkPlayerData* data);

    GuildPerkConfig _config;

    std::mutex _lock;
    std::set<ObjectGuid> _refresh;
};

#define sGuildPerkMgr GuildPerkMgr::instance()

#endif
