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

#include "GuildDecor.h"

#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Guild.h"
#include "GuildHouse.h"
#include "GuildLevel.h"
#include "GuildMgr.h"
#include "Log.h"
#include "Player.h"
#include "StringFormat.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float SCALE_MIN = 0.05f;
    constexpr float SCALE_MAX = 20.0f;
    constexpr float COORD_MAX = 2000.0f;    // the halls are a few hundred yards around the origin

    // The members' interface refreshes (GuildHouse_Client.lua): addon message
    // "GuildHouse", "changed", to the members standing in the guild's house.
    void TellChanged(uint32 guildId)
    {
        Guild* guild = sGuildMgr->GetGuildById(guildId);
        if (!guild)
            return;
        std::string const text = "GuildHouse\tchanged";
        auto tell = [&text](Player* member)
        {
            if (!GuildHouseMgr::IsHouseMap(member->GetMapId()))
                return;
            WorldPacket data(SMSG_MESSAGECHAT, 100);
            data << uint8(CHAT_MSG_WHISPER);
            data << int32(LANG_ADDON);
            data << member->GetGUID();
            data << uint32(0);
            data << member->GetGUID();
            data << uint32(text.length() + 1);
            data << text;
            data << uint8(0);
            member->GetSession()->SendPacket(&data);
        };
        guild->BroadcastWorker(tell);
    }
}

GuildDecorMgr* GuildDecorMgr::instance()
{
    static GuildDecorMgr instance;
    return &instance;
}

void GuildDecorMgr::LoadConfig()
{
    GuildDecorConfig c;
    c.enable = sConfigMgr->GetOption<bool>("GuildLevel.House.Decor.Enable", true);
    c.paragon = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Decor.Paragon", 9), 0, GUILD_PARAGON_LIMIT));
    c.rank = uint8(std::min<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Decor.Rank", 1), 9));
    _config = c;
}

void GuildDecorMgr::LoadFromDB()
{
    std::lock_guard<std::mutex> guard(_lock);
    _kinds.clear();
    _stock.clear();
    _placed.clear();
    _nextId = 1;

    if (QueryResult result = WorldDatabase.Query("SELECT `decor`, `entry`, `name` FROM `mod_guild_level_decor`"))
        do
        {
            Field* f = result->Fetch();
            GuildDecorKind k{ f[0].Get<uint32>(), f[1].Get<uint32>(), f[2].Get<std::string>() };
            _kinds[k.decor] = k;
        } while (result->NextRow());

    if (QueryResult result = CharacterDatabase.Query("SELECT `guildid`, `decor`, `owned` FROM `mod_guild_level_decor_stock`"))
        do
        {
            Field* f = result->Fetch();
            _stock[f[0].Get<uint32>()][f[1].Get<uint32>()] = f[2].Get<uint32>();
        } while (result->NextRow());

    uint32 placed = 0;
    if (QueryResult result = CharacterDatabase.Query("SELECT `id`, `guildid`, `decor`, `map`, `x`, `y`, `z`, "
            "`qx`, `qy`, `qz`, `qw`, `scale` FROM `mod_guild_level_decor_placed`"))
        do
        {
            Field* f = result->Fetch();
            GuildDecorPlaced p;
            p.id = f[0].Get<uint32>();
            p.decor = f[2].Get<uint32>();
            p.map = f[3].Get<uint32>();
            p.pos = { f[4].Get<float>(), f[5].Get<float>(), f[6].Get<float>() };
            p.rot = { f[7].Get<float>(), f[8].Get<float>(), f[9].Get<float>(), f[10].Get<float>() };
            p.scale = f[11].Get<float>();
            _placed[f[1].Get<uint32>()][p.id] = p;
            _nextId = std::max(_nextId, p.id + 1);
            ++placed;
        } while (result->NextRow());

    LOG_INFO("server.loading", ">> mod-guild-level: loaded {} Customization objects, {} placed", _kinds.size(), placed);
}

uint32 GuildDecorMgr::TextOf(GuildDecorResult result)
{
    switch (result)
    {
        case GuildDecorResult::Disabled:    return GUILD_LEVEL_STR_PERKS_DISABLED;
        case GuildDecorResult::NoGuild:     return GUILD_LEVEL_STR_ACTIVE_NO_GUILD;
        case GuildDecorResult::Locked:      return GUILD_LEVEL_STR_DECOR_LOCKED;
        case GuildDecorResult::NotAllowed:  return GUILD_LEVEL_STR_DECOR_NOT_ALLOWED;
        case GuildDecorResult::NotInHouse:  return GUILD_LEVEL_STR_HOUSE_NOT_IN;
        case GuildDecorResult::Unknown:     return GUILD_LEVEL_STR_DECOR_UNKNOWN;
        case GuildDecorResult::NoStock:     return GUILD_LEVEL_STR_DECOR_NO_STOCK;
        case GuildDecorResult::Busy:        return GUILD_LEVEL_STR_DECOR_BUSY;
        case GuildDecorResult::NoPlaced:    return GUILD_LEVEL_STR_DECOR_NO_PLACED;
        case GuildDecorResult::BadPosition: return GUILD_LEVEL_STR_DECOR_BAD;
        default:                            return 0;
    }
}

GuildDecorResult GuildDecorMgr::CanEdit(Player* player) const
{
    if (!_config.enable || !sGuildHouseMgr->Config().enable)
        return GuildDecorResult::Disabled;
    uint32 const guildId = player->GetGuildId();
    if (!guildId)
        return GuildDecorResult::NoGuild;
    GuildLevelState const state = sGuildLevelMgr->Get(guildId);
    if (state.level < GUILD_LEVEL_MAX || state.paragon < _config.paragon)
        return GuildDecorResult::Locked;
    if (player->GetRank() > _config.rank)
        return GuildDecorResult::NotAllowed;
    // in his own house: anyone else in a hall is sent out
    if (!GuildHouseMgr::IsHouseMap(player->GetMapId()))
        return GuildDecorResult::NotInHouse;
    return GuildDecorResult::Ok;
}

uint32 GuildDecorMgr::OwnedOf(uint32 guildId, uint32 decor) const
{
    auto g = _stock.find(guildId);
    if (g == _stock.end())
        return 0;
    auto d = g->second.find(decor);
    return d == g->second.end() ? 0 : d->second;
}

uint32 GuildDecorMgr::PlacedOf(uint32 guildId, uint32 decor) const
{
    auto g = _placed.find(guildId);
    if (g == _placed.end())
        return 0;
    return uint32(std::count_if(g->second.begin(), g->second.end(),
        [decor](auto const& p) { return p.second.decor == decor; }));
}

void GuildDecorMgr::Save(uint32 guildId, GuildDecorPlaced const& p)
{
    // At once: the interface reads the table back right after (GuildHouse.lua).
    CharacterDatabase.DirectExecute("REPLACE INTO `mod_guild_level_decor_placed` (`id`, `guildid`, `decor`, `map`, "
        "`x`, `y`, `z`, `qx`, `qy`, `qz`, `qw`, `scale`) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
        p.id, guildId, p.decor, p.map, p.pos[0], p.pos[1], p.pos[2], p.rot[0], p.rot[1], p.rot[2], p.rot[3], p.scale);
}

void GuildDecorMgr::Changed(uint32 guildId)
{
    sGuildHouseMgr->OnDecorChanged(guildId);
    TellChanged(guildId);
}

GuildDecorResult GuildDecorMgr::Begin(Player* player, uint32 id)
{
    GuildDecorResult r = CanEdit(player);
    if (r != GuildDecorResult::Ok)
        return r;
    uint32 const guildId = player->GetGuildId();
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto g = _placed.find(guildId);
        if (g == _placed.end() || !g->second.count(id))
            return GuildDecorResult::NoPlaced;
        GuildDecorPlaced& p = g->second[id];
        if (!p.editor.IsEmpty() && p.editor != player->GetGUID())
            return GuildDecorResult::Busy;
        p.editor = player->GetGUID();
        ++p.version;
    }
    Changed(guildId);
    return GuildDecorResult::Ok;
}

GuildDecorResult GuildDecorMgr::Place(Player* player, uint32 id, uint32 decor, std::array<float, 3> const& pos,
    std::array<float, 4> const& rot, float scale, uint32& placedId)
{
    GuildDecorResult r = CanEdit(player);
    if (r != GuildDecorResult::Ok)
        return r;
    for (float v : pos)
        if (!std::isfinite(v) || std::fabs(v) > COORD_MAX)
            return GuildDecorResult::BadPosition;
    float n = 0.0f;
    for (float v : rot)
    {
        if (!std::isfinite(v))
            return GuildDecorResult::BadPosition;
        n += v * v;
    }
    if (n < 1e-6f || !std::isfinite(scale))
        return GuildDecorResult::BadPosition;
    n = std::sqrt(n);
    std::array<float, 4> const q{ rot[0] / n, rot[1] / n, rot[2] / n, rot[3] / n };
    scale = std::clamp(scale, SCALE_MIN, SCALE_MAX);

    uint32 const guildId = player->GetGuildId();
    GuildDecorPlaced saved;
    {
        std::lock_guard<std::mutex> guard(_lock);
        GuildDecorPlaced p;
        if (id)
        {
            // the object he has in hand, put back
            auto g = _placed.find(guildId);
            if (g == _placed.end() || !g->second.count(id))
                return GuildDecorResult::NoPlaced;
            p = g->second[id];
            if (!p.editor.IsEmpty() && p.editor != player->GetGUID())
                return GuildDecorResult::Busy;
        }
        else
        {
            // a new one, from the stock
            if (!_kinds.count(decor))
                return GuildDecorResult::Unknown;
            if (PlacedOf(guildId, decor) >= OwnedOf(guildId, decor))
                return GuildDecorResult::NoStock;
            p.id = _nextId++;
            p.decor = decor;
        }
        p.map = player->GetMapId();
        p.pos = pos;
        p.rot = q;
        p.scale = scale;
        p.editor = ObjectGuid::Empty;
        ++p.version;
        _placed[guildId][p.id] = p;
        saved = p;
    }
    Save(guildId, saved);
    placedId = saved.id;
    Changed(guildId);
    return GuildDecorResult::Ok;
}

GuildDecorResult GuildDecorMgr::Cancel(Player* player, uint32 id)
{
    uint32 const guildId = player->GetGuildId();
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto g = _placed.find(guildId);
        if (g == _placed.end() || !g->second.count(id))
            return GuildDecorResult::NoPlaced;
        GuildDecorPlaced& p = g->second[id];
        if (p.editor != player->GetGUID())
            return GuildDecorResult::Busy;
        p.editor = ObjectGuid::Empty;
        ++p.version;
    }
    Changed(guildId);
    return GuildDecorResult::Ok;
}

GuildDecorResult GuildDecorMgr::Remove(Player* player, uint32 id)
{
    GuildDecorResult r = CanEdit(player);
    if (r != GuildDecorResult::Ok)
        return r;
    uint32 const guildId = player->GetGuildId();
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto g = _placed.find(guildId);
        if (g == _placed.end() || !g->second.count(id))
            return GuildDecorResult::NoPlaced;
        GuildDecorPlaced const& p = g->second[id];
        if (!p.editor.IsEmpty() && p.editor != player->GetGUID())
            return GuildDecorResult::Busy;
        g->second.erase(id);
    }
    CharacterDatabase.DirectExecute("DELETE FROM `mod_guild_level_decor_placed` WHERE `id` = {}", id);
    Changed(guildId);
    return GuildDecorResult::Ok;
}

void GuildDecorMgr::CancelAllOf(ObjectGuid player)
{
    std::vector<uint32> guilds;
    {
        std::lock_guard<std::mutex> guard(_lock);
        for (auto& [guildId, placed] : _placed)
            for (auto& [id, p] : placed)
                if (p.editor == player)
                {
                    p.editor = ObjectGuid::Empty;
                    ++p.version;
                    if (std::find(guilds.begin(), guilds.end(), guildId) == guilds.end())
                        guilds.push_back(guildId);
                }
    }
    for (uint32 guildId : guilds)
        Changed(guildId);
}

bool GuildDecorMgr::Give(uint32 guildId, uint32 decor, int32 count)
{
    std::vector<uint32> decors;
    {
        std::lock_guard<std::mutex> guard(_lock);
        if (decor)
        {
            if (!_kinds.count(decor))
                return false;
            decors.push_back(decor);
        }
        else
            for (auto const& [d, k] : _kinds)
                decors.push_back(d);
        for (uint32 d : decors)
        {
            int64 const owned = std::max<int64>(0, int64(OwnedOf(guildId, d)) + count);
            _stock[guildId][d] = uint32(owned);
        }
    }
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    for (uint32 d : decors)
        trans->Append("REPLACE INTO `mod_guild_level_decor_stock` (`guildid`, `decor`, `owned`) VALUES ({}, {}, {})",
            guildId, d, OwnedOf(guildId, d));
    CharacterDatabase.DirectCommitTransaction(trans);
    TellChanged(guildId);
    return true;
}

void GuildDecorMgr::DeleteGuild(uint32 guildId)
{
    {
        std::lock_guard<std::mutex> guard(_lock);
        _stock.erase(guildId);
        _placed.erase(guildId);
    }
    CharacterDatabase.Execute("DELETE FROM `mod_guild_level_decor_stock` WHERE `guildid` = {}", guildId);
    CharacterDatabase.Execute("DELETE FROM `mod_guild_level_decor_placed` WHERE `guildid` = {}", guildId);
}

std::vector<GuildDecorShown> GuildDecorMgr::Snapshot(uint32 guildId, uint32 mapId)
{
    std::vector<GuildDecorShown> shown;
    std::lock_guard<std::mutex> guard(_lock);
    auto g = _placed.find(guildId);
    if (g == _placed.end())
        return shown;
    for (auto const& [id, p] : g->second)
    {
        if (p.map != mapId || !p.editor.IsEmpty())
            continue;
        auto k = _kinds.find(p.decor);
        if (k == _kinds.end())
            continue;
        shown.push_back({ id, k->second.entry, p.version, p.pos, p.rot, p.scale });
    }
    return shown;
}
