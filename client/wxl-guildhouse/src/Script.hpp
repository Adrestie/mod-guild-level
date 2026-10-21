// The editor's script functions (GuildHouse_*). See Script.cpp.
// Copyright (C) 2026 WarcraftXL, mod-guild-level
// GPL-3.0-or-later.

#pragma once

namespace wxl_guildhouse
{
    /// Hooks the client so that the GuildHouse_* functions are registered with every interface it
    /// loads, and accepted when called. False when a hook could not be placed.
    bool InstallScriptFunctions();
}
