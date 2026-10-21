// The gizmo's overlay and input router. See Overlay.hpp.
// Copyright (C) 2026 WarcraftXL, mod-guild-level
// GPL-3.0-or-later. Adapted from wxl-housing's ImGuiHostExt.

#include "Overlay.hpp"

#include "Editor.hpp"
#include "ExtensionApi.hpp"
#include "FreeBuildCamera.hpp"
#include "Targets.hpp"

#include <windows.h>
#include <d3d9.h>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "../third_party/imguizmo/ImGuizmo.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace wxl_guildhouse
{
    namespace
    {
        HWND g_hwnd = nullptr;
        WNDPROC g_origWndProc = nullptr;
        bool g_gizmoClick = false;      // a left button the gizmo took: its release is the gizmo's

        BOOL CALLBACK PickWindow(HWND h, LPARAM out)
        {
            DWORD pid = 0;
            GetWindowThreadProcessId(h, &pid);
            if (pid == GetCurrentProcessId() && GetWindow(h, GW_OWNER) == nullptr && IsWindowVisible(h))
            {
                *reinterpret_cast<HWND*>(out) = h;
                return FALSE;
            }
            return TRUE;
        }

        HWND FindGameWindow()
        {
            HWND h = nullptr;
            EnumWindows(&PickWindow, reinterpret_cast<LPARAM>(&h));
            if (!h) h = FindWindowA("GxWindowClass", nullptr);
            return h;
        }

        // Every message reaches ImGui (the gizmo reads the mouse from it), and the free camera; the
        // game gets everything but the left clicks on the gizmo.
        LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
        {
            auto& overlay = Overlay::Instance();
            if (overlay.Ready())
            {
                ImGuiContext* previous = ImGui::GetCurrentContext();
                ImGui::SetCurrentContext(overlay.Context());
                ImGui_ImplWin32_WndProcHandler(h, m, w, l);
                bool handled = FreeBuildCamera::Instance().HandleWindowMessage(h, m, w, l, false, false);
                if (!handled)
                {
                    if (m == WM_LBUTTONDOWN && Editor::Instance().Ready() && overlay.GizmoBusy())
                    {
                        g_gizmoClick = true;
                        handled = true;
                    }
                    else if (m == WM_LBUTTONDOWN && Targets::Instance().Hovered())
                    {
                        // a placed object picked: the interface takes it in hand
                        Targets::Instance().Pick();
                        g_gizmoClick = true;
                        handled = true;
                    }
                    else if (m == WM_LBUTTONUP && g_gizmoClick)
                    {
                        g_gizmoClick = false;
                        handled = true;
                    }
                }
                if (m == WM_CAPTURECHANGED || m == WM_CANCELMODE)
                    g_gizmoClick = false;
                ImGui::SetCurrentContext(previous);
                if (handled) return 0;
            }
            return CallWindowProcA(g_origWndProc, h, m, w, l);
        }
    }

    Overlay& Overlay::Instance()
    {
        static Overlay s;
        return s;
    }

    void Overlay::EnsureInit(void* device)
    {
        if (ready_) return;
        g_hwnd = FindGameWindow();
        if (!g_hwnd || !device) return;

        IMGUI_CHECKVERSION();
        ImGuiContext* previous = ImGui::GetCurrentContext();
        context_ = ImGui::CreateContext();
        if (!context_)
        {
            ImGui::SetCurrentContext(previous);
            WLOG_ERROR("overlay: CreateContext failed");
            return;
        }
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        if (!ImGui_ImplWin32_Init(g_hwnd) ||
            !ImGui_ImplDX9_Init(reinterpret_cast<IDirect3DDevice9*>(device)))
        {
            ImGui::DestroyContext(context_);
            context_ = nullptr;
            ImGui::SetCurrentContext(previous);
            WLOG_ERROR("overlay: ImGui backend init failed");
            return;
        }
        ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
        g_origWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProc)));
        if (!g_origWndProc)
        {
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext(context_);
            context_ = nullptr;
            ImGui::SetCurrentContext(previous);
            WLOG_ERROR("overlay: window subclass failed (error=%lu)", GetLastError());
            return;
        }
        ready_ = true;
        ImGui::SetCurrentContext(previous);
        WLOG_INFO("overlay: ready (hwnd=%p)", g_hwnd);
    }

    void Overlay::OnDeviceLost(const wxl::events::DeviceResetArgs&)
    {
        if (!ready_) return;
        ImGuiContext* previous = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(context_);
        ImGui_ImplDX9_InvalidateDeviceObjects();
        ImGui::SetCurrentContext(previous);
    }

    void Overlay::OnDeviceReset(const wxl::events::DeviceResetArgs&)
    {
        if (!ready_) return;
        ImGuiContext* previous = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(context_);
        ImGui_ImplDX9_CreateDeviceObjects();
        ImGui::SetCurrentContext(previous);
    }

    void Overlay::OnEndScene(void* device)
    {
        EnsureInit(device);
        FreeBuildCamera::Instance().Tick();
        Editor::Instance().Tick();
        if (!ready_ || (!Editor::Instance().Ready() && !Targets::Instance().Shown()))
        {
            gizmoBusy_ = false;
            return;
        }

        ImGuiContext* previous = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(context_);
        ImGui::GetIO().MouseDrawCursor = false;     // the game's cursor stays the only one
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        Targets::Instance().Draw(reinterpret_cast<IDirect3DDevice9*>(device));
        gizmoBusy_ = Editor::Instance().DrawGizmo();
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        ImGui::SetCurrentContext(previous);
    }
}
