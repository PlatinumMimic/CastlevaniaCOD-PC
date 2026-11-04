// The game's crest, white on a black disc, made from the player's own files: the build's icon (tools/xbe_icon.cpp) and
// the launcher's home page (on Linux, and in a release, which packs nothing of the game). An HD texture pack has it at
// 512x512, the PS2 menu texture drawn with three palettes: the crest in blue, which gives the white, and its ring of
// runes in green, put around it. Without a pack, the xbe's title image, the crest on the Xbox dashboard: its
// $$XTIMAGE section, an XPR0 file holding one 128x128 DXT1 texture, gold on black. Errors are thrown as std::string.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace crest {

struct Image {
    int w = 0, h = 0;
    std::vector<float> px;  // four channels, 0..1, premultiplied in the crest's own images; G second, as RGBA and BGRA
    float* at(int x, int y) { return &px[(size_t(y) * w + x) * 4]; }
};

inline Image Blank(int size) {
    Image img;
    img.w = img.h = size;
    img.px.assign(size_t(size) * size * 4, 0);
    return img;
}

inline uint32_t U32(const std::vector<uint8_t>& d, size_t at) {
    if (at + 4 > d.size()) throw std::string("the xbe is cut short");
    uint32_t v;
    std::memcpy(&v, &d[at], 4);
    return v;
}

// the title image's bytes, from the xbe's section table
inline std::vector<uint8_t> TitleImage(const std::vector<uint8_t>& xbe) {
    if (xbe.size() < 0x178 || std::memcmp(xbe.data(), "XBEH", 4) != 0) throw std::string("not an xbe");
    uint32_t base = U32(xbe, 0x104), count = U32(xbe, 0x11C), headers = U32(xbe, 0x120) - base;
    for (uint32_t i = 0; i < count; i++) {
        size_t h = headers + size_t(i) * 0x38;
        uint32_t raw = U32(xbe, h + 12), size = U32(xbe, h + 16), name = U32(xbe, h + 20) - base;
        if (name < xbe.size() && std::strncmp(reinterpret_cast<const char*>(&xbe[name]), "$$XTIMAGE", 10) == 0) {
            if (size_t(raw) + size > xbe.size()) throw std::string("the title image is cut short");
            return {xbe.begin() + raw, xbe.begin() + raw + size};
        }
    }
    throw std::string("no title image in the xbe");
}

inline void Rgb565(uint16_t c, float* out) {
    out[0] = float((c >> 11) & 31) / 31;
    out[1] = float((c >> 5) & 63) / 63;
    out[2] = float(c & 31) / 31;
    out[3] = 1;
}

// XPR0: magic, total size, header size, then the texture: common, data offset, lock, format (D3DFORMAT bits:
// kind in 8-15, log2 width in 20-23, log2 height in 24-27); the texels after the header
inline Image Decode(const std::vector<uint8_t>& xpr) {
    if (xpr.size() < 32 || std::memcmp(xpr.data(), "XPR0", 4) != 0) throw std::string("the title image is not XPR0");
    uint32_t header = U32(xpr, 8), offset = U32(xpr, 16), format = U32(xpr, 24);
    if (((format >> 8) & 0xFF) != 0x0C) throw std::string("the title image is not DXT1");
    Image img;
    img.w = 1 << ((format >> 20) & 15);
    img.h = 1 << ((format >> 24) & 15);
    img.px.assign(size_t(img.w) * img.h * 4, 0);
    size_t at = size_t(header) + offset;
    if (at + size_t(img.w) * img.h / 2 > xpr.size()) throw std::string("the title image is cut short");
    for (int by = 0; by < img.h; by += 4)
        for (int bx = 0; bx < img.w; bx += 4, at += 8) {
            uint16_t c0 = uint16_t(xpr[at] | xpr[at + 1] << 8), c1 = uint16_t(xpr[at + 2] | xpr[at + 3] << 8);
            uint32_t bits = U32(xpr, at + 4);
            float pal[4][4] = {};
            Rgb565(c0, pal[0]);
            Rgb565(c1, pal[1]);
            for (int k = 0; k < 3; k++) {
                if (c0 > c1) {
                    pal[2][k] = (2 * pal[0][k] + pal[1][k]) / 3;
                    pal[3][k] = (pal[0][k] + 2 * pal[1][k]) / 3;
                } else {
                    pal[2][k] = (pal[0][k] + pal[1][k]) / 2;  // pal[3] stays transparent
                }
            }
            pal[2][3] = 1;
            pal[3][3] = c0 > c1 ? 1.0f : 0.0f;
            for (int k = 0; k < 16; k++) std::copy_n(pal[(bits >> (2 * k)) & 3], 4, img.at(bx + k % 4, by + k / 4));
        }
    return img;
}

// a texture pack's picture, 8 bits a channel (RGBA or BGRA, w x h, square), its PS2 alpha (128 opaque) doubled
inline Image FromPicture(const std::vector<uint8_t>& px, uint32_t w, uint32_t h, const std::string& name) {
    if (w != h || w < 16) throw name + " is not square";
    if (px.size() < size_t(w) * h * 4) throw "cannot read " + name;
    Image img = Blank(int(w));
    for (size_t i = 0; i < img.px.size(); i++) img.px[i] = float(px[i]) / 255;
    for (size_t i = 3; i < img.px.size(); i += 4) img.px[i] = std::min(img.px[i] * 2, 1.0f);
    return img;
}

// from the middle, in half widths
inline float Radius(const Image& img, int x, int y) {
    float half = img.w / 2.0f;
    return std::hypot(x + 0.5f - half, y + 0.5f - half) / half;
}

// how much of the pixel a disc of that radius covers, its edge a pixel and a half soft
inline float Disc(const Image& img, int x, int y, float radius) {
    return std::clamp((radius - Radius(img, x, y)) * img.w / 3, 0.0f, 1.0f);
}

inline void Grey(Image& img, int x, int y, float v, float a) {
    float* p = img.at(x, y);
    p[0] = p[1] = p[2] = v * a;
    p[3] = a;
}

// the pack's crest: its blue palette runs from (0, 0, 128) for the lines to (0, 128, 240) for the fills, so green is
// the light, over black where it is see-through; around it, the runes: the green palette's ring, its light
inline Image FromPack(Image& crest, Image* runes) {
    Image out = Blank(crest.w);
    for (int y = 0; y < out.h; y++)
        for (int x = 0; x < out.w; x++) {
            float* c = crest.at(x, y);
            float v = std::min(c[1] * 2, 1.0f) * c[3];
            if (!runes) {
                Grey(out, x, y, v, Disc(out, x, y, 0.98f));
                continue;
            }
            float* g = runes->at(x, y);
            float ring = Radius(out, x, y) > 0.84f ? std::max({g[0], g[1], g[2]}) * g[3] : 0.0f;
            Grey(out, x, y, std::max(v * Disc(out, x, y, 0.86f), ring), Disc(out, x, y, 0.985f));
        }
    return out;
}

// the Xbox emblem's brightness, its gold made white, on its own black
inline Image FromTitle(Image& title) {
    std::vector<float> light;
    for (int y = 0; y < title.h; y++)
        for (int x = 0; x < title.w; x++) {
            float* p = title.at(x, y);
            light.push_back(0.3f * p[0] + 0.59f * p[1] + 0.11f * p[2]);
        }
    std::vector<float> sorted = light;
    auto top = sorted.begin() + sorted.size() * 99 / 100;
    std::nth_element(sorted.begin(), top, sorted.end());
    Image out = Blank(title.w);
    for (int y = 0; y < out.h; y++)
        for (int x = 0; x < out.w; x++)
            Grey(out, x, y, std::min(light[size_t(y) * out.w + x] / std::max(*top, 0.01f), 1.0f), Disc(out, x, y, 0.99f));
    return out;
}

// smaller: the average over each new pixel's area; larger: bilinear
inline Image Resize(Image& src, int size) {
    Image out;
    out.w = out.h = size;
    out.px.assign(size_t(size) * size * 4, 0);
    float s = float(src.w) / size;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float* o = out.at(x, y);
            if (s <= 1) {
                float fx = std::clamp((x + 0.5f) * s - 0.5f, 0.0f, float(src.w - 1));
                float fy = std::clamp((y + 0.5f) * s - 0.5f, 0.0f, float(src.h - 1));
                int x0 = int(fx), y0 = int(fy), x1 = std::min(x0 + 1, src.w - 1), y1 = std::min(y0 + 1, src.h - 1);
                float ax = fx - x0, ay = fy - y0;
                for (int k = 0; k < 4; k++)
                    o[k] = (src.at(x0, y0)[k] * (1 - ax) + src.at(x1, y0)[k] * ax) * (1 - ay) +
                           (src.at(x0, y1)[k] * (1 - ax) + src.at(x1, y1)[k] * ax) * ay;
                continue;
            }
            float x0 = x * s, y0 = y * s;
            for (int sy = int(y0); sy < std::min(int(std::ceil(y0 + s)), src.h); sy++)
                for (int sx = int(x0); sx < std::min(int(std::ceil(x0 + s)), src.w); sx++) {
                    float wx = std::min(x0 + s, sx + 1.0f) - std::max(x0, float(sx));
                    float wy = std::min(y0 + s, sy + 1.0f) - std::max(y0, float(sy));
                    for (int k = 0; k < 4; k++) o[k] += src.at(sx, sy)[k] * wx * wy / (s * s);
                }
        }
    return out;
}

// the crest as 8 bit BGRA, its alpha not premultiplied (the crest is grey, so the channels' order is the same)
inline std::vector<uint8_t> Bgra(Image& img) {
    std::vector<uint8_t> out(size_t(img.w) * img.h * 4);
    for (size_t i = 0; i < out.size(); i += 4) {
        const float* p = &img.px[i];
        float a = std::clamp(p[3], 0.0f, 1.0f);
        for (int k : {0, 1, 2}) out[i + k] = a > 0 ? uint8_t(std::clamp(p[2 - k] / a, 0.0f, 1.0f) * 255 + 0.5f) : 0;
        out[i + 3] = uint8_t(a * 255 + 0.5f);
    }
    return out;
}

}  // namespace crest
