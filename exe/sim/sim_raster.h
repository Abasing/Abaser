// ============================================================================
//  sim_raster.h — software rasterizer for ImDrawData
// ----------------------------------------------------------------------------
//  Renders ImGui draw lists to an RGBA8888 framebuffer exactly the way the
//  GPU pipelines do in the real backends:
//    * textured triangles with per-vertex color modulation (the font atlas
//      is sampled bilinearly, shape vertices point at the white pixel)
//    * straight-alpha SrcAlpha/OneMinusSrcAlpha blending
//    * pixel-center sampling with the top-left fill rule (no double-blended
//      seams between adjacent triangles)
//
//  This is what makes the headless simulator trustworthy: the pixels it
//  produces are the pixels DirectX 9 would put on screen, minus the GPU.
// ============================================================================
#pragma once
#include "imgui.h"
#include <cstdint>
#include <vector>
#include <cmath>

struct SimFB
{
    int w = 0, h = 0;
    std::vector<uint32_t> px;      // IM_COL32 layout (R,G,B,A lanes per config)
};

namespace sim_detail
{
    static inline uint32_t colR(uint32_t c) { return (c >> IM_COL32_R_SHIFT) & 0xFFu; }
    static inline uint32_t colG(uint32_t c) { return (c >> IM_COL32_G_SHIFT) & 0xFFu; }
    static inline uint32_t colB(uint32_t c) { return (c >> IM_COL32_B_SHIFT) & 0xFFu; }

    // Bilinear sample of the font atlas (clamped at the borders).
    static inline void SampleTex(const uint32_t* tex, int tw, int th, float u, float v,
                                 float out[4])
    {
        float fx = u * (float)tw - 0.5f;
        float fy = v * (float)th - 0.5f;
        if (fx < 0) fx = 0;
        if (fy < 0) fy = 0;
        if (fx > (float)(tw - 1)) fx = (float)(tw - 1);
        if (fy > (float)(th - 1)) fy = (float)(th - 1);
        const int x0 = (int)fx, y0 = (int)fy;
        const int x1 = x0 + 1 < tw ? x0 + 1 : tw - 1;
        const int y1 = y0 + 1 < th ? y0 + 1 : th - 1;
        const float tx = fx - (float)x0, ty = fy - (float)y0;

        const uint32_t c00 = tex[y0 * tw + x0], c10 = tex[y0 * tw + x1];
        const uint32_t c01 = tex[y1 * tw + x0], c11 = tex[y1 * tw + x1];
        static const int SHIFT[4] = { IM_COL32_R_SHIFT, IM_COL32_G_SHIFT, IM_COL32_B_SHIFT, IM_COL32_A_SHIFT };
        for (int ch = 0; ch < 4; ch++) {
            const float a = (float)((c00 >> SHIFT[ch]) & 0xFFu);
            const float b = (float)((c10 >> SHIFT[ch]) & 0xFFu);
            const float c = (float)((c01 >> SHIFT[ch]) & 0xFFu);
            const float d = (float)((c11 >> SHIFT[ch]) & 0xFFu);
            const float top = a + (b - a) * tx;
            const float bot = c + (d - c) * tx;
            out[ch] = top + (bot - top) * ty;
        }
    }

    static inline void RasterTri(const ImDrawVert& a, const ImDrawVert& b, const ImDrawVert& c,
                                 int cx0, int cy0, int cx1, int cy1,
                                 SimFB& fb, const uint32_t* tex, int tw, int th)
    {
        int minX = (int)ImMin(ImMin(a.pos.x, b.pos.x), c.pos.x);
        int minY = (int)ImMin(ImMin(a.pos.y, b.pos.y), c.pos.y);
        int maxX = (int)ImMax(ImMax(a.pos.x, b.pos.x), c.pos.x) + 1;
        int maxY = (int)ImMax(ImMax(a.pos.y, b.pos.y), c.pos.y) + 1;
        if (minX < cx0) minX = cx0;
        if (minY < cy0) minY = cy0;
        if (maxX > cx1) maxX = cx1;
        if (maxY > cy1) maxY = cy1;
        if (minX >= maxX || minY >= maxY) return;

        const float abx = b.pos.x - a.pos.x, aby = b.pos.y - a.pos.y;
        const float bcx = c.pos.x - b.pos.x, bcy = c.pos.y - b.pos.y;
        const float cax = a.pos.x - c.pos.x, cay = a.pos.y - c.pos.y;
        const float area = abx * (c.pos.y - a.pos.y) - aby * (c.pos.x - a.pos.x);
        if (area <= 0) return;

        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                const float px = (float)x + 0.5f;
                const float py = (float)y + 0.5f;
                const float w0 = abx * (py - a.pos.y) - aby * (px - a.pos.x);
                const float w1 = bcx * (py - b.pos.y) - bcy * (px - b.pos.x);
                const float w2 = cax * (py - c.pos.y) - cay * (px - c.pos.x);

                // top-left rule: boundary pixels count only for top/left edges
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                if (w0 == 0 && !(aby < 0 || (aby == 0 && abx > 0))) continue;
                if (w1 == 0 && !(bcy < 0 || (bcy == 0 && bcx > 0))) continue;
                if (w2 == 0 && !(cay < 0 || (cay == 0 && cax > 0))) continue;

                const float lA = w1 / area;   // weight of vertex a = sub-area opposite a (edge b->c)
                const float lB = w2 / area;   // weight of vertex b = edge c->a
                const float lC = w0 / area;   // weight of vertex c = edge a->b

                float texc[4];
                SampleTex(tex, tw, th,
                          a.uv.x * lA + b.uv.x * lB + c.uv.x * lC,
                          a.uv.y * lA + b.uv.y * lB + c.uv.y * lC, texc);

                const float vr = (float)colR(a.col) * lA + (float)colR(b.col) * lB + (float)colR(c.col) * lC;
                const float vg = (float)colG(a.col) * lA + (float)colG(b.col) * lB + (float)colG(c.col) * lC;
                const float vb = (float)colB(a.col) * lA + (float)colB(b.col) * lB + (float)colB(c.col) * lC;
                const float va = (float)((a.col >> IM_COL32_A_SHIFT) & 0xFFu) * lA
                               + (float)((b.col >> IM_COL32_A_SHIFT) & 0xFFu) * lB
                               + (float)((c.col >> IM_COL32_A_SHIFT) & 0xFFu) * lC;

                const float sr = texc[0] * (vr * (1.0f / 255.0f));
                const float sg = texc[1] * (vg * (1.0f / 255.0f));
                const float sb = texc[2] * (vb * (1.0f / 255.0f));
                const float sa = texc[3] * (va * (1.0f / 255.0f)) * (1.0f / 255.0f);

                uint32_t& dst = fb.px[(size_t)y * (size_t)fb.w + (size_t)x];
                const float dr = (float)((dst >> IM_COL32_R_SHIFT) & 0xFFu);
                const float dg = (float)((dst >> IM_COL32_G_SHIFT) & 0xFFu);
                const float db = (float)((dst >> IM_COL32_B_SHIFT) & 0xFFu);

                const float ia = 1.0f - sa;
                const float orr = sr * sa + dr * ia;
                const float ogg = sg * sa + dg * ia;
                const float obb = sb * sa + db * ia;

                const uint32_t r8 = (uint32_t)(orr + 0.5f) & 0xFFu;
                const uint32_t g8 = (uint32_t)(ogg + 0.5f) & 0xFFu;
                const uint32_t b8 = (uint32_t)(obb + 0.5f) & 0xFFu;
                dst = (uint32_t)0xFF000000u | (r8 << IM_COL32_R_SHIFT)
                                          | (g8 << IM_COL32_G_SHIFT)
                                          | (b8 << IM_COL32_B_SHIFT);
            }
        }
    }
} // namespace sim_detail

// Render dd into fb. tex/tw/th describe the RGBA32 font atlas
// (io.Fonts->GetTexDataAsRGBA32). Alpha-blends over what is already there.
inline void SimRenderDrawData(ImDrawData* dd, SimFB& fb, const uint32_t* tex, int tw, int th)
{
    using namespace sim_detail;
    if (dd == nullptr) return;
    for (int n = 0; n < dd->CmdListsCount; n++) {
        const ImDrawList* cmdl = dd->CmdLists[n];
        const ImDrawVert* vtx  = cmdl->VtxBuffer.Data;
        const ImDrawIdx*  idx  = cmdl->IdxBuffer.Data;
        if (vtx == nullptr || idx == nullptr) continue;

        for (int ci = 0; ci < cmdl->CmdBuffer.Size; ci++) {
            const ImDrawCmd& cmd = cmdl->CmdBuffer[ci];
            if (cmd.UserCallback != nullptr) continue;

            int cx0 = (int)cmd.ClipRect.x,     cy0 = (int)cmd.ClipRect.y;      // ClipRect is ImVec4: z,w = max
            int cx1 = (int)(cmd.ClipRect.z + 0.999f), cy1 = (int)(cmd.ClipRect.w + 0.999f);
            if (cx0 < 0) cx0 = 0;
            if (cy0 < 0) cy0 = 0;
            if (cx1 > fb.w) cx1 = fb.w;
            if (cy1 > fb.h) cy1 = fb.h;
            if (cx0 >= cx1 || cy0 >= cy1) continue;

            const unsigned int e0 = cmd.IdxOffset;
            const unsigned int e1 = cmd.IdxOffset + cmd.ElemCount;
            for (unsigned int i = e0; i + 2 < e1; i += 3) {
                RasterTri(vtx[idx[i]], vtx[idx[i + 1]], vtx[idx[i + 2]],
                          cx0, cy0, cx1, cy1, fb, tex, tw, th);
            }
        }
    }
}
