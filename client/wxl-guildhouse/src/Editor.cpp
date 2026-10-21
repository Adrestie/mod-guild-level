// The guild house editor. See Editor.hpp.
// Copyright (C) 2026 WarcraftXL, mod-guild-level
// GPL-3.0-or-later. The spawn and chunk attachment are wxl-housing's (Placement.cpp).

#include "Editor.hpp"

#include "ExtensionApi.hpp"

#include "game/Binding.hpp"
#include "game/Camera.hpp"
#include "game/Doodad.hpp"
#include "game/Io.hpp"
#include "game/World.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "imgui.h"

#include "../third_party/imguizmo/ImGuizmo.h"

namespace wxl_guildhouse
{
    namespace
    {
        namespace doodad = wxl::game::doodad;
        namespace camera = wxl::game::camera;
        namespace io = wxl::game::io;
        namespace world = wxl::game::world;
        namespace ooddad = wxl::game::doodad::off;

        // MDDF record beyond the offsets header's landmarks (0x24 bytes, standard MDDF).
        constexpr size_t kMddfNameId = 0x00;
        constexpr size_t kMddfUniqueId = 0x04;
        constexpr size_t kMddfScale = 0x20;
        constexpr size_t kMddfFlags = 0x22;

        // The reference node the client links a spawned doodad into its chunk with (build 12340),
        // as wxl-housing found it: without it the doodad stays on the global list after the map
        // unloads and the client faults tearing that list down.
        constexpr uintptr_t kCreateDoodadRef = 0x007C0750;
        using CreateDoodadRefFn = void*(__cdecl*)(void* doodad);
        constexpr size_t kRefOwnerChunk = 0x08;
        constexpr size_t kDoodadVisibilityScale = 0x8C;

        constexpr unsigned kLoadFrames = 900;    // frames a model has to load in

        uint32_t g_uniqueId = 0x5A000000u;        // synthetic unique ids, session-local

        bool ClientCanOpen(const std::string& path)
        {
            void* handle = nullptr;
            if (!io::FileOpen(path.c_str(), 0, &handle) || !handle) return false;
            io::FileClose(handle);
            return true;
        }

        bool SafeModelPath(const std::string& path)
        {
            if (path.empty() || path.size() >= 260 || path.find(':') != std::string::npos ||
                path.find("..") != std::string::npos)
                return false;
            const char* ext = std::strrchr(path.c_str(), '.');
            return ext && _stricmp(ext, ".m2") == 0;
        }

        bool CanAttachToChunk(void* chunk)
        {
            if (!doodad::detail::Readable(chunk, ooddad::kChunkDoodadHead + sizeof(void*)))
                return false;
            const uint32_t linkOffset = *reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const char*>(chunk) + ooddad::kChunkDoodadLinkOff);
            if (linkOffset > 0x400) return false;
            void* previousNode = *reinterpret_cast<void**>(
                reinterpret_cast<char*>(chunk) + ooddad::kChunkDoodadHead - sizeof(void*));
            return doodad::detail::Writable(previousNode, 2 * sizeof(void*));
        }

        bool AttachToChunk(void* object, void* chunk)
        {
            if (!object || !CanAttachToChunk(chunk)) return false;
            const uint32_t linkOffset = *reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const char*>(chunk) + ooddad::kChunkDoodadLinkOff);
            void* previousNode = *reinterpret_cast<void**>(
                reinterpret_cast<char*>(chunk) + ooddad::kChunkDoodadHead - sizeof(void*));
            static const auto s_createRef = wxl::game::Native<CreateDoodadRefFn>(kCreateDoodadRef);
            void* reference = s_createRef(object);
            if (!reference) return false;
            char* node = reinterpret_cast<char*>(reference) + linkOffset;
            *reinterpret_cast<void**>(reinterpret_cast<char*>(reference) + kRefOwnerChunk) = chunk;
            // the client's own push-back, as its placement caller does it
            *reinterpret_cast<void**>(node) = previousNode;
            *reinterpret_cast<void**>(node + sizeof(void*)) =
                *reinterpret_cast<void**>(reinterpret_cast<char*>(previousNode) + sizeof(void*));
            *reinterpret_cast<void**>(reinterpret_cast<char*>(previousNode) + sizeof(void*)) = reference;
            *reinterpret_cast<void**>(
                reinterpret_cast<char*>(chunk) + ooddad::kChunkDoodadHead - sizeof(void*)) = node;
            *reinterpret_cast<float*>(reinterpret_cast<char*>(object) + kDoodadVisibilityScale) = 1.0f;
            *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(object) + ooddad::kFlags) |= 4u;
            return true;
        }

        void Normalize(float q[4])
        {
            const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
            if (!(n > 1e-6f) || !std::isfinite(n))
            {
                q[0] = q[1] = q[2] = 0.0f;
                q[3] = 1.0f;
                return;
            }
            for (int i = 0; i < 4; ++i) q[i] /= n;
        }
    }

    void Compose(const Transform& t, float m[16])
    {
        float q[4] = { t.rot[0], t.rot[1], t.rot[2], t.rot[3] };
        Normalize(q);
        const float x = q[0], y = q[1], z = q[2], w = q[3];
        // column-vector rotation R; the row-vector matrix holds its transpose: row i is the image
        // of the model's axis i
        const float r[3][3] = {
            { 1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w) },
            { 2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w) },
            { 2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y) },
        };
        const float s = (t.scale > 0.0f && std::isfinite(t.scale)) ? t.scale : 1.0f;
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
                m[row * 4 + col] = r[col][row] * s;
            m[row * 4 + 3] = 0.0f;
        }
        m[12] = t.pos[0];
        m[13] = t.pos[1];
        m[14] = t.pos[2];
        m[15] = 1.0f;
    }

    Transform Decompose(const float m[16])
    {
        Transform t;
        t.pos[0] = m[12];
        t.pos[1] = m[13];
        t.pos[2] = m[14];
        float s = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
        if (!(s > 1e-6f) || !std::isfinite(s)) s = 1.0f;
        t.scale = s;
        float r[3][3];
        for (int row = 0; row < 3; ++row)
        {
            // each row normalized on its own: the gizmo's scale is uniform, but a rounding error
            // must not leak into the rotation
            const float n = std::sqrt(m[row * 4] * m[row * 4] + m[row * 4 + 1] * m[row * 4 + 1] +
                                      m[row * 4 + 2] * m[row * 4 + 2]);
            for (int col = 0; col < 3; ++col)
                r[col][row] = n > 1e-6f ? m[row * 4 + col] / n : (row == col ? 1.0f : 0.0f);
        }
        const float trace = r[0][0] + r[1][1] + r[2][2];
        float q[4];
        if (trace > 0.0f)
        {
            const float k = 0.5f / std::sqrt(trace + 1.0f);
            q[3] = 0.25f / k;
            q[0] = (r[2][1] - r[1][2]) * k;
            q[1] = (r[0][2] - r[2][0]) * k;
            q[2] = (r[1][0] - r[0][1]) * k;
        }
        else if (r[0][0] > r[1][1] && r[0][0] > r[2][2])
        {
            const float k = 2.0f * std::sqrt(1.0f + r[0][0] - r[1][1] - r[2][2]);
            q[3] = (r[2][1] - r[1][2]) / k;
            q[0] = 0.25f * k;
            q[1] = (r[0][1] + r[1][0]) / k;
            q[2] = (r[0][2] + r[2][0]) / k;
        }
        else if (r[1][1] > r[2][2])
        {
            const float k = 2.0f * std::sqrt(1.0f + r[1][1] - r[0][0] - r[2][2]);
            q[3] = (r[0][2] - r[2][0]) / k;
            q[0] = (r[0][1] + r[1][0]) / k;
            q[1] = 0.25f * k;
            q[2] = (r[1][2] + r[2][1]) / k;
        }
        else
        {
            const float k = 2.0f * std::sqrt(1.0f + r[2][2] - r[0][0] - r[1][1]);
            q[3] = (r[1][0] - r[0][1]) / k;
            q[0] = (r[0][2] + r[2][0]) / k;
            q[1] = (r[1][2] + r[2][1]) / k;
            q[2] = 0.25f * k;
        }
        Normalize(q);
        std::memcpy(t.rot, q, sizeof q);
        return t;
    }

    Editor& Editor::Instance()
    {
        static Editor s;
        return s;
    }

    bool Editor::Begin(const std::string& requested, const Transform& t)
    {
        End();
        if (world::CurrentMapId() < 0) return false;

        std::string path = requested;
        std::replace(path.begin(), path.end(), '/', '\\');
        if (!SafeModelPath(path))
        {
            WLOG_WARN("edit: refused model path %s", requested.c_str());
            return false;
        }
        if (!ClientCanOpen(path))
        {
            WLOG_WARN("edit: the game has no %s", path.c_str());
            return false;
        }

        float at[3] = { t.pos[0], t.pos[1], t.pos[2] };
        void* chunk = doodad::ChunkAt(at);
        if (!CanAttachToChunk(chunk))
        {
            WLOG_WARN("edit: no terrain chunk under (%.2f %.2f %.2f)", at[0], at[1], at[2]);
            return false;
        }

        static const auto s_spawn = wxl::game::Native<ooddad::SpawnFromMDDFFn>(ooddad::kSpawnFromMDDF);
        static const auto s_purge = wxl::game::Native<ooddad::DoodadPurgeFn>(ooddad::kDoodadPurge);
        float origin[3] = { ooddad::kMddfTileOriginX, ooddad::kMddfTileOriginY, 0.0f };
        alignas(4) unsigned char mddf[0x24] = {};
        *reinterpret_cast<uint32_t*>(mddf + kMddfNameId) = 0;
        *reinterpret_cast<uint32_t*>(mddf + kMddfUniqueId) = g_uniqueId++;
        *reinterpret_cast<float*>(mddf + ooddad::kMddfPosX) = origin[1] - at[1];
        *reinterpret_cast<float*>(mddf + ooddad::kMddfPosY) = at[2] - origin[2];
        *reinterpret_cast<float*>(mddf + ooddad::kMddfPosZ) = origin[0] - at[0];
        *reinterpret_cast<uint16_t*>(mddf + kMddfScale) = 1024;
        *reinterpret_cast<uint16_t*>(mddf + kMddfFlags) = 1;

        void* spawned = s_spawn(path.c_str(), mddf, origin);
        if (!spawned)
        {
            WLOG_WARN("edit: the client could not make %s", path.c_str());
            return false;
        }
        if (!AttachToChunk(spawned, chunk))
        {
            s_purge(spawned);      // no reference yet: its purge is safe
            WLOG_WARN("edit: chunk attachment failed for %s", path.c_str());
            return false;
        }
        object_ = spawned;
        pending_ = true;
        frames_ = 0;
        Compose(t, matrix_);
        WLOG_INFO("edit: %s at (%.2f %.2f %.2f) scale %.2f", path.c_str(), at[0], at[1], at[2], t.scale);
        return true;
    }

    void Editor::Hide()
    {
        // A doodad held by a chunk has no safe destructor: it is shrunk out of sight, and the
        // chunk's unload reclaims it.
        float m[16];
        if (!object_ || !doodad::IsValid(object_) || !doodad::WorldMatrix(object_, m)) return;
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                m[row * 4 + col] *= 0.001f;
        m[14] = -10000.0f;
        doodad::SetWorldMatrix(object_, m);
    }

    void Editor::End()
    {
        if (!object_) return;
        Hide();
        object_ = nullptr;
        pending_ = false;
    }

    Transform Editor::Current() const
    {
        return Decompose(matrix_);
    }

    void Editor::Set(const Transform& t)
    {
        Compose(t, matrix_);
        if (Ready()) doodad::SetWorldMatrix(object_, matrix_);
    }

    void Editor::Tick()
    {
        if (!object_) return;
        if (!doodad::IsValid(object_))
        {
            // its chunk unloaded under it (the player left the map)
            object_ = nullptr;
            pending_ = false;
            return;
        }
        if (pending_)
        {
            float ignored[16];
            if (doodad::WorldMatrix(object_, ignored))
            {
                pending_ = false;
                doodad::SetWorldMatrix(object_, matrix_);
            }
            else if (++frames_ > kLoadFrames)
            {
                WLOG_WARN("edit: the model never loaded");
                End();
            }
            return;
        }
        doodad::SetWorldMatrix(object_, matrix_);
    }

    bool Editor::DrawGizmo()
    {
        if (!Ready()) return false;
        const ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        const ImGuizmo::OPERATION op = mode_ == GizmoMode::Rotate ? ImGuizmo::ROTATE
                                     : mode_ == GizmoMode::Scale ? ImGuizmo::SCALEU : ImGuizmo::TRANSLATE;
        // moved along the world's axes, turned and sized around its own
        const ImGuizmo::MODE space = mode_ == GizmoMode::Move ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        float m[16];
        std::memcpy(m, matrix_, sizeof m);
        ImGuizmo::Manipulate(camera::GetView(), camera::GetProjection(), op, space, m);
        if (ImGuizmo::IsUsing())
        {
            // the scale stays uniform and sane, whatever the handle did
            Transform t = Decompose(m);
            t.scale = (std::max)(0.05f, (std::min)(t.scale, 20.0f));
            Compose(t, matrix_);
            doodad::SetWorldMatrix(object_, matrix_);
        }
        return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    }
}
