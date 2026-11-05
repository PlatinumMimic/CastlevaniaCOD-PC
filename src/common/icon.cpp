// The game windows' icon (the taskbar's, the title bar's), for an executable without one of its own: a release, which
// packs nothing of the game. Made from the player's own files as the launcher's crest is (crest.h): the HD texture
// pack's picture of the crest in its ring of runes, else the xbe's title image (the crest on the Xbox dashboard). The
// SDK makes the windows' icon from it (GameInfo::icon, runtime/app/icon.cpp).
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#else
#include "linux/image.h"  // the SDK's: PNG by stb
#endif

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "app/app.h"
#include "common/crest.h"
#include "core/log.h"

namespace fs = std::filesystem;

namespace {

// the pack's crest (the PS2 menu texture in its blue palette) and its ring of runes (in its green one)
constexpr const char* kCrest = "491259d6fb7d9dfb-76ee38f652e03516-";
constexpr const char* kRunes = "491259d6fb7d9dfb-53eede86787ea4c9-";

// the folders a pack's pictures are in, as texpack.cpp looks for them: CV_TEXTURES's (separated by ;), else the
// textures folder beside the game or a few above; in each, the pictures themselves, a replacements folder, or a
// serial's (SLUS-21168\replacements)
std::vector<fs::path> PackFolders() {
    std::error_code ec;
    std::vector<fs::path> roots;
    if (const char* list = std::getenv("CV_TEXTURES")) {
        std::string s = list;
        for (size_t at = 0; at <= s.size();) {
            size_t end = std::min(s.find(';', at), s.size());
            if (end > at) roots.emplace_back(s.substr(at, end - at));
            at = end + 1;
        }
    } else {
        fs::path dir = app::ExeDir();
        for (int i = 0; i < 6; i++) {
            if (fs::is_directory(dir / "textures", ec)) {
                roots.push_back(dir / "textures");
                break;
            }
            if (!dir.has_parent_path() || dir.parent_path() == dir) break;
            dir = dir.parent_path();
        }
    }
    std::vector<fs::path> out;
    for (const fs::path& r : roots) {
        if (!fs::is_directory(r, ec)) continue;
        out.push_back(r);
        if (fs::is_directory(r / "replacements", ec)) out.push_back(r / "replacements");
        for (const auto& e : fs::directory_iterator(r, ec))
            if (e.is_directory(ec) && fs::is_directory(e.path() / "replacements", ec)) out.push_back(e.path() / "replacements");
    }
    return out;
}

// the pack's crest and runes pictures, the first of each found
void FindPack(fs::path& crest, fs::path& runes) {
    std::error_code ec;
    for (const fs::path& dir : PackFolders()) {
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (!e.is_regular_file(ec)) continue;
            std::string name = e.path().filename().string();
            if (crest.empty() && name.rfind(kCrest, 0) == 0) crest = e.path();
            if (runes.empty() && name.rfind(kRunes, 0) == 0) runes = e.path();
        }
        if (!crest.empty()) return;
    }
}

// a picture (PNG, or what else the platform's reader takes) as BGRA
bool ReadPicture(const fs::path& file, std::vector<uint8_t>& px, uint32_t& w, uint32_t& h) {
#ifdef _WIN32
    using Microsoft::WRL::ComPtr;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);  // as the splash's reader (the SDK's splash.cpp)
    ComPtr<IWICImagingFactory> wic;
    ComPtr<IWICBitmapDecoder> dec;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> conv;
    UINT cw = 0, ch = 0;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) ||
        FAILED(wic->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec)) ||
        FAILED(dec->GetFrame(0, &frame)) || FAILED(wic->CreateFormatConverter(&conv)) ||
        FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                WICBitmapPaletteTypeCustom)) ||
        FAILED(conv->GetSize(&cw, &ch)))
        return false;
    px.assign(size_t(cw) * ch * 4, 0);
    if (FAILED(conv->CopyPixels(nullptr, cw * 4, UINT(px.size()), px.data()))) return false;
    w = cw;
    h = ch;
    return true;
#else
    return linuximage::ReadFile(file, px, w, h);
#endif
}

}  // namespace

// GameInfo::icon: the crest at 256 x 256, white on a black disc
bool GameIcon(const std::vector<uint8_t>& xbe, std::vector<uint8_t>& bgra, int& size) {
    try {
        crest::Image art;
        fs::path crest_file, runes_file;
        FindPack(crest_file, runes_file);
        std::vector<uint8_t> px;
        uint32_t w = 0, h = 0;
        const char* from = "the xbe's title image";
        if (!crest_file.empty() && ReadPicture(crest_file, px, w, h)) {
            crest::Image c = crest::FromPicture(px, w, h, crest_file.string());
            crest::Image r;
            bool runes = !runes_file.empty() && ReadPicture(runes_file, px, w, h);
            if (runes) {
                r = crest::FromPicture(px, w, h, runes_file.string());
                if (r.w != c.w) r = crest::Resize(r, c.w);
            }
            art = crest::FromPack(c, runes ? &r : nullptr);
            from = "the HD texture pack's crest";
        } else {
            crest::Image t = crest::Decode(crest::TitleImage(xbe));
            art = crest::FromTitle(t);
        }
        if (art.w != 256) art = crest::Resize(art, 256);
        bgra = crest::Bgra(art);
        size = 256;
        LOGR("window icon: the crest from %s", from);
        return true;
    } catch (const std::string& e) {
        LOGW("window icon: no crest (%s)", e.c_str());
        return false;
    }
}
