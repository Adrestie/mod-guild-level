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
 * mod-guild-level — guild levels after Cataclysm.
 *
 * Members of one guild who kill a monster together earn guild experience: each
 * of them brings the experience the kill is worth to him, so the more of the
 * guild in the group, the more the guild earns. The guild climbs 25 levels,
 * then 9 Paragon levels.
 *
 * This step holds the experience and the levels only. The perks of the levels,
 * the talents, the guild house and its Customization mode come later and read
 * the level through GuildLevelMgr::Get.
 */

#ifndef MOD_GUILD_LEVEL_H_
#define MOD_GUILD_LEVEL_H_

#include "Define.h"
#include <array>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Player;
class Unit;

constexpr char GUILD_LEVEL_MODULE[] = "mod-guild-level";

// A new guild is level 0; level N will grant the Nth perk.
constexpr uint8 GUILD_LEVEL_MAX = 25;
constexpr uint8 GUILD_PARAGON_MAX = 9;

// The texts live in `module_string` / `module_string_locale` (data/sql/world).
// Ids are never reused: an old row could still carry them.
enum GuildLevelStrings : uint32
{
    // Guild announcements, to every member online.
    GUILD_LEVEL_STR_LEVEL_UP        = 1,    // {} level
    GUILD_LEVEL_STR_PARAGON_UP      = 2,    // {} Paragon level
    GUILD_LEVEL_STR_LEVEL_MAX       = 3,    // level 25 reached
    GUILD_LEVEL_STR_PARAGON_MAX     = 4,    // Paragon 9 reached
    GUILD_LEVEL_STR_GATHER_BONUS    = 5,    // {} count, {} item: a perk to the member alone

    // GM commands.
    GUILD_LEVEL_STR_CMD_NO_GUILD    = 101,  // no guild found
    GUILD_LEVEL_STR_CMD_INFO        = 102,  // {} name, {} id, {} level, {} Paragon
    GUILD_LEVEL_STR_CMD_INFO_XP     = 103,  // {} xp, {} needed
    GUILD_LEVEL_STR_CMD_INFO_DONE   = 104,  // Paragon 9: nothing more to earn
    GUILD_LEVEL_STR_CMD_INFO_TOTAL  = 105,  // {} lifetime experience
    GUILD_LEVEL_STR_CMD_BAD_LEVEL   = 106,  // {} maximum
    GUILD_LEVEL_STR_CMD_DONE        = 107,  // the change is applied
    GUILD_LEVEL_STR_CMD_DISABLED    = 108,  // the module is off: gains stopped

    // .guildlevel perks
    GUILD_LEVEL_STR_PERKS_TITLE     = 110,  // {} level
    GUILD_LEVEL_STR_PERKS_GAINS     = 111,  // {} xp %, {} honor %, {} reputation %, {} mounted speed %
    GUILD_LEVEL_STR_PERKS_GOLD      = 112,  // {} vendor %, {} gold to bank %, {} Hearthstone minutes
    GUILD_LEVEL_STR_PERKS_MISC      = 113,  // {} gathering chance %, {} items, {} resurrection, {} consumable minutes
    GUILD_LEVEL_STR_PERKS_ON        = 114,
    GUILD_LEVEL_STR_PERKS_OFF       = 115,
    GUILD_LEVEL_STR_PERKS_DISABLED  = 116,  // GuildLevel.Perks.Enable = 0
    GUILD_LEVEL_STR_PERKS_DURABILITY = 117, // {} items, {} seconds

    // .gperk, the active perks of levels 21 to 25
    GUILD_LEVEL_STR_ACTIVE_LEVEL    = 120,  // {} level needed
    GUILD_LEVEL_STR_ACTIVE_COOLDOWN = 121,  // {} minutes, {} seconds left
    GUILD_LEVEL_STR_ACTIVE_COMBAT   = 122,
    GUILD_LEVEL_STR_ACTIVE_BG       = 123,
    GUILD_LEVEL_STR_ACTIVE_SUMMONED = 124,  // {} members
    GUILD_LEVEL_STR_ACTIVE_REZZED   = 125,  // {} members
    GUILD_LEVEL_STR_ACTIVE_NO_ONE   = 126,
    GUILD_LEVEL_STR_ACTIVE_NO_DEAD  = 127,
    GUILD_LEVEL_STR_ACTIVE_VAULT    = 128,  // {} minutes
    GUILD_LEVEL_STR_ACTIVE_SUPPLIER = 129,  // {} minutes
    GUILD_LEVEL_STR_ACTIVE_NO_HOME  = 130,
    GUILD_LEVEL_STR_ACTIVE_NO_GUILD = 131,
    GUILD_LEVEL_STR_ACTIVE_NO_GROUP = 132,
    GUILD_LEVEL_STR_ACTIVE_TITLE    = 133,
    GUILD_LEVEL_STR_ACTIVE_LINE     = 134,  // {} name, {} subcommand, {} level, {} state
    GUILD_LEVEL_STR_ACTIVE_READY    = 135,
    GUILD_LEVEL_STR_ACTIVE_LOCKED   = 136,
    GUILD_LEVEL_STR_ACTIVE_LEFT     = 137,  // {} minutes, {} seconds
    GUILD_LEVEL_STR_ACTIVE_NAME     = 138,  // 138 to 142: the names, in GuildActivePerk order
    GUILD_LEVEL_STR_ACTIVE_FAILED   = 143
};

struct GuildLevelConfig
{
    bool enable = true;

    // Members of the same guild, alive and within reward distance of the kill,
    // the group needs before the guild earns anything.
    uint32 minMembers = 2;
    float rate = 1.0f;
    bool announce = true;

    // Experience from one level to the next: levelXp[0] takes a new guild from
    // 0 to 1, paragonXp[0] takes a level 25 guild to Paragon 1.
    std::array<uint64, GUILD_LEVEL_MAX> levelXp{};
    std::array<uint64, GUILD_PARAGON_MAX> paragonXp{};

    uint32 saveInterval = 60;               // seconds
};

struct GuildLevelState
{
    uint8 level = 0;
    uint8 paragon = 0;
    uint64 xp = 0;                          // toward the next level or Paragon
    uint64 totalXp = 0;                     // everything ever earned
};

class GuildLevelMgr
{
public:
    static GuildLevelMgr* instance();

    void LoadConfig();
    void LoadFromDB();                      // startup
    void SaveDirty();                       // the world tick, and shutdown

    // THE PUBLIC VIEW, for the next steps of the module. A guild the module
    // never heard of is level 0. Safe from any thread.
    [[nodiscard]] GuildLevelState Get(uint32 guildId) const;
    [[nodiscard]] GuildLevelConfig const& Config() const { return _config; }
    // Experience the guild needs for its next step; 0 at Paragon 9.
    [[nodiscard]] uint64 Needed(GuildLevelState const& state) const;

    // A member received kill experience (PlayerScript::OnPlayerGiveXP). Runs on
    // the map threads.
    void OnKillXP(Player* player, Unit* victim);

    // Experience earned by the guild: levels climbed are announced on the next
    // world tick. Returns what the guild became.
    GuildLevelState AddXP(uint32 guildId, uint64 amount);

    // GM changes: no announcement, saved at once.
    GuildLevelState SetLevel(uint32 guildId, uint8 level);
    GuildLevelState SetParagon(uint32 guildId, uint8 paragon);
    GuildLevelState SetXP(uint32 guildId, uint64 xp);

    void DeleteGuild(uint32 guildId);       // the guild disbanded

    // The world tick: announcements waiting from the map threads, then the
    // periodic save.
    void Update(uint32 diff);

private:
    GuildLevelMgr() = default;

    struct Announcement
    {
        uint32 guildId;
        uint32 text;
        uint32 value;
    };

    // Under _lock.
    void Climb(uint32 guildId, GuildLevelState& state);
    void SaveNow(uint32 guildId, GuildLevelState const& state);

    void Announce(Announcement const& announcement) const;

    GuildLevelConfig _config;

    mutable std::mutex _lock;
    std::unordered_map<uint32, GuildLevelState> _guilds;
    std::unordered_set<uint32> _dirty;
    std::vector<Announcement> _announcements;
    std::unordered_set<uint32> _changed;    // guilds whose level moved: perks to refresh

    uint32 _saveTimer = 0;
};

#define sGuildLevelMgr GuildLevelMgr::instance()

#endif
