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
 * mod-guild-level — the guild house, from Paragon 1.
 *
 * WHERE: the two PvP halls the game left empty in 2.0, Champions' Hall (map
 * 449) for the Alliance and the Hall of Legends (map 450) for the Horde.
 *
 * ONE HOUSE PER GUILD, BY PHASES. An instance per guild cannot work: the core
 * refuses a second player into an instance someone of another group is in
 * (InstanceMap::CannotEnter), and the core is never changed. So the halls stay
 * open maps, and a guild whose house is open owns one of the hall's 31 phases
 * (bits 1 to 31 of the phase mask; bit 0, the normal phase, holds nothing of
 * any guild). Its members, its creatures and its objects live in that phase
 * only, and no guild sees another. The phase goes back to the pool once the
 * house has been empty for a while; what stands in it is in the database and
 * comes back next time.
 *
 * WHAT STANDS IN IT comes from `mod_guild_level_house_spawn` (world): rows of
 * creatures and gameobjects, each unlocked at a Paragon level. They are placed
 * when the house opens, and the ones a new Paragon level unlocks appear at once
 * in an open house.
 *
 * WHO: the members of the guild, and game masters. Anyone else is sent back.
 *
 * THREADS: entering and leaving run in the world thread (commands) or in the
 * hall's map thread (the map hooks). Everything that touches the map --
 * phases, spawns, sending someone out -- is done from the map's own hooks;
 * the tables they share are under one lock.
 */

#ifndef MOD_GUILD_HOUSE_H_
#define MOD_GUILD_HOUSE_H_

#include "Define.h"
#include "ObjectGuid.h"
#include <array>
#include <map>
#include <mutex>
#include <set>
#include <vector>

class ChatHandler;
class Map;
class Player;

constexpr uint32 GUILD_HOUSE_MAP_ALLIANCE = 449;    // Champions' Hall
constexpr uint32 GUILD_HOUSE_MAP_HORDE = 450;       // Hall of Legends
constexpr uint8 GUILD_HOUSE_SLOTS = 31;             // phase bits 1..31 of a hall

struct GuildHouseConfig
{
    bool enable = true;
    uint8 paragon = 1;                  // the Paragon level that opens the house
    uint32 emptyDelay = 60;             // seconds an empty house stays open
    std::array<float, 4> alliance = { -0.299116f, 4.39264f, -0.255884f, 0.0f };
    std::array<float, 4> horde = { 221.322f, 74.4933f, 25.7195f, 0.484836f };
};

// One thing standing in the house (`mod_guild_level_house_spawn`).
struct GuildHouseSpawn
{
    uint32 id;
    uint32 map;
    uint8 paragon;
    uint8 kind;                         // 0 creature, 1 gameobject
    uint32 entry;
    float x, y, z, o;
};

class GuildHouseMgr
{
public:
    static GuildHouseMgr* instance();

    void LoadConfig();
    void LoadFromDB();                  // the spawns
    [[nodiscard]] GuildHouseConfig const& Config() const { return _config; }

    [[nodiscard]] static bool IsHouseMap(uint32 mapId)
    {
        return mapId == GUILD_HOUSE_MAP_ALLIANCE || mapId == GUILD_HOUSE_MAP_HORDE;
    }

    // Into the guild's house; the way back is kept (Leave). False, having told
    // the player why, when it cannot be done.
    bool Enter(ChatHandler* handler, Player* player);
    // Back where the player came from.
    bool Leave(ChatHandler* handler, Player* player);

    // The hooks, in the hall's map thread.
    void OnPlayerEnterMap(Map* map, Player* player);
    void OnPlayerLeaveMap(Map* map, Player* player);
    void OnMapUpdate(Map* map, uint32 diff);
    // World thread.
    void OnGuildChanged(uint32 guildId);        // a Paragon level moved
    void OnMemberLeft(ObjectGuid player);       // left or was removed from his guild
    void DeleteGuild(uint32 guildId);

    // GM: the spawns of a hall, and their edition. Saved in the world table,
    // and every open house of the hall is furnished again at once.
    void ListSpawns(ChatHandler* handler, uint32 mapId);
    bool MoveSpawn(uint32 id, uint32 mapId, float x, float y, float z, float o);
    // The spawn a house's creature or object was placed from, 0 if none.
    uint32 SpawnOf(ObjectGuid guid);
    uint32 AddSpawn(uint32 mapId, uint8 paragon, uint8 kind, uint32 entry, float x, float y, float z, float o);
    bool DeleteSpawn(uint32 id);

private:
    GuildHouseMgr() = default;

    struct Slot
    {
        uint32 guildId = 0;
        uint32 emptyFor = 0;                    // ms with nobody inside
        bool furnish = true;                    // spawns to look at again
        bool rebuild = false;                   // take everything away, then furnish
        std::set<uint32> placed;                // spawn ids standing
        std::vector<ObjectGuid> objects;        // what was placed, to take away
        std::map<ObjectGuid, uint32> spawnOf;   // and the spawn each one came from
    };

    struct Hall
    {
        std::array<Slot, GUILD_HOUSE_SLOTS> slots{};
        uint32 timer = 0;                       // ms since the last look at the slots
    };

    [[nodiscard]] static uint32 MaskOf(uint8 slot) { return 1u << (slot + 1); }
    [[nodiscard]] Hall& HallOf(uint32 mapId) { return mapId == GUILD_HOUSE_MAP_HORDE ? _horde : _alliance; }
    // The guild's slot in the hall, taken if it has none. -1: the hall is full.
    int SlotOf(uint32 mapId, uint32 guildId, bool take);
    [[nodiscard]] uint8 ParagonOf(uint32 guildId) const;
    void Furnish(Map* map, Slot& slot, uint8 index);
    void Empty(Map* map, Slot& slot);
    void RebuildHall(uint32 mapId);             // under _lock

    GuildHouseConfig _config;
    std::vector<GuildHouseSpawn> _spawns;

    std::mutex _lock;
    Hall _alliance, _horde;
    std::set<ObjectGuid> _sendOut;               // to send out of the house
};

#define sGuildHouseMgr GuildHouseMgr::instance()

#endif
