// xbe_icon <xbe> <ico>: the exes' icon, made at build time from the player's own xbe. The Xbox dashboard shows a
// game by its title image, the $$XTIMAGE section: an XPR0 file holding one 128x128 DXT1 texture. Its black
// background is cut away (the black reaching the edges), and it is written at the sizes Windows asks for.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

struct Image {
    int w = 0, h = 0;
    std::vector<float> px;  // RGBA, alpha premultiplied, 0..1
    float* at(int x, int y) { return &px[(size_t(y) * w + x) * 4]; }
};

uint32_t U32(const std::vector<uint8_t>& d, size_t at) {
    if (at + 4 > d.size()) throw std::string("the xbe is cut short");
    uint32_t v;
    std::memcpy(&v, &d[at], 4);
    return v;
}

// the title image's bytes, from the xbe's section table
std::vector<uint8_t> TitleImage(const std::vector<uint8_t>& xbe) {
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

void Rgb565(uint16_t c, float* out) {
    out[0] = float((c >> 11) & 31) / 31;
    out[1] = float((c >> 5) & 63) / 63;
    out[2] = float(c & 31) / 31;
    out[3] = 1;
}

// XPR0: magic, total size, header size, then the texture: common, data offset, lock, format (D3DFORMAT bits:
// kind in 8-15, log2 width in 20-23, log2 height in 24-27); the texels after the header
Image Decode(const std::vector<uint8_t>& xpr) {
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

// the near-black reaching the edges, filled away
void CutBackground(Image& img) {
    auto dark = [&](int x, int y) {
        float* p = img.at(x, y);
        return p[3] > 0 && std::max({p[0], p[1], p[2]}) < 0.16f;
    };
    std::vector<std::pair<int, int>> todo;
    for (int i = 0; i < img.w; i++) todo.insert(todo.end(), {{i, 0}, {i, img.h - 1}});
    for (int i = 0; i < img.h; i++) todo.insert(todo.end(), {{0, i}, {img.w - 1, i}});
    while (!todo.empty()) {
        auto [x, y] = todo.back();
        todo.pop_back();
        if (x < 0 || y < 0 || x >= img.w || y >= img.h || !dark(x, y)) continue;
        std::fill_n(img.at(x, y), 4, 0.0f);
        todo.insert(todo.end(), {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}});
    }
}

// smaller: the average over each new pixel's area; larger: bilinear
Image Resize(Image& src, int size) {
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

// one icon image: a 32-bit DIB of twice the height (colour rows, then the AND mask's), bottom row first
std::vector<uint8_t> Dib(Image& img) {
    std::vector<uint8_t> d(40);
    int32_t head[] = {40, img.w, img.h * 2};
    std::memcpy(d.data(), head, sizeof(head));
    d[12] = 1;   // planes
    d[14] = 32;  // bits per pixel
    int mask_row = (img.w + 31) / 32 * 4;
    std::vector<uint8_t> mask(size_t(mask_row) * img.h, 0);
    for (int y = img.h - 1; y >= 0; y--)
        for (int x = 0; x < img.w; x++) {
            float* p = img.at(x, y);
            float a = std::clamp(p[3], 0.0f, 1.0f);
            for (int k : {2, 1, 0}) d.push_back(a > 0 ? uint8_t(std::clamp(p[k] / a, 0.0f, 1.0f) * 255 + 0.5f) : 0);
            d.push_back(uint8_t(a * 255 + 0.5f));
            if (d.back() == 0) mask[size_t(img.h - 1 - y) * mask_row + x / 8] |= uint8_t(0x80 >> (x % 8));
        }
    d.insert(d.end(), mask.begin(), mask.end());
    return d;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "xbe_icon <xbe> <ico>\n");
        return 2;
    }
    try {
        std::ifstream in(argv[1], std::ios::binary);
        if (!in) throw std::string("cannot read ") + argv[1];
        std::vector<uint8_t> xbe{std::istreambuf_iterator<char>(in), {}};
        Image title = Decode(TitleImage(xbe));
        CutBackground(title);

        const int sizes[] = {256, 128, 64, 48, 32, 24, 16};
        std::vector<std::vector<uint8_t>> images;
        for (int s : sizes) {
            Image img = s == title.w ? title : Resize(title, s);
            images.push_back(Dib(img));
        }
        // ICONDIR, an entry per image (a width or height of 0 is 256), then the images
        std::vector<uint8_t> ico = {0, 0, 1, 0, uint8_t(images.size()), 0};
        uint32_t offset = uint32_t(6 + 16 * images.size());
        for (size_t i = 0; i < images.size(); i++) {
            uint8_t side = uint8_t(sizes[i] & 0xFF);
            uint32_t bytes = uint32_t(images[i].size());
            uint8_t entry[16] = {side, side, 0, 0, 1, 0, 32, 0};
            std::memcpy(entry + 8, &bytes, 4);
            std::memcpy(entry + 12, &offset, 4);
            ico.insert(ico.end(), entry, entry + 16);
            offset += bytes;
        }
        for (auto& img : images) ico.insert(ico.end(), img.begin(), img.end());
        std::ofstream out(argv[2], std::ios::binary);
        out.write(reinterpret_cast<const char*>(ico.data()), std::streamsize(ico.size()));
        if (!out) throw std::string("cannot write ") + argv[2];
    } catch (const std::string& e) {
        std::fprintf(stderr, "xbe_icon: %s\n", e.c_str());
        return 1;
    }
    return 0;
}
