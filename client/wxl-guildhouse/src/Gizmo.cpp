// The editor's turning and sizing handles. See Gizmo.hpp.
// Copyright (C) 2026 mod-guild-level
// GPL-3.0-or-later.

#include "Gizmo.hpp"

#include "ViewProj.hpp"

#include "game/Camera.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "imgui.h"

namespace wxl_guildhouse
{
    namespace
    {
        constexpr float kPi = 3.14159265f;
        constexpr int kSegments = 72;
        constexpr float kGrab = 9.0f;              // pixels from a ring or handle that grab it
        constexpr float kRingShare = 0.12f;        // a ring's radius, a share of its distance to the camera
        constexpr float kHandleShare = 0.10f;
        const ImU32 kAxisColour[3] = { IM_COL32(230, 60, 60, 255), IM_COL32(70, 200, 70, 255), IM_COL32(70, 120, 240, 255) };
        const ImU32 kHot = IM_COL32(255, 210, 40, 255);

        struct Screen
        {
            float vp[16];
            float W, H;
            Screen()
            {
                ViewProj(vp);
                const ImGuiIO& io = ImGui::GetIO();
                W = io.DisplaySize.x;
                H = io.DisplaySize.y;
            }
            bool Project(const float p[3], ImVec2& out) const
            {
                float c[4];
                for (int j = 0; j < 4; ++j)
                    c[j] = p[0] * vp[j] + p[1] * vp[4 + j] + p[2] * vp[8 + j] + vp[12 + j];
                if (c[3] <= 0.05f) return false;
                out = ImVec2((c[0] / c[3] * 0.5f + 0.5f) * W, (0.5f - c[1] / c[3] * 0.5f) * H);
                return true;
            }
        };

        float Distance(const ImVec2& p, const ImVec2& a, const ImVec2& b)
        {
            const float dx = b.x - a.x, dy = b.y - a.y;
            const float len = dx * dx + dy * dy;
            float t = len > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len : 0.0f;
            t = (std::max)(0.0f, (std::min)(1.0f, t));
            const float x = a.x + t * dx - p.x, y = a.y + t * dy - p.y;
            return std::sqrt(x * x + y * y);
        }

        // The object's size on the screen's scale: a share of its distance to the camera.
        float Reach(const float centre[3], float share)
        {
            float eye[3];
            wxl::game::camera::GetPosition(eye);
            const float dx = centre[0] - eye[0], dy = centre[1] - eye[1], dz = centre[2] - eye[2];
            return (std::max)(0.3f, std::sqrt(dx * dx + dy * dy + dz * dz) * share);
        }

        // A point of a ring: around the axis, at angle a.
        void RingPoint(const float c[3], int axis, float r, float a, float out[3])
        {
            const int u = (axis + 1) % 3, v = (axis + 2) % 3;
            out[0] = c[0]; out[1] = c[1]; out[2] = c[2];
            out[u] += r * std::cos(a);
            out[v] += r * std::sin(a);
        }

        // Turns the matrix's three rows about a world axis by angle a, around its own position.
        void TurnRows(const float in[16], int axis, float a, float out[16])
        {
            std::memcpy(out, in, sizeof(float) * 16);
            const int u = (axis + 1) % 3, v = (axis + 2) % 3;
            const float c = std::cos(a), s = std::sin(a);
            for (int row = 0; row < 3; ++row)
            {
                const float pu = in[row * 4 + u], pv = in[row * 4 + v];
                out[row * 4 + u] = pu * c - pv * s;
                out[row * 4 + v] = pu * s + pv * c;
            }
        }

        float AngleAround(const ImVec2& centre, const ImVec2& p)
        {
            return std::atan2(p.y - centre.y, p.x - centre.x);
        }
    }

    bool Gizmo::Rotate(float m[16])
    {
        const Screen screen;
        const float c[3] = { m[12], m[13], m[14] };
        ImVec2 centre;
        if (!screen.Project(c, centre)) return false;
        const float r = Reach(c, kRingShare);
        const ImGuiIO& io = ImGui::GetIO();
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        // the rings on the screen, and the nearest to the cursor
        ImVec2 pts[3][kSegments + 1];
        bool ok[3][kSegments + 1];
        int closest = -1;
        float best = kGrab;
        for (int axis = 0; axis < 3; ++axis)
            for (int k = 0; k <= kSegments; ++k)
            {
                float p[3];
                RingPoint(c, axis, r, 2 * kPi * k / kSegments, p);
                ok[axis][k] = screen.Project(p, pts[axis][k]);
                if (k && ok[axis][k] && ok[axis][k - 1])
                {
                    const float d = Distance(io.MousePos, pts[axis][k - 1], pts[axis][k]);
                    if (d < best) { best = d; closest = axis; }
                }
            }

        if (axis_ < 0 && closest >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            axis_ = closest;
            std::memcpy(start_, m, sizeof start_);
            startAngle_ = AngleAround(centre, io.MousePos);
            // which way the ring's angle runs on the screen: two of its points a little apart
            float p0[3], p1[3];
            RingPoint(c, axis_, r, 0.0f, p0);
            RingPoint(c, axis_, r, 0.2f, p1);
            ImVec2 s0, s1;
            sense_ = 1.0f;
            if (screen.Project(p0, s0) && screen.Project(p1, s1))
            {
                const float cross = (s0.x - centre.x) * (s1.y - centre.y) - (s0.y - centre.y) * (s1.x - centre.x);
                sense_ = cross >= 0 ? 1.0f : -1.0f;
            }
        }
        if (axis_ >= 0)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                axis_ = -1;
            else
            {
                float delta = AngleAround(centre, io.MousePos) - startAngle_;
                TurnRows(start_, axis_, delta * sense_, m);
            }
        }

        const int lit = axis_ >= 0 ? axis_ : closest;
        for (int axis = 0; axis < 3; ++axis)
            for (int k = 1; k <= kSegments; ++k)
                if (ok[axis][k] && ok[axis][k - 1])
                    dl->AddLine(pts[axis][k - 1], pts[axis][k], axis == lit ? kHot : kAxisColour[axis],
                                axis == lit ? 3.0f : 2.0f);
        return closest >= 0 || axis_ >= 0;
    }

    bool Gizmo::Scale(float m[16], float minScale, float maxScale)
    {
        const Screen screen;
        const float c[3] = { m[12], m[13], m[14] };
        ImVec2 centre;
        if (!screen.Project(c, centre)) return false;
        const float len = Reach(c, kHandleShare);
        const ImGuiIO& io = ImGui::GetIO();
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        // the handles at the ends of the object's own axes
        ImVec2 ends[3];
        bool ok[3];
        int closest = -1;
        float best = kGrab + 4.0f;
        for (int axis = 0; axis < 3; ++axis)
        {
            const float* row = &m[axis * 4];
            const float n = std::sqrt(row[0] * row[0] + row[1] * row[1] + row[2] * row[2]);
            const float p[3] = { c[0] + row[0] / n * len, c[1] + row[1] / n * len, c[2] + row[2] / n * len };
            ok[axis] = n > 1e-6f && screen.Project(p, ends[axis]);
            if (!ok[axis]) continue;
            const float dx = io.MousePos.x - ends[axis].x, dy = io.MousePos.y - ends[axis].y;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d < best) { best = d; closest = axis; }
        }

        if (axis_ < 0 && closest >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            axis_ = closest;
            std::memcpy(start_, m, sizeof start_);
            const float dx = io.MousePos.x - centre.x, dy = io.MousePos.y - centre.y;
            startDistance_ = (std::max)(4.0f, std::sqrt(dx * dx + dy * dy));
        }
        if (axis_ >= 0)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                axis_ = -1;
            else
            {
                const float dx = io.MousePos.x - centre.x, dy = io.MousePos.y - centre.y;
                const float ratio = std::sqrt(dx * dx + dy * dy) / startDistance_;
                const float s0 = std::sqrt(start_[0] * start_[0] + start_[1] * start_[1] + start_[2] * start_[2]);
                const float s1 = (std::max)(minScale, (std::min)(maxScale, s0 * ratio));
                const float k = s0 > 1e-6f ? s1 / s0 : 1.0f;
                std::memcpy(m, start_, sizeof(float) * 16);
                for (int row = 0; row < 3; ++row)
                    for (int col = 0; col < 3; ++col)
                        m[row * 4 + col] *= k;
            }
        }

        const int lit = axis_ >= 0 ? axis_ : closest;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (!ok[axis]) continue;
            const ImU32 col = axis == lit ? kHot : kAxisColour[axis];
            dl->AddLine(centre, ends[axis], col, 2.0f);
            dl->AddRectFilled(ImVec2(ends[axis].x - 6, ends[axis].y - 6), ImVec2(ends[axis].x + 6, ends[axis].y + 6), col);
        }
        dl->AddRectFilled(ImVec2(centre.x - 4, centre.y - 4), ImVec2(centre.x + 4, centre.y + 4), IM_COL32(255, 255, 255, 230));
        return closest >= 0 || axis_ >= 0;
    }
}
