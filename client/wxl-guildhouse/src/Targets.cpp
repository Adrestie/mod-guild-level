// The placed objects' outlines and picking. See Targets.hpp.
// Copyright (C) 2026 mod-guild-level
// GPL-3.0-or-later.

#include "Targets.hpp"

#include "ExtensionApi.hpp"
#include "ViewProj.hpp"

#include "game/Io.hpp"
#include "game/Pick.hpp"
#include "game/World.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "imgui.h"

namespace wxl_guildhouse
{
    namespace
    {
        namespace io = wxl::game::io;
        namespace world = wxl::game::world;

        constexpr float kReach = 15.0f;             // yards from the character
        constexpr float kEye = 1.6f;                // the height his eyes look from
        constexpr uint32_t kWalls = 0x01000120;     // terrain and map objects, never a model

        // The character's position, false out of the world.
        bool PlayerPosition(float out[3])
        {
            const unsigned long long guid = world::ActivePlayerGuid();
            void* player = guid ? world::ResolveObject(guid, world::kTypeMaskPlayer) : nullptr;
            if (!player) return false;
            world::UnitPosition(player, out);
            return true;
        }

        // In his room: near enough, and no wall of the hall between his eyes and the object (its
        // middle, or its top).
        bool InHisRoom(const float eye[3], const Target& t)
        {
            const float* m = t.matrix;
            const float mid[3] = { (t.lo[0] + t.hi[0]) / 2, (t.lo[1] + t.hi[1]) / 2, (t.lo[2] + t.hi[2]) / 2 };
            const float top[3] = { mid[0], mid[1], t.hi[2] * 0.9f };
            for (const float* l : { mid, top })
            {
                const float w[3] = { l[0] * m[0] + l[1] * m[4] + l[2] * m[8] + m[12],
                                     l[0] * m[1] + l[1] * m[5] + l[2] * m[9] + m[13],
                                     l[0] * m[2] + l[1] * m[6] + l[2] * m[10] + m[14] };
                const float dx = w[0] - eye[0], dy = w[1] - eye[1], dz = w[2] - eye[2];
                if (dx * dx + dy * dy + dz * dz > kReach * kReach) continue;
                world::WorldHit hit;
                if (!world::TraceLine(eye, w, hit, kWalls) || hit.t > 0.97f)
                    return true;
            }
            return false;
        }

        const ImU32 kOutline = IM_COL32(40, 130, 255, 255);
        const ImU32 kHoverFill = IM_COL32(140, 210, 255, 90);
        const ImU32 kHoverOutline = IM_COL32(150, 215, 255, 255);

        bool ReadFile(const std::string& path, std::vector<uint8_t>& out)
        {
            void* handle = nullptr;
            if (!io::FileOpen(path.c_str(), 0, &handle) || !handle) return false;
            const uint32_t size = io::FileSize(handle, nullptr);
            out.resize(size);
            uint32_t read = 0;
            const bool ok = size && io::FileRead(handle, out.data(), size, &read) && read == size;
            io::FileClose(handle);
            return ok;
        }

        template<typename T> bool Get(const std::vector<uint8_t>& d, size_t at, T& v)
        {
            if (at + sizeof(T) > d.size()) return false;
            std::memcpy(&v, d.data() + at, sizeof(T));
            return true;
        }

        // The model's vertices (MD20, inside the modern chunked file) and its first view's triangles.
        std::unique_ptr<Mesh> Load(const std::string& model)
        {
            std::vector<uint8_t> m2, skin;
            if (!ReadFile(model, m2)) return nullptr;
            const std::string base = model.substr(0, model.size() - 3);
            if (!ReadFile(base + "00.skin", skin)) return nullptr;

            size_t md20 = 0;
            for (size_t i = 0; i + 4 <= m2.size(); ++i)
                if (std::memcmp(m2.data() + i, "MD20", 4) == 0) { md20 = i; break; }
            uint32_t nVerts = 0, ofsVerts = 0;
            if (!Get(m2, md20 + 0x3C, nVerts) || !Get(m2, md20 + 0x40, ofsVerts)) return nullptr;
            if (md20 + ofsVerts + size_t(nVerts) * 48 > m2.size()) return nullptr;

            // SKIN: the view's vertices (indices into the model's), then its triangles (into those)
            uint32_t nLookup = 0, ofsLookup = 0, nIndices = 0, ofsIndices = 0;
            if (skin.size() < 20 || std::memcmp(skin.data(), "SKIN", 4) != 0 ||
                !Get(skin, 4, nLookup) || !Get(skin, 8, ofsLookup) || !Get(skin, 12, nIndices) ||
                !Get(skin, 16, ofsIndices))
                return nullptr;
            if (ofsLookup + size_t(nLookup) * 2 > skin.size() || ofsIndices + size_t(nIndices) * 2 > skin.size())
                return nullptr;

            auto mesh = std::make_unique<Mesh>();
            mesh->pos.resize(size_t(nVerts) * 3);
            for (uint32_t v = 0; v < nVerts; ++v)
                std::memcpy(&mesh->pos[v * 3], m2.data() + md20 + ofsVerts + v * 48, 12);
            mesh->tris.reserve(nIndices);
            for (uint32_t i = 0; i + 2 < nIndices; i += 3)
            {
                uint32_t t[3];
                bool ok = true;
                for (int k = 0; k < 3; ++k)
                {
                    uint16_t li = 0, vi = 0;
                    Get(skin, ofsIndices + (i + k) * 2, li);
                    if (li >= nLookup) { ok = false; break; }
                    Get(skin, ofsLookup + li * 2, vi);
                    if (vi >= nVerts) { ok = false; break; }
                    t[k] = vi;
                }
                if (ok) mesh->tris.insert(mesh->tris.end(), t, t + 3);
            }

            // each edge and the (up to) two triangles on either side: welded by position, as the
            // model's vertices are split along its texture seams
            std::unordered_map<uint64_t, uint32_t> weld;
            std::vector<uint32_t> canon(nVerts);
            for (uint32_t v = 0; v < nVerts; ++v)
            {
                const float* p = &mesh->pos[v * 3];
                const uint64_t key = (uint64_t(int32_t(std::lround(p[0] * 1000.0f)) & 0x1FFFFF) << 42) ^
                                     (uint64_t(int32_t(std::lround(p[1] * 1000.0f)) & 0x1FFFFF) << 21) ^
                                     uint64_t(int32_t(std::lround(p[2] * 1000.0f)) & 0x1FFFFF);
                canon[v] = weld.emplace(key, v).first->second;
            }
            std::unordered_map<uint64_t, size_t> edgeOf;
            const size_t nTris = mesh->tris.size() / 3;
            for (size_t f = 0; f < nTris; ++f)
                for (int k = 0; k < 3; ++k)
                {
                    uint32_t a = canon[mesh->tris[f * 3 + k]], b = canon[mesh->tris[f * 3 + (k + 1) % 3]];
                    if (a == b) continue;
                    if (a > b) std::swap(a, b);
                    const uint64_t key = (uint64_t(a) << 32) | b;
                    auto it = edgeOf.find(key);
                    if (it == edgeOf.end())
                    {
                        edgeOf.emplace(key, mesh->edges.size());
                        mesh->edges.push_back({ a, b, int32_t(f), -1 });
                    }
                    else if (mesh->edges[it->second].f1 < 0)
                        mesh->edges[it->second].f1 = int32_t(f);
                }
            return mesh;
        }

        float Cross(const ImVec2& o, const ImVec2& a, const ImVec2& b)
        {
            return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
        }

        bool InTriangle(const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& q)
        {
            const float d1 = Cross(a, b, q), d2 = Cross(b, c, q), d3 = Cross(c, a, q);
            const bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
            return !(neg && pos);
        }
    }

    Targets& Targets::Instance()
    {
        static Targets s;
        return s;
    }

    const Mesh* Targets::MeshOf(const std::string& model)
    {
        auto it = meshes_.find(model);
        if (it == meshes_.end())
        {
            std::unique_ptr<Mesh> mesh = Load(model);
            if (!mesh) WLOG_WARN("outline: no geometry read from %s", model.c_str());
            it = meshes_.emplace(model, std::move(mesh)).first;
        }
        return it->second.get();
    }

    void Targets::Add(Target t, const std::string& model)
    {
        if (!model.empty()) t.mesh = MeshOf(model);
        targets_.push_back(t);
    }

    void Targets::Draw()
    {
        hovered_ = 0;
        if (!shown_) return;
        float vp[16];
        ViewProj(vp);
        const ImGuiIO& io = ImGui::GetIO();
        const float W = io.DisplaySize.x, H = io.DisplaySize.y;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        // A point of the model on the screen; false behind the camera.
        auto project = [&](const float m[16], const float* l, ImVec2& out, float& w) -> bool
        {
            const float x = l[0] * m[0] + l[1] * m[4] + l[2] * m[8] + m[12];
            const float y = l[0] * m[1] + l[1] * m[5] + l[2] * m[9] + m[13];
            const float z = l[0] * m[2] + l[1] * m[6] + l[2] * m[10] + m[14];
            float clip[4];
            for (int j = 0; j < 4; ++j)
                clip[j] = x * vp[j] + y * vp[4 + j] + z * vp[8 + j] + vp[12 + j];
            if (clip[3] <= 0.05f) return false;
            out = ImVec2((clip[0] / clip[3] * 0.5f + 0.5f) * W, (0.5f - clip[1] / clip[3] * 0.5f) * H);
            w = clip[3];
            return true;
        };

        struct Seen
        {
            const Target* t;
            std::vector<ImVec2> screen;          // per vertex
            std::vector<uint8_t> front;          // per triangle
            float depth;
        };
        float eye[3];
        if (!PlayerPosition(eye)) return;
        eye[2] += kEye;

        std::vector<Seen> seen;
        seen.reserve(targets_.size());
        for (const Target& t : targets_)
        {
            if (!InHisRoom(eye, t)) continue;
            Seen s{ &t, {}, {}, 1e18f };
            bool behind = false;
            if (t.mesh)
            {
                const size_t n = t.mesh->pos.size() / 3;
                s.screen.resize(n);
                for (size_t v = 0; v < n && !behind; ++v)
                {
                    float w;
                    behind = !project(t.matrix, &t.mesh->pos[v * 3], s.screen[v], w);
                    s.depth = (std::min)(s.depth, w);
                }
                if (behind) continue;
                const size_t nTris = t.mesh->tris.size() / 3;
                s.front.resize(nTris);
                for (size_t f = 0; f < nTris; ++f)
                {
                    const uint32_t* tri = &t.mesh->tris[f * 3];
                    s.front[f] = Cross(s.screen[tri[0]], s.screen[tri[1]], s.screen[tri[2]]) > 0.0f;
                }
            }
            else
            {
                // no geometry: its box's corners, drawn as its edges
                s.screen.resize(8);
                for (int k = 0; k < 8 && !behind; ++k)
                {
                    const float l[3] = { (k & 1) ? t.hi[0] : t.lo[0], (k & 2) ? t.hi[1] : t.lo[1],
                                         (k & 4) ? t.hi[2] : t.lo[2] };
                    float w;
                    behind = !project(t.matrix, l, s.screen[k], w);
                    s.depth = (std::min)(s.depth, w);
                }
                if (behind) continue;
            }
            seen.push_back(std::move(s));
        }

        // the nearest under the cursor
        const Seen* hover = nullptr;
        for (const Seen& s : seen)
        {
            bool inside = false;
            if (s.t->mesh)
            {
                const auto& tris = s.t->mesh->tris;
                for (size_t f = 0; f < s.front.size() && !inside; ++f)
                    inside = s.front[f] && InTriangle(s.screen[tris[f * 3]], s.screen[tris[f * 3 + 1]],
                                                      s.screen[tris[f * 3 + 2]], io.MousePos);
            }
            else
            {
                float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
                for (const ImVec2& p : s.screen)
                {
                    x0 = (std::min)(x0, p.x); y0 = (std::min)(y0, p.y);
                    x1 = (std::max)(x1, p.x); y1 = (std::max)(y1, p.y);
                }
                inside = io.MousePos.x >= x0 && io.MousePos.x <= x1 && io.MousePos.y >= y0 && io.MousePos.y <= y1;
            }
            if (inside && (!hover || s.depth < hover->depth))
                hover = &s;
        }
        if (hover) hovered_ = hover->t->id;

        for (const Seen& s : seen)
        {
            const bool hot = &s == hover;
            const ImU32 line = hot ? kHoverOutline : kOutline;
            if (!s.t->mesh)
            {
                static const int box[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
                for (const auto& e : box)
                    dl->AddLine(s.screen[e[0]], s.screen[e[1]], line, 2.0f);
                continue;
            }
            const auto& tris = s.t->mesh->tris;
            if (hot)
                for (size_t f = 0; f < s.front.size(); ++f)
                    if (s.front[f])
                        dl->AddTriangleFilled(s.screen[tris[f * 3]], s.screen[tris[f * 3 + 1]],
                                              s.screen[tris[f * 3 + 2]], kHoverFill);
            // the silhouette: the edges between a face turned towards the camera and one turned
            // away, and the open borders of the faces turned towards it
            for (const Mesh::Edge& e : s.t->mesh->edges)
            {
                const bool f0 = s.front[e.f0];
                const bool f1 = e.f1 >= 0 ? s.front[e.f1] != 0 : !f0;
                if (f0 != f1)
                    dl->AddLine(s.screen[e.a], s.screen[e.b], line, 2.0f);
            }
        }
    }
}
