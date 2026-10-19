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
    The guild's level in the guild window, client side (shipped by AIO).

    A badge and a bar under the window's title: the badge is the ring of the
    level-up toast with the number WRITTEN in it, not drawn, so that no level
    is out of reach -- the Paragon levels go as far as the server's
    GuildLevel.Paragon.Max says. Up to level 25 the badge is gold and says the
    level; past it, it turns purple and says the Paragon level. The bar is the
    experience toward the next step; at the last Paragon level it stays full.
    Its tooltip: the lifetime experience, the step after, the maximum.

    On the stock guild window, or in ForeverUI's guild page (the guild
    column, under the guild's name).
------------------------------------------------------------------------------]]

local AIO = AIO or require("AIO")

if AIO.AddAddon() then
    return                                  -- server side: we stop here
end

local Handlers = AIO.AddHandlers("GuildLevelBar", {})

local fmt = string.format
local ART = "Interface\\GuildLevel\\Toast"
local RING = { 278 / 512, 344 / 512, 228 / 512, 294 / 512 }    -- evergreen-toast-celebration-content-ring

local TEXTS = {
    enUS = {
        level = "Guild level %d", paragon = "Paragon %d", max = "Maximum reached",
        xp = "%s / %s", total = "Lifetime experience: %s", next = "Next: %s",
        paragonMax = "The Paragon levels go up to %d.", nextLevel = "level %d", nextParagon = "Paragon %d",
        noGuild = "Not in a guild",
    },
    frFR = {
        level = "Niveau de guilde %d", paragon = "Parangon %d", max = "Maximum atteint",
        xp = "%s / %s", total = "Expérience totale : %s", next = "Prochain palier : %s",
        paragonMax = "Les niveaux Parangon vont jusqu'à %d.", nextLevel = "niveau %d", nextParagon = "Parangon %d",
        noGuild = "Pas de guilde",
    },
}
local L = TEXTS[GetLocale()] or TEXTS.enUS

-- 1234567 -> "1 234 567" (French) / "1,234,567"
local function Big(n)
    local s = tostring(math.floor(n or 0))
    local sep = GetLocale() == "frFR" and " " or ","
    local out = s:reverse():gsub("(%d%d%d)", "%1" .. sep):reverse()
    return (out:gsub("^" .. (sep == " " and "%s" or ","), ""))
end

local INFO
local bars = {}

local function Update(bar)
    local i = INFO
    if not i or not i.guild then
        bar:Hide()
        return
    end
    bar:Show()
    local paragon = i.level >= i.levelMax
    local n = paragon and i.paragon or i.level
    bar.number:SetText(n)
    -- the number stays in its ring: smaller as it grows longer
    local digits = #tostring(n)
    bar.number:SetFont("Fonts\\FRIZQT__.TTF", digits <= 2 and 20 or (digits == 3 and 16 or 13), "OUTLINE")
    if paragon then
        bar.ring:SetVertexColor(0.85, 0.55, 1)
        bar.number:SetTextColor(0.9, 0.7, 1)
        bar.title:SetText(fmt(L.paragon, i.paragon))
        bar.status:SetStatusBarColor(0.62, 0.3, 0.95)
    else
        bar.ring:SetVertexColor(1, 1, 1)
        bar.number:SetTextColor(1, 0.82, 0)
        bar.title:SetText(fmt(L.level, i.level))
        bar.status:SetStatusBarColor(0.9, 0.7, 0.15)
    end
    if i.need > 0 then
        bar.status:SetMinMaxValues(0, i.need)
        bar.status:SetValue(math.min(i.xp, i.need))
        bar.value:SetText(fmt(L.xp, Big(i.xp), Big(i.need)))
    else
        bar.status:SetMinMaxValues(0, 1)
        bar.status:SetValue(1)
        bar.value:SetText(L.max)
    end
end

local function Tooltip(bar)
    local i = INFO
    if not i or not i.guild then return end
    GameTooltip:SetOwner(bar, "ANCHOR_BOTTOM")
    GameTooltip:AddLine(i.name ~= "" and i.name or bar.title:GetText(), 1, 0.82, 0)
    GameTooltip:AddLine(bar.title:GetText(), 1, 1, 1)
    if i.level >= i.levelMax and i.paragon > 0 then
        GameTooltip:AddLine(fmt(L.level, i.level), 0.8, 0.8, 0.8)
    end
    if i.need > 0 then
        local nextStep = i.level < i.levelMax and fmt(L.nextLevel, i.level + 1) or fmt(L.nextParagon, i.paragon + 1)
        GameTooltip:AddLine(fmt(L.next, nextStep) .. "  (" .. fmt(L.xp, Big(i.xp), Big(i.need)) .. ")", 0.8, 0.8, 0.8)
    else
        GameTooltip:AddLine(L.max, 0.2, 1, 0.2)
    end
    GameTooltip:AddLine(fmt(L.total, Big(i.total)), 0.8, 0.8, 0.8)
    GameTooltip:AddLine(fmt(L.paragonMax, i.paragonMax), 0.6, 0.6, 0.6, true)
    GameTooltip:Show()
end

local function Make(parent, x, y, width)
    if not parent or bars[parent] then return end
    local bar = CreateFrame("Frame", nil, parent)
    bar:SetWidth(width)
    bar:SetHeight(40)
    bar:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y)
    bar:SetFrameLevel(parent:GetFrameLevel() + 10)
    bar:EnableMouse(true)
    bar:SetScript("OnEnter", Tooltip)
    bar:SetScript("OnLeave", function() GameTooltip:Hide() end)

    bar.ring = bar:CreateTexture(nil, "OVERLAY")
    bar.ring:SetTexture(ART)
    bar.ring:SetTexCoord(RING[1], RING[2], RING[3], RING[4])
    bar.ring:SetWidth(44)
    bar.ring:SetHeight(44)
    bar.ring:SetPoint("LEFT", bar, "LEFT", 0, 0)
    bar.disc = bar:CreateTexture(nil, "ARTWORK")
    bar.disc:SetTexture(0, 0, 0, 0.7)
    bar.disc:SetWidth(30)
    bar.disc:SetHeight(30)
    bar.disc:SetPoint("CENTER", bar.ring, "CENTER")
    bar.number = bar:CreateFontString(nil, "OVERLAY")
    bar.number:SetFont("Fonts\\FRIZQT__.TTF", 20, "OUTLINE")
    bar.number:SetPoint("CENTER", bar.ring, "CENTER", 0, 0)

    bar.title = bar:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    bar.title:SetPoint("TOPLEFT", bar.ring, "TOPRIGHT", 4, -4)

    bar.status = CreateFrame("StatusBar", nil, bar)
    bar.status:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
    bar.status:SetHeight(12)
    bar.status:SetPoint("BOTTOMLEFT", bar.ring, "BOTTOMRIGHT", 4, 6)
    bar.status:SetPoint("RIGHT", bar, "RIGHT", 0, 0)
    local bg = bar.status:CreateTexture(nil, "BACKGROUND")
    bg:SetAllPoints()
    bg:SetTexture(0, 0, 0, 0.6)
    bar.value = bar.status:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    bar.value:SetPoint("CENTER")

    bars[parent] = bar
    Update(bar)
end

local function Request()
    AIO.Handle("GuildLevelBar", "Request")
end

function Handlers.Info(_, info)
    INFO = info
    for _, bar in pairs(bars) do Update(bar) end
end

local foreverGuild = _G.ForeverUI and _G.ForeverUI.Guild

local function Attach()
    if foreverGuild and foreverGuild.column then
        Make(foreverGuild.column, 12, -76, 148)
    else
        Make(_G.GuildFrame, 70, -38, 230)
    end
    Request()
end

if foreverGuild and foreverGuild.frame then
    foreverGuild.frame:HookScript("OnShow", Attach)
else
    if FriendsFrame then FriendsFrame:HookScript("OnShow", Attach) end
    if FriendsFrame_Update then hooksecurefunc("FriendsFrame_Update", Attach) end
end

-- fresh after a level-up (the toast's message) and now and then while shown
local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_GUILD_UPDATE")
listener:SetScript("OnEvent", function(_, event, prefix)
    if event == "CHAT_MSG_ADDON" and prefix ~= "GuildLevel" then return end
    Request()
end)
local elapsed = 0
listener:SetScript("OnUpdate", function(_, e)
    elapsed = elapsed + e
    if elapsed < 15 then return end
    elapsed = 0
    local window = foreverGuild and foreverGuild.frame or FriendsFrame
    if window and window:IsShown() then Request() end
end)

Request()
