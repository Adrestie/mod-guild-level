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

#include "GuildContent.h"

#include "Creature.h"
#include "DBCStores.h"
#include "Map.h"
#include "SharedDefines.h"

#include <algorithm>

namespace
{
    char const* const NAMES[GC_FAMILY_COUNT] =
    {
        "VanillaWorld", "VanillaWorldBoss", "VanillaRare",
        "VanillaDungeonTrash", "VanillaDungeonBoss", "VanillaRaidTrash", "VanillaRaidBoss",
        "BcWorld", "BcWorldBoss", "BcRare",
        "BcDungeonTrash", "BcDungeonBoss", "BcRaidTrash", "BcRaidBoss",
        "WotlkWorld", "WotlkWorldBoss", "WotlkRare",
        "WotlkDungeonTrash", "WotlkDungeonBoss", "WotlkHeroicTrash", "WotlkHeroicBoss",
        "WotlkRaidTrash", "WotlkRaidBoss", "IcecrownRaidTrash", "IcecrownRaidBoss",
    };

    // Outside instances, a normal or rare creature of a BC map below this level is
    // Vanilla's (Eversong Woods, Azuremyst Isle and their neighbours).
    constexpr uint32 BC_WORLD_MIN_LEVEL = 57;

    // The families of one expansion, in the order of the enum: world, world
    // boss, rare, dungeon trash, dungeon boss, raid trash, raid boss.
    constexpr GuildContentFamily FIRST[3] = { GC_VANILLA_WORLD, GC_BC_WORLD, GC_WOTLK_WORLD };

    GuildContentFamily Of(uint32 expansion, uint32 offset)
    {
        return GuildContentFamily(FIRST[expansion] + offset);
    }
}

char const* GuildContentName(GuildContentFamily family)
{
    return family < GC_FAMILY_COUNT ? NAMES[family] : "";
}

GuildContentFamily GuildContentOf(Creature const* creature)
{
    uint32 const rank = creature->GetCreatureTemplate()->rank;
    bool const boss = creature->IsDungeonBoss() || creature->isWorldBoss() || rank == CREATURE_ELITE_WORLDBOSS;
    Map const* map = creature->GetMap();
    bool const dungeon = map && map->IsDungeon();
    bool const raid = map && map->IsRaid();
    bool const heroic = map && map->IsHeroic();
    uint32 expansion = 0;
    if (MapEntry const* entry = map ? map->GetEntry() : nullptr)
        expansion = std::min<uint32>(entry->Expansion(), 2);

    // Outside dungeons and raids: a world boss or a rare before the World family.
    if (!dungeon)
    {
        if (rank == CREATURE_ELITE_WORLDBOSS || creature->isWorldBoss())
            return Of(expansion, 1);
        uint32 const age = expansion == 1 && creature->GetLevel() < BC_WORLD_MIN_LEVEL ? 0 : expansion;
        if (rank == CREATURE_ELITE_RARE || rank == CREATURE_ELITE_RAREELITE)
            return Of(age, 2);
        return Of(age, 0);
    }

    if (raid)
    {
        if (creature->GetMapId() == MAP_ICECROWN_CITADEL)
            return boss ? GC_ICECROWN_RAID_BOSS : GC_ICECROWN_RAID_TRASH;
        if (expansion == 2)
            return boss ? GC_WOTLK_RAID_BOSS : GC_WOTLK_RAID_TRASH;
        return Of(expansion, boss ? 6 : 5);
    }

    // WotLK dungeons alone tell heroic from normal.
    if (expansion == 2)
    {
        if (heroic)
            return boss ? GC_WOTLK_HEROIC_BOSS : GC_WOTLK_HEROIC_TRASH;
        return boss ? GC_WOTLK_DUNGEON_BOSS : GC_WOTLK_DUNGEON_TRASH;
    }
    return Of(expansion, boss ? 4 : 3);
}
