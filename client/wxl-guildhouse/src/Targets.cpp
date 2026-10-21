// The placed objects' outlines and picking. See Targets.hpp.
// Copyright (C) 2026 mod-guild-level
// GPL-3.0-or-later.

#include "Targets.hpp"

#include "game/Camera.hpp"

#include <algorithm>
#include <cmath>

#include "imgui.h"

namespace wxl_guildhouse
{
    namespace
    {
        namespace camera = wxl::game::camera;

        const ImU32 kOutline = IM_COL32(60, 150, 255, 230);
        const ImU32 kHoverFill = IM_COL32(130, 200, 255, 70);
        const ImU32 kHoverOutline = IM_COL32(150, 215, 255, 255);

        float Cross(const ImVec2& o, const ImVec2& a, const ImVec2& b)
        {
            return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
        }

        // Andrew's monotone chain: the hull of the projected corners, counter-clockwise.
        std::vector<ImVec2> Hull(std::vector<ImVec2> p)
        {
            std::sort(p.begin(), p.end(), [](const ImVec2& a, const ImVec2& b)
            { return a.x < b.x || (a.x == b.x && a.y < b.y); });
            std::vector<ImVec2> h(p.size() * 2);
            size_t k = 0;
            for (size_t i = 0; i < p.size(); ++i)
            {
                while (k >= 2 && Cross(h[k - 2], h[k - 1], p[i]) <= 0) --k;
                h[k++] = p[i];
            }
            for (size_t i = p.size() - 1, t = k + 1; i > 0; --i)
            {
                while (k >= t && Cross(h[k - 2], h[k - 1], p[i - 1]) <= 0) --k;
                h[k++] = p[i - 1];
            }
            h.resize(k > 1 ? k - 1 : k);
            return h;
        }

        bool Inside(const std::vector<ImVec2>& hull, const ImVec2& q)
        {
            if (hull.size() < 3) return false;
            for (size_t i = 0; i < hull.size(); ++i)
                if (Cross(hull[i], hull[(i + 1) % hull.size()], q) < 0) return false;
            return true;
        }
    }

    Targets& Targets::Instance()
    {
        static Targets s;
        return s;
    }

    void Targets::Draw()
    {
        hovered_ = 0;
        if (!shown_) return;
        const float* vp = camera::GetViewProj();
        const ImGuiIO& io = ImGui::GetIO();
        const float W = io.DisplaySize.x, H = io.DisplaySize.y;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        struct Seen { const Target* t; std::vector<ImVec2> hull; float depth; };
        std::vector<Seen> seen;
        for (const Target& t : targets_)
        {
            std::vector<ImVec2> corners;
            float depth = 1e18f;
            bool behind = false;
            for (int k = 0; k < 8 && !behind; ++k)
            {
                const float l[3] = { (k & 1) ? t.hi[0] : t.lo[0], (k & 2) ? t.hi[1] : t.lo[1],
                                     (k & 4) ? t.hi[2] : t.lo[2] };
                const float* m = t.matrix;
                const float w[3] = { l[0] * m[0] + l[1] * m[4] + l[2] * m[8] + m[12],
                                     l[0] * m[1] + l[1] * m[5] + l[2] * m[9] + m[13],
                                     l[0] * m[2] + l[1] * m[6] + l[2] * m[10] + m[14] };
                float clip[4];
                for (int j = 0; j < 4; ++j)
                    clip[j] = w[0] * vp[j] + w[1] * vp[4 + j] + w[2] * vp[8 + j] + vp[12 + j];
                if (clip[3] <= 0.05f) { behind = true; break; }
                corners.push_back(ImVec2((clip[0] / clip[3] * 0.5f + 0.5f) * W,
                                         (0.5f - clip[1] / clip[3] * 0.5f) * H));
                depth = (std::min)(depth, clip[3]);
            }
            if (behind) continue;
            seen.push_back({ &t, Hull(corners), depth });
        }

        // the nearest under the cursor
        const Seen* hover = nullptr;
        for (const Seen& s : seen)
            if (Inside(s.hull, io.MousePos) && (!hover || s.depth < hover->depth))
                hover = &s;
        if (hover) hovered_ = hover->t->id;

        for (const Seen& s : seen)
        {
            if (s.hull.size() < 3) continue;
            if (&s == hover)
            {
                dl->AddConvexPolyFilled(s.hull.data(), static_cast<int>(s.hull.size()), kHoverFill);
                dl->AddPolyline(s.hull.data(), static_cast<int>(s.hull.size()), kHoverOutline, ImDrawFlags_Closed, 2.5f);
            }
            else
                dl->AddPolyline(s.hull.data(), static_cast<int>(s.hull.size()), kOutline, ImDrawFlags_Closed, 2.0f);
        }
    }
}
