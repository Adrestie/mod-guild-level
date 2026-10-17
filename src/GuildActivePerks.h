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
 * mod-guild-level — the active perks of levels 21 to 25.
 *
 * Each is a spell of the General tab of the spellbook, which the members of
 * a guild of the level asked know (and only they): casting it does what the
 * player command `.gperk raid|rez|bank|vendor|home` does. Each perk has its own
 * cooldown, per character, kept in the characters database across restarts
 * and shown on the spell's button.
 */

#ifndef MOD_GUILD_ACTIVE_PERKS_H_
#define MOD_GUILD_ACTIVE_PERKS_H_

#include "DataMap.h"
#include "DatabaseEnvFwd.h"
#include "Define.h"
#include "ObjectGuid.h"
#include <array>

class ChatHandler;
class Player;

// The supplier summoned by `.gperk vendor` (data/sql/world): tranche 92.
constexpr uint32 GUILD_SUPPLIER_ENTRY = 92001;
// The Guild Chests summoned by `.gperk bank` (data/sql/world): Cataclysm's
// Mobile Banking chest, converted for 3.3.5a, Alliance and Horde.
constexpr uint32 GUILD_CHEST_ALLIANCE_ENTRY = 920000;
constexpr uint32 GUILD_CHEST_HORDE_ENTRY = 920001;

// The spell of each perk, in GuildActivePerk order: server rows in spell_dbc
// (data/sql/world), client rows of Spell.dbc in installer.json. Tranche 92.
constexpr std::array<uint32, 5> GUILD_ACTIVE_SPELLS = { 92021, 92022, 92023, 92024, 92025 };

enum GuildActivePerk : uint8
{
    GUILD_ACTIVE_RAID = 0,      // summon the group
    GUILD_ACTIVE_REZ,           // resurrect the dead of the group
    GUILD_ACTIVE_BANK,          // a guild vault for a while
    GUILD_ACTIVE_VENDOR,        // a reagent vendor and repairer for a while
    GUILD_ACTIVE_HOME,          // to the guild house, once there is one
    GUILD_ACTIVE_COUNT
};

struct GuildActivePerkConfig
{
    std::array<uint8, GUILD_ACTIVE_COUNT> level = { 21, 22, 23, 24, 25 };
    std::array<uint32, GUILD_ACTIVE_COUNT> cooldown = { 3600, 600, 3600, 3600, 3600 };   // seconds
    uint32 rezPercent = 35;             // health and mana of the resurrection offered
    uint32 bankEntryAlliance = GUILD_CHEST_ALLIANCE_ENTRY;
    uint32 bankEntryHorde = GUILD_CHEST_HORDE_ENTRY;
    uint32 bankDuration = 300;          // seconds
    uint32 vendorEntry = GUILD_SUPPLIER_ENTRY;
    uint32 vendorDuration = 300;        // seconds
};

// The cooldowns of a character, on Player::CustomData: the time each perk is
// ready again (Unix seconds), loaded at login.
struct GuildActivePerkPlayerData : public DataMap::Base
{
    std::array<int64, GUILD_ACTIVE_COUNT> readyAt{};
};

class GuildActivePerkMgr
{
public:
    static GuildActivePerkMgr* instance();

    void LoadConfig();
    [[nodiscard]] GuildActivePerkConfig const& Config() const { return _config; }

    void Load(Player* player);                                        // login
    void DeleteCharacter(CharacterDatabaseTransaction trans, uint32 guid);

    // `.gperk <perk>`: every check, the perk itself, its cooldown. Tells the
    // player what happened.
    void Use(ChatHandler* handler, GuildActivePerk perk);

    // The active perks at a guild level: locked, ready, or the time left for
    // `player` (none given: no cooldown shown).
    void List(ChatHandler* handler, uint8 guildLevel, Player* player) const;

    // Every cooldown of a character back to ready, online or not.
    void ResetCooldowns(ObjectGuid guid);

    // Seconds before the perk is ready again for this character.
    [[nodiscard]] uint32 Remaining(Player* player, GuildActivePerk perk) const;

    // The spells of the perks the character's guild has, and those only.
    void SyncSpells(Player* player);

    // The perk a spell casts, -1 for none.
    [[nodiscard]] static int PerkOfSpell(uint32 spellId);

private:
    GuildActivePerkMgr() = default;

    // Each returns false when nothing was done: no cooldown then.
    bool SummonGroup(ChatHandler* handler, Player* player);
    bool ResurrectGroup(ChatHandler* handler, Player* player);
    bool SummonVault(ChatHandler* handler, Player* player);
    bool SummonSupplier(ChatHandler* handler, Player* player);
    bool GoHome(ChatHandler* handler, Player* player);

    // The cooldown on the perk's spell button, as the server holds it.
    void SendCooldown(Player* player, GuildActivePerk perk, uint32 seconds) const;

    GuildActivePerkConfig _config;
};

#define sGuildActivePerkMgr GuildActivePerkMgr::instance()

#endif
