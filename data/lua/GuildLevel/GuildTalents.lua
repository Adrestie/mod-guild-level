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
    The guild talent window, server side.

    It serves the tree (the module's world tables) and the guild's state (its
    points, read back from the characters database), and relays every action
    to the module's `.gtalent` commands, RUN AS THE PLAYER: the rules (rank,
    links, points, the guild bank) and the messages are the module's. Its
    writes are synchronous, so what is read back here is always fresh.

    The edits of the tree go the same way, through the administrator's
    commands: the client cannot do what its player could not type.

    It opens with .gtalent, /gtalent, or the button of the guild window.
------------------------------------------------------------------------------]]

local AIO = require("AIO")

local Handlers = AIO.AddHandlers("GuildTalents", {})

local fmt = string.format

-- WHAT AN OPERATOR TUNES LIVES IN mod-guild-level.conf. The fallback MUST BE
-- THE C++ DEFAULT (GuildTalentMgr::LoadConfig), or the window would announce a
-- figure the module does not honour.
local function Conf(key, fallback)
    return tonumber(GetConfigValue(key)) or fallback
end

local LEVEL_MAX = 25
local ADMIN_RANK = 3                        -- SEC_ADMINISTRATOR: the edits

local function Settings()
    return {
        enable = Conf("GuildLevel.Talents.Enable", 1) ~= 0,
        from = math.max(1, math.min(LEVEL_MAX, Conf("GuildLevel.Talents.FromLevel", 10))),
        perLevel = Conf("GuildLevel.Talents.PointsPerLevel", 1),
        rank = math.min(9, Conf("GuildLevel.Talents.Rank", 1)),
        cost = Conf("GuildLevel.Talents.Reset.Cost", 5000),
        decrease = math.max(0, Conf("GuildLevel.Talents.Reset.Decrease", 10)),
    }
end

-- The tree: nodes and links.
local function Tree()
    local nodes, links = {}, {}
    local q = WorldDBQuery("SELECT id, x, y, effect, amount, max_points, start, icon FROM mod_guild_level_talent_node")
    if q then
        repeat
            nodes[#nodes + 1] = {
                id = q:GetUInt32(0), x = q:GetInt32(1), y = q:GetInt32(2), effect = q:GetUInt32(3),
                amount = q:GetFloat(4), max = q:GetUInt32(5), start = q:GetUInt32(6) ~= 0, icon = q:GetString(7),
            }
        until not q:NextRow()
    end
    q = WorldDBQuery("SELECT node_a, node_b FROM mod_guild_level_talent_link")
    if q then
        repeat
            links[#links + 1] = { q:GetUInt32(0), q:GetUInt32(1) }
        until not q:NextRow()
    end
    return { nodes = nodes, links = links }
end

-- The guild's state, as the player sees it.
local function State(player)
    local s = Settings()
    local guildId = player:GetGuildId()
    local state = {
        guild = guildId ~= 0, name = player:GetGuildName() or "", points = {}, earned = 0, spent = 0,
        price = s.cost, canSpend = false, canEdit = player:GetGMRank() >= ADMIN_RANK, enable = s.enable,
        rank = s.rank, level = 0, paragon = 0,
    }
    if guildId == 0 then
        return state
    end

    local q = CharDBQuery(fmt("SELECT level, paragon FROM mod_guild_level WHERE guildid = %d", guildId))
    if q then
        state.level, state.paragon = q:GetUInt32(0), q:GetUInt32(1)
    end
    local levels = state.level >= s.from and (state.level - s.from + 1) or 0
    state.earned = (levels + state.paragon) * s.perLevel

    q = CharDBQuery(fmt("SELECT node, points FROM mod_guild_level_talent WHERE guildid = %d", guildId))
    if q then
        repeat
            local points = q:GetUInt32(1)
            state.points[q:GetUInt32(0)] = points
            state.spent = state.spent + points
        until not q:NextRow()
    end

    -- The price of a reset: GuildTalentMgr::ResetPrice.
    q = CharDBQuery(fmt("SELECT next_price, last_reset FROM mod_guild_level_talent_reset WHERE guildid = %d", guildId))
    if q and q:GetUInt32(0) > 0 then
        local weeks = math.floor(math.max(0, os.time() - q:GetUInt32(1)) / (7 * 86400))
        state.price = math.floor(math.max(s.cost, q:GetUInt32(0) - s.cost * s.decrease / 100 * weeks) + 0.5)
    end

    state.canSpend = player:GetGuildRank() <= s.rank
    return state
end

local function Send(player, what)
    AIO.Handle(player, "GuildTalents", what, Tree(), State(player))
end

-- At login, for the aura's tooltip: the tree and the points, without the window.
function Handlers.Hello(player)
    Send(player, "Prime")
end

function Handlers.Open(player)
    Send(player, "Show")
end

function Handlers.Refresh(player)
    Send(player, "Update")
end

local function Run(player, command)
    player:RunCommand(command)
    Send(player, "Update")
end

function Handlers.Buy(player, id)
    id = tonumber(id)
    if not id then return end
    Run(player, fmt("gtalent buy %d", id))
end

function Handlers.Reset(player)
    Run(player, "gtalent reset confirm")
end

-- THE EDITS: an administrator's. Checked here too, since a client can call AIO
-- directly; the commands check it again.
local function Admin(player)
    return player:GetGMRank() >= ADMIN_RANK
end

local function Int(v) v = tonumber(v); return v and math.floor(v) or nil end

function Handlers.Add(player, x, y, effect, amount, max)
    x, y, effect, amount, max = Int(x), Int(y), Int(effect), tonumber(amount), Int(max)
    if not Admin(player) or not (x and y and effect and amount and max) then return end
    Run(player, fmt("gtalent node add %d %d %d %s %d", x, y, effect, fmt("%g", amount), max))
end

function Handlers.Set(player, id, effect, amount, max)
    id, effect, amount, max = Int(id), Int(effect), tonumber(amount), Int(max)
    if not Admin(player) or not (id and effect and amount and max) then return end
    Run(player, fmt("gtalent node set %d %d %s %d", id, effect, fmt("%g", amount), max))
end

function Handlers.Move(player, id, x, y)
    id, x, y = Int(id), Int(x), Int(y)
    if not Admin(player) or not (id and x and y) then return end
    Run(player, fmt("gtalent node move %d %d %d", id, x, y))
end

function Handlers.Start(player, id, start)
    id = Int(id)
    if not Admin(player) or not id then return end
    Run(player, fmt("gtalent node start %d %d", id, start and 1 or 0))
end

function Handlers.Icon(player, id, icon)
    id = Int(id)
    icon = tostring(icon or ""):gsub("[^%w_\\/%-%.]", "")
    if not Admin(player) or not id then return end
    Run(player, fmt("gtalent node icon %d %s", id, icon))
end

function Handlers.Delete(player, id)
    id = Int(id)
    if not Admin(player) or not id then return end
    Run(player, fmt("gtalent node delete %d", id))
end

function Handlers.Link(player, a, b)
    a, b = Int(a), Int(b)
    if not Admin(player) or not (a and b) then return end
    Run(player, fmt("gtalent link %d %d", a, b))
end

-- The way in: .gtalent alone opens the window (the command's chat summary
-- stays for a client without the interface).
local function OnCommand(_, player, command)
    if player and command and command:lower():match("^gtalent%s*$") then
        Handlers.Open(player)
        return false
    end
end

RegisterPlayerEvent(42, OnCommand)          -- PLAYER_EVENT_ON_COMMAND

print("GuildLevel: guild talent window loaded.")
