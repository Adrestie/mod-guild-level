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

local EffectName                            -- forward

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
    { id = 28, kind = 1, icon = "Spell_Holy_HolyBolt" },
    { id = 29, kind = 1, icon = "Spell_Fire_FireBolt02" },
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
    { id = 54, kind = 2, icon = "Ability_Rogue_Eviscerate" },
    { id = 61, kind = 2, icon = "Ability_Warrior_Sunder", proc = true },
    { id = 62, kind = 2, icon = "Spell_Nature_Bloodlust", proc = true },
    { id = 63, kind = 2, icon = "Ability_Rogue_SliceDice", proc = true },
    { id = 64, kind = 2, icon = "Spell_Holy_PowerWordShield", proc = true },
    { id = 65, kind = 2, icon = "Ability_Warrior_Riposte", proc = true },
    { id = 66, kind = 2, icon = "Spell_Shadow_ShadowWard", proc = true },
    { id = 67, kind = 2, icon = "Spell_Shadow_ManaBurn", proc = true },
    { id = 68, kind = 2, icon = "Spell_Nature_EnchantArmor", proc = true },
    { id = 69, kind = 2, icon = "Spell_Nature_Rejuvenation", proc = true },
    { id = 60, kind = 3, icon = "Spell_Holy_ChampionsBond" },
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
        kind = { [0] = "Main statistic", [1] = "Secondary statistic", [2] = "Special effect", [3] = "Origin" },
        origin = "The origin of the tree: always held.", tier = "Tier %d", tierLocked = "Tier %d: needs %d points spent by the guild.",
        chance = "Chance (%)", cooldown = "Cooldown (s)", tierLabel = "Tier (1-3)",
        cooldownText = " (%s sec. cooldown)",
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
            [28] = "+%s healing power", [29] = "+%s spell damage", [54] = "+%s%% critical damage",
            [60] = "Origin",
            [61] = "Your attacks have a {c}% chance to ignore {a}% of the target's armor for 3 sec.",
            [62] = "Your attacks have a {c}% chance to increase your damage by {a}% for 10 sec.",
            [63] = "Your melee hits have a {c}% chance to give you {a} extra attack(s).",
            [64] = "Blocking has a {c}% chance to shield you for {a}% of your maximum health.",
            [65] = "Parrying has a {c}% chance to strike back for {a}% of your maximum health.",
            [66] = "Dodging has a {c}% chance to reduce damage taken by {a}% for 5 sec., up to 5 times.",
            [67] = "Your spells have a {c}% chance to make your next spell free.",
            [68] = "Your spells have a {c}% chance to make your next spell instant.",
            [69] = "Your attacks and spells have a {c}% chance to give you {a} rage, energy or runic power, or {m}% of your mana.",
        },
        names = {
            [28] = "Healing power", [29] = "Spell damage", [54] = "Critical damage", [60] = "Origin",
            [61] = "Armor ignored", [62] = "Damage burst", [63] = "Extra attack", [64] = "Shield on block",
            [65] = "Riposte on parry", [66] = "Evasion on dodge", [67] = "Free spell", [68] = "Instant spell",
            [69] = "Resource",
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
        kind = { [0] = "Statistique principale", [1] = "Statistique secondaire", [2] = "Effet spécial", [3] = "Origine" },
        origin = "L'origine de l'arbre : toujours acquise.", tier = "Palier %d",
        tierLocked = "Palier %d : la guilde doit avoir dépensé %d points.",
        chance = "Chance (%)", cooldown = "Recharge (s)", tierLabel = "Palier (1-3)",
        cooldownText = " (recharge %s s)",
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
            [28] = "+%s à la puissance des soins", [29] = "+%s aux dégâts des sorts", [54] = "+%s%% aux dégâts critiques",
            [60] = "Origine",
            [61] = "Vos attaques ont {c}% de chances d'ignorer {a}% de l'armure de la cible pendant 3 s.",
            [62] = "Vos attaques ont {c}% de chances d'augmenter vos dégâts de {a}% pendant 10 s.",
            [63] = "Vos coups en mêlée ont {c}% de chances de vous donner {a} attaque(s) supplémentaire(s).",
            [64] = "Bloquer a {c}% de chances de vous protéger d'un bouclier de {a}% de vos points de vie maximum.",
            [65] = "Parer a {c}% de chances de contre-attaquer pour {a}% de vos points de vie maximum.",
            [66] = "Esquiver a {c}% de chances de réduire les dégâts subis de {a}% pendant 5 s, cumulable 5 fois.",
            [67] = "Vos sorts ont {c}% de chances de rendre votre prochain sort gratuit.",
            [68] = "Vos sorts ont {c}% de chances de rendre votre prochain sort instantané.",
            [69] = "Vos attaques et vos sorts ont {c}% de chances de vous rendre {a} points de rage, d'énergie ou de puissance runique, ou {m}% de votre mana.",
        },
        names = {
            [28] = "Puissance des soins", [29] = "Dégâts des sorts", [54] = "Dégâts critiques", [60] = "Origine",
            [61] = "Armure ignorée", [62] = "Sursaut de dégâts", [63] = "Attaque supplémentaire", [64] = "Bouclier au blocage",
            [65] = "Riposte à la parade", [66] = "Dérobade à l'esquive", [67] = "Sort gratuit", [68] = "Sort instantané",
            [69] = "Ressource",
        },
    },
}
local L = TEXTS[GetLocale()] or TEXTS.enUS
if L ~= TEXTS.enUS then
    setmetatable(L, { __index = TEXTS.enUS })
    setmetatable(L.effects, { __index = TEXTS.enUS.effects })
    if L.names then setmetatable(L.names, { __index = TEXTS.enUS.names }) end
end

local function Num(v)
    v = floor(v * 100 + 0.5) / 100
    if v == floor(v) then return tostring(floor(v)) end
    return fmt("%g", v)
end

-- What an amount of an effect gives; an effect that procs also says its
-- chance and cooldown.
local function EffectText(effect, amount, chance, cooldown)
    local text = L.effects[effect] or ("effect " .. effect .. ": %s")
    if text:find("{a}") or text:find("{c}") then
        text = text:gsub("{a}", Num(amount)):gsub("{c}", Num(chance or 0)):gsub("{m}", Num(amount / 5))
        if cooldown and cooldown > 0 then text = text .. fmt(L.cooldownText, Num(cooldown)) end
        return text
    end
    if not text:find("%%s") then return text end
    return fmt(text, Num(amount))
end

-- An effect's short name, for the lists and the tooltips' titles.
EffectName = function(id)
    if L.names and L.names[id] then return L.names[id] end
    return (EffectText(id, 0):gsub("^[%+%-]?0%%? ?", ""):gsub("^0 ", ""))
end

local function NodeText(n, points)
    return EffectText(n.effect, n.amount * points, n.chance, n.cooldown)
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

local ORIGIN = 60

-- The origin always holds its point.
local function Points(id)
    local n = S.byId[id]
    if n and n.effect == ORIGIN then return 1 end
    return S.state and S.state.points[id] or 0
end

-- Points the guild must have spent before a node of this tier.
local function TierNeed(n)
    local t = S.state and S.state.tiers
    return t and t[n.tier or 1] or 0
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

-- Every effect of the guild, summed: the aura's lines. Effects that proc
-- keep the better chance and the shorter cooldown (GuildTalentMgr::Total).
local function Totals()
    local totals, order = {}, {}
    if not S.tree then return totals, order end
    for _, n in ipairs(S.tree.nodes) do
        local p = min(Points(n.id), n.max)
        if p > 0 and n.effect ~= ORIGIN then
            local t = totals[n.effect]
            if not t then
                t = { amount = 0, chance = n.chance or 0, cooldown = n.cooldown or 0 }
                totals[n.effect] = t
                order[#order + 1] = n.effect
            end
            t.amount = t.amount + n.amount * p
            t.chance = max(t.chance, n.chance or 0)
            t.cooldown = min(t.cooldown, n.cooldown or 0)
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

local WINDOW, EMBED                         -- the places of the pieces (below)

local KIND_COLOR = { [0] = { 1, 0.82, 0.2 }, [1] = { 0.35, 0.7, 1 }, [2] = { 0.8, 0.45, 1 }, [3] = { 1, 1, 1 } }

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
    local p = Points(n.id)
    GameTooltip:AddLine(EffectName(n.effect), 1, 1, 1)
    if e then
        local c = KIND_COLOR[e.kind]
        GameTooltip:AddLine(L.kind[e.kind] .. (n.effect ~= ORIGIN and ("  -  " .. fmt(L.tier, n.tier or 1)) or ""),
            c[1], c[2], c[3])
    end
    if n.effect == ORIGIN then
        GameTooltip:AddLine(L.origin, 0.8, 0.8, 0.8, true)
        if S.edit then GameTooltip:AddLine(fmt(L.node, n.id) .. fmt("  (%d, %d)", n.x, n.y), 0.5, 0.5, 0.5) end
        GameTooltip:Show()
        return
    end
    GameTooltip:AddLine(fmt(L.rank, p, n.max), 1, 0.82, 0)
    if n.max > 1 then
        GameTooltip:AddLine(fmt(L.perPoint, NodeText(n, 1)), 0.8, 0.8, 0.8, true)
    end
    if p > 0 then
        GameTooltip:AddLine(fmt(L.now, NodeText(n, p)), 0.2, 1, 0.2, true)
    end
    if p < n.max then
        GameTooltip:AddLine(p > 0 and fmt(L.next, NodeText(n, p + 1)) or NodeText(n, 1), 0.8, 0.8, 0.8, true)
    end
    if n.start then GameTooltip:AddLine(L.start, 1, 0.82, 0) end
    local need = TierNeed(n)
    if not S.edit and p < n.max and S.state.spent < need then
        GameTooltip:AddLine(fmt(L.tierLocked, n.tier or 1, need), 1, 0.2, 0.2, true)
    end
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

local function BuildEditor()
    local p = CreateFrame("Frame", "GuildTalentsEditor", UI)
    p:SetSize(300, 300)
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
    -- a menu per kind, its effects in a sub-menu: the list stays on screen
    local function Colored(e)
        local c = KIND_COLOR[e.kind]
        return fmt("|cff%02x%02x%02x%s|r", c[1] * 255, c[2] * 255, c[3] * 255, EffectName(e.id))
    end
    UIDropDownMenu_Initialize(dd, function(self, level)
        level = level or 1
        if level == 1 then
            for _, kind in ipairs({ 0, 1, 2, 3 }) do
                local info = UIDropDownMenu_CreateInfo()
                local c = KIND_COLOR[kind]
                info.text = fmt("|cff%02x%02x%02x%s|r", c[1] * 255, c[2] * 255, c[3] * 255, L.kind[kind])
                info.hasArrow = true
                info.notCheckable = true
                info.value = kind
                UIDropDownMenu_AddButton(info, level)
            end
            return
        end
        local kind = UIDROPDOWNMENU_MENU_VALUE
        for _, e in ipairs(EFFECTS) do
            if e.kind == kind then
                local info = UIDropDownMenu_CreateInfo()
                info.text = Colored(e)
                info.value = e.id
                info.checked = p.effect == e.id
                info.func = function()
                    p.effect = e.id
                    UIDropDownMenu_SetText(dd, Colored(e))
                    p:UpdateFields()
                    CloseDropDownMenus()
                end
                UIDropDownMenu_AddButton(info, level)
            end
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

    p.chanceLabel = Label(L.chance, -118)
    p.chance = Box("GuildTalentsEditChance", -130, 60)
    p.cooldownLabel = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    p.cooldownLabel:SetPoint("TOPLEFT", 100, -118)
    p.cooldownLabel:SetText(L.cooldown)
    p.cooldown = Box("GuildTalentsEditCooldown", -130, 60)
    p.cooldown:ClearAllPoints()
    p.cooldown:SetPoint("TOPLEFT", 104, -130)
    local tl = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    tl:SetPoint("TOPLEFT", 190, -118)
    tl:SetText(L.tierLabel)
    p.tier = Box("GuildTalentsEditTier", -130, 40)
    p.tier:ClearAllPoints()
    p.tier:SetPoint("TOPLEFT", 194, -130)

    Label(L.icon, -156)
    p.icon = Box("GuildTalentsEditIcon", -168, 260)

    p.start = CreateFrame("CheckButton", "GuildTalentsEditStart", p, "UICheckButtonTemplate")
    p.start:SetSize(24, 24)
    p.start:SetPoint("TOPLEFT", 10, -194)

    -- the chance and the cooldown belong to the effects that proc
    function p:UpdateFields()
        local e = EFFECT[self.effect]
        local proc = e and e.proc
        if proc then self.chance:Show(); self.cooldown:Show(); self.chanceLabel:Show(); self.cooldownLabel:Show()
        else self.chance:Hide(); self.cooldown:Hide(); self.chanceLabel:Hide(); self.cooldownLabel:Hide() end
    end
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
        local chance = tonumber(p.chance:GetText()) or 0
        local cooldown = floor(tonumber(p.cooldown:GetText()) or 0)
        local tier = max(1, min(3, floor(tonumber(p.tier:GetText()) or 1)))
        if not (EFFECT[p.effect] and EFFECT[p.effect].proc) then chance, cooldown = 0, 0 end
        if p.id then
            local n = S.byId[p.id]
            AIO.Handle("GuildTalents", "Set", p.id, p.effect, amount, maxPoints, chance, cooldown, tier)
            if n and n.start ~= start then AIO.Handle("GuildTalents", "Start", p.id, start) end
            if n and (n.icon or "") ~= icon then AIO.Handle("GuildTalents", "Icon", p.id, icon) end
        else
            AIO.Handle("GuildTalents", "Add", p.x, p.y, p.effect, amount, maxPoints, chance, cooldown, tier)
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
        self.chance:SetText(n and Num(n.chance or 0) or (self.chance:GetText() ~= "" and self.chance:GetText() or "10"))
        self.cooldown:SetText(n and tostring(n.cooldown or 0) or (self.cooldown:GetText() ~= "" and self.cooldown:GetText() or "30"))
        self.tier:SetText(n and tostring(n.tier or 1) or (self.tier:GetText() ~= "" and self.tier:GetText() or "1"))
        self:UpdateFields()
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
        if button ~= "LeftButton" or n.effect == ORIGIN or not S.state.canSpend or Points(n.id) >= n.max
            or not Reachable(n) or Left() <= 0 or S.state.spent < TierNeed(n) then
            return
        end
        local dialog = StaticPopup_Show("GUILD_TALENTS_BUY", fmt(L.confirmBuy, EffectName(n.effect)))
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
        b.rank:SetText(n.effect == ORIGIN and "" or fmt("%d/%d", p, n.max))
        Shown(b.rankBg, n.effect ~= ORIGIN)
        local reach = Reachable(n) and S.state.spent >= TierNeed(n)
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

-- The places of the pieces: in a window of our own, and in ForeverUI's guild window (under its
-- title bar, inside its border).
WINDOW = { infoX = 24, infoY = -34, buttonX = 20, buttonY = 18, viewX = 18, viewTop = -56, viewBottom = 48 }
EMBED = { infoX = 12, infoY = -10, buttonX = 6, buttonY = 6, viewX = 6, viewTop = -6, viewBottom = 34 }

local function Center()
    UI.panX = (UI.view:GetWidth() / UI.zoom - (UI.canvasW or 0)) / 2
    UI.panY = (UI.view:GetHeight() / UI.zoom - (UI.canvasH or 0)) / 2
    ClampPan()
end

-- parent: the view frame of ForeverUI's guild window (the talents are a page of it), else nil (a
-- window of their own).
local function Build(parent)
    UI = CreateFrame("Frame", "GuildTalentsFrame", parent or UIParent)
    UI.embedded = parent ~= nil
    UI.buttons, UI.lines = {}, {}
    UI.zoom, UI.panX, UI.panY = 1, 0, 0
    if UI.embedded then
        -- the right pane: from the guild column to the window's border, as ForeverUI's Guild Info
        -- view (GuildInfo.lua: column TOPRIGHT (4, -40), frame BOTTOMRIGHT (-12, 30))
        local G = _G.ForeverUI.Guild
        UI:SetPoint("TOPLEFT", G.column or parent, G.column and "TOPRIGHT" or "TOPLEFT", 4, -40)
        UI:SetPoint("BOTTOMRIGHT", parent, "BOTTOMRIGHT", -12, 4)
        if _G.ForeverUI.CreateInset then
            UI.inset = _G.ForeverUI.CreateInset(UI, "GuildTalentsInset")
            UI.inset:SetPoint("TOPLEFT", UI, "TOPLEFT", 0, 0)
            UI.inset:SetPoint("BOTTOMRIGHT", UI, "BOTTOMRIGHT", 0, 30)
        end
        -- the window's title says the guild; ours stays for the window of our own
        UI.titleText = UI:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        UI.titleText:Hide()
    else
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

        local header = UI:CreateTexture(nil, "ARTWORK")
        header:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
        header:SetSize(360, 64)
        header:SetPoint("TOP", 0, 12)
        UI.titleText = UI:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        UI.titleText:SetPoint("TOP", header, "TOP", 0, -14)

        local close = CreateFrame("Button", nil, UI, "UIPanelCloseButton")
        close:SetPoint("TOPRIGHT", -6, -6)
    end
    local M = UI.embedded and EMBED or WINDOW

    -- in the page, the points sit inside the tree's frame, in its top left corner: on a frame
    -- above the view, which covers the frame
    local infoParent = UI
    if UI.embedded then
        infoParent = CreateFrame("Frame", nil, UI)
        infoParent:SetAllPoints(UI)
        infoParent:SetFrameLevel(UI:GetFrameLevel() + 30)
    end
    UI.info = infoParent:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    UI.info:SetPoint("TOPLEFT", M.infoX, M.infoY)

    UI.reset = CreateFrame("Button", nil, UI, "UIPanelButtonTemplate")
    UI.reset:SetSize(200, 22)
    UI.reset:SetPoint("BOTTOMLEFT", M.buttonX, M.buttonY)
    UI.reset:SetScript("OnClick", function()
        StaticPopup_Show("GUILD_TALENTS_RESET", fmt(L.confirmReset, S.state.price))
    end)

    UI.editButton = CreateFrame("Button", nil, UI, "UIPanelButtonTemplate")
    UI.editButton:SetSize(110, 22)
    UI.editButton:SetPoint("BOTTOMRIGHT", -M.buttonX, M.buttonY)
    UI.editButton:SetScript("OnClick", function()
        S.edit = not S.edit
        S.linkFrom = nil
        if not S.edit then UI.editor:Hide() end
        Refresh()
        if UI.refit then UI.refit() else Center() end
    end)

    UI.help = UI:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    UI.help:SetPoint("BOTTOM", 0, M.buttonY + 6)

    -- the view: clips the canvas
    UI.view = CreateFrame("ScrollFrame", "GuildTalentsView", UI)
    UI.view:SetPoint("TOPLEFT", M.viewX, M.viewTop)
    UI.view:SetPoint("BOTTOMRIGHT", -M.viewX, M.viewBottom)
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

-- FOREVERUI: the talents are a view of its guild window, behind a side tab (camelot's Guild
-- Benefits place), and the window grows to hold the tree while that view is shown -- as its own
-- tabs size it -- then takes its size back.
local FUI = _G.ForeverUI and _G.ForeverUI.Guild and _G.ForeverUI.Guild.registerView and _G.ForeverUI
local VIEW = "guildtalents"
-- The guild page's own size (camelot's Communities, GuildFrame.lua), never less.
local PAGE_W, PAGE_H = 814, 426

local function Window()
    return FUI and FUI.Social and FUI.Social.frame
end

-- The size the view wants: the tree, plus what surrounds the pane (the guild column on the left,
-- the title bar, the buttons), within the screen.
local function Wanted()
    local win = Window()
    if not (win and UI and S.tree) then return end
    Layout()
    local M = EMBED
    local left = (UI:GetLeft() and win:GetLeft()) and (UI:GetLeft() - win:GetLeft()) or 180
    local top = (UI:GetTop() and win:GetTop()) and (win:GetTop() - UI:GetTop()) or 40
    local w = left + (UI.canvasW or 0) + 2 * M.viewX + 12 + 16
    local h = top + (UI.canvasH or 0) - M.viewTop + M.viewBottom + 4 + 8
    local maxW, maxH = UIParent:GetWidth() * 0.92, UIParent:GetHeight() * 0.88
    return max(PAGE_W, min(w, maxW)), max(PAGE_H, min(h, maxH))
end

-- While the view is shown, the window keeps the view's size: ForeverUI sets the guild page's
-- size again whenever the page shows (a reopened window comes back on this view at the page's
-- size), so the view puts its own back on the next frame, then centres the tree in the pane.
local function Hold(self)
    local win = Window()
    if not win or not self.wantW or InCombatLockdown() then return end
    if math.abs(win:GetWidth() - self.wantW) > 1 or math.abs(win:GetHeight() - self.wantH) > 1 then
        win:SetWidth(self.wantW)
        win:SetHeight(self.wantH)
        self.recenter = 2
        return
    end
    if self.recenter then
        self.recenter = self.recenter - 1
        if self.recenter <= 0 then
            self.recenter = nil
            Center()
        end
    end
end

local function FitWindow()
    if not UI then return end
    local w, h = Wanted()
    if not w then return end
    UI.wantW, UI.wantH = w, h
    UI.recenter = 2
    UI:SetScript("OnUpdate", Hold)
end

local function Refit()
    if UI and UI.embedded and UI:IsVisible() then FitWindow() end
end

local function RestoreWindow()
    local win = Window()
    if UI then
        UI.wantW, UI.wantH = nil, nil
        UI:SetScript("OnUpdate", nil)
    end
    if win and not InCombatLockdown() then
        win:SetWidth(PAGE_W)
        win:SetHeight(PAGE_H)
    end
end

local function Show()
    if FUI then
        -- the Social window on its guild page, our view in it
        if not (FriendsFrame and FriendsFrame:IsShown()) and ToggleFriendsFrame then ToggleFriendsFrame(3) end
        if FriendsFrame and FriendsFrame.selectedTab ~= 3 and FriendsFrameTab3 then FriendsFrameTab3:Click() end
        FUI.Guild.setView(VIEW)
        return
    end
    if not UI then Build() end
    Refresh()
    if not UI:IsShown() then
        UI:Show()
        Center()
    else
        ClampPan()
    end
end

if FUI then
    FUI.Guild.registerView(VIEW, {
        order = 30,
        icon = FUI.Guild.ICONS and FUI.Guild.ICONS.benefits or "Interface\\Icons\\Ability_Marksmanship",
        tooltip = function() return L.title end,
        build = function(frame)
            Build(frame)
            UI.refit = Refit
            UI:Show()
        end,
        show = function()
            Refresh()
            FitWindow()
            AIO.Handle("GuildTalents", "Refresh")
        end,
        hide = function()
            if UI and UI.editor then UI.editor:Hide() end
            RestoreWindow()
        end,
        update = function()
            if UI then Refresh() end
        end,
    })
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
    if UI and UI:IsVisible() then
        Refresh()
        if UI.embedded then FitWindow() end
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
-- THE WAYS IN: /gtalent, and the guild window: a side tab on ForeverUI's guild page (the
-- place of camelot's Guild Benefits tab), a button on the stock one.
-- ---------------------------------------------------------------------------

SLASH_GUILDTALENTS1 = "/gtalent"
SlashCmdList["GUILDTALENTS"] = function()
    if UI and UI:IsVisible() and not UI.embedded then UI:Hide() else AIO.Handle("GuildTalents", "Open") end
end

local function Open()
    AIO.Handle("GuildTalents", "Open")
end

local guildButtons = {}
local function AttachButton(parent, x, y)
    if not parent or guildButtons[parent] then return end
    local b = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    b:SetSize(130, 22)
    b:SetPoint("TOPRIGHT", parent, "TOPRIGHT", x, y)
    b:SetText(L.title)
    b:SetFrameLevel(parent:GetFrameLevel() + 10)
    b:SetScript("OnClick", function() Open() end)
    guildButtons[parent] = b
end

if not FUI then
    local function AttachAll()
        AttachButton(_G.GuildFrame, -44, -44)
    end
    if FriendsFrame then
        FriendsFrame:HookScript("OnShow", AttachAll)
    end
    if FriendsFrame_Update then hooksecurefunc("FriendsFrame_Update", AttachAll) end
    AttachAll()
end

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
        local t = totals[effect]
        tip:AddLine(EffectText(effect, t.amount, t.chance, t.cooldown), 0.2, 1, 0.2, true)
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
