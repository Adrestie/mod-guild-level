// The placed objects' outlines and picking. See Targets.hpp.
// Copyright (C) 2026 mod-guild-level
// GPL-3.0-or-later.

#include "Targets.hpp"

#include "ExtensionApi.hpp"
#include "ViewProj.hpp"

#include "game/Io.hpp"
#include "game/Pick.hpp"
#include "game/World.hpp"

#include <windows.h>
#include <d3d9.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "imgui.h"

namespace wxl_guildhouse
{
    namespace
    {
        namespace io = wxl::game::io;
        namespace world = wxl::game::world;

        constexpr float kReach = 30.0f;             // yards from the character
        constexpr float kEye = 1.6f;                // the height his eyes look from
        constexpr uint32_t kWalls = 0x01000120;     // terrain and map objects, never a model
        constexpr int kMaxCells = 480;              // a mask's widest side, in cells
        constexpr int kBorder = 2;                  // the outline's width, in cells
        constexpr int kAlphaMaxSize = 256;          // the alpha maps' widest side
        constexpr uint8_t kAlphaCut = 128;          // the cut-out test

        const uint32_t kOutline = 0xFF2882FF;       // ARGB
        const uint32_t kHoverOutline = 0xFF96D7FF;
        const uint32_t kHoverFill = 0x5A8CD2FF;

        bool ReadFile(const std::string& path, std::vector<uint8_t>& out)
        {
            void* handle = nullptr;
            if (!io::FileOpen(path.c_str(), 0, &handle) || !handle) return false;
            uint32_t high = 0;
            const uint32_t size = io::FileSize(handle, &high);
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

        // --- BLP2: a texture's alpha only ----------------------------------------------------

        std::unique_ptr<AlphaMap> DecodeAlpha(const std::vector<uint8_t>& b)
        {
            if (b.size() < 148 || std::memcmp(b.data(), "BLP2", 4) != 0) return nullptr;
            const uint8_t compression = b[8], alphaDepth = b[9], alphaType = b[10];
            uint32_t width = 0, height = 0;
            Get(b, 12, width);
            Get(b, 16, height);
            if (!alphaDepth || !width || !height) return nullptr;      // opaque
            // the first mip small enough that the file holds
            int level = 0;
            while (level < 15 && ((width >> level) > uint32_t(kAlphaMaxSize) || (height >> level) > uint32_t(kAlphaMaxSize)))
                ++level;
            uint32_t ofs = 0, size = 0;
            for (;; --level)
            {
                Get(b, 20 + level * 4, ofs);
                Get(b, 84 + level * 4, size);
                if ((ofs && size && ofs + size <= b.size()) || level == 0) break;
            }
            if (!ofs || !size || ofs + size > b.size()) return nullptr;
            const int w = (std::max)(1, int(width >> level)), h = (std::max)(1, int(height >> level));
            auto map = std::make_unique<AlphaMap>();
            map->w = w;
            map->h = h;
            map->a.assign(size_t(w) * h, 255);
            const uint8_t* d = b.data() + ofs;

            if (compression == 2)
            {
                const int bw = (w + 3) / 4, bh = (h + 3) / 4;
                const int blockSize = alphaType == 0 ? 8 : 16;
                if (size_t(bw) * bh * blockSize > size) return nullptr;
                for (int by = 0; by < bh; ++by)
                    for (int bx = 0; bx < bw; ++bx)
                    {
                        const uint8_t* blk = d + (size_t(by) * bw + bx) * blockSize;
                        uint8_t a[16];
                        if (alphaType == 0)
                        {
                            // DXT1: transparent only where a 3-colour block picks its fourth index
                            const uint16_t c0 = uint16_t(blk[0] | blk[1] << 8), c1 = uint16_t(blk[2] | blk[3] << 8);
                            const uint32_t bits = blk[4] | blk[5] << 8 | blk[6] << 16 | uint32_t(blk[7]) << 24;
                            for (int i = 0; i < 16; ++i)
                                a[i] = (c0 <= c1 && ((bits >> (i * 2)) & 3) == 3) ? 0 : 255;
                        }
                        else if (alphaType == 1)
                        {
                            for (int i = 0; i < 16; ++i)
                                a[i] = uint8_t(((blk[i / 2] >> ((i & 1) * 4)) & 0xF) * 17);
                        }
                        else
                        {
                            // DXT5: two levels and six (or four) between them
                            uint8_t lv[8];
                            lv[0] = blk[0];
                            lv[1] = blk[1];
                            if (lv[0] > lv[1])
                                for (int i = 1; i < 7; ++i) lv[1 + i] = uint8_t(((7 - i) * lv[0] + i * lv[1]) / 7);
                            else
                            {
                                for (int i = 1; i < 5; ++i) lv[1 + i] = uint8_t(((5 - i) * lv[0] + i * lv[1]) / 5);
                                lv[6] = 0;
                                lv[7] = 255;
                            }
                            uint64_t bits = 0;
                            for (int i = 0; i < 6; ++i) bits |= uint64_t(blk[2 + i]) << (8 * i);
                            for (int i = 0; i < 16; ++i) a[i] = lv[(bits >> (3 * i)) & 7];
                        }
                        for (int i = 0; i < 16; ++i)
                        {
                            const int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                            if (x < w && y < h) map->a[size_t(y) * w + x] = a[i];
                        }
                    }
            }
            else if (compression == 1)
            {
                // palette: the indices, then the alpha at alphaDepth bits a pixel
                const size_t n = size_t(w) * h;
                const uint8_t* alpha = d + n;
                for (size_t i = 0; i < n; ++i)
                {
                    uint8_t v = 255;
                    if (alphaDepth == 8 && n * 2 <= size) v = alpha[i];
                    else if (alphaDepth == 4 && n + (n + 1) / 2 <= size) v = uint8_t(((alpha[i / 2] >> ((i & 1) * 4)) & 0xF) * 17);
                    else if (alphaDepth == 1 && n + (n + 7) / 8 <= size) v = ((alpha[i / 8] >> (i & 7)) & 1) ? 255 : 0;
                    map->a[i] = v;
                }
            }
            else if (compression == 3)
            {
                if (size_t(w) * h * 4 > size) return nullptr;
                for (size_t i = 0; i < size_t(w) * h; ++i) map->a[i] = d[i * 4 + 3];
            }
            else
                return nullptr;
            return map;
        }

        // --- the character, and his room -----------------------------------------------------

        bool PlayerPosition(float out[3])
        {
            const unsigned long long guid = world::ActivePlayerGuid();
            void* player = guid ? world::ResolveObject(guid, world::kTypeMaskPlayer) : nullptr;
            if (!player) return false;
            world::UnitPosition(player, out);
            return true;
        }

        void ToWorld(const float m[16], const float l[3], float w[3])
        {
            w[0] = l[0] * m[0] + l[1] * m[4] + l[2] * m[8] + m[12];
            w[1] = l[0] * m[1] + l[1] * m[5] + l[2] * m[9] + m[13];
            w[2] = l[0] * m[2] + l[1] * m[6] + l[2] * m[10] + m[14];
        }

        // In his room: ring enough, and no wall of the hall between his eyes and the object (its
        // middle, or its top).
        bool InHisRoom(const float eye[3], const Target& t)
        {
            const float mid[3] = { (t.lo[0] + t.hi[0]) / 2, (t.lo[1] + t.hi[1]) / 2, (t.lo[2] + t.hi[2]) / 2 };
            const float top[3] = { mid[0], mid[1], t.hi[2] * 0.9f };
            for (const float* l : { mid, top })
            {
                float w[3];
                ToWorld(t.matrix, l, w);
                const float dx = w[0] - eye[0], dy = w[1] - eye[1], dz = w[2] - eye[2];
                if (dx * dx + dy * dy + dz * dz > kReach * kReach) continue;
                world::WorldHit hit;
                if (!world::TraceLine(eye, w, hit, kWalls) || hit.t > 0.97f)
                    return true;
            }
            return false;
        }
    }

    Targets& Targets::Instance()
    {
        static Targets s;
        return s;
    }

    const AlphaMap* Targets::AlphaOf(const std::string& texture)
    {
        auto it = alphas_.find(texture);
        if (it == alphas_.end())
        {
            std::vector<uint8_t> data;
            std::unique_ptr<AlphaMap> map = ReadFile(texture, data) ? DecodeAlpha(data) : nullptr;
            it = alphas_.emplace(texture, std::move(map)).first;
        }
        return it->second.get();
    }

    // The model's vertices and texture coordinates (MD20, inside the modern chunked file), its
    // first view's triangles (SKIN), and for each of its sections the first batch's blending and
    // texture's alpha.
    const Mesh* Targets::MeshOf(const std::string& model)
    {
        auto found = meshes_.find(model);
        if (found != meshes_.end()) return found->second.get();

        std::unique_ptr<Mesh> mesh;
        std::vector<uint8_t> m2, skin;
        const std::string base = model.substr(0, model.size() - 3);
        if (ReadFile(model, m2) && ReadFile(base + "00.skin", skin) && skin.size() >= 44)
        {
            size_t b = 0;
            for (size_t i = 0; i + 4 <= m2.size(); ++i)
                if (std::memcmp(m2.data() + i, "MD20", 4) == 0) { b = i; break; }
            auto arr = [&](size_t at, uint32_t& n, uint32_t& o) { return Get(m2, b + at, n) && Get(m2, b + at + 4, o); };
            uint32_t nV = 0, oV = 0, nTex = 0, oTex = 0, nMat = 0, oMat = 0, nLook = 0, oLook = 0;
            uint32_t nLu = 0, oLu = 0, nIdx = 0, oIdx = 0, nSub = 0, oSub = 0, nBat = 0, oBat = 0;
            const bool header = arr(0x3C, nV, oV) && arr(0x50, nTex, oTex) && arr(0x70, nMat, oMat) &&
                arr(0x80, nLook, oLook) && b + oV + size_t(nV) * 48 <= m2.size() &&
                std::memcmp(skin.data(), "SKIN", 4) == 0 && Get(skin, 4, nLu) && Get(skin, 8, oLu) &&
                Get(skin, 12, nIdx) && Get(skin, 16, oIdx) && Get(skin, 28, nSub) && Get(skin, 32, oSub) &&
                Get(skin, 36, nBat) && Get(skin, 40, oBat) &&
                oLu + size_t(nLu) * 2 <= skin.size() && oIdx + size_t(nIdx) * 2 <= skin.size() &&
                oSub + size_t(nSub) * 48 <= skin.size() && oBat + size_t(nBat) * 24 <= skin.size();
            if (header)
            {
                mesh = std::make_unique<Mesh>();
                mesh->pos.resize(size_t(nV) * 3);
                mesh->uv.resize(size_t(nV) * 2);
                for (uint32_t v = 0; v < nV; ++v)
                {
                    std::memcpy(&mesh->pos[v * 3], m2.data() + b + oV + v * 48, 12);
                    std::memcpy(&mesh->uv[v * 2], m2.data() + b + oV + v * 48 + 32, 8);
                }

                // each section's part: its first batch's material and texture
                std::vector<int> partOfSection(nSub, -1);
                for (uint32_t k = 0; k < nBat; ++k)
                {
                    const size_t at = oBat + size_t(k) * 24;
                    uint16_t section = 0, material = 0, combo = 0;
                    Get(skin, at + 4, section);
                    Get(skin, at + 12, material);
                    Get(skin, at + 18, combo);
                    if (section >= nSub || partOfSection[section] >= 0) continue;
                    uint16_t blend = 0;
                    if (material < nMat) Get(m2, b + oMat + material * 4 + 2, blend);
                    if (blend >= 3)
                        continue;           // additive and modulated: glows, not shapes
                    Mesh::Part part;
                    part.clip = blend >= 1;
                    uint16_t tex = 0xFFFF;
                    if (combo < nLook) Get(m2, b + oLook + combo * 2, tex);
                    if (part.clip && tex < nTex)
                    {
                        uint32_t type = 1, len = 0, ofs = 0;
                        Get(m2, b + oTex + tex * 16, type);
                        Get(m2, b + oTex + tex * 16 + 8, len);
                        Get(m2, b + oTex + tex * 16 + 12, ofs);
                        if (type == 0 && len > 1 && b + ofs + len <= m2.size())
                        {
                            std::string name(reinterpret_cast<const char*>(m2.data() + b + ofs), len - 1);
                            name.resize(std::strlen(name.c_str()));
                            part.alpha = AlphaOf(name);
                        }
                    }
                    partOfSection[section] = int(mesh->parts.size());
                    mesh->parts.push_back(part);
                }

                for (uint32_t s = 0; s < nSub; ++s)
                {
                    if (partOfSection[s] < 0) continue;
                    const size_t at = oSub + size_t(s) * 48;
                    uint16_t level = 0, indexStart = 0, indexCount = 0;
                    Get(skin, at + 2, level);
                    Get(skin, at + 8, indexStart);
                    Get(skin, at + 10, indexCount);
                    const uint32_t first = indexStart + (uint32_t(level) << 16);
                    for (uint32_t i = first; i + 2 < first + indexCount && i + 2 < nIdx; i += 3)
                    {
                        uint32_t t[3];
                        bool ok = true;
                        for (int k = 0; k < 3 && ok; ++k)
                        {
                            uint16_t li = 0, vi = 0;
                            Get(skin, oIdx + (i + k) * 2, li);
                            ok = li < nLu && Get(skin, oLu + li * 2, vi) && vi < nV;
                            t[k] = vi;
                        }
                        if (!ok) continue;
                        mesh->tris.insert(mesh->tris.end(), t, t + 3);
                        mesh->partOf.push_back(uint16_t(partOfSection[s]));
                    }
                }
                if (mesh->tris.empty()) mesh.reset();
            }
        }
        if (!mesh) WLOG_WARN("outline: no geometry read from %s", model.c_str());
        return meshes_.emplace(model, std::move(mesh)).first->second.get();
    }

    void Targets::Add(Target t, const std::string& model)
    {
        if (!model.empty()) t.mesh = MeshOf(model);
        targets_.push_back(t);
    }

    IDirect3DTexture9* Targets::Texture(IDirect3DDevice9* device, size_t slot, int w, int h)
    {
        if (slots_.size() <= slot) slots_.resize(slot + 1);
        Slot& s = slots_[slot];
        if (s.tex && (s.w < w || s.h < h))
        {
            s.tex->Release();
            s.tex = nullptr;
        }
        if (!s.tex)
        {
            const int tw = (std::max)(w, 64), th = (std::max)(h, 64);
            if (FAILED(device->CreateTexture(tw, th, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &s.tex, nullptr)))
            {
                s.tex = nullptr;
                return nullptr;
            }
            s.w = tw;
            s.h = th;
        }
        return s.tex;
    }

    void Targets::ReleaseTextures()
    {
        for (Slot& s : slots_)
            if (s.tex) s.tex->Release();
        slots_.clear();
    }

    void Targets::Draw(IDirect3DDevice9* device)
    {
        hovered_ = 0;
        if (!shown_ || !device) return;
        float eye[3];
        if (!PlayerPosition(eye)) return;
        eye[2] += kEye;

        float vp[16];
        ViewProj(vp);
        const ImGuiIO& io = ImGui::GetIO();
        const float W = io.DisplaySize.x, H = io.DisplaySize.y;

        struct Shown
        {
            const Target* t;
            float x0, y0, cell;          // the mask's corner on the screen, and its cells' size
            int gw, gh;
            std::vector<uint8_t> mask;
            float depth;
        };
        std::vector<Shown> shown;

        struct P { float x, y, iw, u, v; };
        std::vector<P> proj;
        for (const Target& t : targets_)
        {
            if (!t.mesh || !InHisRoom(eye, t)) continue;
            const Mesh& m = *t.mesh;
            const size_t n = m.pos.size() / 3;
            proj.resize(n);
            bool behind = false;
            float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f, depth = 1e18f;
            for (size_t v = 0; v < n && !behind; ++v)
            {
                float w[3];
                ToWorld(t.matrix, &m.pos[v * 3], w);
                float clip[4];
                for (int j = 0; j < 4; ++j)
                    clip[j] = w[0] * vp[j] + w[1] * vp[4 + j] + w[2] * vp[8 + j] + vp[12 + j];
                if (clip[3] <= 0.05f) { behind = true; break; }
                P& p = proj[v];
                p.iw = 1.0f / clip[3];
                p.x = (clip[0] * p.iw * 0.5f + 0.5f) * W;
                p.y = (0.5f - clip[1] * p.iw * 0.5f) * H;
                p.u = m.uv[v * 2] * p.iw;
                p.v = m.uv[v * 2 + 1] * p.iw;
                minX = (std::min)(minX, p.x); maxX = (std::max)(maxX, p.x);
                minY = (std::min)(minY, p.y); maxY = (std::max)(maxY, p.y);
                depth = (std::min)(depth, clip[3]);
            }
            if (behind) continue;
            // on the screen only
            minX = (std::max)(minX, 0.0f); minY = (std::max)(minY, 0.0f);
            maxX = (std::min)(maxX, W); maxY = (std::min)(maxY, H);
            if (maxX <= minX || maxY <= minY) continue;
            const float cell = (std::max)(1.0f, (std::max)(maxX - minX, maxY - minY) / kMaxCells);
            const int pad = kBorder + 1;
            Shown s{ &t, minX - pad * cell, minY - pad * cell, cell, 0, 0, {}, depth };
            s.gw = int((maxX - minX) / cell) + 2 * pad + 1;
            s.gh = int((maxY - minY) / cell) + 2 * pad + 1;
            s.mask.assign(size_t(s.gw) * s.gh, 0);

            // the triangles into the mask, each cell's centre kept where its texture lets it
            for (size_t f = 0; f * 3 < m.tris.size(); ++f)
            {
                const Mesh::Part& part = m.parts[m.partOf[f]];
                const P* q[3] = { &proj[m.tris[f * 3]], &proj[m.tris[f * 3 + 1]], &proj[m.tris[f * 3 + 2]] };
                float gx[3], gy[3];
                for (int k = 0; k < 3; ++k)
                {
                    gx[k] = (q[k]->x - s.x0) / cell;
                    gy[k] = (q[k]->y - s.y0) / cell;
                }
                const float area = (gx[1] - gx[0]) * (gy[2] - gy[0]) - (gy[1] - gy[0]) * (gx[2] - gx[0]);
                if (std::fabs(area) < 1e-6f) continue;
                const int bx0 = (std::max)(0, int(std::floor((std::min)({ gx[0], gx[1], gx[2] }))));
                const int by0 = (std::max)(0, int(std::floor((std::min)({ gy[0], gy[1], gy[2] }))));
                const int bx1 = (std::min)(s.gw - 1, int(std::ceil((std::max)({ gx[0], gx[1], gx[2] }))));
                const int by1 = (std::min)(s.gh - 1, int(std::ceil((std::max)({ gy[0], gy[1], gy[2] }))));
                const float inv = 1.0f / area;
                for (int y = by0; y <= by1; ++y)
                    for (int x = bx0; x <= bx1; ++x)
                    {
                        uint8_t& cellMask = s.mask[size_t(y) * s.gw + x];
                        if (cellMask) continue;
                        const float px = x + 0.5f, py = y + 0.5f;
                        const float l0 = ((gx[1] - px) * (gy[2] - py) - (gy[1] - py) * (gx[2] - px)) * inv;
                        const float l1 = ((gx[2] - px) * (gy[0] - py) - (gy[2] - py) * (gx[0] - px)) * inv;
                        const float l2 = 1.0f - l0 - l1;
                        if (l0 < 0 || l1 < 0 || l2 < 0) continue;
                        if (part.clip && part.alpha)
                        {
                            const float iw = l0 * q[0]->iw + l1 * q[1]->iw + l2 * q[2]->iw;
                            const float u = (l0 * q[0]->u + l1 * q[1]->u + l2 * q[2]->u) / iw;
                            const float v = (l0 * q[0]->v + l1 * q[1]->v + l2 * q[2]->v) / iw;
                            const AlphaMap& a = *part.alpha;
                            int tx = int(std::floor(u * a.w)) % a.w, ty = int(std::floor(v * a.h)) % a.h;
                            if (tx < 0) tx += a.w;
                            if (ty < 0) ty += a.h;
                            if (a.a[size_t(ty) * a.w + tx] < kAlphaCut) continue;
                        }
                        cellMask = 1;
                    }
            }
            shown.push_back(std::move(s));
        }

        // the nearest under the cursor
        const Shown* hover = nullptr;
        for (const Shown& s : shown)
        {
            const int cx = int((io.MousePos.x - s.x0) / s.cell), cy = int((io.MousePos.y - s.y0) / s.cell);
            if (cx < 0 || cy < 0 || cx >= s.gw || cy >= s.gh || !s.mask[size_t(cy) * s.gw + cx]) continue;
            if (!hover || s.depth < hover->depth) hover = &s;
        }
        if (hover) hovered_ = hover->t->id;

        // each mask's outline (and the hovered one's fill) into a texture, drawn over the game
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        size_t slot = 0;
        std::vector<uint8_t> ring, grown;
        for (const Shown& s : shown)
        {
            const bool hot = &s == hover;
            IDirect3DTexture9* tex = Texture(device, slot, s.gw, s.gh);
            if (!tex) continue;
            const int tw = slots_[slot].w, th = slots_[slot].h;
            ++slot;
            // the outline: outside the mask, within kBorder cells of it
            ring = s.mask;
            for (int pass = 0; pass < kBorder; ++pass)
            {
                grown = ring;
                for (int y = 0; y < s.gh; ++y)
                    for (int x = 0; x < s.gw; ++x)
                    {
                        if (ring[size_t(y) * s.gw + x]) continue;
                        bool touch = false;
                        for (int dy = -1; dy <= 1 && !touch; ++dy)
                            for (int dx = -1; dx <= 1 && !touch; ++dx)
                            {
                                const int nx = x + dx, ny = y + dy;
                                touch = nx >= 0 && ny >= 0 && nx < s.gw && ny < s.gh && ring[size_t(ny) * s.gw + nx];
                            }
                        if (touch) grown[size_t(y) * s.gw + x] = 1;
                    }
                ring.swap(grown);
            }
            D3DLOCKED_RECT lr;
            if (FAILED(tex->LockRect(0, &lr, nullptr, 0))) continue;
            for (int y = 0; y < th; ++y)
            {
                uint32_t* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(lr.pBits) + size_t(y) * lr.Pitch);
                for (int x = 0; x < tw; ++x)
                {
                    uint32_t c = 0;
                    if (x < s.gw && y < s.gh)
                    {
                        const size_t i = size_t(y) * s.gw + x;
                        if (s.mask[i]) c = hot ? kHoverFill : 0;
                        else if (ring[i]) c = hot ? kHoverOutline : kOutline;
                    }
                    row[x] = c;
                }
            }
            tex->UnlockRect(0);
            const ImVec2 p0(s.x0, s.y0);
            const ImVec2 p1(s.x0 + s.gw * s.cell, s.y0 + s.gh * s.cell);
            const ImVec2 uv1(float(s.gw) / tw, float(s.gh) / th);
            dl->AddImage(reinterpret_cast<ImTextureID>(tex), p0, p1, ImVec2(0, 0), uv1);
        }
    }
}
