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
 * mod-guild-level — the Customization mode of the guild house (Paragon 9).
 *
 * THE OBJECTS: furniture of the modern housing (`mod_guild_level_decor`,
 * world: tools/decor), each one a gameobject the house shows. A guild owns
 * some of each (its stock, `mod_guild_level_decor_stock`) and places at most
 * as many as it owns (`mod_guild_level_decor_placed`): position, rotation (a
 * quaternion) and size, saved at once.
 *
 * AN EDIT, as the members' interface does it (GuildHouse_Client.lua and the
 * client extension wxl-guildhouse): Begin takes an object out of the house
 * for everyone and keeps it for the member (no one else may touch it), who
 * moves a preview only he sees; Place puts it back where he left it (or puts
 * a new one from the stock), Cancel as it was. Remove gives it back to the
 * stock. An edit left open (the member leaves the house, or logs out) is
 * cancelled.
 *
 * HOW A GUILD GETS THEM: each object is an item too (`mod_guild_level_decor_item`).
 * A dungeon or raid boss killed by enough members of a guild of the
 * Customization's Paragon level (4 in a dungeon, 8 in a 10-player raid, 20 in
 * a 25-player one) may drop one: the boss's own objects first, else those of
 * the instance's theme, else one of no theme (`mod_guild_level_decor_loot`,
 * tools/decor/loot.json). Only that guild's members see it in the loot: they
 * carry a hidden mark from the kill (GUILD_DECOR_LOOT_MARK), the item's loot
 * condition. Quests give some too (`mod_guild_level_decor_quest`,
 * tools/decor/build_quests.py): to a member of a guild who rewards one, its
 * object's item, by mail when his bags are full. Used, the item adds its
 * object to the guild's stock (spell
 * GUILD_DECOR_COLLECT_SPELL, spell_guild_decor_collect).
 *
 * WHO: the members of the guild whose rank is allowed (GuildLevel.House.Decor.Rank),
 * standing in their house, once the guild reached the Customization's Paragon
 * level (GuildLevel.House.Decor.Paragon).
 *
 * THREADS: the commands run in the world thread; the house's map thread asks
 * for what stands in each house (Snapshot). Everything is under one lock.
 */

#ifndef MOD_GUILD_DECOR_H_
#define MOD_GUILD_DECOR_H_

#include "Define.h"
#include "ObjectGuid.h"
#include <array>
#include <map>
#include <mutex>
#include <string>
#include <vector>

class ChatHandler;
class Creature;
class Player;

constexpr uint32 GUILD_DECOR_COLLECT_SPELL = 92080;     // the items' "use": into the guild's stock
constexpr uint32 GUILD_DECOR_LOOT_MARK = 92081;         // hidden, on the members who may loot one

struct GuildDecorConfig
{
    bool enable = true;
    uint8 paragon = 9;                  // the Paragon level that opens the Customization
    uint8 rank = 1;                     // highest guild rank id allowed (0 guild master, 1 officer...)
    float dropChance = 10.0f;           // % for a boss of an eligible kill to drop an object
    uint8 dungeonMembers = 4;           // guildmates needed at the kill: a dungeon
    uint8 raid10Members = 8;            // a 10-player raid
    uint8 raid25Members = 20;           // a 25-player raid
};

// One kind of object (`mod_guild_level_decor`).
struct GuildDecorKind
{
    uint32 decor;                       // HouseDecor ID
    uint32 entry;                       // its gameobject
    std::string name;
};

// One object placed in a guild's house.
struct GuildDecorPlaced
{
    uint32 id = 0;
    uint32 decor = 0;
    uint32 map = 0;
    std::array<float, 3> pos{};
    std::array<float, 4> rot{ 0.0f, 0.0f, 0.0f, 1.0f };   // x, y, z, w
    float scale = 1.0f;
    ObjectGuid editor;                  // the member who has it in hand: not shown meanwhile
    uint32 version = 0;                 // moves with every change: the house places it again
};

// What a house shows of one placed object.
struct GuildDecorShown
{
    uint32 id;
    uint32 entry;
    uint32 version;
    std::array<float, 3> pos;
    std::array<float, 4> rot;
    float scale;
};

enum class GuildDecorResult
{
    Ok,
    Disabled,
    NoGuild,
    Locked,                             // Paragon level too low
    NotAllowed,                         // guild rank
    NotInHouse,
    Unknown,                            // no such object kind
    NoStock,
    Busy,                               // another member has it in hand
    NoPlaced,                           // no such placed object
    BadPosition,
    NotAnObject,                        // the item is no Customization object
};

class GuildDecorMgr
{
public:
    static GuildDecorMgr* instance();

    void LoadConfig();
    void LoadFromDB();
    [[nodiscard]] GuildDecorConfig const& Config() const { return _config; }

    // The member's edits (world thread).
    GuildDecorResult Begin(Player* player, uint32 id);
    GuildDecorResult Place(Player* player, uint32 id, uint32 decor, std::array<float, 3> const& pos,
        std::array<float, 4> const& rot, float scale, uint32& placedId);
    GuildDecorResult Cancel(Player* player, uint32 id);
    GuildDecorResult Remove(Player* player, uint32 id);
    // Every edit the player has in hand is cancelled (he left the house, or logged out).
    void CancelAllOf(ObjectGuid player);

    // The item's use (spell_guild_decor_collect): may it go into the player's guild's stock (Check),
    // and put it there (Collect, the stock's new count in owned).
    GuildDecorResult CheckCollect(Player* player, uint32 item) const;
    GuildDecorResult Collect(Player* player, uint32 item, uint32& decor, uint32& owned);
    [[nodiscard]] std::string NameOf(uint32 decor) const;

    // A creature killed by the player (or his pet): a boss of an eligible kill may drop an object.
    void OnCreatureKill(Player* killer, Creature* creature);
    // A quest rewarded: a member of a guild gets its object's item, if it gives one.
    void OnQuestRewarded(Player* player, uint32 questId);

    // GM: count more (or fewer) of an object kind in the guild's stock; 0 decor: every kind.
    bool Give(uint32 guildId, uint32 decor, int32 count);
    void DeleteGuild(uint32 guildId);

    // The house's map thread: what stands in the guild's house on the map.
    std::vector<GuildDecorShown> Snapshot(uint32 guildId, uint32 mapId);

    // The message a result is told with (module_string id), 0 for Ok.
    static uint32 TextOf(GuildDecorResult result);

private:
    GuildDecorMgr() = default;
    GuildDecorResult CanEdit(Player* player) const;
    uint32 OwnedOf(uint32 guildId, uint32 decor) const;
    uint32 PlacedOf(uint32 guildId, uint32 decor) const;
    void Save(uint32 guildId, GuildDecorPlaced const& p);
    void Changed(uint32 guildId);

    GuildDecorConfig _config;
    std::map<uint32, GuildDecorKind> _kinds;                                // by HouseDecor ID
    std::map<uint32, uint32> _itemDecor;                                    // item: decor
    std::map<uint32, uint32> _decorItem;                                    // decor: item
    std::map<uint32, std::vector<uint32>> _bossLoot;                        // creature entry: decor
    std::map<uint32, std::vector<uint32>> _mapLoot;                         // map: decor of its theme
    std::vector<uint32> _anywhere;                                          // decor of no theme
    std::map<uint32, uint32> _questDecor;                                   // quest: decor
    std::map<uint32, std::map<uint32, uint32>> _stock;                      // guild: decor: owned
    std::map<uint32, std::map<uint32, GuildDecorPlaced>> _placed;           // guild: id: object
    uint32 _nextId = 1;
    mutable std::mutex _lock;
};

#define sGuildDecorMgr GuildDecorMgr::instance()

#endif
