// wxl-guildhouse entry point: WXL_Query / WXL_Load.
// Copyright (C) 2026 WarcraftXL, mod-guild-level
// GPL-3.0-or-later.

#include "ExtensionApi.hpp"

#include "wxl/EventScript.hpp"

#include "FreeBuildCamera.hpp"
#include "Overlay.hpp"
#include "Script.hpp"

namespace wxl_guildhouse
{
    const WXL_Api* g_api = nullptr;

    namespace
    {
        class GuildHouseScript : public wxl::ext::EventScript
        {
        public:
            GuildHouseScript()
            {
                on<&GuildHouseScript::OnEndScene>(wxl::events::Event::OnEndScene);
                on<&GuildHouseScript::OnDeviceLost>(wxl::events::Event::OnDeviceLost);
                on<&GuildHouseScript::OnDeviceReset>(wxl::events::Event::OnDeviceReset);
            }

            void OnEndScene(const wxl::events::EndSceneArgs& a) { Overlay::Instance().OnEndScene(a.device); }
            void OnDeviceLost(const wxl::events::DeviceResetArgs& a) { Overlay::Instance().OnDeviceLost(a); }
            void OnDeviceReset(const wxl::events::DeviceResetArgs& a) { Overlay::Instance().OnDeviceReset(a); }
        };

        GuildHouseScript& Script()
        {
            static GuildHouseScript s;
            return s;
        }
    }
}

const WXL_PluginInfo* __cdecl WXL_Query(void)
{
    static const WXL_PluginInfo s_info = {
        sizeof(WXL_PluginInfo),
        WXL_API_VERSION,
        "wxl-guildhouse",
        1,
        WXL_CLIENT_BUILD,
    };
    return &s_info;
}

int __cdecl WXL_Load(const WXL_Api* api)
{
    if (!api || api->apiVersion != WXL_API_VERSION)
        return 0;

    using namespace wxl_guildhouse;
    g_api = api;
    wxl::ext::EventScript::Bind(api);
    (void)Script();
    Overlay::Instance();
    if (!InstallScriptFunctions())
    {
        WLOG_ERROR("the GuildHouse_* script functions could not be installed");
        return 0;
    }
    const bool camera = FreeBuildCamera::Instance().Initialize();
    WLOG_INFO("wxl-guildhouse 1: guild house editor ready; free camera %s", camera ? "ready" : "disabled");
    return 1;
}
