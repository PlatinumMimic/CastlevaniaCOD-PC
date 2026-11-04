// HD texture packs made for PCSX2, drawn in place of the game's textures.
//
// The Xbox release kept the PS2's textures: the same palette indices and colours. A PCSX2 pack names each picture
// by PCSX2's hashes of the PS2 texture, so each P8 texture the game uploads is hashed the way PCSX2 hashes it in GS
// memory, and a picture with that name replaces it:
//
//   <texture hash>-<palette hash>-<TEX0 bits>.png (or .dds, BC1 to BC3, which WIC also decodes)
//
//   texture hash   XXH3-64 of the texture's 256 byte GS blocks, row by row, each block in the GS's own layout of
//                  8 bit (16x16 texels) or 4 bit (32x16) indices; a texture smaller than a block, of its indices
//                  row by row instead
//   palette hash   XXH3-64 of its 256 (8 bit) or 16 (4 bit) colours as the GS holds them: R G B A bytes, alpha
//                  0..128 where the Xbox's is 0..255
//
// A 4 bit texture is 8 bit on the Xbox, its indices under 16. Pictures load in the background, scaled to the size
// set, their PS2 alpha doubled, with every mip level; the game's own texture is drawn until they are ready. They go
// in the textures folder beside the game (any folders inside it), which is looked for like the assets folder.
//
// Material maps: a pack's materials\manifest.json (schema pcsx2-material-replacements-v1, as the PCSX2 build of the
// game with material replacements reads it) names a normal map and an ORM map (occlusion, roughness, metalness) for a
// picture by the same hashes, "<texture>:<palette>:<region>:<TEX0 bits>", with how strongly each shades it. They load
// with the picture, at its size cap, and the renderer shades the picture's colour with them (the SDK's
// gpu/shaders.cpp, material). texture_pack_materials = false leaves them out. surface_depth (menu.cpp) sees into them
// too (parallax), as deep as the material's family has it (or the manifest's parallax_strength): its height map
// ("height" beside "normal" and "orm", grey, white the top) is how deep each point lies, or where it has none, its
// occlusion map, dark in their crevices and along a wall's mortar.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef _WIN32
#include <wincodec.h>
#include <wrl/client.h>
#else
#include <fstream>

#include "linux/image.h"  // Linux: PNG and DDS as WIC reads and writes them
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "app/app.h"
#include "core/log.h"
#include "gpu/ext.h"
#include "gpu/texture.h"

bool g_texture_pack = true;         // settings file: texture_pack
bool g_texture_pack_materials = true;  // texture_pack_materials: a pack's material maps (manifest.json) too
uint32_t g_texture_pack_size = 2048;  // texture_pack_size: the largest side a picture is loaded at, 0 as it is
uint32_t g_button_layout = 0;         // button_layout: 0 Xbox, 1 PS2 (its icons here, its shoulders in keyboard.cpp)

int RoomId();  // telemetry.cpp: map * 256 + room, -1 before the game has one

namespace {

namespace fs = std::filesystem;
#ifdef _WIN32
using Microsoft::WRL::ComPtr;
#endif

// where a texel of a 16x16 8 bit block is in the block's bytes, and of a 32x16 4 bit block in its nibbles: four
// columns of four rows, each column's two halves of rows swapped on odd columns
uint32_t Block8(uint32_t x, uint32_t y) {
    uint32_t c = y >> 2, r = y & 3;
    return c * 64 + ((((x >> 2) & 1) ^ (r >> 1) ^ (c & 1)) << 5) + (((x >> 1) & 1) << 4) + ((r & 1) << 3) +
           ((x & 1) << 2) + (((x >> 3) & 1) << 1) + (r >> 1);
}
uint32_t Block4(uint32_t x, uint32_t y) {
    uint32_t c = y >> 2, r = y & 3;
    return c * 128 + ((((x >> 2) & 1) ^ (r >> 1) ^ (c & 1)) << 6) + (((x >> 1) & 1) << 5) + ((r & 1) << 4) +
           ((x & 1) << 3) + (((x >> 3) & 3) << 1) + (r >> 1);
}

uint64_t TextureHash(const std::vector<uint8_t>& idx, uint32_t w, uint32_t h, bool four) {
    uint32_t bw = four ? 32 : 16, bh = 16;
    if (w < bw || h < bh || w % bw || h % bh) return XXH3_64bits(idx.data(), idx.size());
    XXH3_state_t st;
    XXH3_64bits_reset(&st);
    uint8_t block[256];
    for (uint32_t by = 0; by < h; by += bh)
        for (uint32_t bx = 0; bx < w; bx += bw) {
            std::fill(std::begin(block), std::end(block), uint8_t(0));
            for (uint32_t y = 0; y < bh; y++) {
                const uint8_t* row = &idx[size_t(by + y) * w + bx];
                for (uint32_t x = 0; x < bw; x++) {
                    if (four) {
                        uint32_t n = Block4(x, y);
                        block[n >> 1] |= uint8_t((row[x] & 15) << ((n & 1) * 4));
                    } else {
                        block[Block8(x, y)] = row[x];
                    }
                }
            }
            XXH3_64bits_update(&st, block, sizeof(block));
        }
    return XXH3_64bits_digest(&st);
}

uint64_t PaletteHash(const uint32_t* palette, uint32_t n) {
    uint32_t gs[256];
    for (uint32_t i = 0; i < n; i++) {
        uint32_t c = palette[i];  // A R G B
        uint32_t a = ((c >> 24) + 1) >> 1;
        gs[i] = a << 24 | (c & 0xFF) << 16 | (c & 0xFF00) | ((c >> 16) & 0xFF);
    }
    return XXH3_64bits(gs, n * sizeof(uint32_t));
}

struct Key {
    uint64_t texture, palette;
    bool operator<(const Key& o) const { return texture != o.texture ? texture < o.texture : palette < o.palette; }
    bool operator==(const Key& o) const { return texture == o.texture && palette == o.palette; }
};

// The button icons are cells of a sheet of item icons, drawn with a palette of their own. The Xbox repainted those
// cells (A B X Y, L R, white and black: 8 bit, for their shading) in the PS2's places, so the PS2's 4 bit sheet,
// in the palette its icons were drawn with, is the Xbox sheet with the PS2's icons: O, triangle, square, X where B
// Y X A are, L1 L2 R1 R2 where L, white, R and black are (the PS2's shoulders, keyboard.cpp). A pack made from the
// PS2 game has that sheet; the PS2 layout draws it for the Xbox one. (The Xbox sheet's name came from
// CV_TEXPACK_MISSES on the controller settings screen.)
constexpr Key kXboxButtons = {0xf381b771eed6d1a7ull, 0x485f27ac31c8011full};
constexpr Key kPs2Buttons = {0x27818fa40a1117afull, 0x66279948575c23d7ull};

// Xbox textures a pack has under other names. On the PS2 each of these used part of its texture only (the rest is
// never sampled), and the rest held whatever else was in GS memory there, which PCSX2 hashed with it; the Xbox has
// that part blank, so the names cannot match. The used parts are the same pictures: found by their colours (the
// same palette, but for the two marked) and those parts (CV_TEXPACK_MISSES pictures against a pack's), each looked
// at. The names are the PS2 game's, so any PCSX2 pack of it has them; a picture caught with two or three different
// leftovers is there as often, any of them will do. Not here: Xbox textures that differ (the save screens' HARD DISK
// where the PS2 has MEMORY CARD, an icon sheet with an extra NEW!), and those no pack has.
struct Alias {
    Key xbox;
    Key ps2[3];
};
constexpr Alias kAliases[] = {
    {{0x0cf3381b4682dc75ull, 0xb014ec40749e17abull}, {{0xb6b4a1f431c15313ull, 0xb014ec40749e17abull}}},  // a misty backdrop
    {{0x146b17b78291a0daull, 0x46361247dbb7b045ull}, {{0x51240b26560f7efdull, 0x46361247dbb7b045ull}}},  // the Konami logo
    {{0x2df803e5a08f824aull, 0x887cfe00634f8c92ull},  // an arch and carved panels
     {{0x19a172cc2f3576b3ull, 0x887cfe00634f8c92ull}, {0x37b7ed40d9a49f51ull, 0x887cfe00634f8c92ull},
      {0x09c7b8db618feadfull, 0x887cfe00634f8c92ull}}},
    {{0x43cd7f2402145e02ull, 0xd63eec7aa2ab2be3ull}, {{0xff56bb0d4b8bb6d3ull, 0xd63eec7aa2ab2be3ull}}},  // a cloudy sky
    {{0x4cbd1e391f78ce11ull, 0x3b3c2ba9916b03e1ull},  // a floor of carved scrolls
     {{0x10a949fa295fb1fbull, 0x3b3c2ba9916b03e1ull}, {0x25be1d1b1ecdd45cull, 0x3b3c2ba9916b03e1ull}}},
    {{0x6c619b9c19ccc7acull, 0x5ee9432c853b6524ull}, {{0x6ef4962b126286eeull, 0x44d9cb6a52ad4540ull}}},  // dark water, 4 bit,
                                                                                                        // other colours
    {{0xc57de7c4b83fd34aull, 0x47911e17dd5e98b8ull},  // a mossy floor
     {{0x90bbc4ca79461f9dull, 0x47911e17dd5e98b8ull}, {0xe529931929ecbfccull, 0x47911e17dd5e98b8ull}}},
    {{0xe0274fd1ae6986b8ull, 0x990f730a3507fd8dull}, {{0x6d7882647efda698ull, 0x32ee5c1e2825e7dfull}}},  // pale rock, other
                                                                                                        // colours
};

// just enough JSON for a material manifest: objects, arrays, strings, numbers, true, false, null
struct Json {
    enum Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = kNull;
    double number = 0;
    std::string text;
    std::vector<Json> items;
    // an object's, key by key (two lists: a vector of pairs would hold a Json while it is incomplete)
    std::vector<std::string> keys;
    std::vector<Json> values;
    const Json* Get(const char* key) const {
        for (size_t i = 0; i < keys.size(); i++)
            if (keys[i] == key) return &values[i];
        return nullptr;
    }
    double Num(const char* key, double fallback) const {
        const Json* v = Get(key);
        return v && v->kind == kNumber ? v->number : fallback;
    }
    std::string Str(const char* key) const {
        const Json* v = Get(key);
        return v && v->kind == kString ? v->text : std::string();
    }
};

class JsonReader {
public:
    explicit JsonReader(const std::string& s) : s_(s) {}
    bool Read(Json& out) {
        bool ok = Value(out, 0);
        Space();
        return ok && i_ == s_.size();
    }

private:
    void Space() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) i_++;
    }
    bool Value(Json& v, int depth) {
        if (depth > 64) return false;
        Space();
        if (i_ >= s_.size()) return false;
        char c = s_[i_];
        if (c == '{') {
            v.kind = Json::kObject;
            i_++;
            Space();
            if (i_ < s_.size() && s_[i_] == '}') return ++i_, true;
            for (;;) {
                Space();
                std::string key;
                if (!String(key)) return false;
                Space();
                if (i_ >= s_.size() || s_[i_++] != ':') return false;
                Json item;
                if (!Value(item, depth + 1)) return false;
                v.keys.push_back(std::move(key));
                v.values.push_back(std::move(item));
                Space();
                if (i_ < s_.size() && s_[i_] == ',') {
                    i_++;
                    continue;
                }
                return i_ < s_.size() && s_[i_++] == '}';
            }
        }
        if (c == '[') {
            v.kind = Json::kArray;
            i_++;
            Space();
            if (i_ < s_.size() && s_[i_] == ']') return ++i_, true;
            for (;;) {
                Json item;
                if (!Value(item, depth + 1)) return false;
                v.items.push_back(std::move(item));
                Space();
                if (i_ < s_.size() && s_[i_] == ',') {
                    i_++;
                    continue;
                }
                return i_ < s_.size() && s_[i_++] == ']';
            }
        }
        if (c == '"') {
            v.kind = Json::kString;
            return String(v.text);
        }
        for (const char* word : {"true", "false", "null"})
            if (s_.compare(i_, std::strlen(word), word) == 0) {
                v.kind = word[0] == 'n' ? Json::kNull : Json::kBool;
                v.number = word[0] == 't';
                i_ += std::strlen(word);
                return true;
            }
        char* end = nullptr;
        v.number = std::strtod(s_.c_str() + i_, &end);
        if (end == s_.c_str() + i_) return false;
        v.kind = Json::kNumber;
        i_ = size_t(end - s_.c_str());
        return true;
    }
    // a string, its escapes kept as the character after the backslash (a manifest's names have none)
    bool String(std::string& out) {
        if (i_ >= s_.size() || s_[i_] != '"') return false;
        for (i_++; i_ < s_.size(); i_++) {
            char c = s_[i_];
            if (c == '"') return ++i_, true;
            if (c == '\\' && i_ + 1 < s_.size()) c = s_[++i_];
            out += c;
        }
        return false;
    }
    const std::string& s_;
    size_t i_ = 0;
};

// a manifest's material: its two maps, its height map if it has one, and how they shade (gpu::Material::params)
struct MaterialEntry {
    fs::path normal, orm, height;
    float params[3][4];
};

class Pack {
public:
    // at the start: the pack's pictures indexed when it is on (else when it is first switched on, Epoch)
    void Init() {
        on_ = g_texture_pack;
        if (on_ && !indexing_.exchange(true)) Index();
    }

    // the replacements' epoch (gpu::Extension::ReplaceEpoch), asked once a frame: the pack switched on or off while
    // the game runs moves it on, every texture then asking again; switched on for the first time, its pictures are
    // indexed on a thread of their own, the epoch moving on again once they are
    uint32_t Epoch() {
        bool on = g_texture_pack;
        if (on != on_) {
            on_ = on;
            epoch_++;
            LOGR("texpack: switched %s", on ? "on" : "off");
            if (on && !indexing_.exchange(true)) std::thread([this] { Index(); }).detach();
        }
        return epoch_ + (ready_ ? 1u : 0u);
    }

private:
    void Index() {
        std::string dirs;
        std::vector<fs::path> manifests;
        for (const fs::path& dir : Folders()) {
            std::error_code ec;
            for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (!it->is_regular_file(ec)) continue;
                if (it->path().filename() == "manifest.json" && it->path().parent_path().filename() == "materials") {
                    manifests.push_back(it->path());
                    continue;
                }
                std::string ext = it->path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                if (ext != ".png" && ext != ".dds") continue;
                // texture-palette-bits, no region
                std::string name = it->path().stem().string();
                unsigned long long t = 0, p = 0;
                unsigned bits = 0;
                char tail = 0;
                if (std::sscanf(name.c_str(), "%llx-%llx-%8x%c", &t, &p, &bits, &tail) != 3) continue;
                uint32_t psm = bits & 0x3F;
                if (psm != 0x13 && psm != 0x14) continue;  // PSMT8, PSMT4
                if (index_.emplace(Key{t, p}, uint32_t(files_.size())).second) files_.push_back(it->path());
            }
            dirs += (dirs.empty() ? "" : "; ") + dir.string();
        }
        if (dirs.empty()) return;
        LOGR("texpack: %zu pictures in %s, up to %u px", files_.size(), dirs.c_str(), g_texture_pack_size);
        if (files_.empty()) return;
        state_.resize(files_.size());
        material_of_.assign(files_.size(), -1);
        static const bool off = std::getenv("CV_MATERIALS") && std::string(std::getenv("CV_MATERIALS")) == "0";
        if (g_texture_pack_materials && !off)
            for (const fs::path& m : manifests) ReadManifest(m);
        unsigned n = std::clamp(std::thread::hardware_concurrency() / 2, 2u, 6u);
        for (unsigned i = 0; i < n; i++) std::thread([this] { Work(); }).detach();
        ready_ = true;
    }

public:
    uint64_t Replace(const gpu::TextureDesc& d, const uint8_t* guest, const uint32_t* palette) {
        Key tried{};
        // the pack off (or its pictures not indexed yet): the game's own texture, the census as without a pack
        uint64_t id = g_texture_pack && ready_ ? Match(d, guest, palette, tried) : 0;
        asked_.fetch_add(1, std::memory_order_relaxed);
        if (id) matched_.fetch_add(1, std::memory_order_relaxed);
        Census(d, guest, palette, id, tried);
        return id;
    }

    uint64_t Match(const gpu::TextureDesc& d, const uint8_t* guest, const uint32_t* palette, Key& tried) {
        if (files_.empty() || !palette) return 0;
        std::vector<uint8_t> idx;
        if (!gpu::PaletteIndices(d, guest, idx)) return 0;
        Key key{TextureHash(idx, d.width, d.height, false), PaletteHash(palette, 256)};
        auto it = Find(key == kXboxButtons && g_button_layout == 1 ? kPs2Buttons : key);
        bool four = *std::max_element(idx.begin(), idx.end()) < 16;
        if (it == index_.end() && four) {
            key = Key{TextureHash(idx, d.width, d.height, true), PaletteHash(palette, 16)};
            it = Find(key);
        }
        tried = key;
        if (it == index_.end()) {
            Missed(key, d, idx, palette, four);
            return 0;
        }
        uint32_t i = it->second;
        std::lock_guard lock(m_);
        if (state_[i].stage == Stage::Idle) {
            state_[i].stage = Stage::Queued;
            queue_.push_back(i);
            cv_.notify_one();
        }
        return uint64_t(i) + 1;
    }

    std::shared_ptr<const gpu::Replacement> Replaced(uint64_t id) {
        std::lock_guard lock(m_);
        Entry& e = state_[size_t(id - 1)];
        if (e.stage == Stage::Failed) return std::make_shared<gpu::Replacement>();  // empty: drawn as it was
        if (e.stage != Stage::Ready) return nullptr;
        e.stage = Stage::Idle;  // the renderer keeps it now; loaded again should it drop it
        LOG_FIRST(40, "texpack: %s at %ux%u", files_[size_t(id - 1)].filename().string().c_str(), e.pixels->width, e.pixels->height);
        return std::move(e.pixels);
    }

private:
    // a material manifest: each material whose picture the pack has, by that picture
    void ReadManifest(const fs::path& file) {
        std::ifstream f(file, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        Json root;
        if (!JsonReader(text).Read(root) || root.kind != Json::kObject) {
            LOGW("texpack: %s is not a material manifest", file.string().c_str());
            return;
        }
        if (root.Str("schema") != "pcsx2-material-replacements-v1") {
            LOGW("texpack: %s has schema %s, not pcsx2-material-replacements-v1", file.string().c_str(),
                 root.Str("schema").c_str());
            return;
        }
        if (const Json* on = root.Get("enabled"); on && on->kind == Json::kBool && !on->number) {
            LOGR("texpack: %s has its materials turned off", file.string().c_str());
            return;
        }
        const Json* list = root.Get("materials");
        size_t listed = list && list->kind == Json::kArray ? list->items.size() : 0, matched = 0, missing = 0,
               heights = 0;
        for (size_t n = 0; n < listed; n++) {
            const Json& m = list->items[n];
            unsigned long long t = 0, p = 0;
            if (std::sscanf(m.Str("semantic_key").c_str(), "%llx:%llx:", &t, &p) != 2) continue;
            const Json* maps = m.Get("maps");
            if (!maps) continue;
            // a map's path is the manifest folder's, never above it
            auto map = [&](const char* key) {
                std::string rel = maps->Str(key);
                fs::path path = (file.parent_path() / fs::path(rel)).lexically_normal();
                bool inside = !rel.empty() && rel.find("..") == std::string::npos && fs::path(rel).is_relative();
                std::error_code ec;
                return inside && fs::is_regular_file(path, ec) ? path : fs::path();
            };
            MaterialEntry e{map("normal"), map("orm"), map("height"), {}};
            if (e.normal.empty() || e.orm.empty()) {
                missing++;
                continue;
            }
            // as the PCSX2 build's defaults where the manifest says nothing
            const Json* r = m.Get("response");
            Json none;
            const Json& q = r ? *r : none;
            float light[3] = {-0.35f, -0.45f, 0.82f};
            if (const Json* l = q.Get("light_direction"); l && l->kind == Json::kArray && l->items.size() == 3)
                for (int i = 0; i < 3; i++) light[i] = float(l->items[size_t(i)].number);
            // how deep it reads under parallax (surface_depth: its height map, or the occlusion map as the depth):
            // masonry, rock and plaster fully; wood and iron less; leaves and grass, whose maps are veins and blades,
            // not at all
            std::string family = m.Str("material_family");
            float parallax = family == "vegetation" ? 0.0f : family == "iron" ? 0.5f : family == "wood" ? 0.6f : 1.0f;
            float params[3][4] = {
                {1.0f, float(q.Num("normal_strength", 1.0)), float(q.Num("response_strength", 0.75)),
                 float(q.Num("specular_strength", 0.35))},
                {light[0], light[1], light[2], 0.0f},
                {float(q.Num("ao_strength", 0.35)), float(q.Num("roughness_bias", 0.0)),
                 float(q.Num("metalness_scale", 1.0)), float(q.Num("parallax_strength", parallax))},
            };
            std::memcpy(e.params, params, sizeof(params));
            auto it = Find(Key{t, p});
            if (it == index_.end()) continue;
            if (material_of_[it->second] < 0) {
                material_of_[it->second] = int(materials_.size());
                heights += !e.height.empty();
                materials_.push_back(std::move(e));
                matched++;
            }
        }
        LOGR("texpack: %zu materials in %s, %zu for pictures of the pack, %zu of them with height maps%s", listed,
             file.string().c_str(), matched, heights,
             missing ? (", " + std::to_string(missing) + " without both maps").c_str() : "");
    }

    // the pack's picture for a texture, under its own name or the PS2's (kAliases)
    std::map<Key, uint32_t>::const_iterator Find(const Key& key) const {
        auto it = index_.find(key);
        if (it != index_.end()) return it;
        for (const Alias& a : kAliases)
            if (a.xbox == key)
                for (const Key& ps2 : a.ps2)
                    if (ps2.texture && (it = index_.find(ps2)) != index_.end()) return it;
        return index_.end();
    }

    // CV_TEXPACK_CENSUS set: each texture the game uploads, once (its level 0 bytes and colours), with its format,
    // size and what the pack made of it: the picture, none for a P8 texture (its name as the pack would have it), or
    // not looked for, as the pack has pictures of P8 textures only
    void Census(const gpu::TextureDesc& d, const uint8_t* guest, const uint32_t* palette, uint64_t id, const Key& tried) {
        static const bool on = std::getenv("CV_TEXPACK_CENSUS") != nullptr;
        if (!on || !guest) return;
        uint64_t seen = XXH3_64bits(guest, gpu::LevelSize(d, 0)) ^ (palette ? XXH3_64bits(palette, 1024) * 31 : 0) ^
                        (uint64_t(d.format) << 56 | uint64_t(d.width) << 32 | d.height);
        std::string what;
        if (id) what = "picture " + files_[size_t(id - 1)].filename().string();
        else if (!palette) what = "not looked for";
        else {
            char name[48];
            std::snprintf(name, sizeof(name), "none %016llx-%016llx", (unsigned long long)tried.texture,
                          (unsigned long long)tried.palette);
            what = name;
        }
        {
            std::lock_guard lock(m_);
            if (census_.insert(seen).second)
                LOGR("texpack census: %s %ux%u %u levels%s: %s", gpu::Format(d.format).name, d.width, d.height, d.levels,
                     d.cube ? " cube" : "", what.c_str());
        }
        if (palette && tried.texture) CensusEntry(d, guest, palette, id, tried);
    }

public:
    // CV_TEXPACK_CENSUS=<folder> (not 1): census.jsonl there, every paletted texture the game uploaded since it
    // started, by the name a pack's picture of it has, rewritten every few seconds (CensusFlush): its size, the pack's
    // picture and whether it has a material, how it was drawn (into the 3D world by a plain or a skinned vertex
    // program, the characters', or in 2D, and adding to the target or not), the maps it was seen on and its first
    // room; the game's
    // own texture is saved once as originals\<name>.png, as CV_TEXPACK_MISSES saves one
    static const fs::path& CensusDir() {
        static const fs::path dir = [] {
            const char* v = std::getenv("CV_TEXPACK_CENSUS");
            return v && std::string(v) != "1" ? fs::path(v) : fs::path();
        }();
        return dir;
    }

    // a draw sampled the texture at data
    void Used(uint32_t data, bool world, bool skinned, bool additive) {
        auto it = census_by_data_.find(data);
        if (it == census_by_data_.end()) return;
        CensusRow& c = census_list_[size_t(it->second)];
        (world ? (skinned ? c.skinned : c.world) : c.flat)++;
        c.additive += additive;
        int room = RoomId();
        if (room >= 0) {
            c.maps |= 1u << ((room >> 8) & 31);
            if (c.room < 0) c.room = room;
        }
    }

    void CensusFlush() {
        if (CensusDir().empty() || census_list_.empty()) return;
        std::error_code ec;
        fs::create_directories(CensusDir(), ec);
        fs::path tmp = CensusDir() / "census.jsonl.tmp";
        {
            std::ofstream f(tmp);
            for (const CensusRow& c : census_list_) {
                std::string maps;
                for (int m = 0; m < 32; m++)
                    if ((c.maps >> m) & 1) maps += (maps.empty() ? "" : ",") + std::to_string(m);
                char room[16] = "";
                if (c.room >= 0) std::snprintf(room, sizeof(room), "%d.%d", c.room >> 8, c.room & 255);
                f << "{\"name\":\"" << c.name << "\",\"w\":" << c.w << ",\"h\":" << c.h << ",\"picture\":\"" << c.picture
                  << "\",\"material\":" << (c.material ? "true" : "false") << ",\"world\":" << c.world
                  << ",\"skinned\":" << c.skinned << ",\"flat\":" << c.flat << ",\"additive\":" << c.additive
                  << ",\"maps\":[" << maps << "],\"room\":\"" << room << "\"}\n";
            }
        }
        fs::rename(tmp, CensusDir() / "census.jsonl", ec);
    }

private:
    struct CensusRow {
        std::string name, picture;
        uint32_t w = 0, h = 0;
        bool material = false;
        uint64_t world = 0, skinned = 0, flat = 0, additive = 0;
        uint32_t maps = 0;
        int room = -1;
    };

    // the census's entry for a texture just uploaded at d.data, made the first time its name is met
    void CensusEntry(const gpu::TextureDesc& d, const uint8_t* guest, const uint32_t* palette, uint64_t id,
                     const Key& tried) {
        if (CensusDir().empty()) return;
        auto it = census_index_.find(tried);
        if (it == census_index_.end()) {
            std::vector<uint8_t> idx;
            if (!gpu::PaletteIndices(d, guest, idx)) return;
            bool four = Key{TextureHash(idx, d.width, d.height, false), PaletteHash(palette, 256)} != tried;
            char name[64];
            std::snprintf(name, sizeof(name), "%016llx-%016llx-000000%02x", (unsigned long long)tried.texture,
                          (unsigned long long)tried.palette, four ? 0x14 : 0x13);
            CensusRow c;
            c.name = name;
            c.w = d.width;
            c.h = d.height;
            if (id) {
                c.picture = files_[size_t(id - 1)].filename().string();
                c.material = material_of_.size() > id - 1 && material_of_[size_t(id - 1)] >= 0;
            }
            it = census_index_.emplace(tried, int(census_list_.size())).first;
            census_list_.push_back(std::move(c));
            fs::path file = CensusDir() / "originals" / (std::string(name) + ".png");
            std::vector<uint8_t> px(idx.size() * 4);  // B G R A, the PS2's alpha
            for (size_t i = 0; i < idx.size(); i++) {
                uint32_t col = palette[idx[i]];
                px[i * 4] = uint8_t(col);
                px[i * 4 + 1] = uint8_t(col >> 8);
                px[i * 4 + 2] = uint8_t(col >> 16);
                px[i * 4 + 3] = uint8_t(((col >> 24) + 1) >> 1);
            }
            std::thread([file, px = std::move(px), w = d.width, h = d.height] { SavePng(file, px, w, h); }).detach();
        }
        census_by_data_[d.data] = it->second;
    }

    // CV_TEXPACK_MISSES set: each texture the pack has no picture for, once, as the pack would name it, with a 4 bit
    // one's colours (RRGGBB), to find a texture's name for a pack. Set to a folder rather than 1, the texture is saved
    // there too, a PNG named as its picture would be (the PSM alone in the TEX0 bits, which is all this reads) with
    // the PS2's alpha, as PCSX2 dumps a texture: an upscaled copy of it in the textures folder replaces it.
    void Missed(const Key& key, const gpu::TextureDesc& d, const std::vector<uint8_t>& idx, const uint32_t* palette,
                bool four) {
        static const char* to = std::getenv("CV_TEXPACK_MISSES");
        if (!to) return;
        {
            std::lock_guard lock(m_);
            if (!missed_.insert(key).second) return;
        }
        char colours[16 * 7 + 1] = {};
        for (uint32_t i = 0; four && i < 16; i++) std::snprintf(colours + i * 7, 8, " %06X", palette[i] & 0xFFFFFF);
        char name[64];
        std::snprintf(name, sizeof(name), "%016llx-%016llx-000000%02x", (unsigned long long)key.texture,
                      (unsigned long long)key.palette, four ? 0x14 : 0x13);
        LOGR("texpack: no picture for %s, %ux%u%s%s", name, d.width, d.height, four ? ", 4 bit:" : "", colours);
        if (std::string(to) == "1") return;
        fs::path file = fs::path(to) / (std::string(name) + ".png");
        std::vector<uint8_t> px(idx.size() * 4);  // B G R A
        for (size_t i = 0; i < idx.size(); i++) {
            uint32_t c = palette[idx[i]];
            px[i * 4] = uint8_t(c);
            px[i * 4 + 1] = uint8_t(c >> 8);
            px[i * 4 + 2] = uint8_t(c >> 16);
            px[i * 4 + 3] = uint8_t(((c >> 24) + 1) >> 1);
        }
        std::thread([file, px = std::move(px), w = d.width, h = d.height] { SavePng(file, px, w, h); }).detach();
    }

    static void SavePng(const fs::path& file, const std::vector<uint8_t>& bgra, uint32_t w, uint32_t h) {
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
#ifndef _WIN32
        if (!linuximage::WritePng(file, bgra.data(), w, h, w * 4)) LOGW("texpack: %s not written", file.string().c_str());
    }
#else
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ComPtr<IWICImagingFactory> wic;
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapEncoder> enc;
        ComPtr<IWICBitmapFrameEncode> frame;
        WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
        bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) &&
                  SUCCEEDED(wic->CreateStream(&stream)) &&
                  SUCCEEDED(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE)) &&
                  SUCCEEDED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) &&
                  SUCCEEDED(enc->Initialize(stream.Get(), WICBitmapEncoderNoCache)) &&
                  SUCCEEDED(enc->CreateNewFrame(&frame, nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) &&
                  SUCCEEDED(frame->SetSize(w, h)) && SUCCEEDED(frame->SetPixelFormat(&fmt)) &&
                  fmt == GUID_WICPixelFormat32bppBGRA &&
                  SUCCEEDED(frame->WritePixels(h, w * 4, UINT(bgra.size()), const_cast<BYTE*>(bgra.data()))) &&
                  SUCCEEDED(frame->Commit()) && SUCCEEDED(enc->Commit());
        if (!ok) LOGW("texpack: %s not written", file.string().c_str());
        frame.Reset();
        enc.Reset();
        stream.Reset();
        wic.Reset();
        CoUninitialize();
    }
#endif

    enum class Stage { Idle, Queued, Ready, Failed };
    struct Entry {
        Stage stage = Stage::Idle;
        std::shared_ptr<gpu::Replacement> pixels;
    };

    // the textures folder; CV_TEXTURES set, its folders instead (separated by ;), a picture in an earlier one taking
    // the place of one with the same name in a later one: pictures tried out beside a pack without changing it
    static std::vector<fs::path> Folders() {
        std::error_code ec;
        std::vector<fs::path> dirs;
        if (const char* list = std::getenv("CV_TEXTURES")) {
            std::string s = list;
            for (size_t at = 0; at <= s.size();) {
                size_t end = std::min(s.find(';', at), s.size());
                if (end > at && fs::is_directory(s.substr(at, end - at), ec)) dirs.emplace_back(s.substr(at, end - at));
                at = end + 1;
            }
            return dirs;
        }
        fs::path dir = app::ExeDir();
        for (int i = 0; i < 6; i++) {
            if (fs::is_directory(dir / "textures", ec)) return {dir / "textures"};
            if (!dir.has_parent_path() || dir.parent_path() == dir) break;
            dir = dir.parent_path();
        }
        return {};
    }

    void Work() {
#ifdef _WIN32
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ComPtr<IWICImagingFactory> wic;
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
        Codec codec = wic.Get();
#else
        Codec codec = nullptr;
#endif
        for (;;) {
            uint32_t i;
            {
                std::unique_lock lock(m_);
                cv_.wait(lock, [this] { return !queue_.empty(); });
                i = queue_.front();
                queue_.pop_front();
            }
            std::shared_ptr<gpu::Replacement> r = Load(codec, files_[i]);
            if (!r) LOGW("texpack: %s does not load", files_[i].filename().string().c_str());
            if (r && material_of_[i] >= 0) {
                const MaterialEntry& e = materials_[size_t(material_of_[i])];
                auto m = std::make_shared<gpu::Material>();
                bool ok = LoadPicture(codec, e.normal, false, m->normal) && LoadPicture(codec, e.orm, false, m->orm);
                if (ok) {
                    std::memcpy(m->params, e.params, sizeof(m->params));
                    // the height map into the ORM map's alpha, each level from its own (the ORM map's colours are
                    // averaged unweighted, its alpha 255 as they were); the parallax's share made negative says so
                    if (!e.height.empty()) {
                        if (AddHeight(codec, e.height, m->orm))
                            m->params[2][3] = -std::abs(m->params[2][3]);
                        else
                            LOGW("texpack: %s does not load at its ORM map's size; its occlusion is read as the depth",
                                 e.height.filename().string().c_str());
                    }
                    r->material = std::move(m);
                    mapped_.fetch_add(1, std::memory_order_relaxed);
                    static std::atomic<int> logged{0};
                    if (logged.fetch_add(1) < 8)
                        LOGR("texpack: %s with its material maps", files_[i].filename().string().c_str());
                } else {
                    LOGW("texpack: the material maps of %s do not load", files_[i].filename().string().c_str());
                }
            }
            std::lock_guard lock(m_);
            state_[i].pixels = std::move(r);
            state_[i].stage = state_[i].pixels ? Stage::Ready : Stage::Failed;
        }
    }

    // BGRA at half the size, each texel the mean of four, their colour weighted by their alpha: the colour under
    // transparent texels (a PS2 picture's is often a key colour, a green) would otherwise run into the edges of what
    // is drawn where a picture is drawn smaller than it is
    static std::vector<uint8_t> Half(const std::vector<uint8_t>& a, uint32_t lw, uint32_t lh) {
        uint32_t nw = std::max(lw / 2, 1u), nh = std::max(lh / 2, 1u);
        std::vector<uint8_t> b(size_t(nw) * nh * 4);
        for (uint32_t y = 0; y < nh; y++)
            for (uint32_t x = 0; x < nw; x++) {
                uint32_t x0 = std::min(x * 2, lw - 1), x1 = std::min(x * 2 + 1, lw - 1);
                uint32_t y0 = std::min(y * 2, lh - 1), y1 = std::min(y * 2 + 1, lh - 1);
                const uint8_t* t[4] = {&a[(size_t(y0) * lw + x0) * 4], &a[(size_t(y0) * lw + x1) * 4],
                                       &a[(size_t(y1) * lw + x0) * 4], &a[(size_t(y1) * lw + x1) * 4]};
                uint32_t alpha = t[0][3] + t[1][3] + t[2][3] + t[3][3];
                uint8_t* o = &b[(size_t(y) * nw + x) * 4];
                for (uint32_t c = 0; c < 3; c++) {
                    uint32_t s = 0, w = 0;
                    for (const uint8_t* p : t) {
                        s += alpha ? p[c] * p[3] : p[c];
                        w += alpha ? p[3] : 1;
                    }
                    o[c] = uint8_t((s + w / 2) / w);
                }
                o[3] = uint8_t((alpha + 2) / 4);
            }
        return b;
    }

    // the worker's picture decoder: WIC on Windows; on Linux stb and the DDS reader (runtime/linux/image.h) need none
#ifdef _WIN32
    using Codec = IWICImagingFactory*;
#else
    using Codec = std::nullptr_t;
#endif

    // BGRA, PS2 alpha (128 opaque) to the Xbox's, halved to the size set, then every level below
    static std::shared_ptr<gpu::Replacement> Load(Codec codec, const fs::path& file) {
        auto r = std::make_shared<gpu::Replacement>();
        if (!LoadPicture(codec, file, true, *r)) return nullptr;
        return r;
    }

    // a height map (grey, 1 the top) into the alpha of orm's levels: its own levels from the one at orm's size (a
    // larger map's halves; the levels are made with orm's alpha 255, so their colours were averaged unweighted)
    static bool AddHeight(Codec codec, const fs::path& file, gpu::Picture& orm) {
        gpu::Picture h;
        if (!LoadPicture(codec, file, false, h)) return false;
        size_t first = 0;
        while (first < h.levels.size() && (std::max(h.width >> first, 1u) > orm.width ||
                                           std::max(h.height >> first, 1u) > orm.height))
            first++;
        if (first >= h.levels.size() || std::max(h.width >> first, 1u) != orm.width ||
            std::max(h.height >> first, 1u) != orm.height || h.levels.size() - first < orm.levels.size())
            return false;
        for (size_t l = 0; l < orm.levels.size(); l++) {
            std::vector<uint8_t>& o = orm.levels[l];
            const std::vector<uint8_t>& g = h.levels[first + l];
            if (g.size() != o.size()) return false;
            for (size_t i = 3; i < o.size(); i += 4) o[i] = g[i - 1];  // its red, a grey's
        }
        return true;
    }

    // a picture file as BGRA pixels
    static bool Decode(Codec codec, const fs::path& file, std::vector<uint8_t>& px, uint32_t& w, uint32_t& h) {
#ifdef _WIN32
        if (!codec) return false;
        ComPtr<IWICBitmapDecoder> dec;
        ComPtr<IWICBitmapFrameDecode> frame;
        ComPtr<IWICFormatConverter> conv;
        if (FAILED(codec->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                                    &dec)) ||
            FAILED(dec->GetFrame(0, &frame)) || FAILED(codec->CreateFormatConverter(&conv)) ||
            FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                    WICBitmapPaletteTypeCustom)))
            return false;
        UINT cw = 0, ch = 0;
        conv->GetSize(&cw, &ch);
        if (!cw || !ch) return false;
        px.resize(size_t(cw) * ch * 4);
        if (FAILED(conv->CopyPixels(nullptr, cw * 4, UINT(px.size()), px.data()))) return false;
        w = cw;
        h = ch;
        return true;
#else
        std::string ext = file.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (ext == ".dds") {
            std::ifstream f(file, std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
            return linuximage::ReadDds(bytes, px, w, h) && w && h;
        }
        return linuximage::ReadFile(file, px, w, h) && w && h;
#endif
    }

    // a picture as BGRA levels; ps2_alpha: its alpha is the PS2's (128 opaque), a material map's is not
    static bool LoadPicture(Codec codec, const fs::path& file, bool ps2_alpha, gpu::Picture& out) {
        std::vector<uint8_t> px;
        uint32_t w = 0, h = 0;
        if (!Decode(codec, file, px, w, h)) return false;
        if (ps2_alpha)
            for (size_t i = 3; i < px.size(); i += 4) px[i] = uint8_t(std::min(255u, (px[i] * 255u + 64) / 128));
        uint32_t cap = g_texture_pack_size ? g_texture_pack_size : 16384;
        while (std::max(w, h) > cap && w > 1 && h > 1) {
            px = Half(px, w, h);
            w /= 2;
            h /= 2;
        }
        out.width = w;
        out.height = h;
        out.levels.clear();
        out.levels.push_back(std::move(px));
        for (uint32_t lw = w, lh = h; lw > 1 || lh > 1;) {
            out.levels.push_back(Half(out.levels.back(), lw, lh));
            lw = std::max(lw / 2, 1u);
            lh = std::max(lh / 2, 1u);
        }
        return true;
    }

public:
    // telemetry's "texpack" (telemetry.cpp): on, its pictures indexed, and the textures that asked for a picture and
    // got one since the start, the materials its manifests list and the pictures loaded with theirs
    std::string Telemetry() const {
        return std::string("{\"on\":") + (g_texture_pack ? "true" : "false") + ",\"ready\":" +
               (ready_ ? "true" : "false") + ",\"asked\":" + std::to_string(asked_.load()) +
               ",\"matched\":" + std::to_string(matched_.load()) +
               ",\"materials\":" + std::to_string(ready_ ? materials_.size() : 0) +
               ",\"mapped\":" + std::to_string(mapped_.load()) + "}";
    }

private:
    std::atomic<uint64_t> asked_{0}, matched_{0};
    std::atomic<uint64_t> mapped_{0};  // pictures loaded with their material maps
    bool on_ = false;                     // g_texture_pack as Epoch last saw it
    uint32_t epoch_ = 0;
    std::atomic<bool> indexing_{false};   // Index started, once
    std::atomic<bool> ready_{false};      // and done: index_, files_ and the workers there
    std::map<Key, uint32_t> index_;
    std::vector<fs::path> files_;
    std::mutex m_;
    std::condition_variable cv_;
    std::deque<uint32_t> queue_;
    std::vector<Entry> state_;
    std::set<Key> missed_;
    std::set<uint64_t> census_;
    std::vector<int> material_of_;  // by picture, its material in materials_, -1 none
    std::vector<MaterialEntry> materials_;
    // CV_TEXPACK_CENSUS's, on the render thread
    std::map<Key, int> census_index_;
    std::vector<CensusRow> census_list_;
    std::unordered_map<uint32_t, int> census_by_data_;
};

Pack g_pack;

}  // namespace

void TexturePackInit() { g_pack.Init(); }
uint32_t TexturePackEpoch() { return g_pack.Epoch(); }
std::string TexturePackJson() { return g_pack.Telemetry(); }
uint64_t TexturePackReplace(const gpu::TextureDesc& d, const uint8_t* guest, const uint32_t* palette) {
    return g_pack.Replace(d, guest, palette);
}
std::shared_ptr<const gpu::Replacement> TexturePackReplaced(uint64_t id) { return g_pack.Replaced(id); }
bool TexturePackCensus() { return !Pack::CensusDir().empty(); }
void TexturePackUsed(uint32_t data, bool world, bool skinned, bool additive) { g_pack.Used(data, world, skinned, additive); }
void TexturePackCensusFlush() { g_pack.CensusFlush(); }
