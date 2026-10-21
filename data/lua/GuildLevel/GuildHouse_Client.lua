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
        move = "Move (1)", rotate = "Turn (2)", scale = "Size (3)", world = "World", localAxes = "Local", reset = "Reset turn", follow = "Follow the mouse",
        ground = "Set on the floor", camera = "Free camera", exit = "Leave (Esc)",
        noext = "The Customization mode needs WarcraftXL and its guild house editor (Extensions\\wxl-guildhouse).",
        cannot = "You cannot decorate the house: Paragon level %d, guild rank %d or above, in the house.",
        aim = "Point at the hall's floor to place the object.",
        carry = "Left click: set it down. Wheel: turn it. Right click: give up.",
        hint = "Click an outlined object to take it in hand.",
        categories = { Furnishings = "Furniture", Lighting = "Lights", Accents = "Decoration", Nature = "Plants",
            Structural = "Structure", Functional = "Utility", Miscellaneous = "Misc" },
    },
    frFR = {
        button = "Personnalisation",
        title = "Objets de guilde", validate = "Valider", cancel = "Annuler", store = "Ranger",
        move = "Déplacer (1)", rotate = "Pivoter (2)", scale = "Taille (3)", world = "Monde", localAxes = "Local", reset = "Rotation d'origine", follow = "Suivre la souris",
        ground = "Poser au sol", camera = "Caméra libre", exit = "Quitter (Échap)",
        noext = "Le mode Personnalisation demande WarcraftXL et son éditeur de maison de guilde (Extensions\\wxl-guildhouse).",
        cannot = "Vous ne pouvez pas décorer la maison : Parangon %d, rang de guilde %d ou mieux, dans la maison.",
        aim = "Visez le sol du hall pour poser l'objet.",
        carry = "Clic gauche : poser. Molette : tourner. Clic droit : abandonner.",
        hint = "Cliquez sur un objet entouré pour le prendre en main.",
        categories = { Furnishings = "Mobilier", Lighting = "Lumières", Accents = "Décoration", Nature = "Plantes",
            Structural = "Structure", Functional = "Utilitaire", Miscellaneous = "Divers" },
    },
}
L = L[GetLocale()] or L.enUS

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
        if da[2] ~= db[2] then return da[2] < db[2] end
        return da[4] < db[4]
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
    self.name:SetText(d[4])
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
        GameTooltip:AddLine(d[4])
        GameTooltip:AddLine(d[2], 0.7, 0.7, 0.7)
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
            r.name:SetText(GuildHouseDecor[decor][4])
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
        local d = GuildHouseDecor[S.edit.decor]
        Bar.text:SetText(d and d[4] or "")
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

SLASH_GUILDHOUSEDECO1 = "/gdeco"
SlashCmdList.GUILDHOUSEDECO = function()
    if S.active then UI:Leave() else AIO.Handle("GuildHouse", "Open") end
end

-- This file is run when AIO hands it over, after the first loading screen's
-- PLAYER_ENTERING_WORLD: the state is asked for at once, and again a little later.
AIO.Handle("GuildHouse", "Refresh")
again = { 2, 6 }
