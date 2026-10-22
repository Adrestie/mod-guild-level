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
        level = "Guild level %d", paragon = "Paragon %d", talent = "A guild talent point", nothing = "The guild grows stronger",
        perks = {
            [1] = "+5% experience", [2] = "Damaged gear mended over time", [3] = "+5% honor",
            [4] = "Hearthstone cooldown 5 minutes shorter", [5] = "Lucky gathering", [6] = "Vendor prices 5% lower",
            [7] = "5% of looted gold to the guild bank", [8] = "+5% mounted speed", [9] = "+5% reputation",
            [10] = "Resurrected by a guildmate at full health", [11] = "+15% experience", [12] = "Gear mended faster",
            [13] = "+15% honor", [14] = "Hearthstone cooldown 15 minutes shorter", [15] = "Luckier gathering",
            [16] = "Vendor prices 10% lower", [17] = "10% of looted gold to the guild bank", [18] = "+10% mounted speed",
            [19] = "+10% reputation", [20] = "Flasks, elixirs and food last 30 minutes longer",
            [21] = "New spell: Guild Rally", [22] = "New spell: Mass Resurrection", [23] = "New spell: Mobile Banking",
            [24] = "New spell: Guild Supplier", [25] = "Your guild house opens",
        },
        paragons = {
            [1] = "A reagent vendor in the house", [2] = "A banker in the house",
            [3] = "Class trainers and a stable master", [4] = "Profession trainers, forge and anvil",
            [5] = "An auction house", [6] = "Portals to the capitals", [7] = "The house's Customization",
        },
    },
    frFR = {
        level = "Niveau de guilde %d", paragon = "Parangon %d", talent = "Un point de talent de guilde",
        nothing = "La guilde gagne en puissance",
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
            [25] = "Votre maison de guilde ouvre ses portes",
        },
        paragons = {
            [1] = "Un marchand de composants dans la maison", [2] = "Un banquier dans la maison",
            [3] = "Maîtres de classe et maître des écuries", [4] = "Maîtres des métiers, forge et enclume",
            [5] = "Un hôtel des ventes", [6] = "Des portails vers les capitales", [7] = "La Personnalisation de la maison",
        },
    },
    deDE = {
        level = "Gildenstufe %d", paragon = "Paragon %d", talent = "Ein Gildentalentpunkt", nothing = "Die Gilde wird stärker",
        perks = {
            [1] = "+5 % Erfahrung", [2] = "Beschädigte Ausrüstung wird mit der Zeit repariert", [3] = "+5 % Ehre",
            [4] = "Abklingzeit des Ruhesteins 5 Minuten kürzer", [5] = "Glückliches Sammeln", [6] = "Händlerpreise 5 % niedriger",
            [7] = "5 % des erbeuteten Goldes an die Gildenbank", [8] = "+5 % Reittempo", [9] = "+5 % Ruf",
            [10] = "Von einem Gildenmitglied mit voller Gesundheit wiederbelebt", [11] = "+15 % Erfahrung",
            [12] = "Ausrüstung wird schneller repariert", [13] = "+15 % Ehre",
            [14] = "Abklingzeit des Ruhesteins 15 Minuten kürzer", [15] = "Noch glücklicheres Sammeln",
            [16] = "Händlerpreise 10 % niedriger", [17] = "10 % des erbeuteten Goldes an die Gildenbank",
            [18] = "+10 % Reittempo", [19] = "+10 % Ruf", [20] = "Fläschchen, Elixiere und Essen wirken 30 Minuten länger",
            [21] = "Neuer Zauber: Gildensammlung", [22] = "Neuer Zauber: Massenwiederbelebung",
            [23] = "Neuer Zauber: Mobiles Banking", [24] = "Neuer Zauber: Gildenversorger",
            [25] = "Euer Gildenhaus öffnet seine Tore",
        },
        paragons = {
            [1] = "Ein Reagenzienhändler im Haus", [2] = "Ein Bankier im Haus",
            [3] = "Klassenlehrer und ein Stallmeister", [4] = "Berufslehrer, Schmiede und Amboss",
            [5] = "Ein Auktionshaus", [6] = "Portale zu den Hauptstädten", [7] = "Die Gestaltung des Hauses",
        },
    },
    esES = {
        level = "Nivel de hermandad %d", paragon = "Paragón %d", talent = "Un punto de talento de hermandad",
        nothing = "La hermandad se hace más fuerte",
        perks = {
            [1] = "+5 % de experiencia", [2] = "Equipo dañado reparado con el tiempo", [3] = "+5 % de honor",
            [4] = "Piedra de hogar: 5 minutos menos de reutilización", [5] = "Recolección afortunada",
            [6] = "Precios de los vendedores un 5 % más bajos", [7] = "5 % del oro despojado para el banco de hermandad",
            [8] = "+5 % de velocidad de montura", [9] = "+5 % de reputación",
            [10] = "Resucitado por un miembro de la hermandad con toda la salud", [11] = "+15 % de experiencia",
            [12] = "Equipo reparado más rápido", [13] = "+15 % de honor",
            [14] = "Piedra de hogar: 15 minutos menos de reutilización", [15] = "Recolección más afortunada",
            [16] = "Precios de los vendedores un 10 % más bajos", [17] = "10 % del oro despojado para el banco de hermandad",
            [18] = "+10 % de velocidad de montura", [19] = "+10 % de reputación",
            [20] = "Frascos, elixires y comida duran 30 minutos más",
            [21] = "Nuevo hechizo: Reunión de hermandad", [22] = "Nuevo hechizo: Resurrección en masa",
            [23] = "Nuevo hechizo: Banca móvil", [24] = "Nuevo hechizo: Proveedor de hermandad",
            [25] = "Tu casa de hermandad abre sus puertas",
        },
        paragons = {
            [1] = "Un vendedor de componentes en la casa", [2] = "Un banquero en la casa",
            [3] = "Instructores de clase y un maestro de establos", [4] = "Instructores de profesión, forja y yunque",
            [5] = "Una casa de subastas", [6] = "Portales a las capitales", [7] = "La Personalización de la casa",
        },
    },
    ruRU = {
        level = "Уровень гильдии %d", paragon = "Парагон %d", talent = "Очко таланта гильдии", nothing = "Гильдия становится сильнее",
        perks = {
            [1] = "+5% опыта", [2] = "Поврежденная экипировка постепенно чинится", [3] = "+5% чести",
            [4] = "Камень возвращения: перезарядка на 5 минут короче", [5] = "Удачный сбор", [6] = "Цены торговцев ниже на 5%",
            [7] = "5% добытого золота идет в банк гильдии", [8] = "+5% к скорости верхом", [9] = "+5% репутации",
            [10] = "Воскрешение соратником с полным здоровьем", [11] = "+15% опыта", [12] = "Экипировка чинится быстрее",
            [13] = "+15% чести", [14] = "Камень возвращения: перезарядка на 15 минут короче", [15] = "Еще более удачный сбор",
            [16] = "Цены торговцев ниже на 10%", [17] = "10% добытого золота идет в банк гильдии", [18] = "+10% к скорости верхом",
            [19] = "+10% репутации", [20] = "Настойки, эликсиры и еда действуют на 30 минут дольше",
            [21] = "Новое заклинание: Сбор гильдии", [22] = "Новое заклинание: Массовое воскрешение",
            [23] = "Новое заклинание: Мобильный банк", [24] = "Новое заклинание: Поставщик гильдии",
            [25] = "Ваш дом гильдии открывает двери",
        },
        paragons = {
            [1] = "Торговец реагентами в доме", [2] = "Банкир в доме",
            [3] = "Учителя классов и смотритель стойл", [4] = "Учителя профессий, кузница и наковальня",
            [5] = "Аукцион", [6] = "Порталы в столицы", [7] = "Оформление дома",
        },
    },
}
local L = TEXTS[GetLocale()] or TEXTS.enUS
-- the talent points start at this Paragon level (GuildLevel.Talents.FromParagon's default)
local TALENTS_FROM = 10

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
        T.RewardDescription:SetText(L.paragons[paragon] or (paragon >= TALENTS_FROM and L.talent) or L.nothing)
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
