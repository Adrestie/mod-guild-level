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

#include "GuildLevel.h"
#include "GuildHouse.h"
#include "GuildPerks.h"

#include "Chat.h"
#include "Common.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Log.h"
#include "Player.h"
#include "QueryResult.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Timer.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
    // The built-in curves, the ones of conf/mod-guild-level.conf.dist. For
    // scale: a level 80 monster is worth about 500 experience to a level 80
    // character, so five guildmates bring the guild about 2,500 per kill.
    constexpr char DEFAULT_LEVEL_CURVE[] =
        "20000 30000 45000 60000 80000 100000 125000 150000 180000 210000 "
        "250000 290000 330000 380000 430000 490000 550000 620000 700000 780000 "
        "870000 960000 1060000 1170000 1300000";
    constexpr char DEFAULT_PARAGON_CURVE[] =
        "1500000 1750000 2000000 2250000 2500000 2750000 3000000 3500000 4000000";

    // A list of amounts, separated by spaces or commas. A short list repeats
    // its last value; a missing or unreadable one keeps the built-in curve.
    // A list of amounts, separated by spaces or commas, into `count` steps. A
    // short list repeats its last value; a missing or unreadable one keeps the
    // built-in curve.
    std::vector<uint64> ReadCurve(char const* option, char const* fallback, std::size_t count)
    {
        auto parse = [](std::string text, std::vector<uint64>& out)
        {
            std::replace(text.begin(), text.end(), ',', ' ');
            std::istringstream in(text);
            std::string token;
            while (in >> token)
            {
                Optional<uint64> value = Acore::StringTo<uint64>(token);
                if (!value)
                    return false;
                out.push_back(std::max<uint64>(*value, 1));
            }
            return !out.empty();
        };

        std::vector<uint64> values;
        if (!parse(sConfigMgr->GetOption<std::string>(option, fallback), values))
        {
            LOG_ERROR("server.loading", "mod-guild-level: {} is not a list of amounts, the built-in curve is used.", option);
            values.clear();
            parse(fallback, values);
        }
        else if (values.size() > count)
            LOG_ERROR("server.loading", "mod-guild-level: {} holds {} amounts, only the first {} are used.", option, values.size(), count);

        std::vector<uint64> curve(count);
        for (std::size_t i = 0; i < count; ++i)
            curve[i] = i < values.size() ? values[i] : values.back();
        return curve;
    }
}

GuildLevelMgr* GuildLevelMgr::instance()
{
    static GuildLevelMgr instance;
    return &instance;
}

void GuildLevelMgr::LoadConfig()
{
    GuildLevelConfig c;

    c.enable = sConfigMgr->GetOption<bool>("GuildLevel.Enable", true);
    c.minMembers = std::max<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.MinMembers", 2), 1);
    c.rate = std::max(sConfigMgr->GetOption<float>("GuildLevel.Rate", 1.0f), 0.0f);
    c.announce = sConfigMgr->GetOption<bool>("GuildLevel.Announce", true);
    std::vector<uint64> const levels = ReadCurve("GuildLevel.Curve.Levels", DEFAULT_LEVEL_CURVE, GUILD_LEVEL_MAX);
    std::copy(levels.begin(), levels.end(), c.levelXp.begin());
    c.paragonMax = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.Paragon.Max", 99), 1, GUILD_PARAGON_LIMIT));
    c.paragonXp = ReadCurve("GuildLevel.Curve.Paragon", DEFAULT_PARAGON_CURVE, c.paragonMax);
    c.saveInterval = std::max<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.SaveInterval", 10), 1);

    std::lock_guard<std::mutex> guard(_lock);
    _config = c;

    // A shorter curve may leave a guild with more than its step needs: it
    // climbs at once, without announcement (the change is the server's). A
    // lower Paragon.Max brings the guilds above it down to it.
    for (auto& [guildId, state] : _guilds)
    {
        if (state.paragon > c.paragonMax)
        {
            state.paragon = c.paragonMax;
            _changed.insert(guildId);
        }
        Climb(guildId, state);
        _dirty.insert(guildId);
    }
    _announcements.clear();
}

void GuildLevelMgr::LoadFromDB()
{
    uint32 const oldMSTime = getMSTime();

    // A guild disbanded while the module was not installed leaves its row
    // behind: dropped here.
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mod_guild_level` WHERE `guildid` NOT IN (SELECT `guildid` FROM `guild`)");

    std::lock_guard<std::mutex> guard(_lock);
    _guilds.clear();
    _dirty.clear();
    _announcements.clear();

    if (QueryResult result = CharacterDatabase.Query(
            "SELECT `guildid`, `level`, `paragon`, `xp`, `total_xp` FROM `mod_guild_level`"))
    {
        do
        {
            Field* fields = result->Fetch();
            GuildLevelState state;
            state.level = std::min<uint8>(fields[1].Get<uint8>(), GUILD_LEVEL_MAX);
            state.paragon = state.level < GUILD_LEVEL_MAX ? 0 : std::min<uint8>(fields[2].Get<uint8>(), _config.paragonMax);
            state.xp = fields[3].Get<uint64>();
            state.totalXp = fields[4].Get<uint64>();
            _guilds[fields[0].Get<uint32>()] = state;
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", ">> mod-guild-level: loaded {} guild levels in {} ms", _guilds.size(), GetMSTimeDiffToNow(oldMSTime));
}

GuildLevelState GuildLevelMgr::Get(uint32 guildId) const
{
    std::lock_guard<std::mutex> guard(_lock);
    auto itr = _guilds.find(guildId);
    return itr != _guilds.end() ? itr->second : GuildLevelState();
}

uint64 GuildLevelMgr::Needed(GuildLevelState const& state) const
{
    if (state.level < GUILD_LEVEL_MAX)
        return _config.levelXp[state.level];
    if (state.paragon < _config.paragonMax)
        return _config.paragonXp[state.paragon];
    return 0;
}

void GuildLevelMgr::Climb(uint32 guildId, GuildLevelState& state)
{
    while (uint64 const need = Needed(state))
    {
        if (state.xp < need)
            return;
        state.xp -= need;
        _changed.insert(guildId);

        if (state.level < GUILD_LEVEL_MAX)
        {
            ++state.level;
            _announcements.push_back({ guildId,
                state.level == GUILD_LEVEL_MAX ? GUILD_LEVEL_STR_LEVEL_MAX : GUILD_LEVEL_STR_LEVEL_UP, state.level });
        }
        else
        {
            ++state.paragon;
            _announcements.push_back({ guildId,
                state.paragon == _config.paragonMax ? GUILD_LEVEL_STR_PARAGON_MAX : GUILD_LEVEL_STR_PARAGON_UP, state.paragon });
        }
    }

    // The last Paragon level: the bar stays empty, only the lifetime total grows.
    state.xp = 0;
}

void GuildLevelMgr::OnKillXP(Player* player, Unit* victim)
{
    if (!_config.enable || !player || !victim || !victim->IsCreature())
        return;

    uint32 const guildId = player->GetGuildId();
    if (!guildId)
        return;

    // THE GUILDMATES PRESENT: the ones the kill rewards (the killer, and the
    // group within reward distance, see KillRewarder), alive, of the same
    // guild. Each of them runs this hook for himself and brings his own share,
    // so the guild earns the kill once per guildmate present.
    uint32 present = 1;
    if (Group* group = player->GetGroup())
    {
        present = 0;
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            if (Player* member = itr->GetSource())
                if (member->GetGuildId() == guildId && member->IsAlive() &&
                    (member == player || member->IsAtGroupRewardDistance(victim)))
                    ++present;
    }
    if (present < _config.minMembers)
        return;

    // The kill as this member would earn it alone: not split by the group,
    // not cut by a gray member, before any personal bonus. The server's kill
    // rate is included; a monster gray to him is worth nothing.
    uint32 xp = Acore::XP::Gain(player, victim, false);
    if (CreatureTemplate const* ct = victim->ToCreature()->GetCreatureTemplate())
        if (ct->ModHealth <= 0.75f && ct->ModHealth >= 0.0f)
            xp = uint32(xp * ct->ModHealth);            // as KillRewarder does

    uint64 const amount = uint64(std::llround(double(xp) * _config.rate));
    if (amount)
        AddXP(guildId, amount);
}

GuildLevelState GuildLevelMgr::AddXP(uint32 guildId, uint64 amount)
{
    std::lock_guard<std::mutex> guard(_lock);
    GuildLevelState& state = _guilds[guildId];
    state.totalXp += amount;
    if (Needed(state))
    {
        state.xp += amount;
        std::size_t const before = _announcements.size();
        Climb(guildId, state);

        // A level is worth a write at once; the rest waits for the tick.
        if (_announcements.size() != before)
        {
            SaveNow(guildId, state);
            _dirty.erase(guildId);
            return state;
        }
    }
    _dirty.insert(guildId);
    return state;
}

GuildLevelState GuildLevelMgr::SetLevel(uint32 guildId, uint8 level)
{
    std::lock_guard<std::mutex> guard(_lock);
    GuildLevelState& state = _guilds[guildId];
    state.level = std::min(level, GUILD_LEVEL_MAX);
    state.paragon = 0;
    _changed.insert(guildId);
    state.xp = 0;
    SaveNow(guildId, state);
    _dirty.erase(guildId);
    return state;
}

GuildLevelState GuildLevelMgr::SetParagon(uint32 guildId, uint8 paragon)
{
    std::lock_guard<std::mutex> guard(_lock);
    GuildLevelState& state = _guilds[guildId];
    state.level = GUILD_LEVEL_MAX;
    state.paragon = std::min(paragon, _config.paragonMax);
    _changed.insert(guildId);
    state.xp = 0;
    SaveNow(guildId, state);
    _dirty.erase(guildId);
    return state;
}

GuildLevelState GuildLevelMgr::SetXP(uint32 guildId, uint64 xp)
{
    std::lock_guard<std::mutex> guard(_lock);
    GuildLevelState& state = _guilds[guildId];
    // Within the current step: a GM sets the bar, he does not climb with it.
    uint64 const need = Needed(state);
    state.xp = need ? std::min(xp, need - 1) : 0;
    SaveNow(guildId, state);
    _dirty.erase(guildId);
    return state;
}

void GuildLevelMgr::DeleteGuild(uint32 guildId)
{
    {
        std::lock_guard<std::mutex> guard(_lock);
        _guilds.erase(guildId);
        _dirty.erase(guildId);
    }
    CharacterDatabase.Execute("DELETE FROM `mod_guild_level` WHERE `guildid` = {}", guildId);
}

void GuildLevelMgr::SaveNow(uint32 guildId, GuildLevelState const& state)
{
    CharacterDatabase.Execute(
        "REPLACE INTO `mod_guild_level` (`guildid`, `level`, `paragon`, `xp`, `total_xp`) VALUES ({}, {}, {}, {}, {})",
        guildId, uint32(state.level), uint32(state.paragon), state.xp, state.totalXp);
}

void GuildLevelMgr::SaveDirty()
{
    std::lock_guard<std::mutex> guard(_lock);
    for (uint32 guildId : _dirty)
    {
        auto itr = _guilds.find(guildId);
        if (itr != _guilds.end())
            SaveNow(guildId, itr->second);
    }
    _dirty.clear();
}

void GuildLevelMgr::Update(uint32 diff)
{
    // The announcements were queued by the map threads: the guilds and their
    // member lists belong to the world thread, where we are now.
    std::vector<Announcement> pending;
    std::unordered_set<uint32> changed;
    {
        std::lock_guard<std::mutex> guard(_lock);
        pending.swap(_announcements);
        changed.swap(_changed);
    }
    if (_config.announce)
        for (Announcement const& announcement : pending)
            Announce(announcement);

    // The perks follow the level: auras of the members online, at once.
    for (uint32 guildId : changed)
    {
        sGuildPerkMgr->RefreshGuild(guildId);
        sGuildHouseMgr->OnGuildChanged(guildId);
    }

    _saveTimer += diff;
    if (_saveTimer >= _config.saveInterval * IN_MILLISECONDS)
    {
        _saveTimer = 0;
        SaveDirty();
    }
}

void GuildLevelMgr::Announce(Announcement const& announcement) const
{
    Guild* guild = sGuildMgr->GetGuildById(announcement.guildId);
    if (!guild)
        return;

    // Each member in his own language, in the green of the guild channel.
    auto tell = [&announcement](Player* member)
    {
        WorldSession* session = member->GetSession();
        std::string const* text = session->GetModuleString(GUILD_LEVEL_MODULE, announcement.text);
        if (!text)
            return;
        ChatHandler(session).SendSysMessage(
            "|cff40ff40" + Acore::StringFormat(*text, announcement.value) + "|r");
    };
    guild->BroadcastWorker(tell);
}
