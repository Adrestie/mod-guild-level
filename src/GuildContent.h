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
 * mod-guild-level — the content a creature belongs to, for the guild
 * experience of its kill (GuildLevel.Content.<family>).
 *
 * The families are those DynamicLoot sorts creatures into, same names and same
 * rules (expansion of the map, world, rare, world boss, dungeon or raid, boss
 * or not, WotLK heroic, Icecrown Citadel apart), written again here: the module
 * does not need DynamicLoot.
 */

#ifndef MOD_GUILD_CONTENT_H_
#define MOD_GUILD_CONTENT_H_

#include "Define.h"

class Creature;

enum GuildContentFamily : uint8
{
    GC_VANILLA_WORLD, GC_VANILLA_WORLD_BOSS, GC_VANILLA_RARE,
    GC_VANILLA_DUNGEON_TRASH, GC_VANILLA_DUNGEON_BOSS, GC_VANILLA_RAID_TRASH, GC_VANILLA_RAID_BOSS,
    GC_BC_WORLD, GC_BC_WORLD_BOSS, GC_BC_RARE,
    GC_BC_DUNGEON_TRASH, GC_BC_DUNGEON_BOSS, GC_BC_RAID_TRASH, GC_BC_RAID_BOSS,
    GC_WOTLK_WORLD, GC_WOTLK_WORLD_BOSS, GC_WOTLK_RARE,
    GC_WOTLK_DUNGEON_TRASH, GC_WOTLK_DUNGEON_BOSS, GC_WOTLK_HEROIC_TRASH, GC_WOTLK_HEROIC_BOSS,
    GC_WOTLK_RAID_TRASH, GC_WOTLK_RAID_BOSS, GC_ICECROWN_RAID_TRASH, GC_ICECROWN_RAID_BOSS,
    GC_FAMILY_COUNT
};

// The family's name, as in the configuration.
char const* GuildContentName(GuildContentFamily family);

// The family of a creature, from its rank, its boss flags and its map.
GuildContentFamily GuildContentOf(Creature const* creature);

#endif
