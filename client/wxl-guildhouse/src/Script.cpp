// The script functions the interface drives the editor with (GuildHouse_Client.lua), and the two
// seams that let the client call them: functions outside Wow.exe's own code are refused by its
// callback check unless the check is told about them, and a global put up at the interface load is
// in force for the whole of that interface's life.
// Copyright (C) 2026 WarcraftXL, mod-guild-level
// GPL-3.0-or-later.

#include "Script.hpp"

#include "Editor.hpp"
#include "ExtensionApi.hpp"
#include "FreeBuildCamera.hpp"
#include "Overlay.hpp"

#include "game/Camera.hpp"
#include "game/Pick.hpp"
#include "game/Script.hpp"
#include "game/World.hpp"

#include <windows.h>

#include <cmath>
#include <cstring>
#include <string>

namespace wxl_guildhouse
{
    namespace
    {
        namespace script = wxl::game::script;
        namespace camera = wxl::game::camera;
        namespace world = wxl::game::world;

        constexpr float kReach = 250.0f;          // yards a cursor ray looks into the world

        script::ValidateCallbackFn g_validate = nullptr;
        script::InterfaceLoadFn g_interfaceLoad = nullptr;

        float Num(void* L, int i, float fallback = 0.0f)
        {
            return script::IsNumber(L, i) ? static_cast<float>(script::ToNumber(L, i)) : fallback;
        }

        Transform ReadTransform(void* L, int first)
        {
            Transform t;
            for (int k = 0; k < 3; ++k) t.pos[k] = Num(L, first + k);
            for (int k = 0; k < 4; ++k) t.rot[k] = Num(L, first + 3 + k, k == 3 ? 1.0f : 0.0f);
            t.scale = Num(L, first + 7, 1.0f);
            return t;
        }

        int PushTransform(void* L, const Transform& t)
        {
            for (float v : t.pos) script::PushNumber(L, v);
            for (float v : t.rot) script::PushNumber(L, v);
            script::PushNumber(L, t.scale);
            return 8;
        }

        bool Invert(const float m[16], float out[16])
        {
            float inv[16];
            inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
            inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
            inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
            inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
            inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
            inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
            inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
            inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
            inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
            inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
            inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
            inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
            inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
            inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
            inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
            inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
            const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
            if (!(std::fabs(det) > 1e-12f)) return false;
            for (int i = 0; i < 16; ++i) out[i] = inv[i] / det;
            return true;
        }

        // A clip-space point back into the world (row vectors).
        bool Unproject(const float inv[16], float x, float y, float z, float out[3])
        {
            float w = 0.0f;
            for (int j = 0; j < 4; ++j)
            {
                const float v = x * inv[j] + y * inv[4 + j] + z * inv[8 + j] + inv[12 + j];
                if (j < 3) out[j] = v; else w = v;
            }
            if (!(std::fabs(w) > 1e-9f)) return false;
            for (int j = 0; j < 3; ++j) out[j] /= w;
            return true;
        }

        // The world point under the cursor, on the hall's own collision (terrain and map objects:
        // the ray goes through models, the edited one first of all).
        bool CursorWorld(float out[3])
        {
            HWND hwnd = GetActiveWindow();
            POINT p;
            RECT rc;
            if (!hwnd || !GetCursorPos(&p) || !ScreenToClient(hwnd, &p) || !GetClientRect(hwnd, &rc))
                return false;
            const float w = static_cast<float>(rc.right - rc.left), h = static_cast<float>(rc.bottom - rc.top);
            if (w <= 0.0f || h <= 0.0f) return false;
            const float nx = p.x / w * 2.0f - 1.0f, ny = 1.0f - p.y / h * 2.0f;
            float inv[16], nearPoint[3], farPoint[3];
            if (!Invert(camera::GetViewProj(), inv) || !Unproject(inv, nx, ny, 0.0f, nearPoint) ||
                !Unproject(inv, nx, ny, 1.0f, farPoint))
                return false;
            float dir[3] = { farPoint[0] - nearPoint[0], farPoint[1] - nearPoint[1], farPoint[2] - nearPoint[2] };
            const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
            if (!(len > 1e-6f)) return false;
            const float to[3] = { nearPoint[0] + dir[0] / len * kReach, nearPoint[1] + dir[1] / len * kReach,
                                  nearPoint[2] + dir[2] / len * kReach };
            world::WorldHit hit;
            if (!world::TraceLine(nearPoint, to, hit)) return false;
            out[0] = hit.pos.x;
            out[1] = hit.pos.y;
            out[2] = hit.pos.z;
            return true;
        }

        // --- the functions -------------------------------------------------------------------

        // GuildHouse_BeginEdit(model, x, y, z, qx, qy, qz, qw, scale) -> started
        int BeginEdit(void* L)
        {
            const char* path = script::IsString(L, 1) ? script::ToString(L, 1) : nullptr;
            script::PushBoolean(L, path && Editor::Instance().Begin(path, ReadTransform(L, 2)));
            return 1;
        }

        // GuildHouse_EndEdit()
        int EndEdit(void*)
        {
            Editor::Instance().End();
            return 0;
        }

        // GuildHouse_IsEditing() -> editing, ready (the model loaded)
        int IsEditing(void* L)
        {
            script::PushBoolean(L, Editor::Instance().Editing());
            script::PushBoolean(L, Editor::Instance().Ready());
            return 2;
        }

        // GuildHouse_GetTransform() -> x, y, z, qx, qy, qz, qw, scale, or nothing
        int GetTransform(void* L)
        {
            if (!Editor::Instance().Editing()) return 0;
            return PushTransform(L, Editor::Instance().Current());
        }

        // GuildHouse_SetTransform(x, y, z, qx, qy, qz, qw, scale)
        int SetTransform(void* L)
        {
            if (Editor::Instance().Editing()) Editor::Instance().Set(ReadTransform(L, 1));
            return 0;
        }

        // GuildHouse_SetPosition(x, y, z)
        int SetPosition(void* L)
        {
            if (!Editor::Instance().Editing()) return 0;
            Transform t = Editor::Instance().Current();
            for (int k = 0; k < 3; ++k) t.pos[k] = Num(L, 1 + k, t.pos[k]);
            Editor::Instance().Set(t);
            return 0;
        }

        // GuildHouse_SetMode("move" | "rotate" | "scale")
        int SetMode(void* L)
        {
            const char* mode = script::IsString(L, 1) ? script::ToString(L, 1) : "";
            Editor::Instance().SetMode(std::strcmp(mode, "rotate") == 0 ? GizmoMode::Rotate
                                     : std::strcmp(mode, "scale") == 0 ? GizmoMode::Scale : GizmoMode::Move);
            return 0;
        }

        // GuildHouse_IsGizmoBusy() -> the cursor is over the gizmo or drags it
        int IsGizmoBusy(void* L)
        {
            script::PushBoolean(L, Overlay::Instance().GizmoBusy());
            return 1;
        }

        // GuildHouse_CursorWorld() -> x, y, z of the hall under the cursor, or nothing
        int CursorWorldFn(void* L)
        {
            float p[3];
            if (!CursorWorld(p)) return 0;
            for (float v : p) script::PushNumber(L, v);
            return 3;
        }

        // GuildHouse_GroundZ(x, y, z) -> the height of the surface below (or just above), or nothing
        int GroundZ(void* L)
        {
            float z;
            if (!world::GroundZ(Num(L, 1), Num(L, 2), Num(L, 3), z)) return 0;
            script::PushNumber(L, z);
            return 1;
        }

        // GuildHouse_WorldToScreen(x, y, z) -> fx, fy (0..1 from the bottom left), or nothing
        // when the point is behind the camera
        int WorldToScreen(void* L)
        {
            const float* vp = camera::GetViewProj();
            const float w[3] = { Num(L, 1), Num(L, 2), Num(L, 3) };
            float clip[4];
            for (int j = 0; j < 4; ++j)
                clip[j] = w[0] * vp[j] + w[1] * vp[4 + j] + w[2] * vp[8 + j] + vp[12 + j];
            if (clip[3] <= 0.001f) return 0;
            script::PushNumber(L, clip[0] / clip[3] * 0.5f + 0.5f);
            script::PushNumber(L, clip[1] / clip[3] * 0.5f + 0.5f);
            return 2;
        }

        // GuildHouse_FreeCamera([on]) -> on: toggles, or sets, the free build camera
        int FreeCamera(void* L)
        {
            auto& cam = FreeBuildCamera::Instance();
            if (script::ArgCount(L) >= 1)
            {
                const bool on = script::ToBoolean(L, 1);
                if (on && !cam.Active()) cam.Activate();
                else if (!on && cam.Active()) cam.Deactivate();
            }
            else
                cam.Toggle();
            script::PushBoolean(L, cam.Active());
            return 1;
        }

        struct Entry { const char* name; script::Function fn; };
        const Entry kFunctions[] = {
            { "GuildHouse_BeginEdit", &BeginEdit },
            { "GuildHouse_EndEdit", &EndEdit },
            { "GuildHouse_IsEditing", &IsEditing },
            { "GuildHouse_GetTransform", &GetTransform },
            { "GuildHouse_SetTransform", &SetTransform },
            { "GuildHouse_SetPosition", &SetPosition },
            { "GuildHouse_SetMode", &SetMode },
            { "GuildHouse_IsGizmoBusy", &IsGizmoBusy },
            { "GuildHouse_CursorWorld", &CursorWorldFn },
            { "GuildHouse_GroundZ", &GroundZ },
            { "GuildHouse_WorldToScreen", &WorldToScreen },
            { "GuildHouse_FreeCamera", &FreeCamera },
        };

        bool Ours(uintptr_t fn)
        {
            for (const Entry& e : kFunctions)
                if (reinterpret_cast<uintptr_t>(e.fn) == fn) return true;
            return false;
        }

        void __cdecl ValidateDetour(uintptr_t fn)
        {
            if (Ours(fn)) return;
            g_validate(fn);
        }

        int __cdecl InterfaceLoadDetour(const char* toc, const char* addOn, void* md5, void* status)
        {
            for (const Entry& e : kFunctions) script::Register(e.name, e.fn);
            return g_interfaceLoad(toc, addOn, md5, status);
        }
    }

    bool InstallScriptFunctions()
    {
        if (!g_api->HookAttach("wxl-guildhouse.validate", script::kValidateCallbackSeam,
                               reinterpret_cast<void*>(&ValidateDetour), reinterpret_cast<void**>(&g_validate),
                               WXL_HOOK_DEFAULT_PRIORITY))
            return false;
        if (!g_api->HookAttach("wxl-guildhouse.interface", script::kInterfaceLoadSeam,
                               reinterpret_cast<void*>(&InterfaceLoadDetour),
                               reinterpret_cast<void**>(&g_interfaceLoad), WXL_HOOK_DEFAULT_PRIORITY))
            return false;
        return true;
    }
}
