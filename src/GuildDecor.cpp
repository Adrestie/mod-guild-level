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
#include "GuildText.h"

#include "Chat.h"

#include "Config.h"
#include "ConditionMgr.h"
#include "Creature.h"
#include "Group.h"
#include "GuildContent.h"
#include "Item.h"
#include "LootMgr.h"
#include "Mail.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Random.h"
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
#include <set>

namespace
{
    constexpr float SCALE_MIN = 0.1f;      // the sizes the editor allows (wxl-guildhouse)
    constexpr float SCALE_MAX = 2.0f;
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
    c.paragon = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Decor.Paragon", 7), 0, GUILD_PARAGON_LIMIT));
    c.rank = uint8(std::min<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Decor.Rank", 1), 9));
    c.dropChance = std::clamp(sConfigMgr->GetOption<float>("GuildLevel.House.Decor.DropChance", 10.0f), 0.0f, 100.0f);
    c.themeChance = std::clamp(sConfigMgr->GetOption<float>("GuildLevel.House.Decor.ThemeChance", 60.0f), 0.0f, 100.0f);
    c.dungeonDrops = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Decor.Drops.Dungeon", 1), 1, 5);
    c.raid10Drops = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Decor.Drops.Raid10", 2), 1, 10);
    c.raid25Drops = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Decor.Drops.Raid25", 3), 1, 25);
    c.dungeonMembers = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Decor.Members.Dungeon", 4), 1, 5));
    c.raid10Members = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Decor.Members.Raid10", 8), 1, 10));
    c.raid25Members = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("GuildLevel.House.Decor.Members.Raid25", 20), 1, 25));
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

    _itemDecor.clear();
    _decorItem.clear();
    if (QueryResult result = WorldDatabase.Query("SELECT `decor`, `item` FROM `mod_guild_level_decor_item`"))
        do
        {
            Field* f = result->Fetch();
            _itemDecor[f[1].Get<uint32>()] = f[0].Get<uint32>();
            _decorItem[f[0].Get<uint32>()] = f[1].Get<uint32>();
        } while (result->NextRow());

    // where they drop: a boss's own, a map's theme, and what has no theme (anywhere)
    _bossLoot.clear();
    _mapLoot.clear();
    _anywhere.clear();
    std::set<uint32> themed;
    if (QueryResult result = WorldDatabase.Query("SELECT `decor`, `map`, `creature` FROM `mod_guild_level_decor_loot`"))
        do
        {
            Field* f = result->Fetch();
            uint32 const decor = f[0].Get<uint32>();
            if (!_decorItem.count(decor))
                continue;
            if (uint32 creature = f[2].Get<uint32>())
                _bossLoot[creature].push_back(decor);
            else if (uint32 map = f[1].Get<uint32>())
            {
                _mapLoot[map].push_back(decor);
                themed.insert(decor);
            }
        } while (result->NextRow());
    for (auto const& [decor, item] : _decorItem)
        if (!themed.count(decor))
            _anywhere.push_back(decor);

    _questDecor.clear();
    if (QueryResult result = WorldDatabase.Query("SELECT `quest`, `decor` FROM `mod_guild_level_decor_quest`"))
        do
        {
            Field* f = result->Fetch();
            if (_decorItem.count(f[1].Get<uint32>()))
                _questDecor[f[0].Get<uint32>()] = f[1].Get<uint32>();
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
        case GuildDecorResult::NotAnObject: return GUILD_LEVEL_STR_DECOR_UNKNOWN;
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
    // what still stands in its house goes at once
    sGuildHouseMgr->OnDecorChanged(guildId);
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

// ---------------------------------------------------------------- the items, and their drops

std::string GuildDecorMgr::NameOf(uint32 decor, uint8 locale) const
{
    std::string name;
    uint32 item = 0;
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto k = _kinds.find(decor);
        if (k == _kinds.end())
            return name;
        name = k->second.name;
        auto i = _decorItem.find(decor);
        item = i == _decorItem.end() ? 0 : i->second;
    }
    if (item && locale)
        if (ItemLocale const* l = sObjectMgr->GetItemLocale(item))
            ObjectMgr::GetLocaleString(l->Name, locale, name);
    return name;
}

GuildDecorResult GuildDecorMgr::CheckCollect(Player* player, uint32 item) const
{
    if (!_config.enable)
        return GuildDecorResult::Disabled;
    {
        std::lock_guard<std::mutex> guard(_lock);
        if (!_itemDecor.count(item))
            return GuildDecorResult::NotAnObject;
    }
    uint32 const guildId = player->GetGuildId();
    if (!guildId)
        return GuildDecorResult::NoGuild;
    GuildLevelState const state = sGuildLevelMgr->Get(guildId);
    if (state.level < GUILD_LEVEL_MAX || state.paragon < _config.paragon)
        return GuildDecorResult::Locked;
    return GuildDecorResult::Ok;
}

GuildDecorResult GuildDecorMgr::Collect(Player* player, uint32 item, uint32& decor, uint32& owned)
{
    GuildDecorResult const r = CheckCollect(player, item);
    if (r != GuildDecorResult::Ok)
        return r;
    uint32 const guildId = player->GetGuildId();
    {
        std::lock_guard<std::mutex> guard(_lock);
        decor = _itemDecor[item];
        owned = ++_stock[guildId][decor];
    }
    CharacterDatabase.DirectExecute("REPLACE INTO `mod_guild_level_decor_stock` (`guildid`, `decor`, `owned`) "
        "VALUES ({}, {}, {})", guildId, decor, owned);
    TellChanged(guildId);
    return GuildDecorResult::Ok;
}

void GuildDecorMgr::OnCreatureKill(Player* killer, Creature* creature)
{
    if (!_config.enable || !killer || !creature || _config.dropChance <= 0.0f)
        return;
    Map* map = creature->GetMap();
    if (!map || !map->IsDungeon())
        return;
    // a dungeon or raid boss
    switch (GuildContentOf(creature))
    {
        case GC_VANILLA_DUNGEON_BOSS: case GC_VANILLA_RAID_BOSS: case GC_BC_DUNGEON_BOSS: case GC_BC_RAID_BOSS:
        case GC_WOTLK_DUNGEON_BOSS: case GC_WOTLK_HEROIC_BOSS: case GC_WOTLK_RAID_BOSS: case GC_ICECROWN_RAID_BOSS:
            break;
        default:
            return;
    }
    uint32 const needed = !map->IsRaid() ? _config.dungeonMembers
        : (map->Is25ManRaid() ? _config.raid25Members : _config.raid10Members);

    // the guildmates at the kill: the killer's group within reward distance, alive
    std::map<uint32, std::vector<Player*>> present;
    if (Group* group = killer->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            if (Player* member = itr->GetSource())
                if (member->GetGuildId() && member->IsAlive() &&
                    (member == killer || member->IsAtGroupRewardDistance(creature)))
                    present[member->GetGuildId()].push_back(member);
    }
    else if (killer->GetGuildId())
        present[killer->GetGuildId()].push_back(killer);

    for (auto const& [guildId, members] : present)
    {
        if (members.size() < needed)
            continue;
        GuildLevelState const state = sGuildLevelMgr->Get(guildId);
        if (state.level < GUILD_LEVEL_MAX || state.paragon < _config.paragon)
            continue;
        if (!roll_chance_f(_config.dropChance))
            continue;

        // seen and looted by that guild's members only: their mark, the items' condition
        for (Player* member : members)
            member->CastSpell(member, GUILD_DECOR_LOOT_MARK, true);
        static Condition* mark = []
        {
            Condition* c = new Condition();
            c->ConditionType = CONDITION_AURA;
            c->ConditionValue1 = GUILD_DECOR_LOOT_MARK;
            c->ConditionValue2 = 0;
            c->ConditionTarget = 0;
            return c;
        }();

        // as many objects as members get one: a dungeon's group, a 10-player raid, a 25-player one;
        // each the boss's own, else the instance's theme (ThemeChance) or one of no theme
        uint32 const drops = !map->IsRaid() ? _config.dungeonDrops
            : (map->Is25ManRaid() ? _config.raid25Drops : _config.raid10Drops);
        for (uint32 i = 0; i < drops; ++i)
        {
            uint32 decor = 0, item = 0;
            {
                std::lock_guard<std::mutex> guard(_lock);
                std::vector<uint32> const* pool = nullptr;
                auto b = _bossLoot.find(creature->GetEntry());
                auto m = _mapLoot.find(map->GetId());
                if (b != _bossLoot.end() && !b->second.empty())
                    pool = &b->second;
                else if (m != _mapLoot.end() && !m->second.empty() && (_anywhere.empty() || roll_chance_f(_config.themeChance)))
                    pool = &m->second;
                else if (!_anywhere.empty())
                    pool = &_anywhere;
                if (!pool)
                    break;
                decor = Acore::Containers::SelectRandomContainerElement(*pool);
                item = _decorItem[decor];
            }
            LootStoreItem drop(item, 0, 100.0f, false, LOOT_MODE_DEFAULT, 0, 1, 1);
            drop.conditions.push_back(mark);
            creature->loot.AddItem(drop);
            LOG_DEBUG("module", "mod-guild-level: {} dropped Customization object {} for guild {}", creature->GetEntry(),
                decor, guildId);
        }
    }
}

void GuildDecorMgr::OnQuestRewarded(Player* player, uint32 questId)
{
    if (!_config.enable || !player || !player->GetGuildId())
        return;
    uint32 decor = 0, item = 0;
    {
        std::lock_guard<std::mutex> guard(_lock);
        auto q = _questDecor.find(questId);
        if (q == _questDecor.end())
            return;
        decor = q->second;
        item = _decorItem[decor];
    }

    // in his bags, else by mail
    ItemPosCountVec dest;
    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item, 1) == EQUIP_ERR_OK)
    {
        if (Item* stored = player->StoreNewItem(dest, item, true))
            player->SendNewItem(stored, 1, true, false);
    }
    else if (Item* mailed = Item::CreateItem(item, 1, player))
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        mailed->SaveToDB(trans);
        MailDraft(NameOf(decor, player->GetSession()->GetSessionDbLocaleIndex()), "").AddItem(mailed)
            .SendMailTo(trans, MailReceiver(player), MailSender(MAIL_NORMAL, player->GetGUID().GetCounter(), MAIL_STATIONERY_GM));
        CharacterDatabase.CommitTransaction(trans);
    }
    GuildTell(player, GUILD_LEVEL_STR_DECOR_QUEST,
        NameOf(decor, player->GetSession()->GetSessionDbLocaleIndex()));
}
