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
    The guild talent window, client side (shipped by AIO).

    A tree of nodes on a grid, linked by lines. A node lights up with the
    guild's points: full colour when it holds some, plain when a point can go
    in (a start node, or linked to a node that holds one), dark otherwise. The
    guild master and the officers click to spend; every member looks.

    EDIT MODE, for an administrator: a click on an empty spot of the grid
    creates a node there, a click on a node edits it, dragging a node moves it,
    and a Shift-click on two nodes links or unlinks them.

    Also here: the lines the guild's aura (spell 92026) shows under its own
    text, worked out from the same tree and points.
------------------------------------------------------------------------------]]

local AIO = AIO or require("AIO")

if AIO.AddAddon() then
    return                                  -- server side: we stop here
end

local Handlers = AIO.AddHandlers("GuildTalents", {})

local floor, max, min, abs, sqrt = math.floor, math.max, math.min, math.abs, math.sqrt
local fmt = string.format

-- 3.3.5 has no Region:SetShown.
local function Shown(region, shown)
    if shown then region:Show() else region:Hide() end
end

local AURA = 92026
local CELL = 64                             -- pixels between two grid points
local NODE = 40                             -- a node's icon

-- ---------------------------------------------------------------------------
-- THE EFFECTS: GuildTalentEffect (src/GuildTalents.h). kind: 0 main
-- statistic, 1 secondary, 2 special. text: what one amount gives, %s the
-- figure.
-- ---------------------------------------------------------------------------

local I = "Interface\\Icons\\"
local EFFECTS = {
    { id = 1,  kind = 0, icon = "Spell_Holy_WordFortitude" },
    { id = 2,  kind = 0, icon = "Spell_Nature_Strength" },
    { id = 3,  kind = 0, icon = "Ability_Hunter_Pet_Cat" },
    { id = 4,  kind = 0, icon = "Spell_Holy_MagicalSentry" },
    { id = 5,  kind = 0, icon = "Spell_Shadow_BurningSpirit" },
    { id = 10, kind = 1, icon = "Ability_CriticalStrike" },
    { id = 11, kind = 1, icon = "Spell_Nature_Bloodlust" },
    { id = 12, kind = 1, icon = "Ability_Marksmanship" },
    { id = 13, kind = 1, icon = "INV_Sword_27" },
    { id = 14, kind = 1, icon = "Ability_Warrior_Sunder" },
    { id = 15, kind = 1, icon = "Ability_Warrior_BattleShout" },
    { id = 16, kind = 1, icon = "Spell_Fire_FireBolt02" },
    { id = 17, kind = 1, icon = "Spell_Nature_ManaRegenTotem" },
    { id = 18, kind = 1, icon = "Ability_Defend" },
    { id = 19, kind = 1, icon = "Spell_Nature_Invisibilty" },
    { id = 20, kind = 1, icon = "Ability_Parry" },
    { id = 21, kind = 1, icon = "INV_Shield_06" },
    { id = 22, kind = 1, icon = "Ability_Warrior_ShieldWall" },
    { id = 23, kind = 1, icon = "INV_Chest_Plate01" },
    { id = 24, kind = 1, icon = "Spell_Shadow_ShadowWordPain" },
    { id = 25, kind = 1, icon = "Spell_Frost_WizardMark" },
    { id = 26, kind = 1, icon = "INV_Potion_54" },
    { id = 27, kind = 1, icon = "INV_Potion_76" },
    { id = 40, kind = 2, icon = "Spell_Shadow_UnholyFrenzy" },
    { id = 41, kind = 2, icon = "Spell_Holy_GreaterHeal" },
    { id = 42, kind = 2, icon = "Spell_Holy_PowerWordShield" },
    { id = 43, kind = 2, icon = "Spell_Holy_PrayerOfFortitude" },
    { id = 44, kind = 2, icon = "Ability_Rogue_Sprint" },
    { id = 45, kind = 2, icon = "INV_Misc_Book_11" },
    { id = 46, kind = 2, icon = "Spell_Holy_ChampionsBond" },
    { id = 47, kind = 2, icon = "INV_Misc_Note_02" },
    { id = 48, kind = 2, icon = "Ability_Mount_RidingHorse" },
    { id = 49, kind = 2, icon = "INV_Misc_Coin_02" },
    { id = 50, kind = 2, icon = "INV_Misc_Coin_01" },
    { id = 51, kind = 2, icon = "Trade_Herbalism" },
    { id = 52, kind = 2, icon = "INV_Potion_97" },
    { id = 53, kind = 2, icon = "INV_Misc_Rune_01" },
}
local EFFECT = {}
for _, e in ipairs(EFFECTS) do EFFECT[e.id] = e end

-- ---------------------------------------------------------------------------
-- TEXTS, in the client's language (English for the others).
-- ---------------------------------------------------------------------------

local TEXTS = {
    enUS = {
        title = "Guild Talents", noGuild = "You are not in a guild.", disabled = "The guild talents are disabled.",
        points = "Points: |cffffffff%d|r available  (%d earned, %d spent)",
        level = "Level %d, Paragon %d", reset = "Reset", resetPrice = "Reset: %s gold",
        confirmBuy = "Spend a point of the guild in |cffffd100%s|r?", confirmReset =
            "Reset the guild's talents? Every point comes back, and %s gold leave the guild bank.",
        yes = "Yes", no = "No", empty = "The tree is empty.",
        rank = "Rank %d/%d", perPoint = "%s per point", now = "Now: %s", next = "Next rank: %s",
        start = "Start talent", clickBuy = "Click to spend a point.", needLink = "Needs a linked talent that holds a point.",
        officers = "The guild master and the officers spend the points.", noPoints = "No talent point left.",
        full = "Complete.",
        kind = { [0] = "Main statistic", [1] = "Secondary statistic", [2] = "Special effect" },
        edit = "Edit", editHelp = "Click: new talent or edit one.  Drag: move.  Shift+click two talents: link.",
        effect = "Effect", amount = "Amount per point", maxPoints = "Max points", icon = "Icon (empty: the effect's)",
        create = "Create", save = "Save", delete = "Delete", cancel = "Cancel", newNode = "New talent at %d, %d",
        node = "Talent %d", linkFrom = "Shift+click another talent to link it with %d.",
        auraTitle = "Guild talents:",
        effects = {
            [1] = "+%s Stamina", [2] = "+%s Strength", [3] = "+%s Agility", [4] = "+%s Intellect", [5] = "+%s Spirit",
            [10] = "+%s critical strike rating", [11] = "+%s haste rating", [12] = "+%s hit rating",
            [13] = "+%s expertise rating", [14] = "+%s armor penetration rating", [15] = "+%s attack power",
            [16] = "+%s spell power", [17] = "+%s mana every 5 seconds", [18] = "+%s defense rating",
            [19] = "+%s dodge rating", [20] = "+%s parry rating", [21] = "+%s block rating",
            [22] = "+%s resilience rating", [23] = "+%s armor", [24] = "+%s spell penetration",
            [25] = "+%s resistance to all schools", [26] = "+%s health", [27] = "+%s mana",
            [40] = "+%s%% damage done", [41] = "+%s%% healing done", [42] = "%s%% less damage taken",
            [43] = "+%s%% maximum health", [44] = "+%s%% run speed", [45] = "+%s%% experience",
            [46] = "+%s%% honor", [47] = "+%s%% reputation", [48] = "+%s%% mounted speed",
            [49] = "%s%% off vendor prices and repairs", [50] = "%s%% of looted gold added to the guild bank",
            [51] = "+%s%% chance of a lucky gathering", [52] = "+%s minutes on flasks, elixirs and food buffs",
            [53] = "%s minutes off the Hearthstone cooldown",
        },
    },
    frFR = {
        title = "Talents de guilde", noGuild = "Vous n'êtes pas dans une guilde.",
        disabled = "Les talents de guilde sont désactivés.",
        points = "Points : |cffffffff%d|r disponibles  (%d gagnés, %d dépensés)",
        level = "Niveau %d, Parangon %d", reset = "Réinitialiser", resetPrice = "Réinitialisation : %s po",
        confirmBuy = "Dépenser un point de la guilde dans |cffffd100%s|r ?", confirmReset =
            "Réinitialiser les talents de la guilde ? Tous les points reviennent, et %s po sortent de la banque de guilde.",
        yes = "Oui", no = "Non", empty = "L'arbre est vide.",
        rank = "Rang %d/%d", perPoint = "%s par point", now = "Actuellement : %s", next = "Rang suivant : %s",
        start = "Talent de départ", clickBuy = "Cliquez pour dépenser un point.",
        needLink = "Doit être relié à un talent qui a au moins un point.",
        officers = "Le maître de guilde et les officiers dépensent les points.", noPoints = "Plus aucun point de talent.",
        full = "Complet.",
        kind = { [0] = "Statistique principale", [1] = "Statistique secondaire", [2] = "Effet spécial" },
        edit = "Édition", editHelp = "Clic : nouveau talent ou modifier.  Glisser : déplacer.  Maj+clic sur deux talents : relier.",
        effect = "Effet", amount = "Valeur par point", maxPoints = "Points max", icon = "Icône (vide : celle de l'effet)",
        create = "Créer", save = "Enregistrer", delete = "Supprimer", cancel = "Annuler", newNode = "Nouveau talent en %d, %d",
        node = "Talent %d", linkFrom = "Maj+clic sur un autre talent pour le relier à %d.",
        auraTitle = "Talents de guilde :",
        effects = {
            [1] = "+%s Endurance", [2] = "+%s Force", [3] = "+%s Agilité", [4] = "+%s Intelligence", [5] = "+%s Esprit",
            [10] = "+%s au score de coup critique", [11] = "+%s au score de hâte", [12] = "+%s au score de toucher",
            [13] = "+%s au score d'expertise", [14] = "+%s au score de pénétration d'armure", [15] = "+%s à la puissance d'attaque",
            [16] = "+%s à la puissance des sorts", [17] = "+%s points de mana toutes les 5 secondes",
            [18] = "+%s au score de défense", [19] = "+%s au score d'esquive", [20] = "+%s au score de parade",
            [21] = "+%s au score de blocage", [22] = "+%s au score de résilience", [23] = "+%s à l'armure",
            [24] = "+%s à la pénétration des sorts", [25] = "+%s aux résistances à toutes les écoles",
            [26] = "+%s points de vie", [27] = "+%s points de mana",
            [40] = "+%s%% aux dégâts infligés", [41] = "+%s%% aux soins prodigués", [42] = "%s%% de dégâts subis en moins",
            [43] = "+%s%% de points de vie maximum", [44] = "+%s%% de vitesse de course", [45] = "+%s%% d'expérience",
            [46] = "+%s%% d'honneur", [47] = "+%s%% de réputation", [48] = "+%s%% de vitesse en monture",
            [49] = "%s%% de réduction chez les marchands et sur les réparations",
            [50] = "%s%% de l'or ramassé versé à la banque de guilde",
            [51] = "+%s%% de chances de récolte chanceuse", [52] = "+%s minutes aux flacons, élixirs et nourritures",
            [53] = "%s minutes de moins au temps de recharge de la pierre de foyer",
        },
    },
}
local L = TEXTS[GetLocale()] or TEXTS.enUS
if L ~= TEXTS.enUS then
    setmetatable(L, { __index = TEXTS.enUS })
    setmetatable(L.effects, { __index = TEXTS.enUS.effects })
end

local function Num(v)
    v = floor(v * 100 + 0.5) / 100
    if v == floor(v) then return tostring(floor(v)) end
    return fmt("%g", v)
end

local function EffectText(effect, amount)
    local text = L.effects[effect] or ("effect " .. effect .. ": %s")
    return fmt(text, Num(amount))
end

local function IconOf(node)
    if node.icon and node.icon ~= "" then
        if node.icon:find("\\") or node.icon:find("/") then return node.icon end
        return I .. node.icon
    end
    local e = EFFECT[node.effect]
    return I .. (e and e.icon or "INV_Misc_QuestionMark")
end

-- ---------------------------------------------------------------------------
-- THE DATA: the tree and the guild's state, from the server.
-- ---------------------------------------------------------------------------

local S = { tree = nil, state = nil, byId = {}, edit = false, linkFrom = nil }

local function Take(tree, state)
    S.tree, S.state = tree, state
    S.byId = {}
    for _, n in ipairs(tree.nodes) do
        n.links = {}
        S.byId[n.id] = n
    end
    for _, l in ipairs(tree.links) do
        local a, b = S.byId[l[1]], S.byId[l[2]]
        if a and b then
            a.links[#a.links + 1] = b.id
            b.links[#b.links + 1] = a.id
        end
    end
end

local function Points(id)
    return S.state and S.state.points[id] or 0
end

local function Left()
    return S.state and max(0, S.state.earned - S.state.spent) or 0
end

local function Reachable(n)
    if n.start or Points(n.id) > 0 then return true end
    for _, o in ipairs(n.links) do
        if Points(o) > 0 then return true end
    end
    return false
end

-- Every effect of the guild, summed: the aura's lines.
local function Totals()
    local totals, order = {}, {}
    if not S.tree then return totals, order end
    for _, n in ipairs(S.tree.nodes) do
        local p = min(Points(n.id), n.max)
        if p > 0 then
            if not totals[n.effect] then order[#order + 1] = n.effect end
            totals[n.effect] = (totals[n.effect] or 0) + n.amount * p
        end
    end
    table.sort(order)
    return totals, order
end

-- ---------------------------------------------------------------------------
-- LINES between two points of a frame: the taxi map's rotated texture.
-- ---------------------------------------------------------------------------

local LINE_TEX = "Interface\\TaxiFrame\\UI-Taxi-Line"
local LINEFACTOR_2 = (128 / 126) / 2

local function DrawLine(T, C, sx, sy, ex, ey, w)
    local dx, dy = ex - sx, ey - sy
    local cx, cy = (sx + ex) / 2, (sy + ey) / 2
    if dx < 0 then dx, dy = -dx, -dy end
    local l = sqrt(dx * dx + dy * dy)
    T:ClearAllPoints()
    if l == 0 then
        T:SetTexCoord(0, 0, 0, 0, 0, 0, 0, 0)
        T:SetPoint("BOTTOMLEFT", C, "BOTTOMLEFT", cx, cy)
        T:SetPoint("TOPRIGHT", C, "BOTTOMLEFT", cx, cy)
        return
    end
    local s, c = -dy / l, dx / l
    local sc = s * c
    local Bwid, Bhgt, BLx, BLy, TLx, TLy, TRx, TRy, BRx, BRy
    if dy >= 0 then
        Bwid = ((l * c) - (w * s)) * LINEFACTOR_2
        Bhgt = ((w * c) - (l * s)) * LINEFACTOR_2
        BLx, BLy, BRy = (w / l) * sc, s * s, (l / w) * sc
        BRx, TLx, TLy, TRx = 1 - BLy, BLy, 1 - BRy, 1 - BLx
        TRy = BRx
    else
        Bwid = ((l * c) + (w * s)) * LINEFACTOR_2
        Bhgt = ((w * c) + (l * s)) * LINEFACTOR_2
        BLx, BLy, BRx = s * s, -(l / w) * sc, 1 + (w / l) * sc
        BRy, TLx, TLy, TRy = BLx, 1 - BRx, 1 - BLx, 1 - BLy
        TRx = TLy
    end
    T:SetTexCoord(TLx, TLy, BLx, BLy, TRx, TRy, BRx, BRy)
    T:SetPoint("BOTTOMLEFT", C, "BOTTOMLEFT", cx - Bwid, cy - Bhgt)
    T:SetPoint("TOPRIGHT", C, "BOTTOMLEFT", cx + Bwid, cy + Bhgt)
end

-- ---------------------------------------------------------------------------
-- THE WINDOW
-- ---------------------------------------------------------------------------

local UI
local Refresh                               -- forward

local KIND_COLOR = { [0] = { 1, 0.82, 0.2 }, [1] = { 0.35, 0.7, 1 }, [2] = { 0.8, 0.45, 1 } }

StaticPopupDialogs["GUILD_TALENTS_BUY"] = {
    text = "%s", button1 = YES, button2 = NO, timeout = 0, whileDead = true, hideOnEscape = true,
    OnAccept = function(self) AIO.Handle("GuildTalents", "Buy", self.data) end,
}
StaticPopupDialogs["GUILD_TALENTS_RESET"] = {
    text = "%s", button1 = YES, button2 = NO, timeout = 0, whileDead = true, hideOnEscape = true,
    OnAccept = function() AIO.Handle("GuildTalents", "Reset") end,
}

-- The canvas: grid point (x, y) at pixels (ox + x*CELL, oy + y*CELL) from its
-- top left, y downward; the view pans by moving the canvas inside a clipping frame.
local function GridToCanvas(x, y)
    return UI.ox + x * CELL, UI.oy + y * CELL
end

local function CursorGrid()
    local cx, cy = GetCursorPosition()
    local scale = UI.canvas:GetEffectiveScale()
    cx, cy = cx / scale, cy / scale
    local left, top = UI.canvas:GetLeft(), UI.canvas:GetTop()
    local px, py = cx - left, top - cy
    return floor((px - UI.ox) / CELL + 0.5), floor((py - UI.oy) / CELL + 0.5)
end

local function NodeTooltip(button)
    local n = button.node
    if not n then return end
    local e = EFFECT[n.effect]
    GameTooltip:SetOwner(button, "ANCHOR_RIGHT")
    GameTooltip:AddLine(EffectText(n.effect, n.amount * max(1, Points(n.id))), 1, 1, 1)
    if e then
        local c = KIND_COLOR[e.kind]
        GameTooltip:AddLine(L.kind[e.kind], c[1], c[2], c[3])
    end
    local p = Points(n.id)
    GameTooltip:AddLine(fmt(L.rank, p, n.max), 1, 0.82, 0)
    GameTooltip:AddLine(fmt(L.perPoint, EffectText(n.effect, n.amount)), 0.8, 0.8, 0.8, true)
    if p > 0 then
        GameTooltip:AddLine(fmt(L.now, EffectText(n.effect, n.amount * p)), 0.2, 1, 0.2, true)
    end
    if p < n.max and p > 0 then
        GameTooltip:AddLine(fmt(L.next, EffectText(n.effect, n.amount * (p + 1))), 0.8, 0.8, 0.8, true)
    end
    if n.start then GameTooltip:AddLine(L.start, 1, 0.82, 0) end
    if S.edit then
        GameTooltip:AddLine(" ")
        GameTooltip:AddLine(fmt(L.node, n.id) .. fmt("  (%d, %d)", n.x, n.y), 0.5, 0.5, 0.5)
    elseif p >= n.max then
        GameTooltip:AddLine(L.full, 0.5, 0.5, 0.5)
    elseif not Reachable(n) then
        GameTooltip:AddLine(L.needLink, 1, 0.2, 0.2, true)
    elseif not S.state.canSpend then
        GameTooltip:AddLine(L.officers, 0.6, 0.6, 0.6, true)
    elseif Left() <= 0 then
        GameTooltip:AddLine(L.noPoints, 1, 0.2, 0.2)
    else
        GameTooltip:AddLine(L.clickBuy, 0.2, 1, 0.2)
    end
    GameTooltip:Show()
end

-- EDIT PANEL --------------------------------------------------------------

local function EffectName(id)
    return (EffectText(id, 0):gsub("^[%+%-]?0%%? ?", ""):gsub("^0 ", ""))
end

local function BuildEditor()
    local p = CreateFrame("Frame", "GuildTalentsEditor", UI)
    p:SetSize(300, 236)
    p:SetPoint("TOPRIGHT", UI, "TOPRIGHT", -18, -70)
    p:SetBackdrop({ bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", tile = true, tileSize = 16, edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 } })
    p:SetFrameLevel(UI:GetFrameLevel() + 20)
    p:EnableMouse(true)
    p:Hide()

    p.title = p:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    p.title:SetPoint("TOPLEFT", 12, -10)

    local function Label(text, y)
        local f = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        f:SetPoint("TOPLEFT", 12, y)
        f:SetText(text)
        return f
    end
    local function Box(name, y, w)
        local b = CreateFrame("EditBox", name, p, "InputBoxTemplate")
        b:SetSize(w, 20)
        b:SetPoint("TOPLEFT", 16, y)
        b:SetAutoFocus(false)
        b:SetScript("OnEscapePressed", b.ClearFocus)
        b:SetScript("OnEnterPressed", b.ClearFocus)
        return b
    end

    Label(L.effect, -32)
    local dd = CreateFrame("Frame", "GuildTalentsEffectDropDown", p, "UIDropDownMenuTemplate")
    dd:SetPoint("TOPLEFT", -4, -44)
    UIDropDownMenu_SetWidth(dd, 250)
    UIDropDownMenu_Initialize(dd, function(self, level)
        for _, e in ipairs(EFFECTS) do
            local info = UIDropDownMenu_CreateInfo()
            local c = KIND_COLOR[e.kind]
            info.text = fmt("|cff%02x%02x%02x%s|r", c[1] * 255, c[2] * 255, c[3] * 255, EffectName(e.id))
            info.value = e.id
            info.checked = p.effect == e.id
            info.func = function()
                p.effect = e.id
                UIDropDownMenu_SetText(dd, info.text)
                CloseDropDownMenus()
            end
            UIDropDownMenu_AddButton(info, level)
        end
    end)
    p.dropdown = dd

    Label(L.amount, -80)
    p.amount = Box("GuildTalentsEditAmount", -92, 80)
    local mx = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    mx:SetPoint("TOPLEFT", 150, -80)
    mx:SetText(L.maxPoints)
    p.max = Box("GuildTalentsEditMax", -92, 60)
    p.max:ClearAllPoints()
    p.max:SetPoint("TOPLEFT", 154, -92)

    Label(L.icon, -118)
    p.icon = Box("GuildTalentsEditIcon", -130, 260)

    p.start = CreateFrame("CheckButton", "GuildTalentsEditStart", p, "UICheckButtonTemplate")
    p.start:SetSize(24, 24)
    p.start:SetPoint("TOPLEFT", 10, -156)
    _G["GuildTalentsEditStartText"]:SetText(L.start)

    local function Button(text, x, w)
        local b = CreateFrame("Button", nil, p, "UIPanelButtonTemplate")
        b:SetSize(w, 22)
        b:SetPoint("BOTTOMLEFT", x, 12)
        b:SetText(text)
        return b
    end
    p.ok = Button(L.save, 12, 90)
    p.del = Button(L.delete, 106, 90)
    p.cancel = Button(L.cancel, 200, 88)

    p.cancel:SetScript("OnClick", function() p:Hide() end)
    p.del:SetScript("OnClick", function()
        if p.id then AIO.Handle("GuildTalents", "Delete", p.id) end
        p:Hide()
    end)
    p.ok:SetScript("OnClick", function()
        local amount = tonumber(p.amount:GetText())
        local maxPoints = tonumber(p.max:GetText())
        if not (p.effect and amount and maxPoints and maxPoints >= 1) then return end
        maxPoints = floor(maxPoints)
        local start = p.start:GetChecked() and true or false
        local icon = p.icon:GetText() or ""
        if p.id then
            local n = S.byId[p.id]
            AIO.Handle("GuildTalents", "Set", p.id, p.effect, amount, maxPoints)
            if n and n.start ~= start then AIO.Handle("GuildTalents", "Start", p.id, start) end
            if n and (n.icon or "") ~= icon then AIO.Handle("GuildTalents", "Icon", p.id, icon) end
        else
            AIO.Handle("GuildTalents", "Add", p.x, p.y, p.effect, amount, maxPoints)
        end
        p:Hide()
    end)

    -- Opens on a node (n) or on an empty grid point (x, y).
    function p:Open(n, x, y)
        self.id = n and n.id or nil
        self.x, self.y = x, y
        self.effect = n and n.effect or self.effect or 1
        self.title:SetText(n and fmt(L.node, n.id) or fmt(L.newNode, x, y))
        UIDropDownMenu_SetText(self.dropdown, EffectName(self.effect))
        self.amount:SetText(n and Num(n.amount) or (self.amount:GetText() ~= "" and self.amount:GetText() or "10"))
        self.max:SetText(n and tostring(n.max) or (self.max:GetText() ~= "" and self.max:GetText() or "5"))
        self.icon:SetText(n and n.icon or "")
        self.start:SetChecked(n and n.start or false)
        Shown(self.start, n ~= nil)
        self.ok:SetText(n and L.save or L.create)
        Shown(self.del, n ~= nil)
        self:Show()
    end

    UI.editor = p
end

-- NODES AND LINES ---------------------------------------------------------

local function NodeButton(i)
    local b = UI.buttons[i]
    if b then return b end
    b = CreateFrame("Button", nil, UI.canvas)
    b:SetSize(NODE, NODE)
    b:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    b:RegisterForDrag("LeftButton")
    b.glow = b:CreateTexture(nil, "BACKGROUND")
    b.glow:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
    b.glow:SetBlendMode("ADD")
    b.glow:SetSize(NODE * 1.9, NODE * 1.9)
    b.glow:SetPoint("CENTER")
    b.icon = b:CreateTexture(nil, "ARTWORK")
    b.icon:SetAllPoints()
    b.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    b.border = b:CreateTexture(nil, "OVERLAY")
    b.border:SetTexture("Interface\\Buttons\\UI-Quickslot2")
    b.border:SetSize(NODE * 1.65, NODE * 1.65)
    b.border:SetPoint("CENTER")
    b.rankBg = b:CreateTexture(nil, "OVERLAY")
    b.rankBg:SetTexture(0, 0, 0, 0.75)
    b.rankBg:SetSize(30, 13)
    b.rankBg:SetPoint("BOTTOMRIGHT", 6, -6)
    b.rank = b:CreateFontString(nil, "OVERLAY", "NumberFontNormalSmall")
    b.rank:SetPoint("CENTER", b.rankBg, "CENTER", 0, 0)
    b.startMark = b:CreateTexture(nil, "OVERLAY")
    b.startMark:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcon_1")
    b.startMark:SetSize(14, 14)
    b.startMark:SetPoint("TOPLEFT", -6, 6)

    b:SetScript("OnEnter", NodeTooltip)
    b:SetScript("OnLeave", function() GameTooltip:Hide() end)
    b:SetScript("OnDragStart", function(self)
        if not S.edit then return end
        self.dragging = true
        self:SetFrameLevel(self:GetFrameLevel() + 5)
        self:SetScript("OnUpdate", function(me)
            local cx, cy = GetCursorPosition()
            local scale = UI.canvas:GetEffectiveScale()
            me:ClearAllPoints()
            me:SetPoint("CENTER", UI.canvas, "BOTTOMLEFT", cx / scale - UI.canvas:GetLeft(), cy / scale - UI.canvas:GetBottom())
        end)
    end)
    b:SetScript("OnDragStop", function(self)
        self:SetScript("OnUpdate", nil)
        if not self.dragging then return end
        self.dragging = false
        self.dragged = true
        local x, y = CursorGrid()
        local n = self.node
        if n and (x ~= n.x or y ~= n.y) then
            AIO.Handle("GuildTalents", "Move", n.id, x, y)
            n.x, n.y = x, y
        end
        Refresh()
    end)
    b:SetScript("OnClick", function(self, button)
        local n = self.node
        if not n then return end
        if self.dragged then
            self.dragged = false
            return
        end
        if S.edit then
            if IsShiftKeyDown() then
                if S.linkFrom and S.linkFrom ~= n.id then
                    AIO.Handle("GuildTalents", "Link", S.linkFrom, n.id)
                    S.linkFrom = nil
                else
                    S.linkFrom = n.id
                end
                Refresh()
            else
                S.linkFrom = nil
                UI.editor:Open(n)
            end
            return
        end
        if button ~= "LeftButton" or not S.state.canSpend or Points(n.id) >= n.max or not Reachable(n) or Left() <= 0 then
            return
        end
        local dialog = StaticPopup_Show("GUILD_TALENTS_BUY", fmt(L.confirmBuy, EffectText(n.effect, n.amount)))
        if dialog then dialog.data = n.id end
    end)
    UI.buttons[i] = b
    return b
end

local function LineTexture(i)
    local t = UI.lines[i]
    if t then return t end
    t = UI.canvas:CreateTexture(nil, "BORDER")
    t:SetTexture(LINE_TEX)
    UI.lines[i] = t
    return t
end

-- The canvas fits the tree with a margin all around; the grid's origin sits
-- where the tree's top left needs it.
local function Layout()
    local minX, minY, maxX, maxY = 0, 0, 0, 0
    for _, n in ipairs(S.tree.nodes) do
        minX, minY = min(minX, n.x), min(minY, n.y)
        maxX, maxY = max(maxX, n.x), max(maxY, n.y)
    end
    local margin = S.edit and 8 or 2
    UI.ox = (margin - minX) * CELL
    UI.oy = (margin - minY) * CELL
    UI.canvasW = (maxX - minX + 2 * margin) * CELL
    UI.canvasH = (maxY - minY + 2 * margin) * CELL
    UI.canvas:SetSize(max(UI.canvasW, 10), max(UI.canvasH, 10))
end

Refresh = function()
    if not UI or not S.tree or not S.state then return end
    local st = S.state

    UI.titleText:SetText(L.title .. (st.name ~= "" and (" - " .. st.name) or ""))
    if not st.guild then
        UI.info:SetText(L.noGuild)
    elseif not st.enable then
        UI.info:SetText(L.disabled)
    else
        UI.info:SetText(fmt(L.points, Left(), st.earned, st.spent) .. "     |cffaaaaaa" .. fmt(L.level, st.level, st.paragon) .. "|r")
    end
    UI.reset:SetText(fmt(L.resetPrice, st.price))
    Shown(UI.reset, st.guild and st.canSpend and st.spent > 0)
    Shown(UI.editButton, st.canEdit)
    UI.editButton:SetText(L.edit .. (S.edit and " (on)" or ""))
    UI.help:SetText(S.edit and (S.linkFrom and fmt(L.linkFrom, S.linkFrom) or L.editHelp) or
        (#S.tree.nodes == 0 and L.empty or ""))
    Shown(UI.gridTex, S.edit)

    Layout()

    -- lines
    local li = 0
    for _, l in ipairs(S.tree.links) do
        local a, b = S.byId[l[1]], S.byId[l[2]]
        if a and b then
            li = li + 1
            local t = LineTexture(li)
            local ax, ay = GridToCanvas(a.x, a.y)
            local bx, by = GridToCanvas(b.x, b.y)
            DrawLine(t, UI.canvas, ax, UI.canvasH - ay, bx, UI.canvasH - by, 26)
            local lit = Points(a.id) > 0 and Points(b.id) > 0
            local half = Points(a.id) > 0 or Points(b.id) > 0
            if lit then t:SetVertexColor(1, 0.85, 0.3, 1)
            elseif half then t:SetVertexColor(0.7, 0.7, 0.7, 0.9)
            else t:SetVertexColor(0.35, 0.35, 0.35, 0.8) end
            t:Show()
        end
    end
    for i = li + 1, #UI.lines do UI.lines[i]:Hide() end

    -- nodes
    for i, n in ipairs(S.tree.nodes) do
        local b = NodeButton(i)
        b.node = n
        local x, y = GridToCanvas(n.x, n.y)
        b:ClearAllPoints()
        b:SetPoint("CENTER", UI.canvas, "TOPLEFT", x, -y)
        b.icon:SetTexture(IconOf(n))
        local p = Points(n.id)
        local e = EFFECT[n.effect]
        local c = KIND_COLOR[e and e.kind or 2]
        b.border:SetVertexColor(c[1], c[2], c[3])
        b.rank:SetText(fmt("%d/%d", p, n.max))
        local reach = Reachable(n)
        if p > 0 then
            b.icon:SetDesaturated(false); b.icon:SetVertexColor(1, 1, 1)
            b.rank:SetTextColor(p >= n.max and 1 or 0.2, p >= n.max and 0.82 or 1, p >= n.max and 0 or 0.2)
            b.glow:SetVertexColor(c[1], c[2], c[3]); b.glow:Show()
        elseif reach then
            b.icon:SetDesaturated(false); b.icon:SetVertexColor(0.75, 0.75, 0.75)
            b.rank:SetTextColor(0.2, 1, 0.2)
            b.glow:Hide()
        else
            b.icon:SetDesaturated(true); b.icon:SetVertexColor(0.45, 0.45, 0.45)
            b.rank:SetTextColor(0.5, 0.5, 0.5)
            b.glow:Hide()
        end
        if S.edit and S.linkFrom == n.id then
            b.glow:SetVertexColor(0.2, 1, 0.2); b.glow:Show()
        end
        Shown(b.startMark, n.start)
        b:Show()
    end
    for i = #S.tree.nodes + 1, #UI.buttons do UI.buttons[i]:Hide() end

    if GameTooltip:IsOwned(UI) then GameTooltip:Hide() end
end

-- PAN AND ZOOM ------------------------------------------------------------

local function ClampPan()
    local vw, vh = UI.view:GetWidth() / UI.zoom, UI.view:GetHeight() / UI.zoom
    local w, h = UI.canvasW or 0, UI.canvasH or 0
    local minX, maxX = min(0, vw - w), max(0, vw - w)
    local minY, maxY = min(0, vh - h), max(0, vh - h)
    if w < vw then UI.panX = (vw - w) / 2 else UI.panX = max(minX, min(maxX, UI.panX)) end
    if h < vh then UI.panY = (vh - h) / 2 else UI.panY = max(minY, min(maxY, UI.panY)) end
    UI.canvas:ClearAllPoints()
    UI.canvas:SetPoint("TOPLEFT", UI.view, "TOPLEFT", UI.panX, -UI.panY)
end

local function Center()
    UI.panX = (UI.view:GetWidth() / UI.zoom - (UI.canvasW or 0)) / 2
    UI.panY = (UI.view:GetHeight() / UI.zoom - (UI.canvasH or 0)) / 2
    ClampPan()
end

local function Build()
    UI = CreateFrame("Frame", "GuildTalentsFrame", UIParent)
    UI:SetSize(820, 600)
    UI:SetPoint("CENTER")
    UI:SetFrameStrata("HIGH")
    UI:SetToplevel(true)
    UI:SetMovable(true)
    UI:EnableMouse(true)
    UI:SetClampedToScreen(true)
    UI:SetBackdrop({ bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border", tile = true, tileSize = 32, edgeSize = 32,
        insets = { left = 11, right = 12, top = 12, bottom = 11 } })
    UI:RegisterForDrag("LeftButton")
    UI:SetScript("OnDragStart", UI.StartMoving)
    UI:SetScript("OnDragStop", UI.StopMovingOrSizing)
    tinsert(UISpecialFrames, "GuildTalentsFrame")
    UI.buttons, UI.lines = {}, {}
    UI.zoom, UI.panX, UI.panY = 1, 0, 0

    local header = UI:CreateTexture(nil, "ARTWORK")
    header:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
    header:SetSize(360, 64)
    header:SetPoint("TOP", 0, 12)
    UI.titleText = UI:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    UI.titleText:SetPoint("TOP", header, "TOP", 0, -14)

    local close = CreateFrame("Button", nil, UI, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -6, -6)

    UI.info = UI:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    UI.info:SetPoint("TOPLEFT", 24, -34)

    UI.reset = CreateFrame("Button", nil, UI, "UIPanelButtonTemplate")
    UI.reset:SetSize(200, 22)
    UI.reset:SetPoint("BOTTOMLEFT", 20, 18)
    UI.reset:SetScript("OnClick", function()
        StaticPopup_Show("GUILD_TALENTS_RESET", fmt(L.confirmReset, S.state.price))
    end)

    UI.editButton = CreateFrame("Button", nil, UI, "UIPanelButtonTemplate")
    UI.editButton:SetSize(110, 22)
    UI.editButton:SetPoint("BOTTOMRIGHT", -20, 18)
    UI.editButton:SetScript("OnClick", function()
        S.edit = not S.edit
        S.linkFrom = nil
        if not S.edit then UI.editor:Hide() end
        Refresh()
        Center()
    end)

    UI.help = UI:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    UI.help:SetPoint("BOTTOM", 0, 24)

    -- the view: clips the canvas
    UI.view = CreateFrame("ScrollFrame", "GuildTalentsView", UI)
    UI.view:SetPoint("TOPLEFT", 18, -56)
    UI.view:SetPoint("BOTTOMRIGHT", -18, 48)
    local bg = UI.view:CreateTexture(nil, "BACKGROUND")
    bg:SetAllPoints()
    bg:SetTexture(0, 0, 0, 0.45)

    local holder = CreateFrame("Frame", nil, UI.view)
    holder:SetSize(1, 1)
    UI.view:SetScrollChild(holder)
    holder:SetAllPoints(UI.view)
    UI.canvas = CreateFrame("Frame", nil, holder)
    UI.canvas:SetSize(10, 10)
    UI.canvas:SetPoint("TOPLEFT")

    -- the grid of edit mode: a dot every cell
    UI.gridTex = UI.canvas:CreateTexture(nil, "BACKGROUND")
    UI.gridTex:SetAllPoints()
    UI.gridTex:SetTexture(1, 1, 1, 0.03)

    UI.view:EnableMouse(true)
    UI.view:EnableMouseWheel(true)
    UI.view:SetScript("OnMouseWheel", function(_, delta)
        local old = UI.zoom
        UI.zoom = max(0.4, min(1.6, UI.zoom + delta * 0.1))
        if old == UI.zoom then return end
        -- keep the view's middle where it was
        local vw, vh = UI.view:GetWidth(), UI.view:GetHeight()
        UI.panX = (UI.panX - vw / 2 / old) + vw / 2 / UI.zoom
        UI.panY = (UI.panY - vh / 2 / old) + vh / 2 / UI.zoom
        holder:SetScale(UI.zoom)
        ClampPan()
    end)
    UI.view:SetScript("OnMouseDown", function(_, button)
        if button ~= "LeftButton" then return end
        local cx, cy = GetCursorPosition()
        UI.drag = { x = cx, y = cy, px = UI.panX, py = UI.panY, moved = false }
        UI.view:SetScript("OnUpdate", function()
            local d = UI.drag
            if not d then return end
            local nx, ny = GetCursorPosition()
            local s = holder:GetEffectiveScale()
            local dx, dy = (nx - d.x) / s, (ny - d.y) / s
            if abs(dx) + abs(dy) > 3 then d.moved = true end
            UI.panX, UI.panY = d.px + dx, d.py - dy
            ClampPan()
        end)
    end)
    UI.view:SetScript("OnMouseUp", function(_, button)
        UI.view:SetScript("OnUpdate", nil)
        local d = UI.drag
        UI.drag = nil
        if button ~= "LeftButton" or not d or d.moved or not S.edit then return end
        -- a click on an empty spot of the grid: a new node there
        local x, y = CursorGrid()
        for _, n in ipairs(S.tree.nodes) do
            if n.x == x and n.y == y then return end
        end
        S.linkFrom = nil
        UI.editor:Open(nil, x, y)
    end)

    BuildEditor()
    UI:Hide()
end

local function Show()
    if not UI then Build() end
    Refresh()
    if not UI:IsShown() then
        UI:Show()
        Center()
    else
        ClampPan()
    end
end

-- ---------------------------------------------------------------------------
-- THE SERVER'S MESSAGES
-- ---------------------------------------------------------------------------

function Handlers.Prime(_, tree, state)
    Take(tree, state)
end

function Handlers.Show(_, tree, state)
    Take(tree, state)
    Show()
end

function Handlers.Update(_, tree, state)
    Take(tree, state)
    if UI and UI:IsShown() then
        Refresh()
        ClampPan()
    end
end

-- The module says the guild's talents or the tree moved (CHAT_MSG_ADDON).
local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_GUILD_UPDATE")
listener:SetScript("OnEvent", function(_, event, prefix)
    if event == "CHAT_MSG_ADDON" and prefix ~= "GuildTalents" then return end
    AIO.Handle("GuildTalents", "Refresh")
end)

-- ---------------------------------------------------------------------------
-- THE WAYS IN: /gtalent, and a button on the guild window (the stock one, or
-- ForeverUI's guild page).
-- ---------------------------------------------------------------------------

SLASH_GUILDTALENTS1 = "/gtalent"
SlashCmdList["GUILDTALENTS"] = function()
    if UI and UI:IsShown() then UI:Hide() else AIO.Handle("GuildTalents", "Open") end
end

local guildButtons = {}
local function AttachButton(parent, x, y)
    if not parent or guildButtons[parent] then return end
    local b = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    b:SetSize(130, 22)
    b:SetPoint("TOPRIGHT", parent, "TOPRIGHT", x, y)
    b:SetText(L.title)
    b:SetFrameLevel(parent:GetFrameLevel() + 10)
    b:SetScript("OnClick", function() AIO.Handle("GuildTalents", "Open") end)
    guildButtons[parent] = b
end

local function AttachAll()
    AttachButton(_G.GuildFrame, -44, -44)
    AttachButton(_G.ForeverUISocialPage3, -40, -34)
end

if FriendsFrame then
    FriendsFrame:HookScript("OnShow", AttachAll)
end
if FriendsFrame_Update then hooksecurefunc("FriendsFrame_Update", AttachAll) end
AttachAll()

-- ---------------------------------------------------------------------------
-- THE AURA'S TOOLTIP: the guild's effects, under the aura's own text.
-- ---------------------------------------------------------------------------

function GUILD_TALENTS_ON_BUFF_TOOLTIP(tip, unit, index, filter)
    local name, _, _, _, _, _, _, _, _, _, spellId = UnitAura(unit, index, filter)
    if not name then return end
    if spellId ~= AURA and name ~= GetSpellInfo(AURA) then return end
    local totals, order = Totals()
    if #order == 0 then return end
    tip:AddLine(" ")
    for _, effect in ipairs(order) do
        tip:AddLine(EffectText(effect, totals[effect]), 0.2, 1, 0.2, true)
    end
    tip:Show()
end

if not GUILD_TALENTS_AURA_HOOKED then
    GUILD_TALENTS_AURA_HOOKED = true
    hooksecurefunc(GameTooltip, "SetUnitBuff", function(tip, unit, index, filter)
        GUILD_TALENTS_ON_BUFF_TOOLTIP(tip, unit, index, filter or "HELPFUL")
    end)
    hooksecurefunc(GameTooltip, "SetUnitAura", function(tip, unit, index, filter)
        GUILD_TALENTS_ON_BUFF_TOOLTIP(tip, unit, index, filter)
    end)
end

AIO.Handle("GuildTalents", "Hello")
