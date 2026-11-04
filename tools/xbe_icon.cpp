// xbe_icon <xbe> <ico> [<crest> [<runes>]]: the exes' icon, made at build time from the player's own files: the
// game's crest, white on a black disc (src/common/crest.h, which the launcher makes it with too when it has no icon),
// at the sizes Windows asks for, the ring of runes around it at the larger sizes when the pack has them.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/common/crest.h"

namespace {

using Microsoft::WRL::ComPtr;

// a texture pack's picture, through WIC (PNG, or DDS BC1 to BC3), its PS2 alpha (128 opaque) doubled
crest::Image Load(IWICImagingFactory* wic, const std::filesystem::path& file) {
    ComPtr<IWICBitmapDecoder> dec;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> conv;
    UINT w = 0, h = 0;
    if (FAILED(wic->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec)) ||
        FAILED(dec->GetFrame(0, &frame)) || FAILED(wic->CreateFormatConverter(&conv)) ||
        FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                WICBitmapPaletteTypeCustom)) ||
        FAILED(conv->GetSize(&w, &h)))
        throw "cannot read " + file.string();
    std::vector<uint8_t> raw(size_t(w) * h * 4);
    if (FAILED(conv->CopyPixels(nullptr, w * 4, UINT(raw.size()), raw.data()))) throw "cannot read " + file.string();
    return crest::FromPicture(raw, w, h, file.string());
}

// one icon image: a 32-bit DIB of twice the height (colour rows, then the AND mask's), bottom row first
std::vector<uint8_t> Dib(crest::Image& img) {
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
    if (argc < 3 || argc > 5) {
        std::fprintf(stderr, "xbe_icon <xbe> <ico> [<crest> [<runes>]]\n");
        return 2;
    }
    try {
        const int sizes[] = {256, 128, 64, 48, 32, 24, 16};
        std::vector<crest::Image> art;  // the largest first: the runes' ring, then the crest alone
        if (argc >= 4) {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            ComPtr<IWICImagingFactory> wic;
            if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))))
                throw std::string("no WIC");
            crest::Image crest = Load(wic.Get(), argv[3]);
            if (argc == 5) {
                crest::Image runes = Load(wic.Get(), argv[4]);
                if (runes.w != crest.w) runes = crest::Resize(runes, crest.w);
                art.push_back(crest::FromPack(crest, &runes));
            }
            art.push_back(crest::FromPack(crest, nullptr));
        } else {
            std::ifstream in(argv[1], std::ios::binary);
            if (!in) throw std::string("cannot read ") + argv[1];
            std::vector<uint8_t> xbe{std::istreambuf_iterator<char>(in), {}};
            crest::Image title = crest::Decode(crest::TitleImage(xbe));
            art.push_back(crest::FromTitle(title));
        }

        std::vector<std::vector<uint8_t>> images;
        for (int s : sizes) {
            crest::Image& from = s >= 128 ? art.front() : art.back();  // the runes are a blur below 128
            crest::Image img = s == from.w ? from : crest::Resize(from, s);
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
