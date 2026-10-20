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
    The guild's level-up, client side (shipped by AIO): a toast and a sound.

    The toast is the modern client's renown celebration
    (MajorFactionCelebrationBannerTemplate, MajorFactionsRenownToast): same
    pieces, same places, same timeline, rebuilt for 3.3.5, which has neither
    MaskTexture nor the animation types it uses:
      - the art is the atlas evergreentoastcelebration as it is
        (Interface\GuildLevel\Toast), cut by texture coordinates;
      - the shine that sweeps the top filigree under its mask is a baked
        flipbook (Interface\GuildLevel\ToastShine, tools/fx/bake_toast.py);
      - the round icons are SetPortraitToTexture;
      - every animation is driven by one OnUpdate from the XML's timings.

    The module tells (addon message "GuildLevel", "up<TAB>level<TAB>paragon")
    and casts the visual, spell 92028, on every member online.
------------------------------------------------------------------------------]]

local AIO = AIO or require("AIO")

if AIO.AddAddon() then
    return                                  -- server side: we stop here
end

local floor, max, min = math.floor, math.max, math.min
local fmt = string.format

local ART = "Interface\\GuildLevel\\Toast"
local SHINE = "Interface\\GuildLevel\\ToastShine"
local SOUND = "Sound\\GuildLevel\\guild_levelup.mp3"
local SHEET = 512

-- The atlas pieces (left, top, width, height in the 512 x 512 sheet).
local PIECES = {
    bg = { 1, 328, 275, 77 },
    bottomline = { 1, 407, 275, 43 },
    ring = { 278, 228, 66, 66 },
    glow = { 1, 1, 282, 125 },
    star = { 278, 128, 133, 51 },
    topfiligree = { 1, 228, 275, 98 },
}

-- ---------------------------------------------------------------------------
-- TEXTS
-- ---------------------------------------------------------------------------

local TEXTS = {
    enUS = {
        level = "Guild level %d", paragon = "Paragon %d", talent = "A guild talent point",
        perks = {
            [1] = "+5% experience", [2] = "Damaged gear mended over time", [3] = "+5% honor",
            [4] = "Hearthstone cooldown 5 minutes shorter", [5] = "Lucky gathering", [6] = "Vendor prices 5% lower",
            [7] = "5% of looted gold to the guild bank", [8] = "+5% mounted speed", [9] = "+5% reputation",
            [10] = "Resurrected by a guildmate at full health", [11] = "+15% experience", [12] = "Gear mended faster",
            [13] = "+15% honor", [14] = "Hearthstone cooldown 15 minutes shorter", [15] = "Luckier gathering",
            [16] = "Vendor prices 10% lower", [17] = "10% of looted gold to the guild bank", [18] = "+10% mounted speed",
            [19] = "+10% reputation", [20] = "Flasks, elixirs and food last 30 minutes longer",
            [21] = "New spell: Guild Rally", [22] = "New spell: Mass Resurrection", [23] = "New spell: Mobile Banking",
            [24] = "New spell: Guild Supplier", [25] = "New spell: Guild House",
        },
        paragons = {
            [1] = "Your guild house opens", [2] = "A reagent vendor in the house", [3] = "A banker in the house",
            [5] = "Class trainers and a stable master", [6] = "Profession trainers, forge and anvil",
            [7] = "An auction house", [8] = "Portals to the capitals",
        },
    },
    frFR = {
        level = "Niveau de guilde %d", paragon = "Parangon %d", talent = "Un point de talent de guilde",
        perks = {
            [1] = "+5 % d'expérience", [2] = "Équipement réparé au fil du temps", [3] = "+5 % d'honneur",
            [4] = "Pierre de foyer : 5 minutes de recharge en moins", [5] = "Récolte chanceuse",
            [6] = "Prix des marchands 5 % moins chers", [7] = "5 % de l'or ramassé versé à la banque de guilde",
            [8] = "+5 % de vitesse en monture", [9] = "+5 % de réputation",
            [10] = "Ressuscité par un membre de la guilde avec toute sa vie", [11] = "+15 % d'expérience",
            [12] = "Équipement réparé plus vite", [13] = "+15 % d'honneur",
            [14] = "Pierre de foyer : 15 minutes de recharge en moins", [15] = "Récolte plus chanceuse",
            [16] = "Prix des marchands 10 % moins chers", [17] = "10 % de l'or ramassé versé à la banque de guilde",
            [18] = "+10 % de vitesse en monture", [19] = "+10 % de réputation",
            [20] = "Flacons, élixirs et nourriture : 30 minutes de plus",
            [21] = "Nouveau sort : Ralliement de guilde", [22] = "Nouveau sort : Résurrection de masse",
            [23] = "Nouveau sort : Banque mobile", [24] = "Nouveau sort : Fournisseur de guilde",
            [25] = "Nouveau sort : Maison de guilde",
        },
        paragons = {
            [1] = "Votre maison de guilde ouvre ses portes", [2] = "Un marchand de composants dans la maison",
            [3] = "Un banquier dans la maison", [5] = "Maîtres de classe et maître des écuries",
            [6] = "Maîtres des métiers, forge et enclume", [7] = "Un hôtel des ventes", [8] = "Des portails vers les capitales",
        },
    },
}
local L = TEXTS[GetLocale()] or TEXTS.enUS

local PERK_ICONS = {
    [1] = "INV_Misc_Book_11", [2] = "Trade_BlackSmithing", [3] = "INV_BannerPVP_02", [4] = "INV_Misc_Rune_01",
    [5] = "Trade_Herbalism", [6] = "INV_Misc_Coin_02", [7] = "INV_Misc_Coin_01", [8] = "Ability_Mount_RidingHorse",
    [9] = "INV_Misc_Note_02", [10] = "Spell_Holy_Resurrection", [20] = "INV_Potion_97", [21] = "Spell_Shadow_Twilight",
    [22] = "Spell_Holy_Resurrection", [23] = "INV_Misc_Bag_10", [24] = "INV_Misc_Bag_08", [25] = "INV_Misc_Rune_06",
}
local function PerkIcon(level)
    local icon = PERK_ICONS[level] or PERK_ICONS[level - 10]
    return "Interface\\Icons\\" .. (icon or "INV_Misc_QuestionMark")
end

-- ---------------------------------------------------------------------------
-- THE TOAST
-- ---------------------------------------------------------------------------

local T

local function Piece(parent, layer, name, blend)
    local p = PIECES[name]
    local t = parent:CreateTexture(nil, layer)
    t:SetTexture(ART)
    t:SetTexCoord(p[1] / SHEET, (p[1] + p[3]) / SHEET, p[2] / SHEET, (p[2] + p[4]) / SHEET)
    t:SetWidth(p[3])
    t:SetHeight(p[4])
    if blend then t:SetBlendMode(blend) end
    t:SetAlpha(0)
    return t
end

local function Build()
    T = CreateFrame("Frame", "GuildLevelUpToast", UIParent)
    T:SetWidth(128)
    T:SetHeight(128)
    T:SetPoint("TOP", UIParent, "TOP", 0, -250)
    T:SetFrameStrata("DIALOG")
    T:Hide()

    T.ToastBG = Piece(T, "BACKGROUND", "bg")
    T.ToastBG:SetWidth(275)
    T.ToastBG:SetHeight(240)
    -- its scale (origin TOP) is a height that grows from its top: anchored there
    T.ToastBG:SetPoint("TOP", T, "CENTER", 0, 10 + 120)

    T.Glow1 = Piece(T, "ARTWORK", "glow", "ADD")
    T.Glow1:SetPoint("CENTER", T, "CENTER", 0, 190)
    T.Star1 = Piece(T, "ARTWORK", "star")
    T.Star2 = Piece(T, "ARTWORK", "star")
    T.GlowLineBottom = Piece(T, "ARTWORK", "bottomline")
    T.TopBorder = Piece(T, "ARTWORK", "topfiligree")
    T.TopBorder:SetPoint("CENTER", T, "CENTER", 0, 150)
    T.Glow2 = Piece(T, "ARTWORK", "glow", "ADD")
    T.Glow2:SetPoint("CENTER", T, "CENTER", 0, 190)

    T.TopIcon = T:CreateTexture(nil, "OVERLAY")
    T.TopIcon:SetWidth(54)
    T.TopIcon:SetHeight(54)
    T.TopIcon:SetPoint("CENTER", T.TopBorder, "CENTER")
    T.TopIcon:SetAlpha(0)

    -- the shine of the top filigree, masked: the baked flipbook, at the mask's place
    T.Mask = CreateFrame("Frame", nil, T)
    T.Mask:SetAllPoints(T)
    T.Mask:SetAlpha(0.4)
    T.Shine = T.Mask:CreateTexture(nil, "ARTWORK")
    T.Shine:SetTexture(SHINE)
    T.Shine:SetBlendMode("ADD")
    T.Shine:SetWidth(512)
    T.Shine:SetHeight(128)
    T.Shine:SetPoint("CENTER", T, "CENTER", 0, 150)
    T.Shine:Hide()

    T.RenownLabel = T:CreateFontString(nil, "ARTWORK")
    T.RenownLabel:SetFont("Fonts\\MORPHEUS.TTF", 30)
    T.RenownLabel:SetShadowOffset(1, -1)
    T.RenownLabel:SetShadowColor(0, 0, 0)
    T.RenownLabel:SetTextColor(1, 0.82, 0)
    T.RenownLabel:SetPoint("TOP", T, "TOP", 0, 20)
    T.RenownLabel:SetAlpha(0)

    T.RewardIcon = T:CreateTexture(nil, "ARTWORK")
    T.RewardIcon:SetWidth(52)
    T.RewardIcon:SetHeight(52)
    T.RewardIcon:SetPoint("TOP", T.RenownLabel, "BOTTOM", 0, -18)
    T.RewardIcon:SetAlpha(0)
    T.RewardIconRing = Piece(T, "OVERLAY", "ring")
    T.RewardIconRing:SetPoint("CENTER", T.RewardIcon, "CENTER")

    T.RewardDescription = T:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
    T.RewardDescription:SetWidth(256)
    T.RewardDescription:SetTextColor(0.1, 1.0, 0.1)
    T.RewardDescription:SetShadowOffset(1, -1)
    T.RewardDescription:SetShadowColor(0, 0, 0)
    T.RewardDescription:SetPoint("TOP", T.RewardIcon, "BOTTOM", 0, -10)
    T.RewardDescription:SetAlpha(0)
end

-- a value from a to b during [start, start + duration], held outside
local function Ramp(t, start, duration, a, b, smooth)
    if t <= start then return a end
    if t >= start + duration then return b end
    local k = (t - start) / duration
    if smooth == "IN" then k = k * k elseif smooth == "OUT" then k = 1 - (1 - k) * (1 - k) end
    return a + (b - a) * k
end

local OUT_DELAY, OUT_TIME = 1.9 + 3.05, 1.0    -- ShowAnim, then waitAndAnimOut

local function Animate(self, elapsed)
    self.t = self.t + elapsed
    local t = self.t

    self.TopBorder:SetAlpha(Ramp(t, 0, 0.13333, 0, 1))
    self.TopIcon:SetAlpha(Ramp(t, 0, 0.13333, 0, 1))

    local s = Ramp(t, 0.13333, 0.43333, 0, 1, "IN")
    self.ToastBG:SetHeight(max(240 * s, 0.01))
    self.ToastBG:SetAlpha(Ramp(t, 0.13333, 0.43333, 0, 0.72))

    self.GlowLineBottom:SetAlpha(Ramp(t, 0.13333, 0.43333, 0, 1))
    self.GlowLineBottom:ClearAllPoints()
    self.GlowLineBottom:SetPoint("CENTER", self, "CENTER", 0, -107 + Ramp(t, 0.13333, 0.43333, 242, 0, "IN"))

    self.Glow1:SetAlpha(Ramp(t, 0.33333, 0.5, 0, 0.65))
    self.Glow2:SetAlpha(t < 1.2 and Ramp(t, 0.33333, 0.86666, 0, 0.65) or Ramp(t, 1.19999, 0.66666, 0.65, 0))

    for i, st in ipairs({ { self.Star1, 0.33333, 1.16666 }, { self.Star2, 0.6, 1.3 } }) do
        local star, start, fade = st[1], st[2], st[3]
        star:ClearAllPoints()
        star:SetPoint("CENTER", self, "CENTER", 0, 160 + Ramp(t, start, 1, 0, 50))
        star:SetAlpha(t < fade and Ramp(t, start, 0.3, 0, 1) or Ramp(t, fade, 0.3, 1, 0))
    end

    local a = Ramp(t, 0.5, 0.16, 0, 1)
    self.RenownLabel:SetAlpha(a)
    self.RewardIcon:SetAlpha(a)
    self.RewardIconRing:SetAlpha(a)
    self.RewardDescription:SetAlpha(a)

    -- the masked shine: one of 16 frames over 1 s from 0.5 s
    if t >= 0.5 and t <= 1.5 then
        local f = min(15, floor((t - 0.5) * 16))
        local col, row = f % 2, floor(f / 2)
        self.Shine:SetTexCoord(col / 2, (col + 1) / 2, row / 8, (row + 1) / 8)
        self.Shine:Show()
    else
        self.Shine:Hide()
    end

    self:SetAlpha(Ramp(t, OUT_DELAY, OUT_TIME, 1, 0))
    if t >= OUT_DELAY + OUT_TIME then
        self:SetScript("OnUpdate", nil)
        self:Hide()
    end
end

local function Show(level, paragon)
    if not T then Build() end
    if paragon > 0 then
        T.RenownLabel:SetText(fmt(L.paragon, paragon))
        T.RenownLabel:SetTextColor(0.9, 0.7, 1)     -- the Paragon purple of the guild bar
        T.RewardDescription:SetText(L.paragons[paragon] or L.talent)
        SetPortraitToTexture(T.RewardIcon, L.paragons[paragon] and "Interface\\Icons\\INV_Misc_Rune_06"
            or "Interface\\Icons\\Ability_Marksmanship")
    else
        T.RenownLabel:SetText(fmt(L.level, level))
        T.RenownLabel:SetTextColor(1, 0.82, 0)
        T.RewardDescription:SetText(L.perks[level] or L.talent)
        SetPortraitToTexture(T.RewardIcon, PerkIcon(level))
    end
    local faction = UnitFactionGroup("player")
    -- the faction's crest (tools/fx/bake_crests.py), where the modern client puts the faction's emblem
    T.TopIcon:SetTexture(faction == "Horde" and "Interface\\GuildLevel\\CrestHorde"
        or "Interface\\GuildLevel\\CrestAlliance")
    T.t = 0
    T:SetAlpha(1)
    T:Show()
    T:SetScript("OnUpdate", Animate)
    PlaySoundFile(SOUND, "Master")
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, _, prefix, message)
    if prefix ~= "GuildLevel" then return end
    local what, level, paragon = strsplit("\t", message)
    if what == "up" then
        Show(tonumber(level) or 0, tonumber(paragon) or 0)
    end
end)

-- /glevelup [level] [paragon]: shows the toast, to try it
SLASH_GUILDLEVELUPTEST1 = "/glevelup"
SlashCmdList["GUILDLEVELUPTEST"] = function(msg)
    local level, paragon = strsplit(" ", msg or "")
    Show(tonumber(level) or 12, tonumber(paragon) or 0)
end
