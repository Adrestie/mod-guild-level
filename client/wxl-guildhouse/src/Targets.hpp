// The placed objects the member can take in hand: their outline (blue) while the Customization mode
// is on, a lighter overlay on the one under the cursor, and the click that picks it. The interface
// hands them over (GuildHouse_SetTargets, from the server's list) with their model's bounds; the
// outline is their box as the screen sees it.
// Copyright (C) 2026 mod-guild-level
// GPL-3.0-or-later.

#pragma once

#include <cstdint>
#include <vector>

namespace wxl_guildhouse
{
    struct Target
    {
        uint32_t id = 0;
        float matrix[16] = {};           // world matrix (row vectors)
        float lo[3] = {};                // the model's bounds
        float hi[3] = {};
    };

    class Targets
    {
    public:
        static Targets& Instance();

        void Clear() { targets_.clear(); hovered_ = 0; }
        void Add(const Target& t) { targets_.push_back(t); }
        void SetShown(bool shown) { shown_ = shown; if (!shown) hovered_ = 0; }
        bool Shown() const { return shown_ && !targets_.empty(); }

        /// Inside the overlay's ImGui frame: the outlines, and which one the cursor is on.
        void Draw();
        /// The cursor is on one: a left click is the mode's, not the game's.
        bool Hovered() const { return shown_ && hovered_ != 0; }
        /// The window's click on the hovered one; the interface collects it (TakePicked).
        void Pick() { if (Hovered()) picked_ = hovered_; }
        uint32_t TakePicked() { const uint32_t p = picked_; picked_ = 0; return p; }

    private:
        Targets() = default;
        std::vector<Target> targets_;
        bool shown_ = false;
        uint32_t hovered_ = 0;
        uint32_t picked_ = 0;
    };
}
