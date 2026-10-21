// The placed objects the member can take in hand -- those within 30 yards of his character, in his
// room (no wall of the hall between them) -- outlined while the Customization mode is on: the
// object's whole silhouette, its textures' cut-outs (alpha-keyed leaves, grids...) taken out as the
// game does, traced in blue; the one under the cursor filled in a lighter blue; a click on it picks it.
//
// Each object is drawn, every frame, into a mask of its own on the screen: its triangles, each
// pixel kept when its texture's alpha passes the material's test. The outline is the mask's outer
// border, the fill the mask itself, both shown through a texture of the overlay.
//
// The interface hands the objects over (GuildHouse_AddTarget, from the server's list) with their
// model; the model's geometry, materials and textures' alpha are read from the game's files once.
// Copyright (C) 2026 mod-guild-level
// GPL-3.0-or-later.

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace wxl_guildhouse
{
    /// A texture's alpha, at a size small enough to sample fast.
    struct AlphaMap
    {
        int w = 0, h = 0;
        std::vector<uint8_t> a;
    };

    /// A model as its outline needs it.
    struct Mesh
    {
        std::vector<float> pos;          // x, y, z per vertex
        std::vector<float> uv;           // u, v per vertex
        std::vector<uint32_t> tris;      // three vertex indices per triangle
        std::vector<uint16_t> partOf;    // per triangle
        struct Part { const AlphaMap* alpha = nullptr; bool clip = false; };
        std::vector<Part> parts;
    };

    struct Target
    {
        uint32_t id = 0;
        float matrix[16] = {};           // world matrix (row vectors)
        float lo[3] = {};                // the model's bounds
        float hi[3] = {};
        const Mesh* mesh = nullptr;
    };

    class Targets
    {
    public:
        static Targets& Instance();

        void Clear() { targets_.clear(); hovered_ = 0; }
        /// model: the game path of its .m2 (read once, then kept).
        void Add(Target t, const std::string& model);
        void SetShown(bool shown) { shown_ = shown; if (!shown) { hovered_ = 0; armed_ = 0; } }
        bool Shown() const { return shown_ && !targets_.empty(); }

        /// Inside the overlay's ImGui frame: the outlines, and which one the cursor is on.
        void Draw(IDirect3DDevice9* device);
        /// The cursor is on one: a left click is the mode's, not the game's.
        bool Hovered() const { return shown_ && hovered_ != 0; }
        /// The window's click on the hovered one; the interface collects it (TakePicked).
        void Pick() { if (Hovered()) picked_ = hovered_; }
        uint32_t TakePicked() { const uint32_t p = picked_; picked_ = 0; return p; }
        /// A right click on the hovered one: the first marks it (red), the second on the same one
        /// gives it back to the stock (the interface collects it: TakeRemoved). Elsewhere: unmarked.
        void RightClick();
        void Disarm() { armed_ = 0; }
        uint32_t TakeRemoved() { const uint32_t r = removed_; removed_ = 0; return r; }
        /// The model at that world matrix covers that point of the screen (its triangles).
        bool Covers(const std::string& model, const float matrix[16], float x, float y);

        /// The device is going away (D3D reset): the overlay's textures with it.
        void ReleaseTextures();

    private:
        Targets() = default;
        const Mesh* MeshOf(const std::string& model);
        const AlphaMap* AlphaOf(const std::string& texture);
        IDirect3DTexture9* Texture(IDirect3DDevice9* device, size_t slot, int w, int h);

        std::vector<Target> targets_;
        std::map<std::string, std::unique_ptr<Mesh>> meshes_;          // null: could not be read
        std::map<std::string, std::unique_ptr<AlphaMap>> alphas_;      // null: opaque
        struct Slot { IDirect3DTexture9* tex = nullptr; int w = 0, h = 0; };
        std::vector<Slot> slots_;
        bool shown_ = false;
        uint32_t hovered_ = 0;
        uint32_t picked_ = 0;
        uint32_t armed_ = 0;
        uint32_t removed_ = 0;
    };
}
