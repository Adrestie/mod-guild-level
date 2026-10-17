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

#include "GuildHouse.h"
#include "GuildActivePerks.h"
#include "GuildLevel.h"
#include "GuildPerks.h"

#include "Chat.h"
#include "Common.h"
#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "MapMgr.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QueryResult.h"
#include "SmartAI.h"
#include "StringFormat.h"
#include "TemporarySummon.h"

#include <algorithm>

namespace
{
    template<typename... Args>
    void Say(ChatHandler* handler, uint32 id, Args&&... args)
    {
        handler->SendSysMessage(handler->PGetParseModuleString(GUILD_LEVEL_MODULE, id, std::forward<Args>(args)...));
    }

}

// In front of a dungeon or raid without a way out of its own: where the client
// brings a ghost back to it (Map.dbc), on the floor at the height of the
// trigger that leads in (a citadel has floors above its gate).
static bool EntranceOf(uint32 mapId, WorldLocation& out)
{
    MapEntry const* entry = sMapStore.LookupEntry(mapId);
    int32 entranceMap;
    float x, y;
    if (!entry || !entry->GetEntrancePos(entranceMap, x, y))
        return false;
    Map* map = sMapMgr->CreateBaseMap(uint32(entranceMap));
    if (!map)
        return false;
    float z = INVALID_HEIGHT;
    if (QueryResult result = WorldDatabase.Query("SELECT a.`z` FROM `areatrigger_teleport` t JOIN `areatrigger` a ON a.`entry` = t.`ID` "
            "WHERE t.`target_map` = {} AND a.`map` = {} LIMIT 1", mapId, entranceMap))
        z = map->GetHeight(PHASEMASK_NORMAL, x, y, result->Fetch()[0].Get<float>() + 5.0f, true, 30.0f);
    else
        z = map->GetHeight(PHASEMASK_NORMAL, x, y, MAX_HEIGHT, true, MAX_HEIGHT);
    if (z <= INVALID_HEIGHT)
        return false;
    out.WorldRelocate(uint32(entranceMap), x, y, z + 0.5f, 0.0f);
    return true;
}

GuildHouseMgr* GuildHouseMgr::instance()
{
    static GuildHouseMgr instance;
    return &instance;
}

void GuildHouseMgr::LoadConfig()
{
    GuildHouseConfig c;
    c.enable = sConfigMgr->GetOption<bool>("GuildLevel.House.Enable", true);
    c.paragon = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Paragon", 1), 0, GUILD_PARAGON_MAX));
    c.emptyDelay = sConfigMgr->GetOption<uint32>("GuildLevel.House.EmptyDelay", 60);
    _config = c;
}

void GuildHouseMgr::LoadFromDB()
{
    _spawns.clear();
    if (QueryResult result = WorldDatabase.Query(
            "SELECT `id`, `map`, `paragon`, `kind`, `entry`, `x`, `y`, `z`, `o` FROM `mod_guild_level_house_spawn`"))
    {
        do
        {
            Field* f = result->Fetch();
            GuildHouseSpawn s{ f[0].Get<uint32>(), f[1].Get<uint32>(), f[2].Get<uint8>(), f[3].Get<uint8>(),
                f[4].Get<uint32>(), f[5].Get<float>(), f[6].Get<float>(), f[7].Get<float>(), f[8].Get<float>() };
            if (!IsHouseMap(s.map))
            {
                LOG_ERROR("sql.sql", "mod-guild-level: house spawn {} is on map {}, not a hall: skipped.", s.id, s.map);
                continue;
            }
            _spawns.push_back(s);
        } while (result->NextRow());
    }
    LOG_INFO("server.loading", ">> mod-guild-level: loaded {} guild house spawns", _spawns.size());
}

uint8 GuildHouseMgr::ParagonOf(uint32 guildId) const
{
    GuildLevelState const state = sGuildLevelMgr->Get(guildId);
    return state.level < GUILD_LEVEL_MAX ? 0 : state.paragon;
}

int GuildHouseMgr::SlotOf(uint32 mapId, uint32 guildId, bool take)
{
    Hall& hall = HallOf(mapId);
    for (uint8 i = 0; i < GUILD_HOUSE_SLOTS; ++i)
        if (hall.slots[i].guildId == guildId)
            return i;
    if (!take)
        return -1;
    for (uint8 i = 0; i < GUILD_HOUSE_SLOTS; ++i)
        if (!hall.slots[i].guildId)
        {
            hall.slots[i] = Slot();
            hall.slots[i].guildId = guildId;
            return i;
        }
    return -1;
}

bool GuildHouseMgr::Enter(ChatHandler* handler, Player* player)
{
    if (!_config.enable)
    {
        Say(handler, GUILD_LEVEL_STR_PERKS_DISABLED);
        return false;
    }
    uint32 const guildId = player->GetGuildId();
    if (!guildId)
    {
        Say(handler, GUILD_LEVEL_STR_ACTIVE_NO_GUILD);
        return false;
    }
    if (ParagonOf(guildId) < _config.paragon)
    {
        Say(handler, GUILD_LEVEL_STR_HOUSE_LOCKED, uint32(_config.paragon));
        return false;
    }
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

    bool const horde = player->GetTeamId() == TEAM_HORDE;
    uint32 const mapId = horde ? GUILD_HOUSE_MAP_HORDE : GUILD_HOUSE_MAP_ALLIANCE;
    {
        std::lock_guard<std::mutex> guard(_lock);
        if (SlotOf(mapId, guildId, true) < 0)
        {
            Say(handler, GUILD_LEVEL_STR_HOUSE_FULL);
            return false;
        }
    }

    // The phase is set on arrival (OnPlayerEnterMap): set now, the player
    // would vanish where he still stands.
    if (!IsHouseMap(player->GetMapId()))
    {
        player->SetEntryPoint();
        // The way back: where he stands, or in front of the entrance of the
        // dungeon or raid he is in (the trigger that leads out of it).
        WorldLocation origin(player->GetMapId(), player->GetPositionX(), player->GetPositionY(),
            player->GetPositionZ(), player->GetOrientation());
        if (player->GetMap()->IsDungeon())
        {
            if (AreaTriggerTeleport const* out = sObjectMgr->GetGoBackTrigger(player->GetMapId()))
                origin.WorldRelocate(out->target_mapId, out->target_X, out->target_Y, out->target_Z, out->target_Orientation);
            else if (!EntranceOf(player->GetMapId(), origin))
                origin = player->GetEntryPoint();
        }
        {
            std::lock_guard<std::mutex> guard(_lock);
            _origins[player->GetGUID()] = origin;
        }
        CharacterDatabase.Execute("REPLACE INTO `mod_guild_level_house_origin` (`guid`, `map`, `x`, `y`, `z`, `o`) "
            "VALUES ({}, {}, {}, {}, {}, {})", player->GetGUID().GetCounter(), origin.GetMapId(),
            origin.GetPositionX(), origin.GetPositionY(), origin.GetPositionZ(), origin.GetOrientation());
    }
    auto const& p = horde ? _config.horde : _config.alliance;
    return player->TeleportTo(mapId, p[0], p[1], p[2], p[3]);
}

bool GuildHouseMgr::Leave(ChatHandler* handler, Player* player)
{
    if (!IsHouseMap(player->GetMapId()))
    {
        Say(handler, GUILD_LEVEL_STR_HOUSE_NOT_IN);
        return false;
    }
    SendBack(player);
    return true;
}

void GuildHouseMgr::OnPlayerEnterMap(Map* map, Player* player)
{
    if (!IsHouseMap(map->GetId()))
        return;

    // Inside, the house's spell takes him back out, whatever its cooldown,
    // which counts for going in only: shown again on the way out.
    sGuildActivePerkMgr->HideCooldown(player, GUILD_ACTIVE_HOME);

    // A game master keeps his phase: `.modify phase` shows him any house.
    if (player->IsGameMaster())
        return;

    uint32 const guildId = player->GetGuildId();
    bool const allowed = _config.enable && guildId && ParagonOf(guildId) >= _config.paragon;

    std::lock_guard<std::mutex> guard(_lock);
    int const slot = allowed ? SlotOf(map->GetId(), guildId, true) : -1;
    if (slot < 0)
    {
        // Not his house, or no room left: out on the next update of the map.
        _sendOut.insert(player->GetGUID());
        return;
    }

    Slot& s = HallOf(map->GetId()).slots[slot];
    s.emptyFor = 0;
    s.furnish = true;
    player->SetPhaseMask(MaskOf(uint8(slot)), true);
}

void GuildHouseMgr::OnPlayerLeaveMap(Map* map, Player* player)
{
    if (!IsHouseMap(map->GetId()))
        return;
    sGuildActivePerkMgr->ShowCooldown(player, GUILD_ACTIVE_HOME);
    // Nothing of a house's phase follows the player out.
    if (!player->IsGameMaster() && player->GetPhaseMask() != PHASEMASK_NORMAL)
        player->SetPhaseMask(PHASEMASK_NORMAL, false);
}

void GuildHouseMgr::Furnish(Map* map, Slot& slot, uint8 index)
{
    uint8 const paragon = ParagonOf(slot.guildId);
    uint32 const mask = MaskOf(index);
    for (GuildHouseSpawn const& s : _spawns)
    {
        if (s.map != map->GetId() || s.paragon > paragon || slot.placed.count(s.id))
            continue;

        // On the floor: the row's height comes from the navigation mesh or a
        // GM's feet, the collision of the hall says where the floor really is.
        float z = s.z;
        float const ground = map->GetHeight(PHASEMASK_NORMAL, s.x, s.y, s.z + 2.0f, true, 6.0f);
        if (ground > INVALID_HEIGHT)
            z = ground;

        if (s.kind == 0)
        {
            if (TempSummon* creature = map->SummonCreature(s.entry, Position(s.x, s.y, z, s.o)))
            {
                // Standing at its place: no patrol of its own (Katherine Lee
                // walks the streets of Dalaran from her script).
                if (SmartAI* ai = dynamic_cast<SmartAI*>(creature->AI()))
                    ai->StopPath();
                creature->GetMotionMaster()->Clear();
                creature->GetMotionMaster()->MoveIdle();
                creature->SetHomePosition(s.x, s.y, z, s.o);
                creature->SetFacingTo(s.o);
                creature->SetPhaseMask(mask, true);
                slot.objects.push_back(creature->GetGUID());
                slot.spawnOf[creature->GetGUID()] = s.id;
                slot.placed.insert(s.id);
            }
        }
        else if (GameObject* go = map->SummonGameObject(s.entry, s.x, s.y, z, s.o, 0.0f, 0.0f, 0.0f, 0.0f, 0))
        {
            go->SetPhaseMask(mask, true);
            slot.objects.push_back(go->GetGUID());
            slot.spawnOf[go->GetGUID()] = s.id;
            slot.placed.insert(s.id);
        }
    }
}

void GuildHouseMgr::Empty(Map* map, Slot& slot)
{
    for (ObjectGuid const& guid : slot.objects)
    {
        if (guid.IsGameObject())
        {
            if (GameObject* go = map->GetGameObject(guid))
            {
                go->SetRespawnTime(0);
                go->Delete();
            }
        }
        else if (Creature* creature = map->GetCreature(guid))
            creature->DespawnOrUnsummon();
    }
    slot.objects.clear();
    slot.spawnOf.clear();
    slot.placed.clear();
}

uint32 GuildHouseMgr::SpawnOf(ObjectGuid guid)
{
    std::lock_guard<std::mutex> guard(_lock);
    for (Hall* hall : { &_alliance, &_horde })
        for (Slot const& slot : hall->slots)
        {
            auto itr = slot.spawnOf.find(guid);
            if (itr != slot.spawnOf.end())
                return itr->second;
        }
    return 0;
}

void GuildHouseMgr::OnMapUpdate(Map* map, uint32 diff)
{
    if (!IsHouseMap(map->GetId()))
        return;

    // Those who have nothing to do in the house.
    std::vector<ObjectGuid> out;
    {
        std::lock_guard<std::mutex> guard(_lock);
        for (auto itr = _sendOut.begin(); itr != _sendOut.end();)
        {
            if (ObjectAccessor::GetPlayer(map, *itr))
            {
                out.push_back(*itr);
                itr = _sendOut.erase(itr);
            }
            else
                ++itr;
        }
    }
    for (ObjectGuid const& guid : out)
        if (Player* player = ObjectAccessor::GetPlayer(map, guid))
        {
            ChatHandler(player->GetSession()).PSendModuleSysMessage(GUILD_LEVEL_MODULE, GUILD_LEVEL_STR_HOUSE_NOT_YOURS);
            SendBack(player);
        }

    Hall& hall = HallOf(map->GetId());
    hall.timer += diff;
    if (hall.timer < IN_MILLISECONDS)
        return;
    uint32 const elapsed = hall.timer;
    hall.timer = 0;

    // Who is in which house.
    std::array<uint32, GUILD_HOUSE_SLOTS> inside{};
    for (auto const& ref : map->GetPlayers())
        if (Player* player = ref.GetSource())
            if (!player->IsGameMaster())
                for (uint8 i = 0; i < GUILD_HOUSE_SLOTS; ++i)
                    if (player->GetPhaseMask() & MaskOf(i))
                        ++inside[i];

    std::lock_guard<std::mutex> guard(_lock);
    for (uint8 i = 0; i < GUILD_HOUSE_SLOTS; ++i)
    {
        Slot& slot = hall.slots[i];
        if (!slot.guildId)
            continue;

        if (!inside[i])
        {
            // Empty long enough: the phase goes back to the pool.
            slot.emptyFor += elapsed;
            if (slot.emptyFor >= _config.emptyDelay * IN_MILLISECONDS)
            {
                Empty(map, slot);
                slot = Slot();
            }
            continue;
        }

        slot.emptyFor = 0;
        if (slot.rebuild)
        {
            // The Paragon level went down (a GM): what it no longer unlocks goes.
            Empty(map, slot);
            slot.rebuild = false;
            slot.furnish = true;
        }
        if (slot.furnish)
        {
            Furnish(map, slot, i);
            slot.furnish = false;
        }
    }
}

void GuildHouseMgr::RebuildHall(uint32 mapId)
{
    for (Slot& slot : HallOf(mapId).slots)
        if (slot.guildId)
            slot.rebuild = true;
}

void GuildHouseMgr::ListSpawns(ChatHandler* handler, uint32 mapId)
{
    std::lock_guard<std::mutex> guard(_lock);
    for (GuildHouseSpawn const& s : _spawns)
        if (s.map == mapId)
            handler->SendSysMessage(Acore::StringFormat("{}: Paragon {} {} {} at {:.2f} {:.2f} {:.2f} o {:.2f}",
                s.id, uint32(s.paragon), s.kind ? "object" : "creature", s.entry, s.x, s.y, s.z, s.o));
}

bool GuildHouseMgr::MoveSpawn(uint32 id, uint32 mapId, float x, float y, float z, float o)
{
    std::lock_guard<std::mutex> guard(_lock);
    for (GuildHouseSpawn& s : _spawns)
        if (s.id == id)
        {
            if (s.map != mapId)
                return false;
            s.x = x; s.y = y; s.z = z; s.o = o;
            WorldDatabase.Execute("UPDATE `mod_guild_level_house_spawn` SET `x` = {}, `y` = {}, `z` = {}, `o` = {} WHERE `id` = {}",
                x, y, z, o, id);
            RebuildHall(mapId);
            return true;
        }
    return false;
}

uint32 GuildHouseMgr::AddSpawn(uint32 mapId, uint8 paragon, uint8 kind, uint32 entry, float x, float y, float z, float o)
{
    std::lock_guard<std::mutex> guard(_lock);
    // A server's own rows start at 1000: the module's are rewritten on update.
    uint32 id = 1000;
    for (GuildHouseSpawn const& s : _spawns)
        id = std::max(id, s.id + 1);
    _spawns.push_back({ id, mapId, paragon, kind, entry, x, y, z, o });
    WorldDatabase.Execute("INSERT INTO `mod_guild_level_house_spawn` (`id`, `map`, `paragon`, `kind`, `entry`, `x`, `y`, `z`, `o`, `comment`) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, 'added in game')", id, mapId, uint32(paragon), uint32(kind), entry, x, y, z, o);
    RebuildHall(mapId);
    return id;
}

bool GuildHouseMgr::DeleteSpawn(uint32 id)
{
    std::lock_guard<std::mutex> guard(_lock);
    for (auto itr = _spawns.begin(); itr != _spawns.end(); ++itr)
        if (itr->id == id)
        {
            uint32 const mapId = itr->map;
            _spawns.erase(itr);
            WorldDatabase.Execute("DELETE FROM `mod_guild_level_house_spawn` WHERE `id` = {}", id);
            RebuildHall(mapId);
            return true;
        }
    return false;
}

void GuildHouseMgr::OnGuildChanged(uint32 guildId)
{
    std::lock_guard<std::mutex> guard(_lock);
    for (Hall* hall : { &_alliance, &_horde })
        for (Slot& slot : hall->slots)
            if (slot.guildId == guildId)
                slot.rebuild = true;
}

void GuildHouseMgr::SendBack(Player* player)
{
    WorldLocation origin;
    bool known = false;
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto itr = _origins.find(player->GetGUID());
        if (itr != _origins.end())
        {
            origin = itr->second;
            known = true;
        }
    }
    if (!known)
    {
        // Logged in inside the house: the way back is in the database.
        if (QueryResult result = CharacterDatabase.Query(
                "SELECT `map`, `x`, `y`, `z`, `o` FROM `mod_guild_level_house_origin` WHERE `guid` = {}",
                player->GetGUID().GetCounter()))
        {
            Field* f = result->Fetch();
            origin.WorldRelocate(f[0].Get<uint16>(), f[1].Get<float>(), f[2].Get<float>(), f[3].Get<float>(), f[4].Get<float>());
            known = true;
        }
    }
    if (known && !IsHouseMap(origin.GetMapId()) && player->TeleportTo(origin))
        return;
    if (!player->TeleportToEntryPoint())
        player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY,
            player->m_homebindZ, player->GetOrientation());
}

void GuildHouseMgr::DeleteCharacter(CharacterDatabaseTransaction trans, uint32 guid)
{
    trans->Append("DELETE FROM `mod_guild_level_house_origin` WHERE `guid` = {}", guid);
}

void GuildHouseMgr::OnMemberLeft(ObjectGuid guid)
{
    // Only someone standing in a house has to go: the others never get in.
    Player* player = ObjectAccessor::FindConnectedPlayer(guid);
    if (!player || !IsHouseMap(player->GetMapId()) || player->IsGameMaster())
        return;
    std::lock_guard<std::mutex> guard(_lock);
    _sendOut.insert(guid);
}

void GuildHouseMgr::DeleteGuild(uint32 guildId)
{
    // Its members are removed one by one before (OnMemberLeft): the house is
    // empty soon, and its phase goes back to the pool as usual.
    std::lock_guard<std::mutex> guard(_lock);
    for (Hall* hall : { &_alliance, &_horde })
        for (Slot& slot : hall->slots)
            if (slot.guildId == guildId)
                slot.rebuild = true;
}
