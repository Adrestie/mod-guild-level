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
    The Customization mode of the guild house, client side.

    In the house, a member allowed to decorate enters the mode (the button at
    the top of the screen, or /gdeco): the usual interface gives way to the
    guild's objects (left) and the edit bar (bottom). An object of the stock is
    picked, follows the cursor on the hall's floors and walls (the mouse wheel
    turns it), and is set down with a click; its gizmo then moves, turns and
    sizes it. Valider, or a click away from it, places it; Annuler gives the
    edit up. The objects already placed are outlined (the extension draws
    them): a click takes one in hand, a first right click marks one in red and
    a second puts it back in the stock.

    The preview, the gizmo and the cursor's ray are the client extension's
    (WarcraftXL, Extensions/wxl-guildhouse: the GuildHouse_* functions); the
    server is told only when an edit starts and when it ends (GuildHouse.lua,
    the module's .ghouse commands).
------------------------------------------------------------------------------]]

if AIO.AddAddon() then
    return
end

local Handlers = AIO.AddHandlers("GuildHouse", {})

local fmt = string.format
local floor, min, max, sin, cos = math.floor, math.min, math.max, math.sin, math.cos

local L = {
    enUS = {
        button = "Customization",
        title = "Guild objects", validate = "Place", cancel = "Cancel", store = "To the stock",
        move = "Move (1)", rotate = "Turn (2)", scale = "Size (3)", world = "World", localAxes = "Local", reset = "Reset turn",
        ground = "Set on the floor", exit = "Leave (Esc)",
        noext = "The Customization mode needs WarcraftXL and its guild house editor (Extensions\\wxl-guildhouse).",
        cannot = "You cannot decorate the house: Paragon level %d, guild rank %d or above, in the house.",
        aim = "Point at the hall's floor to place the object.",
        carry = "Left click: set it down. Wheel: turn it. Right click: give up.",
        hint = "Click an outlined object to take it in hand.",
        categories = { Furnishings = "Furniture", Lighting = "Lights", Accents = "Decoration", Nature = "Plants",
            Structural = "Structure", Functional = "Utility", Miscellaneous = "Misc" },
        collection = "Collection", collectionTitle = "Guild collection",
        progress = "%d / %d objects owned by your guild", noguild = "You are not in a guild: nothing is owned yet.",
        all = "All", owned = "Owned", missing = "Missing", search = "Search",
        loading = "Looking for where it is found...", count = "Owned: %d (placed: %d)", none = "Not owned yet",
        quest = "Quest:", level = "level %d",
        questRule = "Handed in by a member of a guild, the quest gives its object.",
        boss = "Dropped by:", themed = "Dungeons and raids:", anywhere = "Dungeons and raids: any boss.",
        dropRule = "A boss drops objects for a Paragon %d guild there in numbers: %d members in a dungeon, %d in a 10-player raid, %d in a 25-player raid.",
        tradable = "The object can be traded and sold; used, it joins your guild's stock.",
    },
    frFR = {
        button = "Personnalisation",
        title = "Objets de guilde", validate = "Valider", cancel = "Annuler", store = "Ranger",
        move = "Déplacer (1)", rotate = "Pivoter (2)", scale = "Taille (3)", world = "Monde", localAxes = "Local", reset = "Rotation d'origine",
        ground = "Poser au sol", exit = "Quitter (Échap)",
        noext = "Le mode Personnalisation demande WarcraftXL et son éditeur de maison de guilde (Extensions\\wxl-guildhouse).",
        cannot = "Vous ne pouvez pas décorer la maison : Parangon %d, rang de guilde %d ou mieux, dans la maison.",
        aim = "Visez le sol du hall pour poser l'objet.",
        carry = "Clic gauche : poser. Molette : tourner. Clic droit : abandonner.",
        hint = "Cliquez sur un objet entouré pour le prendre en main.",
        categories = { Furnishings = "Mobilier", Lighting = "Lumières", Accents = "Décoration", Nature = "Plantes",
            Structural = "Structure", Functional = "Utilitaire", Miscellaneous = "Divers" },
        collection = "Collection", collectionTitle = "Collection de la guilde",
        progress = "%d / %d objets possédés par votre guilde", noguild = "Vous n'êtes pas dans une guilde : rien n'est encore possédé.",
        all = "Tous", owned = "Possédés", missing = "Manquants", search = "Rechercher",
        loading = "Recherche de sa provenance...", count = "Possédés : %d (placés : %d)", none = "Pas encore possédé",
        quest = "Quête :", level = "niveau %d",
        questRule = "Rendue par un membre d'une guilde, la quête donne son objet.",
        boss = "Butin de :", themed = "Donjons et raids :", anywhere = "Donjons et raids : n'importe quel boss.",
        dropRule = "Un boss donne des objets à une guilde de Parangon %d présente en nombre : %d membres en donjon, %d en raid à 10, %d en raid à 25.",
        tradable = "L'objet s'échange et se vend ; utilisé, il rejoint le stock de votre guilde.",
    },
    deDE = {
        button = "Gestaltung",
        title = "Gildengegenstände", validate = "Platzieren", cancel = "Abbrechen", store = "Ins Lager",
        move = "Bewegen (1)", rotate = "Drehen (2)", scale = "Größe (3)", world = "Welt", localAxes = "Lokal", reset = "Drehung zurück",
        ground = "Auf den Boden", exit = "Verlassen (Esc)",
        noext = "Der Gestaltungsmodus benötigt WarcraftXL und seinen Gildenhaus-Editor (Extensions\\wxl-guildhouse).",
        cannot = "Ihr könnt das Haus nicht gestalten: Paragonstufe %d, Gildenrang %d oder höher, im Haus.",
        aim = "Zielt auf den Boden der Halle, um den Gegenstand zu platzieren.",
        carry = "Linksklick: abstellen. Mausrad: drehen. Rechtsklick: abbrechen.",
        hint = "Klickt auf einen umrandeten Gegenstand, um ihn aufzunehmen.",
        categories = { Furnishings = "Möbel", Lighting = "Licht", Accents = "Deko", Nature = "Pflanzen",
            Structural = "Bauteile", Functional = "Nützliches", Miscellaneous = "Sonstiges" },
        collection = "Sammlung", collectionTitle = "Gildensammlung",
        progress = "%d / %d Gegenstände im Besitz Eurer Gilde", noguild = "Ihr seid in keiner Gilde: noch nichts im Besitz.",
        all = "Alle", owned = "Im Besitz", missing = "Fehlend", search = "Suchen",
        loading = "Fundorte werden gesucht...", count = "Im Besitz: %d (platziert: %d)", none = "Noch nicht im Besitz",
        quest = "Quest:", level = "Stufe %d",
        questRule = "Gibt ein Gildenmitglied die Quest ab, bringt sie ihren Gegenstand ein.",
        boss = "Beute von:", themed = "Dungeons und Schlachtzüge:", anywhere = "Dungeons und Schlachtzüge: jeder Boss.",
        dropRule = "Ein Boss lässt Gegenstände für eine Gilde der Paragonstufe %d fallen, die zahlreich da ist: %d Mitglieder im Dungeon, %d im 10er-, %d im 25er-Schlachtzug.",
        tradable = "Der Gegenstand ist handelbar und verkäuflich; benutzt, kommt er ins Lager Eurer Gilde.",
    },
    esES = {
        button = "Personalización",
        title = "Objetos de hermandad", validate = "Colocar", cancel = "Cancelar", store = "Al almacén",
        move = "Mover (1)", rotate = "Girar (2)", scale = "Tamaño (3)", world = "Mundo", localAxes = "Local", reset = "Giro original",
        ground = "Al suelo", exit = "Salir (Esc)",
        noext = "El modo Personalización necesita WarcraftXL y su editor de casa de hermandad (Extensions\\wxl-guildhouse).",
        cannot = "No puedes decorar la casa: nivel de Paragón %d, rango de hermandad %d o superior, dentro de la casa.",
        aim = "Apunta al suelo del salón para colocar el objeto.",
        carry = "Clic izquierdo: colocarlo. Rueda: girarlo. Clic derecho: abandonar.",
        hint = "Haz clic en un objeto contorneado para tomarlo.",
        categories = { Furnishings = "Mobiliario", Lighting = "Luces", Accents = "Decoración", Nature = "Plantas",
            Structural = "Estructura", Functional = "Utilidad", Miscellaneous = "Varios" },
        collection = "Colección", collectionTitle = "Colección de la hermandad",
        progress = "%d / %d objetos en posesión de tu hermandad", noguild = "No estás en una hermandad: aún no tienes nada.",
        all = "Todos", owned = "Obtenidos", missing = "Faltan", search = "Buscar",
        loading = "Buscando dónde conseguirlo...", count = "Obtenidos: %d (colocados: %d)", none = "Aún no obtenido",
        quest = "Misión:", level = "nivel %d",
        questRule = "Entregada por un miembro de una hermandad, la misión otorga su objeto.",
        boss = "Botín de:", themed = "Mazmorras y bandas:", anywhere = "Mazmorras y bandas: cualquier jefe.",
        dropRule = "Un jefe suelta objetos para una hermandad de Paragón %d presente en número: %d miembros en mazmorra, %d en banda de 10, %d en banda de 25.",
        tradable = "El objeto se puede comerciar y vender; al usarlo, se suma al almacén de tu hermandad.",
    },
    ruRU = {
        button = "Оформление",
        title = "Предметы гильдии", validate = "Разместить", cancel = "Отмена", store = "На склад",
        move = "Двигать (1)", rotate = "Вращать (2)", scale = "Размер (3)", world = "Мир", localAxes = "Локально", reset = "Сброс поворота",
        ground = "На пол", exit = "Выйти (Esc)",
        noext = "Для режима оформления нужен WarcraftXL и его редактор дома гильдии (Extensions\\wxl-guildhouse).",
        cannot = "Вы не можете обустраивать дом: нужен %d-й уровень Парагона, звание гильдии %d или выше и нахождение в доме.",
        aim = "Наведите курсор на пол зала, чтобы разместить предмет.",
        carry = "Левый клик: поставить. Колесо: повернуть. Правый клик: отменить.",
        hint = "Щелкните по выделенному предмету, чтобы взять его.",
        categories = { Furnishings = "Мебель", Lighting = "Свет", Accents = "Декор", Nature = "Растения",
            Structural = "Постройки", Functional = "Полезное", Miscellaneous = "Разное" },
        collection = "Коллекция", collectionTitle = "Коллекция гильдии",
        progress = "Предметов у гильдии: %d из %d", noguild = "Вы не состоите в гильдии: пока ничего нет.",
        all = "Все", owned = "Есть", missing = "Нет", search = "Поиск",
        loading = "Поиск источников...", count = "Есть: %d (размещено: %d)", none = "Пока нет",
        quest = "Задание:", level = "уровень %d",
        questRule = "Когда член гильдии сдает задание, оно приносит свой предмет.",
        boss = "Добыча с:", themed = "Подземелья и рейды:", anywhere = "Подземелья и рейды: любой босс.",
        dropRule = "Босс оставляет предметы гильдии %d-го уровня Парагона, если её членов достаточно: %d в подземелье, %d в рейде на 10, %d в рейде на 25.",
        tradable = "Предмет можно передавать и продавать; при использовании он попадает на склад гильдии.",
    },
}
L.esMX = L.esES
L = L[GetLocale()] or L.enUS

-- The families, dungeons and zones in the player's language (GuildHouse_Locale.lua, an addon of
-- its own: looked up when needed, whichever of the two the client ran first).
local function Loc()
    local all = _G.GuildHouseLocale
    return all and (all[GetLocale()] or all.enUS) or {}
end

-- The objects' names in the player's language: the server's (their items'), English till then.
local names = {}
local function DecorName(decor)
    local d = GuildHouseDecor and GuildHouseDecor[decor]
    return names[decor] or (d and d[4]) or ""
end

local function KindName(kind)
    local kinds = Loc().kinds
    return kinds and kinds[kind] or kind
end

local CATEGORIES = { "Furnishings", "Lighting", "Accents", "Nature", "Structural", "Functional", "Miscellaneous" }
local ROWS = 16
local ROW_HEIGHT = 22

local S = {
    state = nil,        -- the server's: can, owned, placed...
    active = false,     -- in the mode
    category = "Furnishings",
    edit = nil,         -- { id (0: new), decor, carry = true while following the cursor }
    yaw = 0,            -- radians, the carried object's turn
}

local function Has(fn)
    return type(_G[fn]) == "function"
end

local function Say(text)
    DEFAULT_CHAT_FRAME:AddMessage("|cff40ff40" .. text .. "|r")
end

-- What is left to place of an object: owned, less placed (the one in hand counts as free).
local function Available(decor)
    local s = S.state
    if not s then return 0 end
    local n = s.owned[decor] or 0
    for _, p in ipairs(s.placed) do
        if p.decor == decor and not (S.edit and S.edit.id == p.id) then
            n = n - 1
        end
    end
    return n
end

local function Kinds()
    local list = {}
    for decor, d in pairs(GuildHouseDecor or {}) do
        if d[1] == S.category then
            list[#list + 1] = decor
        end
    end
    table.sort(list, function(a, b)
        local da, db = GuildHouseDecor[a], GuildHouseDecor[b]
        if da[2] ~= db[2] then return KindName(da[2]) < KindName(db[2]) end
        return DecorName(a) < DecorName(b)
    end)
    return list
end

-- A turn around the vertical axis, as a quaternion (x, y, z, w).
local function Yaw(a)
    return 0, 0, sin(a / 2), cos(a / 2)
end

------------------------------------------------------------------------------
-- The frames
------------------------------------------------------------------------------

local UI = CreateFrame("Frame", "GuildHouseUI", nil)
UI:SetAllPoints(WorldFrame)
UI:SetFrameStrata("HIGH")
UI:Hide()

-- ForeverUI's look when it is installed (its window, buttons and scroll bar), else the game's.
local FUI = _G.ForeverUI and _G.ForeverUI.Templates

-- A panel's frame, and its title if any.
local function Backdrop(f, title)
    -- without a title, ForeverUI's dialog frame (its window has a title band)
    if FUI and not title and FUI.DialogFrame then
        FUI.DialogFrame(f)
        return
    end
    if FUI and FUI.Window then
        local parts = FUI.Window(f, title)
        return parts and parts.title
    end
    f:SetBackdrop({
        bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
        tile = true, tileSize = 32, edgeSize = 24,
        insets = { left = 6, right = 6, top = 6, bottom = 6 },
    })
    if title then
        local t = f:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
        t:SetPoint("TOP", 0, -16)
        t:SetText(title)
        return t
    end
end

local function Button(parent, text, width)
    local b = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    b:SetWidth(width or 110)
    b:SetHeight(24)
    b:SetText(text)
    if FUI and FUI.PanelButton then FUI.PanelButton(b) end
    return b
end

-- The guild's objects, on the left.
local Panel = CreateFrame("Frame", nil, UI)
Panel:SetWidth(300)
Panel:SetHeight(ROWS * ROW_HEIGHT + 162)
Panel:SetPoint("LEFT", UI, "LEFT", 20, 40)
Panel:EnableMouse(true)
Panel.title = Backdrop(Panel, L.title)

Panel.tabs = {}
for i, cat in ipairs(CATEGORIES) do
    -- rows of three
    local t = Button(Panel, L.categories[cat], 88)
    t:SetPoint("TOPLEFT", 14 + ((i - 1) % 3) * 91, -36 - math.floor((i - 1) / 3) * 26)
    t:SetScript("OnClick", function()
        S.category = cat
        UI:Refresh()
    end)
    Panel.tabs[cat] = t
end

Panel.scroll = CreateFrame("ScrollFrame", "GuildHouseUIScroll", Panel, "FauxScrollFrameTemplate")
Panel.scroll:SetPoint("TOPLEFT", 14, -124)
Panel.scroll:SetPoint("BOTTOMRIGHT", -34, 16)
Panel.scroll:SetScript("OnVerticalScroll", function(self, offset)
    FauxScrollFrame_OnVerticalScroll(self, offset, ROW_HEIGHT, function() UI:Refresh() end)
end)
if FUI then
    if FUI.Inset then FUI.Inset(Panel, Panel.scroll) end
    if FUI.Bar then FUI.Bar(_G.GuildHouseUIScrollScrollBar) end
end

-- The object under the cursor in the list, its camera orbiting it, above the list.
local Preview = CreateFrame("Frame", nil, UI)
Preview:SetWidth(300)
Preview:SetHeight(260)
Preview:SetPoint("BOTTOM", Panel, "TOP", 0, 6)
Backdrop(Preview)
Preview.model = CreateFrame("Model", nil, Preview)
Preview.model:SetPoint("TOPLEFT", 10, -26)
Preview.model:SetPoint("BOTTOMRIGHT", -10, 10)
Preview.name = Preview:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
Preview.name:SetPoint("BOTTOM", 0, 14)
Preview:Hide()
Preview:SetScript("OnUpdate", function(self, elapsed)
    -- the object's presentation camera (tools/decor/preview_camera.py): set again every frame, a
    -- model taking its first camera at each load
    if self.camera then self.model:SetCamera(self.camera) end
end)

function Preview:ShowDecor(decor)
    local d = GuildHouseDecor[decor]
    if not d then return self:Hide() end
    self.model:ClearModel()
    self.model:SetModel((d[3]:gsub("%.m2$", ".mdx")))
    -- its own camera frames it whole, whichever way it turns
    self.camera = d[12] or 0
    self.model:SetModelScale(1)
    self.model:SetPosition(0, 0, 0)
    self.model:SetCamera(self.camera)
    -- the camera orbits the object by itself (its own loop); lit from the front and above
    self.model:SetFacing(0)
    self.model:SetLight(1, 0, -0.5, -0.6, -0.6, 0.8, 1, 1, 1, 0.8, 1, 0.95, 0.85)
    self.name:SetText(DecorName(decor))
    self:Show()
end

Panel.rows = {}
for i = 1, ROWS do
    local r = CreateFrame("Button", nil, Panel)
    r:SetHeight(ROW_HEIGHT)
    r:SetPoint("TOPLEFT", 16, -124 - (i - 1) * ROW_HEIGHT)
    r:SetPoint("RIGHT", Panel, "RIGHT", -36, 0)
    r:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
    r.name = r:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    r.name:SetPoint("LEFT", 4, 0)
    r.name:SetPoint("RIGHT", -40, 0)
    r.name:SetJustifyH("LEFT")
    r.count = r:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
    r.count:SetPoint("RIGHT", -4, 0)
    r:SetScript("OnClick", function(self)
        if self.decor and Available(self.decor) > 0 then
            UI:Take(self.decor)
        end
    end)
    r:SetScript("OnEnter", function(self)
        if not self.decor then return end
        Preview:ShowDecor(self.decor)
        local d = GuildHouseDecor[self.decor]
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:AddLine(DecorName(self.decor))
        GameTooltip:AddLine(KindName(d[2]), 0.7, 0.7, 0.7)
        GameTooltip:AddLine(fmt("%d / %d", Available(self.decor), (S.state and S.state.owned[self.decor]) or 0), 1, 0.82, 0)
        GameTooltip:Show()
    end)
    r:SetScript("OnLeave", function()
        GameTooltip:Hide()
        Preview:Hide()
    end)
    Panel.rows[i] = r
end

-- The edit bar, at the bottom.
local Bar = CreateFrame("Frame", nil, UI)
Bar:SetHeight(76)
Bar:SetWidth(720)
Bar:SetPoint("BOTTOM", UI, "BOTTOM", 0, 30)
Bar:EnableMouse(true)
Backdrop(Bar)
Bar.text = Bar:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
Bar.text:SetPoint("TOP", 0, -12)

local function Mode(mode)
    if Has("GuildHouse_SetMode") then GuildHouse_SetMode(mode) end
    S.mode = mode
    UI:Refresh()
end

Bar.buttons = {}
local barButtons = {
    { "move", L.move, function() Mode("move") end },
    { "rotate", L.rotate, function() Mode("rotate") end },
    { "scale", L.scale, function() Mode("scale") end },
    { "ground", L.ground, function() UI:Ground() end },
    { "store", L.store, function() UI:Store() end },
    { "validate", L.validate, function() UI:Validate() end },
    { "cancel", L.cancel, function() UI:Cancel() end },
}
for i, b in ipairs(barButtons) do
    local btn = Button(Bar, b[2], 96)
    btn:SetPoint("BOTTOMLEFT", 14 + (i - 1) * 99, 12)
    btn:SetScript("OnClick", b[3])
    Bar.buttons[b[1]] = btn
end

-- While turning: about the world's axes or the object's own, and back to the rotation it had.
local Turn = CreateFrame("Frame", nil, Bar)
Turn:SetWidth(340)
Turn:SetHeight(30)
Turn:SetPoint("BOTTOM", Bar, "TOP", 0, 2)
Turn.world = Button(Turn, L.world, 100)
Turn.world:SetPoint("LEFT", 0, 0)
Turn.world:SetScript("OnClick", function() S.localAxes = false; UI:Refresh() end)
Turn.localAxes = Button(Turn, L.localAxes, 100)
Turn.localAxes:SetPoint("LEFT", Turn.world, "RIGHT", 4, 0)
Turn.localAxes:SetScript("OnClick", function() S.localAxes = true; UI:Refresh() end)
Turn.reset = Button(Turn, L.reset, 130)
Turn.reset:SetPoint("LEFT", Turn.localAxes, "RIGHT", 4, 0)
Turn.reset:SetScript("OnClick", function() UI:ResetTurn() end)
Turn:Hide()

local Exit = Button(UI, L.exit, 130)
Exit:SetPoint("TOPRIGHT", UI, "TOPRIGHT", -20, -20)
Exit:SetScript("OnClick", function() UI:Leave() end)

-- While an object follows the cursor: the clicks on the world are the mode's.
local Catcher = CreateFrame("Button", nil, UI)
Catcher:SetAllPoints(UI)
Catcher:SetFrameLevel(UI:GetFrameLevel() + 1)
Catcher:RegisterForClicks("LeftButtonUp", "RightButtonUp")
Catcher:EnableMouseWheel(true)
Catcher:Hide()
Catcher:SetScript("OnClick", function(_, button)
    if button == "RightButton" then
        UI:Cancel()
    elseif S.edit and S.edit.started then
        S.edit.carry = false
        Catcher:Hide()
        UI:Refresh()
    end
end)
Catcher:SetScript("OnMouseWheel", function(_, delta)
    S.yaw = S.yaw + delta * math.pi / 12
    if S.edit and S.edit.started and Has("GuildHouse_GetTransform") then
        local x, y, z, _, _, _, _, s = GuildHouse_GetTransform()
        if x then
            local qx, qy, qz, qw = Yaw(S.yaw)
            GuildHouse_SetTransform(x, y, z, qx, qy, qz, qw, s)
        end
    end
end)
Panel:SetFrameLevel(Catcher:GetFrameLevel() + 2)
Bar:SetFrameLevel(Catcher:GetFrameLevel() + 2)
Exit:SetFrameLevel(Catcher:GetFrameLevel() + 2)

-- The placed objects the member can take in hand: the extension outlines them
-- (blue, lighter under the cursor) and tells which one was clicked.
local function Targets()
    if not Has("GuildHouse_ClearTargets") then return end
    GuildHouse_ClearTargets()
    local s = S.state
    for _, p in ipairs(s and s.placed or {}) do
        local d = GuildHouseDecor[p.decor]
        if d and p.map == s.map and not (S.edit and S.edit.id == p.id) then
            GuildHouse_AddTarget(p.id, p.x, p.y, p.z, p.qx, p.qy, p.qz, p.qw, p.scale,
                d[6], d[7], d[8], d[9], d[10], d[11], d[3])
        end
    end
    -- shown in the mode, while nothing is in hand
    GuildHouse_ShowTargets(S.active and not S.edit)
end

------------------------------------------------------------------------------
-- The mode
------------------------------------------------------------------------------

function UI:Refresh()
    Targets()
    if Has("GuildHouse_ShowGizmo") then
        GuildHouse_ShowGizmo(not (S.edit and S.edit.carry))
    end
    local list = Kinds()
    for cat, t in pairs(Panel.tabs) do
        if cat == S.category then t:LockHighlight() else t:UnlockHighlight() end
    end
    FauxScrollFrame_Update(Panel.scroll, #list, ROWS, ROW_HEIGHT)
    local offset = FauxScrollFrame_GetOffset(Panel.scroll)
    for i = 1, ROWS do
        local r = Panel.rows[i]
        local decor = list[i + offset]
        r.decor = decor
        if decor then
            local n = Available(decor)
            r.name:SetText(DecorName(decor))
            r.count:SetText(n)
            if n > 0 then
                r.name:SetTextColor(1, 1, 1)
            else
                r.name:SetTextColor(0.5, 0.5, 0.5)
            end
            r:Show()
        else
            r:Hide()
        end
    end

    local editing = S.edit ~= nil
    for key, btn in pairs(Bar.buttons) do
        if key == "camera" then
            btn:Enable()
        elseif editing then
            btn:Enable()
        else
            btn:Disable()
        end
        if btn.LockHighlight then
            if key == S.mode and editing then btn:LockHighlight() else btn:UnlockHighlight() end
        end
    end
    if Has("GuildHouse_SetLocal") then GuildHouse_SetLocal(S.localAxes == true) end
    if editing and S.mode == "rotate" and not S.edit.carry then
        Turn:Show()
        if S.localAxes then
            Turn.localAxes:LockHighlight(); Turn.world:UnlockHighlight()
        else
            Turn.world:LockHighlight(); Turn.localAxes:UnlockHighlight()
        end
    else
        Turn:Hide()
    end
    if S.edit and S.edit.carry then
        Bar.text:SetText(L.carry)
    elseif S.edit then
        Bar.text:SetText(DecorName(S.edit.decor))
    else
        Bar.text:SetText(L.hint)
    end
end

function UI:Enter()
    if not Has("GuildHouse_BeginEdit") then
        Say(L.noext)
        return
    end
    local s = S.state
    if not (s and s.can) then
        Say(fmt(L.cannot, s and s.paragonNeeded or 9, s and s.rankAllowed or 1))
        return
    end
    S.active = true
    S.mode = "move"
    UIParent:Hide()
    UI:SetScale(UIParent:GetScale())
    UI:Show()
    SetOverrideBindingClick(UI, true, "ESCAPE", "GuildHouseUIEscape")
    SetOverrideBinding(UI, true, "1", "CLICK GuildHouseUIMode1:LeftButton")
    SetOverrideBinding(UI, true, "2", "CLICK GuildHouseUIMode2:LeftButton")
    SetOverrideBinding(UI, true, "3", "CLICK GuildHouseUIMode3:LeftButton")
    UI:Refresh()
end

function UI:Leave()
    if S.edit then
        UI:Cancel()
    end
    ClearOverrideBindings(UI)
    if Has("GuildHouse_FreeCamera") then GuildHouse_FreeCamera(false) end
    S.active = false
    if Has("GuildHouse_ShowTargets") then GuildHouse_ShowTargets(false) end
    UI:Hide()
    UIParent:Show()
end

-- A new object from the stock: it follows the cursor once it points at the hall.
function UI:Take(decor)
    if S.edit then UI:Cancel() end
    S.edit = { id = 0, decor = decor, carry = true }
    S.yaw = 0
    Catcher:Show()
    UI:Refresh()
end

-- A placed object, taken in hand: out of the house for the others.
function UI:Edit(p)
    if S.edit then UI:Cancel() end
    local d = GuildHouseDecor[p.decor]
    if not d then return end
    AIO.Handle("GuildHouse", "Begin", p.id)
    if not GuildHouse_BeginEdit(d[3], p.x, p.y, p.z, p.qx, p.qy, p.qz, p.qw, p.scale) then
        AIO.Handle("GuildHouse", "Cancel", p.id)
        return
    end
    S.edit = { id = p.id, decor = p.decor, started = true, carry = false, rot = { p.qx, p.qy, p.qz, p.qw } }
    S.yaw = 2 * math.atan2(p.qz, p.qw)
    GuildHouse_SetMode(S.mode or "move")
    UI:Refresh()
end

-- The object in hand follows the cursor again.
function UI:Carry()
    if not (S.edit and S.edit.started) then return end
    S.edit.carry = true
    Catcher:Show()
    UI:Refresh()
end

function UI:Ground()
    if not (S.edit and S.edit.started) then return end
    local x, y, z = GuildHouse_GetTransform()
    if not x then return end
    local g = GuildHouse_GroundZ(x, y, z + 0.5)
    if g then GuildHouse_SetPosition(x, y, g) end
end

function UI:Validate()
    if not (S.edit and S.edit.started) then return end
    local x, y, z, qx, qy, qz, qw, s = GuildHouse_GetTransform()
    if not x then return end
    AIO.Handle("GuildHouse", "Place", S.edit.id, S.edit.decor, x, y, z, qx, qy, qz, qw, s)
    GuildHouse_EndEdit()
    S.edit = nil
    Catcher:Hide()
    UI:Refresh()
end

-- The object in hand turned back as it was when taken (or set down), where it stands now.
function UI:ResetTurn()
    if not (S.edit and S.edit.started and S.edit.rot) then return end
    local x, y, z, _, _, _, _, s = GuildHouse_GetTransform()
    if not x then return end
    local r = S.edit.rot
    GuildHouse_SetTransform(x, y, z, r[1], r[2], r[3], r[4], s)
end

-- The object in hand goes back to the stock (a new one is only given up).
function UI:Store()
    if not S.edit then return end
    local id = S.edit.id
    if Has("GuildHouse_EndEdit") then GuildHouse_EndEdit() end
    S.edit = nil
    Catcher:Hide()
    if id ~= 0 then
        AIO.Handle("GuildHouse", "Remove", id)
    end
    UI:Refresh()
end

function UI:Cancel()
    if not S.edit then return end
    if S.edit.id ~= 0 then
        AIO.Handle("GuildHouse", "Cancel", S.edit.id)
    end
    if Has("GuildHouse_EndEdit") then GuildHouse_EndEdit() end
    S.edit = nil
    Catcher:Hide()
    UI:Refresh()
end

-- The keys of the mode.
local escape = CreateFrame("Button", "GuildHouseUIEscape", UI)
escape:SetScript("OnClick", function()
    if S.edit then UI:Cancel() else UI:Leave() end
end)
for i, mode in ipairs({ "move", "rotate", "scale" }) do
    local b = CreateFrame("Button", "GuildHouseUIMode" .. i, UI)
    b:SetScript("OnClick", function() if S.edit then Mode(mode) end end)
end

UI:SetScript("OnUpdate", function()
    -- the object in hand follows the cursor on the hall's floors and walls
    local e = S.edit
    if e and e.carry then
        local x, y, z = GuildHouse_CursorWorld()
        if x then
            if not e.started then
                local d = GuildHouseDecor[e.decor]
                local qx, qy, qz, qw = Yaw(S.yaw)
                if d and GuildHouse_BeginEdit(d[3], x, y, z, qx, qy, qz, qw, d[5] or 1) then
                    e.started = true
                    e.rot = { qx, qy, qz, qw }
                    GuildHouse_SetMode(S.mode or "move")
                else
                    Say(L.aim)
                    UI:Cancel()
                    return
                end
            else
                GuildHouse_SetPosition(x, y, z)
            end
        end
    end

    -- a click away from the object in hand lets it go where it stands
    if GuildHouse_TakeClickedAway() and S.edit and not S.edit.carry then
        UI:Validate()
    end

    -- a placed object right-clicked twice: back to the stock
    local removed = GuildHouse_TakeRemoved()
    if removed then
        AIO.Handle("GuildHouse", "Remove", removed)
    end

    -- a placed object clicked: taken in hand
    local picked = GuildHouse_TakePicked()
    if picked and not S.edit then
        for _, p in ipairs(S.state and S.state.placed or {}) do
            if p.id == picked then
                UI:Edit(p)
                break
            end
        end
    end
end)

------------------------------------------------------------------------------
-- The Collection: every object, those the guild owns and those it lacks, and
-- where to find them (the server's: Handlers.Sources). Open anywhere: /gcoll,
-- the Customization panel, the guild window.
------------------------------------------------------------------------------

local COLL_ROWS = 14
local COLL_ROW = 26
local ICONS = "Interface\\Icons\\GuildLevel_Decor_"
local NO_ICON = "Interface\\Icons\\INV_Crate_01"

local C = {
    category = nil,     -- nil: every family
    filter = "all",     -- all, owned, missing
    search = "",
    list = {},
    selected = nil,     -- the object clicked (its details stay)
    shown = nil,        -- the object whose details are shown (the one under the cursor first)
    sources = {},       -- decor: the server's answer
    asked = {},         -- decor: asked for, no answer yet
}

-- Above everything, the mode's screen included (the usual interface is hidden in the mode).
local Coll = CreateFrame("Frame", "GuildHouseCollection", nil)
Coll:SetWidth(660)
Coll:SetHeight(COLL_ROWS * COLL_ROW + 166)
Coll:SetPoint("CENTER", UIParent, "CENTER", 0, 20)
Coll:SetFrameStrata("DIALOG")
Coll:SetToplevel(true)
Coll:EnableMouse(true)
Coll:SetMovable(true)
Coll:SetClampedToScreen(true)
Coll:RegisterForDrag("LeftButton")
Coll:SetScript("OnDragStart", Coll.StartMoving)
Coll:SetScript("OnDragStop", Coll.StopMovingOrSizing)
Coll:Hide()
tinsert(UISpecialFrames, "GuildHouseCollection")
Backdrop(Coll, L.collectionTitle)

Coll.close = CreateFrame("Button", nil, Coll, "UIPanelCloseButton")
Coll.close:SetPoint("TOPRIGHT", -4, -4)

Coll.progress = Coll:CreateFontString(nil, "ARTWORK", "GameFontNormal")
Coll.progress:SetPoint("TOPLEFT", 18, -40)

-- the families: every one, then each
Coll.tabs = {}
local families = { false }
for _, cat in ipairs(CATEGORIES) do families[#families + 1] = cat end
for i, cat in ipairs(families) do
    local t = Button(Coll, cat and L.categories[cat] or L.all, 80)
    t:SetPoint("TOPLEFT", 16 + ((i - 1) % 4) * 83, -60 - floor((i - 1) / 4) * 26)
    t:SetScript("OnClick", function()
        C.category = cat or nil
        Coll:Rebuild()
    end)
    Coll.tabs[i] = { button = t, category = cat or nil }
end

Coll.filters = {}
for i, f in ipairs({ "all", "owned", "missing" }) do
    local b = Button(Coll, L[f], 80)
    b:SetPoint("TOPLEFT", 16 + (i - 1) * 83, -116)
    b:SetScript("OnClick", function()
        C.filter = f
        Coll:Rebuild()
    end)
    Coll.filters[f] = b
end

Coll.search = CreateFrame("EditBox", "GuildHouseCollectionSearch", Coll, "InputBoxTemplate")
Coll.search:SetWidth(82)
Coll.search:SetHeight(20)
Coll.search:SetPoint("TOPLEFT", 270, -118)
Coll.search:SetAutoFocus(false)
Coll.search.hint = Coll.search:CreateFontString(nil, "ARTWORK", "GameFontDisableSmall")
Coll.search.hint:SetPoint("LEFT", 2, 0)
Coll.search.hint:SetText(L.search)
Coll.search:SetScript("OnTextChanged", function(self)
    C.search = (self:GetText() or ""):lower()
    if C.search == "" then self.hint:Show() else self.hint:Hide() end
    Coll:Rebuild()
end)
Coll.search:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
Coll.search:SetScript("OnEnterPressed", function(self) self:ClearFocus() end)

Coll.scroll = CreateFrame("ScrollFrame", "GuildHouseCollectionScroll", Coll, "FauxScrollFrameTemplate")
Coll.scroll:SetPoint("TOPLEFT", 16, -146)
Coll.scroll:SetWidth(330)
Coll.scroll:SetHeight(COLL_ROWS * COLL_ROW)
Coll.scroll:SetScript("OnVerticalScroll", function(self, offset)
    FauxScrollFrame_OnVerticalScroll(self, offset, COLL_ROW, function() Coll:Refresh() end)
end)
if FUI then
    if FUI.Inset then FUI.Inset(Coll, Coll.scroll) end
    if FUI.Bar then FUI.Bar(_G.GuildHouseCollectionScrollScrollBar) end
end

-- The details, on the right: the object turning, its name, its family, how many the guild has,
-- where it is found.
local Detail = CreateFrame("Frame", nil, Coll)
Detail:SetPoint("TOPLEFT", 372, -36)
Detail:SetPoint("BOTTOMRIGHT", -16, 16)
Detail.model = CreateFrame("Model", nil, Detail)
Detail.model:SetPoint("TOPLEFT", 0, 0)
Detail.model:SetPoint("TOPRIGHT", 0, 0)
Detail.model:SetHeight(200)
Detail.model:SetScript("OnUpdate", function(self)
    if self.camera then self:SetCamera(self.camera) end
end)
Detail.name = Detail:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
Detail.name:SetPoint("TOPLEFT", Detail.model, "BOTTOMLEFT", 0, -6)
Detail.name:SetPoint("RIGHT", Detail, "RIGHT", 0, 0)
Detail.name:SetJustifyH("LEFT")
Detail.kind = Detail:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
Detail.kind:SetPoint("TOPLEFT", Detail.name, "BOTTOMLEFT", 0, -4)
Detail.count = Detail:CreateFontString(nil, "ARTWORK", "GameFontNormal")
Detail.count:SetPoint("TOPLEFT", Detail.kind, "BOTTOMLEFT", 0, -6)
Detail.sources = Detail:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
Detail.sources:SetPoint("TOPLEFT", Detail.count, "BOTTOMLEFT", 0, -8)
Detail.sources:SetPoint("RIGHT", Detail, "RIGHT", 0, 0)
Detail.sources:SetPoint("BOTTOM", Detail, "BOTTOM", 0, 0)
Detail.sources:SetJustifyH("LEFT")
Detail.sources:SetJustifyV("TOP")

local function Owned(decor)
    return S.state and S.state.owned and S.state.owned[decor] or 0
end

local function Placed(decor)
    local n = 0
    for _, p in ipairs(S.state and S.state.placed or {}) do
        if p.decor == decor then n = n + 1 end
    end
    return n
end

-- Where the object is found, in words.
local function SourceText(decor)
    local src = C.sources[decor]
    if not src then return L.loading end
    local loc, lines = Loc(), {}
    for _, q in ipairs(src.quests or {}) do
        local zone = loc.zones and loc.zones[q[3]]
        lines[#lines + 1] = fmt("|cffffd100%s|r %s (%s%s)", L.quest, q[1], fmt(L.level, q[2]), zone and (", " .. zone) or "")
    end
    if #(src.quests or {}) > 0 then
        lines[#lines + 1] = "|cff999999" .. L.questRule .. "|r"
        lines[#lines + 1] = " "
    end
    if #(src.bosses or {}) > 0 then
        lines[#lines + 1] = fmt("|cffffd100%s|r %s", L.boss, table.concat(src.bosses, ", "))
    end
    if #(src.maps or {}) > 0 then
        local maps = {}
        for _, m in ipairs(src.maps) do
            maps[#maps + 1] = loc.maps and loc.maps[m] or tostring(m)
        end
        table.sort(maps)
        lines[#lines + 1] = fmt("|cffffd100%s|r %s", L.themed, table.concat(maps, ", "))
    else
        lines[#lines + 1] = "|cffffd100" .. L.anywhere .. "|r"
    end
    local s, m = S.state or {}, (S.state and S.state.members) or { 4, 8, 20 }
    lines[#lines + 1] = "|cff999999" .. fmt(L.dropRule, s.paragonNeeded or 9, m[1], m[2], m[3]) .. "|r"
    lines[#lines + 1] = " "
    lines[#lines + 1] = "|cff999999" .. L.tradable .. "|r"
    return table.concat(lines, "\n")
end

function Detail:Display(decor)
    C.shown = decor
    local d = decor and GuildHouseDecor[decor]
    if not d then
        self.model:ClearModel()
        self.model.camera = nil
        self.name:SetText("")
        self.kind:SetText("")
        self.count:SetText("")
        self.sources:SetText("")
        return
    end
    if self.decor ~= decor then
        self.decor = decor
        self.model:ClearModel()
        self.model:SetModel((d[3]:gsub("%.m2$", ".mdx")))
        self.model.camera = d[12] or 0
        self.model:SetModelScale(1)
        self.model:SetPosition(0, 0, 0)
        self.model:SetCamera(self.model.camera)
        self.model:SetFacing(0)
        self.model:SetLight(1, 0, -0.5, -0.6, -0.6, 0.8, 1, 1, 1, 0.8, 1, 0.95, 0.85)
    end
    self.name:SetText(DecorName(decor))
    local cats = L.categories
    self.kind:SetText((cats[d[1]] or d[1]) .. " - " .. KindName(d[2]))
    local owned = Owned(decor)
    if owned > 0 then
        self.count:SetText(fmt(L.count, owned, Placed(decor)))
        self.count:SetTextColor(0.25, 1, 0.25)
    else
        self.count:SetText(L.none)
        self.count:SetTextColor(0.6, 0.6, 0.6)
    end
    self.sources:SetText(SourceText(decor))
    if not C.sources[decor] and not C.asked[decor] then
        C.asked[decor] = true
        AIO.Handle("GuildHouse", "Sources", decor)
    end
end

Coll.rows = {}
for i = 1, COLL_ROWS do
    local r = CreateFrame("Button", nil, Coll)
    r:SetHeight(COLL_ROW)
    r:SetPoint("TOPLEFT", 18, -146 - (i - 1) * COLL_ROW)
    r:SetPoint("RIGHT", Coll.scroll, "RIGHT", -4, 0)
    r:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
    r.selection = r:CreateTexture(nil, "BACKGROUND")
    r.selection:SetAllPoints()
    r.selection:SetTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
    r.selection:SetBlendMode("ADD")
    r.selection:SetVertexColor(1, 0.82, 0)
    r.icon = r:CreateTexture(nil, "ARTWORK")
    r.icon:SetWidth(COLL_ROW - 4)
    r.icon:SetHeight(COLL_ROW - 4)
    r.icon:SetPoint("LEFT", 2, 0)
    r.name = r:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    r.name:SetPoint("LEFT", r.icon, "RIGHT", 6, 0)
    r.name:SetPoint("RIGHT", -34, 0)
    r.name:SetJustifyH("LEFT")
    r.count = r:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
    r.count:SetPoint("RIGHT", -4, 0)
    r:SetScript("OnClick", function(self)
        C.selected = self.decor
        Detail:Display(self.decor)
        Coll:Refresh()
    end)
    r:SetScript("OnEnter", function(self)
        if self.decor then Detail:Display(self.decor) end
    end)
    r:SetScript("OnLeave", function()
        Detail:Display(C.selected)
    end)
    Coll.rows[i] = r
end

-- The list for the family, the filter and the search.
function Coll:Rebuild()
    local list = {}
    for decor, d in pairs(GuildHouseDecor or {}) do
        if not C.category or d[1] == C.category then
            local owned = Owned(decor) > 0
            if C.filter == "all" or (C.filter == "owned") == owned then
                if C.search == "" or DecorName(decor):lower():find(C.search, 1, true) then
                    list[#list + 1] = decor
                end
            end
        end
    end
    table.sort(list, function(a, b) return DecorName(a) < DecorName(b) end)
    C.list = list
    Coll:Refresh()
end

function Coll:Refresh()
    local total, owned = 0, 0
    for decor in pairs(GuildHouseDecor or {}) do
        total = total + 1
        if Owned(decor) > 0 then owned = owned + 1 end
    end
    if S.state and S.state.guild then
        Coll.progress:SetText(fmt(L.progress, owned, total))
    else
        Coll.progress:SetText(L.noguild)
    end
    for _, t in ipairs(Coll.tabs) do
        if t.category == C.category then t.button:LockHighlight() else t.button:UnlockHighlight() end
    end
    for f, b in pairs(Coll.filters) do
        if f == C.filter then b:LockHighlight() else b:UnlockHighlight() end
    end
    local list = C.list
    FauxScrollFrame_Update(Coll.scroll, #list, COLL_ROWS, COLL_ROW)
    local offset = FauxScrollFrame_GetOffset(Coll.scroll)
    for i = 1, COLL_ROWS do
        local r = Coll.rows[i]
        local decor = list[i + offset]
        r.decor = decor
        if decor then
            local n = Owned(decor)
            if not r.icon:SetTexture(ICONS .. decor) then r.icon:SetTexture(NO_ICON) end
            r.icon:SetDesaturated(n == 0)
            r.name:SetText(DecorName(decor))
            r.count:SetText(n > 0 and n or "")
            if n > 0 then r.name:SetTextColor(1, 1, 1) else r.name:SetTextColor(0.55, 0.55, 0.55) end
            if decor == C.selected then r.selection:Show() else r.selection:Hide() end
            r:Show()
        else
            r:Hide()
        end
    end
    Detail:Display(C.shown or C.selected)
end

Coll:SetScript("OnShow", function(self)
    self:SetScale(UIParent:GetScale())
    AIO.Handle("GuildHouse", "Refresh")
    self:Rebuild()
end)

function Coll:Toggle()
    if self:IsShown() then self:Hide() else self:Show() end
end

function Handlers.Sources(_, sources)
    if type(sources) ~= "table" or not sources.decor then return end
    C.sources[sources.decor] = sources
    C.asked[sources.decor] = nil
    if Coll:IsShown() and C.shown == sources.decor then
        Detail:Display(C.shown)
    end
end

-- The way in from the Customization panel: in its third row of families, beside the last one.
local collButton = Button(Panel, L.collection, 88)
collButton:SetPoint("TOPLEFT", 14 + 2 * 91, -36 - 2 * 26)
collButton:SetScript("OnClick", function() Coll:Toggle() end)

------------------------------------------------------------------------------
-- The way in
------------------------------------------------------------------------------

-- The way in, at the top of the screen, while the member stands in his house and may decorate it.
local Open = CreateFrame("Button", "GuildHouseOpenButton", UIParent, "UIPanelButtonTemplate")
Open:SetWidth(150)
Open:SetHeight(24)
Open:SetPoint("TOP", UIParent, "TOP", 0, -8)
Open:SetText(L.button)
Open:Hide()
Open:SetScript("OnClick", function() UI:Enter() end)

local function Shown(state)
    S.state = state
    if state.inHouse and state.can then Open:Show() else Open:Hide() end
    if S.active and not state.inHouse then
        UI:Leave()
    end
    if S.active then UI:Refresh() end
    if Coll:IsShown() then Coll:Rebuild() end
end

-- The objects' names in the player's language, a part at a time (none for English).
function Handlers.Names(_, part, done)
    for decor, name in pairs(type(part) == "table" and part or {}) do
        names[decor] = name
    end
    if done then
        if S.active then UI:Refresh() end
        if Coll:IsShown() then Coll:Rebuild() end
    end
end

function Handlers.Show(_, state)
    Shown(state)
    if state.inHouse then UI:Enter() end
end

function Handlers.Update(_, state)
    Shown(state)
end

-- The module says the guild's objects moved (CHAT_MSG_ADDON), or the player
-- went somewhere else.
local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_ENTERING_WORLD")
listener:RegisterEvent("ZONE_CHANGED_NEW_AREA")
-- After a loading screen the server may not have the player on his new map
-- yet, nor AIO its channel: asked again a little later.
local again = {}
listener:SetScript("OnEvent", function(_, event, prefix)
    if event == "CHAT_MSG_ADDON" and prefix ~= "GuildHouse" then return end
    AIO.Handle("GuildHouse", "Refresh")
    if event ~= "CHAT_MSG_ADDON" then
        again = { 2, 6 }
    end
end)
local waited = 0
listener:SetScript("OnUpdate", function(_, elapsed)
    if not again[1] then
        waited = 0
        return
    end
    waited = waited + elapsed
    if waited >= again[1] then
        table.remove(again, 1)
        AIO.Handle("GuildHouse", "Refresh")
    end
end)

-- Out of the mode before the world goes away (logout, loading screen): the interface comes back
-- first. Hidden at logout, UIParent leaves its frames without a place, and the addons that save
-- their windows' positions then (AIO's LibWindow) fail on them.
local leaving = CreateFrame("Frame")
leaving:RegisterEvent("PLAYER_LEAVING_WORLD")
leaving:RegisterEvent("PLAYER_LOGOUT")
leaving:SetScript("OnEvent", function()
    if S.active then UI:Leave() end
    UIParent:Show()
end)

SLASH_GUILDHOUSECOLLECTION1 = "/gcoll"
SLASH_GUILDHOUSECOLLECTION2 = "/gcollection"
SlashCmdList.GUILDHOUSECOLLECTION = function() Coll:Toggle() end

-- And from the stock guild window, under the talents' button (ForeverUI's guild page: /gcoll).
local guildButton
local function AttachGuildButton()
    local parent = _G.GuildFrame
    if guildButton or not parent then return end
    guildButton = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    guildButton:SetWidth(130)
    guildButton:SetHeight(22)
    guildButton:SetPoint("TOPRIGHT", parent, "TOPRIGHT", -44, -68)
    guildButton:SetText(L.collection)
    guildButton:SetFrameLevel(parent:GetFrameLevel() + 10)
    guildButton:SetScript("OnClick", function() Coll:Toggle() end)
end
if FriendsFrame then FriendsFrame:HookScript("OnShow", AttachGuildButton) end
if FriendsFrame_Update then hooksecurefunc("FriendsFrame_Update", AttachGuildButton) end
AttachGuildButton()

SLASH_GUILDHOUSEDECO1 = "/gdeco"
SlashCmdList.GUILDHOUSEDECO = function()
    if S.active then UI:Leave() else AIO.Handle("GuildHouse", "Open") end
end

-- This file is run when AIO hands it over, after the first loading screen's
-- PLAYER_ENTERING_WORLD: the state is asked for at once, and again a little later.
AIO.Handle("GuildHouse", "Refresh")
again = { 2, 6 }
-- the names, once, in a language other than English
if GetLocale() ~= "enUS" and GetLocale() ~= "enGB" then
    AIO.Handle("GuildHouse", "Names")
end
