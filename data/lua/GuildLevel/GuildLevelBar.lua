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
    The guild's level in the guild window, server side: the level, the Paragon
    level, the experience toward the next step and what that step costs, read
    from the characters database (mod_guild_level, saved every
    GuildLevel.SaveInterval seconds) and from mod-guild-level.conf.

    THE CURVES ARE READ AS THE MODULE READS THEM (GuildLevel.cpp, ReadCurve):
    a list of amounts, spaces or commas, a short list repeating its last one;
    the fallbacks are the C++ defaults.
------------------------------------------------------------------------------]]

local AIO = require("AIO")

local Handlers = AIO.AddHandlers("GuildLevelBar", {})

local LEVEL_MAX = 25
local DEFAULT_LEVELS = "20000 30000 45000 60000 80000 100000 125000 150000 180000 210000 "
    .. "250000 290000 330000 380000 430000 490000 550000 620000 700000 780000 "
    .. "870000 960000 1060000 1170000 1300000"
local DEFAULT_PARAGON = "1500000 1750000 2000000 2250000 2500000 2750000 3000000 3500000 4000000"

local function Parse(text)
    local out = {}
    for v in text:gsub(",", " "):gmatch("%S+") do
        local n = tonumber(v)
        if not n then return nil end
        out[#out + 1] = math.max(n, 1)
    end
    return #out > 0 and out or nil
end

local function Curve(key, fallback)
    local text = GetConfigValue(key)
    return (type(text) == "string" and Parse(text)) or Parse(fallback)
end

local function Step(curve, i)
    return curve[i] or curve[#curve]
end

function Handlers.Request(player)
    local guildId = player:GetGuildId()
    local info = { guild = guildId ~= 0 }
    if guildId ~= 0 then
        local paragonMax = math.max(1, math.min(255, tonumber(GetConfigValue("GuildLevel.Paragon.Max")) or 99))
        local level, paragon, xp, total = 0, 0, 0, 0
        local q = CharDBQuery(string.format(
            "SELECT level, paragon, xp, total_xp FROM mod_guild_level WHERE guildid = %d", guildId))
        if q then
            level, paragon, xp, total = q:GetUInt32(0), q:GetUInt32(1), q:GetUInt64(2), q:GetUInt64(3)
        end
        local need = 0
        if level < LEVEL_MAX then
            need = Step(Curve("GuildLevel.Curve.Levels", DEFAULT_LEVELS), level + 1)
        elseif paragon < paragonMax then
            need = Step(Curve("GuildLevel.Curve.Paragon", DEFAULT_PARAGON), paragon + 1)
        end
        info.level, info.paragon, info.paragonMax = level, paragon, paragonMax
        info.levelMax = LEVEL_MAX
        info.xp, info.need, info.total = tonumber(xp) or 0, need, tonumber(total) or 0
        info.name = player:GetGuildName() or ""
    end
    AIO.Handle(player, "GuildLevelBar", "Info", info)
end
