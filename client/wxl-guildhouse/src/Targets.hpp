// The placed objects the member can take in hand: their silhouette outlined in blue while the
// Customization mode is on, the faces of the one under the cursor filled in a lighter blue, and the
// click that picks it. The interface hands them over (GuildHouse_AddTarget, from the server's list)
// with their model; the model's geometry (its .m2 and first .skin) is read from the game's files once.
// Copyright (C) 2026 mod-guild-level
// GPL-3.0-or-later.

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wxl_guildhouse
{
    /// A model's geometry: its vertices, its triangles, and each edge's two triangles (-1: none).
    struct Mesh
    {
        std::vector<float> pos;          // x, y, z per vertex
        std::vector<uint32_t> tris;      // three vertex indices per triangle
        struct Edge { uint32_t a, b; int32_t f0, f1; };
        std::vector<Edge> edges;
    };

    struct Target
    {
        uint32_t id = 0;
        float matrix[16] = {};           // world matrix (row vectors)
        float lo[3] = {};                // the model's bounds: its outline when its geometry is missing
        float hi[3] = {};
        const Mesh* mesh = nullptr;
    };

    class Targets
    {
    public:
        static Targets& Instance();

        void Clear() { targets_.clear(); hovered_ = 0; }
        /// model: the game path of its .m2 (its geometry is read once, then kept).
        void Add(Target t, const std::string& model);
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
        const Mesh* MeshOf(const std::string& model);

        std::vector<Target> targets_;
        std::map<std::string, std::unique_ptr<Mesh>> meshes_;   // null: could not be read
        bool shown_ = false;
        uint32_t hovered_ = 0;
        uint32_t picked_ = 0;
    };
}
