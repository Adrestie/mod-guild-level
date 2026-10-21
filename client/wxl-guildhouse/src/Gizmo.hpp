// The editor's own handles for turning and sizing (the moving arrows are ImGuizmo's): drawn and
// hit-tested on the screen from the object's world position, with the same view-projection as the
// outlines, so that they stay on the object whatever the camera does.
//  * turning: three rings around the object, one per axis (the world's, or the object's own); a ring dragged turns the object
//    about its axis by the angle the cursor sweeps around the object's centre;
//  * sizing: three lines from the centre along the object's own axes, a handle at each end; a
//    handle dragged sizes the whole object by how much farther from the centre the cursor goes.
// Copyright (C) 2026 mod-guild-level
// GPL-3.0-or-later.

#pragma once

namespace wxl_guildhouse
{
    class Gizmo
    {
    public:
        /// Inside the overlay's ImGui frame. matrix: the object's world matrix, changed while a ring
        /// is dragged. Returns true while the cursor is on a handle or drags one.
        /// local: the rings about the object's own axes, else the world's.
        bool Rotate(float matrix[16], bool local);
        /// The same for the size; the scale kept within [minScale, maxScale].
        bool Scale(float matrix[16], float minScale, float maxScale);
        /// A drag is going on.
        bool Dragging() const { return axis_ >= 0; }
        void Reset() { axis_ = -1; }

    private:
        int axis_ = -1;              // the ring or handle dragged
        float start_[16] = {};       // the matrix when the drag began
        float startAngle_ = 0.0f;    // the cursor's angle around the centre then
        float startDistance_ = 0.0f; // its distance from the centre then
        float sense_ = 1.0f;         // which way the ring's angle turns on the screen
    };
}
