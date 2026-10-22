--[[
    This file is part of mod-guild-level.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
    Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program. If not, see <http://www.gnu.org/licenses/>.
]]

--[[----------------------------------------------------------------------------
    The Customization mode of the guild house, server side.

    It serves the guild's state (its stock and the objects placed in its
    house, read back from the characters database) and relays every edit to
    the module's `.ghouse` commands, RUN AS THE PLAYER: the rules (Paragon
    level, rank, the house, the stock, one member per object) and the
    messages are the module's (src/GuildDecor.h). Its writes are synchronous,
    so what is read back here is always fresh.
------------------------------------------------------------------------------]]

local AIO = require("AIO")

local Handlers = AIO.AddHandlers("GuildHouse", {})

local fmt = string.format

-- The fallback MUST BE THE C++ DEFAULT (GuildDecorMgr::LoadConfig).
local function Conf(key, fallback)
    return tonumber(GetConfigValue(key)) or fallback
end

local LEVEL_MAX = 25
-- the game's locale numbers (LocaleConstant): their names in the *_locale tables
local LOCALES = { [2] = "frFR", [3] = "deDE", [6] = "esES", [8] = "ruRU" }
local HALLS = { [449] = true, [450] = true }

local function State(player)
    local guildId = player:GetGuildId()
    local state = {
        guild = guildId ~= 0, inHouse = HALLS[player:GetMapId()] == true, map = player:GetMapId(),
        paragonNeeded = Conf("GuildLevel.House.Decor.Paragon", 9), rankAllowed = Conf("GuildLevel.House.Decor.Rank", 1),
        enable = Conf("GuildLevel.House.Decor.Enable", 1) ~= 0, owned = {}, placed = {},
        members = { Conf("GuildLevel.House.Decor.Members.Dungeon", 4), Conf("GuildLevel.House.Decor.Members.Raid10", 8),
            Conf("GuildLevel.House.Decor.Members.Raid25", 20) },
    }
    if guildId == 0 then
        return state
    end
    local level, paragon = 0, 0
    local q = CharDBQuery(fmt("SELECT level, paragon FROM mod_guild_level WHERE guildid = %d", guildId))
    if q then
        level, paragon = q:GetUInt32(0), q:GetUInt32(1)
    end
    state.unlocked = level >= LEVEL_MAX and paragon >= state.paragonNeeded
    state.allowed = player:GetGuildRank() <= state.rankAllowed
    state.can = state.enable and state.unlocked and state.allowed and state.inHouse

    q = CharDBQuery(fmt("SELECT decor, owned FROM mod_guild_level_decor_stock WHERE guildid = %d", guildId))
    if q then
        repeat
            state.owned[q:GetUInt32(0)] = q:GetUInt32(1)
        until not q:NextRow()
    end
    q = CharDBQuery(fmt("SELECT id, decor, map, x, y, z, qx, qy, qz, qw, scale FROM mod_guild_level_decor_placed "
        .. "WHERE guildid = %d", guildId))
    if q then
        repeat
            state.placed[#state.placed + 1] = {
                id = q:GetUInt32(0), decor = q:GetUInt32(1), map = q:GetUInt32(2),
                x = q:GetFloat(3), y = q:GetFloat(4), z = q:GetFloat(5),
                qx = q:GetFloat(6), qy = q:GetFloat(7), qz = q:GetFloat(8), qw = q:GetFloat(9), scale = q:GetFloat(10),
            }
        until not q:NextRow()
    end
    return state
end

local function Send(player, what)
    AIO.Handle(player, "GuildHouse", what or "Update", State(player))
end

local function Num(v)
    v = tonumber(v)
    if not v or v ~= v or v == math.huge or v == -math.huge then
        return nil
    end
    return v
end

function Handlers.Open(player)
    Send(player, "Show")
end

function Handlers.Refresh(player)
    Send(player)
end

function Handlers.Begin(player, id)
    id = Num(id)
    if id then
        player:RunCommand(fmt("ghouse begin %d", id))
    end
    Send(player)
end

function Handlers.Place(player, id, decor, x, y, z, qx, qy, qz, qw, scale)
    local v = { Num(id), Num(decor), Num(x), Num(y), Num(z), Num(qx), Num(qy), Num(qz), Num(qw), Num(scale) }
    for i = 1, 10 do
        if not v[i] then
            return Send(player)
        end
    end
    player:RunCommand(fmt("ghouse place %d %d %.4f %.4f %.4f %.6f %.6f %.6f %.6f %.4f",
        v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]))
    Send(player)
end

function Handlers.Cancel(player, id)
    id = Num(id)
    if id then
        player:RunCommand(fmt("ghouse cancel %d", id))
    end
    Send(player)
end

function Handlers.Remove(player, id)
    id = Num(id)
    if id then
        player:RunCommand(fmt("ghouse remove %d", id))
    end
    Send(player)
end

-- The objects' names in the player's language (their items', item_template_locale), a few hundred
-- at a time: none for English, the interface's own.
local NAMES_AT_ONCE = 400

function Handlers.Names(player)
    local locale = LOCALES[player:GetDbLocaleIndex()]
    if not locale then
        return AIO.Handle(player, "GuildHouse", "Names", {}, true)
    end
    local names, n = {}, 0
    local q = WorldDBQuery(fmt("SELECT di.decor, l.Name FROM mod_guild_level_decor_item di "
        .. "JOIN item_template_locale l ON l.ID = di.item AND l.locale = '%s'", locale))
    if q then
        repeat
            names[q:GetUInt32(0)] = q:GetString(1)
            n = n + 1
            if n % NAMES_AT_ONCE == 0 then
                AIO.Handle(player, "GuildHouse", "Names", names, false)
                names = {}
            end
        until not q:NextRow()
    end
    AIO.Handle(player, "GuildHouse", "Names", names, true)
end

-- Where an object is found, for the Collection window: the bosses that drop it first (their names),
-- the dungeons and raids of its theme (Map IDs: the interface names them) or none (every one), and
-- the quests that give it (title in the player's language, level, zone).
function Handlers.Sources(player, decor)
    decor = Num(decor)
    if not decor then return end
    local locale = LOCALES[player:GetDbLocaleIndex()] or "enUS"
    local sources = { decor = decor, bosses = {}, maps = {}, quests = {} }
    local q = WorldDBQuery(fmt("SELECT l.map, l.creature, COALESCE(c.Name, t.name, '') FROM mod_guild_level_decor_loot l "
        .. "LEFT JOIN creature_template t ON t.entry = l.creature "
        .. "LEFT JOIN creature_template_locale c ON c.entry = l.creature AND c.locale = '%s' "
        .. "WHERE l.decor = %d", locale, decor))
    if q then
        repeat
            if q:GetUInt32(1) ~= 0 then
                sources.bosses[#sources.bosses + 1] = q:GetString(2)
            else
                sources.maps[#sources.maps + 1] = q:GetUInt32(0)
            end
        until not q:NextRow()
    end
    q = WorldDBQuery(fmt("SELECT COALESCE(NULLIF(l.Title, ''), t.LogTitle), t.QuestLevel, t.QuestSortID "
        .. "FROM mod_guild_level_decor_quest m JOIN quest_template t ON t.ID = m.quest "
        .. "LEFT JOIN quest_template_locale l ON l.ID = m.quest AND l.locale = '%s' "
        .. "WHERE m.decor = %d", locale, decor))
    if q then
        repeat
            sources.quests[#sources.quests + 1] = { q:GetString(0), q:GetInt32(1), q:GetInt32(2) }
        until not q:NextRow()
    end
    AIO.Handle(player, "GuildHouse", "Sources", sources)
end
