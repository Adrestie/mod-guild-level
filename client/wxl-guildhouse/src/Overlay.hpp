// The overlay the gizmo is drawn on: an ImGui context of the extension's own, no window of it ever
// shown (the windows are the client's interface), and the input router that gives the gizmo the
// mouse only while the cursor is on it.
// Copyright (C) 2026 WarcraftXL, mod-guild-level
// GPL-3.0-or-later. Adapted from wxl-housing's ImGuiHostExt.

#pragma once

#include "wxl/EventScript.hpp"

struct ImGuiContext;

namespace wxl_guildhouse
{
    class Overlay
    {
    public:
        static Overlay& Instance();

        void OnEndScene(void* device);
        void OnDeviceLost(const wxl::events::DeviceResetArgs& a);
        void OnDeviceReset(const wxl::events::DeviceResetArgs& a);

        bool Ready() const { return ready_; }
        ImGuiContext* Context() const { return context_; }
        /// The cursor is over the gizmo, or drags it.
        bool GizmoBusy() const { return gizmoBusy_; }

    private:
        Overlay() = default;
        void EnsureInit(void* device);

        bool ready_ = false;
        bool gizmoBusy_ = false;
        ImGuiContext* context_ = nullptr;
    };
}
