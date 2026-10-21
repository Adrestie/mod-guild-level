// Extension service-table glue and log macros.
// Copyright (C) 2026 WarcraftXL, mod-guild-level
// GPL-3.0-or-later.

#pragma once

#include "wxl/PluginApi.h"

namespace wxl_guildhouse
{
    /// Service table the core handed us in WXL_Load. Never null after a successful load.
    extern const WXL_Api* g_api;
}

#define WLOG_INFO(...)  wxl_guildhouse::g_api->Log(WXL_LOG_INFO, "wxl-guildhouse", __VA_ARGS__)
#define WLOG_WARN(...)  wxl_guildhouse::g_api->Log(WXL_LOG_WARN, "wxl-guildhouse", __VA_ARGS__)
#define WLOG_ERROR(...) wxl_guildhouse::g_api->Log(WXL_LOG_ERROR, "wxl-guildhouse", __VA_ARGS__)
