// The guild house editor: one local preview model at a time, held by the terrain chunk under it
// (the hall's map has a flat terrain deep below it for that), moved by an ImGuizmo gizmo drawn over
// the game. The interface (FrameXML/Lua, mod-guild-level's GuildHouse_Client.lua) drives it through
// the script functions of Script.cpp and tells the server only when an edit starts and ends.
// Copyright (C) 2026 WarcraftXL, mod-guild-level
// GPL-3.0-or-later. The spawn and chunk attachment are wxl-housing's (Placement.cpp).

#pragma once

#include <string>

#include "Gizmo.hpp"

namespace wxl_guildhouse
{
    /// A placement: world position, rotation (unit quaternion x, y, z, w) and uniform scale.
    struct Transform
    {
        float pos[3] = { 0.0f, 0.0f, 0.0f };
        float rot[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float scale = 1.0f;
    };

    enum class GizmoMode { Move, Rotate, Scale };

    class Editor
    {
    public:
        static Editor& Instance();

        /// Shows the model at the transform and selects it, ending any edit in progress.
        bool Begin(const std::string& path, const Transform& t);
        /// Takes the preview away.
        void End();

        bool Editing() const { return object_ != nullptr; }
        /// The model is loaded and follows its transform.
        bool Ready() const { return object_ != nullptr && !pending_; }

        Transform Current() const;
        void Set(const Transform& t);
        void SetMode(GizmoMode mode) { mode_ = mode; }
        /// Turning about the object's own axes, or the world's.
        void SetLocal(bool local) { local_ = local; }
        GizmoMode Mode() const { return mode_; }
        /// No gizmo while the object follows the cursor: it would be under the cursor, and take the
        /// click that sets the object down.
        void ShowGizmo(bool shown) { gizmo_ = shown; if (!shown) handles_.Reset(); }
        bool GizmoShown() const { return gizmo_; }
        /// The model's path, while editing.
        const std::string& Model() const { return path_; }
        const float* Matrix() const { return matrix_; }

        /// Out of the way of a ray for a moment (Script.cpp's traces), and back.
        void Stash();
        void Unstash();

        /// Per frame, in EndScene: promotes a loaded model, keeps it on its transform.
        void Tick();
        /// Per frame, inside the overlay's ImGui frame: the gizmo. Returns true while the cursor is
        /// over it or drags it (the click is then the gizmo's, not the game's).
        bool DrawGizmo();

    private:
        Editor() = default;
        void Hide();
        bool Spawn();                    // the model at matrix_, held by the chunk under it

        std::string path_;
        void* object_ = nullptr;
        void* chunk_ = nullptr;
        bool pending_ = false;
        unsigned frames_ = 0;
        float matrix_[16] = {};          // the wanted world matrix (row vectors, translation last)
        GizmoMode mode_ = GizmoMode::Move;
        bool gizmo_ = true;
        bool local_ = false;
        Gizmo handles_;                  // turning and sizing
    };

    /// Row-vector world matrix of a transform, and back.
    void Compose(const Transform& t, float m[16]);
    Transform Decompose(const float m[16]);
}
