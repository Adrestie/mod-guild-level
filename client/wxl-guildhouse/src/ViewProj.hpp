// The view-projection the gizmo draws with: the camera's view times its projection (row vectors). The
// engine's own combined matrix is not always the one of the frame being drawn (another pass may
// have left its own there), which put the outlines off the objects.
// Copyright (C) 2026 mod-guild-level
// GPL-3.0-or-later.

#pragma once

#include "game/Camera.hpp"

namespace wxl_guildhouse
{
    inline void ViewProj(float out[16])
    {
        const float* v = wxl::game::camera::GetView();
        const float* p = wxl::game::camera::GetProjection();
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) sum += v[r * 4 + k] * p[k * 4 + c];
                out[r * 4 + c] = sum;
            }
    }
}
