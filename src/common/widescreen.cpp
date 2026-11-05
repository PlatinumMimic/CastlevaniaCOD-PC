// Widescreen, and the frame pass's overscan, through a renderer extension.
//
// Widescreen: on a 16:9 output the console frame is shown 16:9, and every draw into the back buffer is squeezed to
// 3/4 of its width around the centre, so nothing is stretched (16:10, widescreen_aspect: shown 16:10, squeezed to 5/6). The game projects with a PS2 style view screen matrix
// (c68..c71, or c85..c88 with the world matrix in it) into PS2 screen space, which c94 and c95 then scale to console
// pixels. Squeezing a projected draw is the same as narrowing the camera's focal length, so the 3D view shows more
// at the sides (Hor+). 2D draws (HUD, menus, text, backdrops, movies) keep their 4:3 shape in the middle, a movie's
// frame drawn larger about it as movie_view says (movies.cpp). Draws over
// the whole width that are no picture of their own stay whole: fades and tints, panels whose texture does not change
// across them (a message's dimmed panel), and passes that draw the frame itself back (which already holds the
// squeezed scene).
//
// Backdrops: the menus, the map and the save and load screens draw theirs in pieces, none over the whole width. The
// pieces at the frame's left and right edges have those edges pulled out to the screen's, so the backdrop reaches
// the sides, its outer pieces that much wider. Only for a backdrop of its own, drawn before the 3D world and any 4:3
// picture of the frame: the world's effects touch the edges too, and the title's logo is in pieces at the edges over
// its castle. And for a window over the world while one is up (ModalUp: naming an Innocent Devil), before the HUD:
// drawn over the 3D world as six pieces each half the frame wide, its dark edges stood as a 4:3 box in the middle of
// the screen. A tip is one picture over the whole frame instead, its words in it: it goes to the sides as kSides do,
// its middle as it was and its edges' black reaching the screen's: its texture read clamped past them (Clamp), as
// wrapped (the program makes its coordinates, so the renderer cannot tell they stay in it) its far side showed at
// each edge, the "Tip" heading again at the right.
//
// HUD at the edges (hud_edges): the HUD's draws, from the first of the frame that takes one of its textures (kHud: the
// sheet of the health bar's frame, digits and icons, the bar's fill, the icon at the bottom left) to the frame pass, go
// to the screen's left or right edge, rather than the 4:3 middle's: as on a 4:3 screen, the same way from the screen's
// corner. They go by the piece of the HUD they are part of, the draws that touch one another (the health bar's frame,
// its fill, its digits that reach past the middle of the frame; a message's box at the bottom right and its text),
// taken from the frame before: a piece at least twice as far from one side of the frame as from the other goes to
// that side's screen edge, one about the middle stays, and so does one with none of the HUD's textures. Before the
// HUD in the frame are the world's 2D effects (glows, sparks), drawn at the points they light, which stay; nothing
// moves while the pause menu is up, or in a story scene. While a window is up (ModalUp) the HUD stays at the edges:
// it went back to the 4:3 middle, over the window, as one came up. The window's own draws after it (from its dimmed
// panel or its page on) stay on the window.
//
// The pause menu across the screen (menu_spread): its pages are a left column (Hector's stats, a list), a middle one
// (the page buttons and their cursor) and a right one (status, equipment), all in the 4:3 middle, under a title bar
// across the page. Taken from the frame before, its draws are gathered into groups (MenuGroups): draws of the same
// texture that touch (a word's letters, a list's rows) or share a row close by (a row of figures, of icons and their
// counts), and a draw onto the one drawn before it that it covers most (the cursor on its button, a title on its bar,
// not the figures the cursor also lies over). A group whose middle lies in the left 45% of the frame goes left, one in
// the right 45% goes right, the rest stays; each side by one amount, the most its groups all have room for, up to 70%
// of the screen's extra width, so a column keeps its own alignment. A group half the frame wide or more is a strip, the
// title bar: its vertices move as far as their side's column, those in the middle tenth by as far as they lie across,
// so its tiles stay joined as it stretches. The backdrop stays: its edge pieces reach the screen's sides as before, and
// its large pieces that join them stay put. Only the stat pages (Hector's, each Innocent Devil's) spread: a sub-menu
// (Item, Equip, Skill, Tips, Options, Summon) is built on panels across the page, a description's at the bottom, a
// list's frame, with text on them that a stretch would pull apart, and stays as the game draws it.
//
// Skies past the fog: the game builds its skies from painted strips, a moon and clouds placed far out in the world,
// past a room's far plane where its fog has hidden everything (the rampart of 5.7: a ring of sky strips up to 62000
// out, the moon at 45000, with fog from 14000 to 39000 and the far plane at 39000). With the fog and the far plane
// pushed out (game/view.cpp) they show as the cards they are, their edges and overlaps never meant to be seen. The
// draws of a sky's textures (kSky) get the room's own fog back (the coefficients in c91, as the game set them) and
// are not drawn where wholly past the room's own far plane; everything else is drawn as far as set. A texture is
// listed by hand: level geometry lies past a room's far plane too, and only the sky's cards are held back.
// CV_SKY_TRACE logs the candidates, CV_SKY_HIDE hides them, CV_BACKDROPS=0 draws the skies pushed out too.
//
// Overscan: the last pass of each frame draws a copy of the frame back over it with rows -16 to 496 of the copy
// across rows 0 to 480, the PS2's 448 lines and their border shrunk into the NTSC frame. A TV's overscan hid the 16
// rows at the top and bottom, which read past the copy and smear its first and last rows into streaks. The pass is
// drawn that much taller instead, so the copy fills the frame row for row, as square pixels as the scene was drawn.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <set>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/log.h"
#include "gpu/ext.h"

uint64_t TexturePackReplace(const gpu::TextureDesc& d, const uint8_t* guest, const uint32_t* palette);  // texpack.cpp
extern bool g_hud_edges;  // menu.cpp
bool PauseMenuUp();       // telemetry.cpp
bool ViewBackdrop(float game[2], float drawn[2], float& far_z, uint32_t& room);  // game/view.cpp
bool ModalUp();           // telemetry.cpp
bool CameraSpecial();     // telemetry.cpp: the title's camera, the map screen's or a debug view's
uint32_t SceneWord();     // telemetry.cpp
std::shared_ptr<const gpu::Replacement> TexturePackReplaced(uint64_t id);                              // texpack.cpp
bool TexturePackCensus();                                                                // texpack.cpp
void TexturePackUsed(uint32_t data, bool world, bool skinned, bool additive);            // texpack.cpp
std::string TexturePackName(uint32_t data);  // texpack.cpp: the census's name of a texture (CV_TEXPACK_CENSUS)
int RoomId();  // telemetry.cpp: map * 256 + room, -1 before the game has one
void TexturePackCensusFlush();                                                           // texpack.cpp
// the game's movies (movies.cpp): their frames from a movie pack's HD versions, and drawn larger
void MoviesFrame(uint64_t presents);
uint64_t MoviesReplace(const gpu::TextureDesc& d, const uint8_t* guest);
bool MoviesOwn(uint64_t id);
std::shared_ptr<const gpu::Replacement> MoviesReplaced(uint64_t id);
bool MoviesScale(const gpu::TextureDesc& d, float squeeze, float qx, float qy, float& sx, float& sy);

// the skies of kSky with their room's own fog (Backdrop): off unless CV_BACKDROPS=1. It was made for the rampart's
// strips showing as hard-edged cards, which were their scrolled texture clamped rather than wrapped (the SDK's
// TexcoordsAsGiven); drawn wrapped they show as the PS2 shows them with the fog pushed out. The F10 menu's Tools page
// switches it for comparing (menu.cpp)
std::atomic<bool> g_sky_backdrops{std::getenv("CV_BACKDROPS") && std::atoi(std::getenv("CV_BACKDROPS")) != 0};
bool g_menu_spread = true;  // the pause menu's columns toward the screen's sides (menu.cpp, the settings file)

// the next frame's draws logged as CV_WIDE_TRACE logs them: the F10 menu's frame capture asks for it (menu.cpp)
std::atomic<bool> g_wide_trace_frame{false};

namespace {

// a widescreen draw's width as a part of its own: the console's 4:3 over the frame's 16:9 (3/4) or 16:10 (5/6)
float Squeeze() { return (4.0f / 3.0f) / app::WideAspect(); }

// hardware constant order: D3D register + 96
constexpr uint32_t kCamera = 68 + 96;       // view screen matrix
constexpr uint32_t kCameraWorld = 85 + 96;  // world view screen matrix
constexpr uint32_t kScreenScale = 94 + 96;  // PS2 screen to console pixels
constexpr uint32_t kScreenOffset = 95 + 96;
constexpr uint32_t kFog = 91 + 96;          // the fog's coefficients
// the game's skies by their textures' Name: painted cards it placed past a room's far plane, in its fog
constexpr uint64_t kSky[] = {
    0xa13869efdd70cb49ull,  // 5.7, the Abandoned Castle's rampart: its ring of sky strips
    0x8713e8116bb8057bull,  // its moon
    0x2d6da7cc8094311cull,  // its clouds
};
constexpr int kFrameTexCoord = 2;  // the frame pass's texture coordinates, in texels
constexpr int kTexCoord = 2;       // a 2D draw's, as the game's programs take them
// the HUD's textures by their Name (CV_WIDE_TRACE shows them): its sheet, the health bar's fill, the icon at the
// bottom left
constexpr uint64_t kHud[] = {0x3ff08a280ddd7171ull, 0x9d83fce80f5f78f6ull, 0x8d0b7015b1ee08baull};
// an area's name card's effects drawn apart from the picture the game composed of it: the red flame it scrolls over
// the crest as the card burns away. They go with the card
constexpr uint64_t kCardFx[] = {0xe8728269d4380016ull};

// whole pictures that widen to the screen's sides with their middle as it was: the quad to the sides, its texture
// coordinates stretched with it, so past the 4:3 middle they reach out of the picture: the misty castle behind the
// title's logo, a panorama the game scrolls (wrapping, it goes on), and the Konami logo, on white to its edges
constexpr uint64_t kSides[] = {0x538892ce4eebc65cull, 0x5aa1017e3e7eb511ull};

struct Program {
    bool camera = false;  // projects with the game's camera
    bool screen = false;  // takes PS2 screen coordinates: x * c94.x / w + c95.x
    bool world = false;   // with the world matrix too (c85)
    bool skinned = false;  // indexes its constants (arl): a character's bones
};

struct Span {
    float lo = 1e30f, hi = -1e30f;
    void Add(float v) {
        lo = std::fmin(lo, v);
        hi = std::fmax(hi, v);
    }
    bool Empty() const { return lo > hi; }
};

class Hooks : public gpu::ShaderHooks {
public:
    // ext.z: a vertex on the console frame's left or right edge goes out to the screen's instead
    // ext.w: the HUD's move to the screen's edge, a part of its width
    // ext.z 2: a whole picture to the sides (kSides), its first texture coordinates stretched about ext.w, the middle
    // of their span, by as much as the quad, so the 4:3 middle shows as it was
    // ext.z 3: a strip across the pause menu (its title bar): each vertex moves by ext.y left of the frame's middle
    // tenth (kStripRamp) and by ext.w right of it, in between by as far as it lies across, so its tiles stay joined
    // as it stretches; its height as drawn
    // ext.z 4: a fade or tint over the whole frame (a loading screen's black, a door's): a vertex at any of the console
    // frame's edges goes exactly to the screen's, top and bottom too (the PS2's grid puts it under a pixel in, which
    // left a line of the picture under it at the left and top of the screen)
    std::string Position() const override {
        // a vertex at the console frame's edge (within a few pixels: the PS2's grid puts a backdrop's left edge a pixel
        // in) goes exactly to the screen's, or the screen's first columns stay as they were under it (a bright line at
        // the left edge of the pause menu)
        return "    bool sides = ext.z > 1.5 && ext.z < 2.5;\n"
               "    bool strip = ext.z > 2.5 && ext.z < 3.5;\n"
               "    bool fill = ext.z > 3.5;\n"
               "    if (sides) oT0.x = ext.w + (oT0.x - ext.w) / ext.x;\n"
               "    bool at_edge = (ext.z > 0.5 && ext.z < 2.5 || fill) && abs(abs(o.pos.x / o.pos.w) - 1) < 0.008;\n"
               "    bool at_top = fill && abs(abs(o.pos.y / o.pos.w) - 1) < 0.008;\n"
               "    float across = saturate((o.pos.x / o.pos.w + 0.1) * 5);\n"
               "    o.pos.x = at_edge ? sign(o.pos.x) * o.pos.w : o.pos.x * ext.x;\n"
               "    o.pos.x += (sides ? 0 : strip ? lerp(ext.y, ext.w, across) : ext.w) * o.pos.w;\n"
               "    o.pos.y = at_top ? sign(o.pos.y) * o.pos.w : o.pos.y * (strip ? 1 : ext.y);\n";
    }
};

// the water textures' names (Widescreen::Name): the Abandoned Castle courtyard's pool (found by hiding the room's
// draws a few at a time, CV_WATER_HIDE; its caustics, d569d26d69e71f2a, drawn over the stone around it after the frame
// is anti-aliased, are light on stone, not water)
constexpr uint64_t kWater[] = {0x0b0b643ecc9b58ddull};

class Widescreen : public gpu::Extension {
public:
    const gpu::ShaderHooks* Shaders() const override { return &hooks_; }

    void BeginDraw(const gpu::State& s) override {
        d_ = {};
        clamp_ = false;
        // adds to what is there (a glow, a spark): blending on, the target's colour kept whole
        d_.additive = s.simple[gpu::kMBlendEnable >> 2] && s.simple[gpu::kMDestBlend >> 2] == 1;
        // for the trace (CV_WIDE_TRACE): how it blends, whether it writes depth
        d_.blend = s.simple[gpu::kMBlendEnable >> 2] ? (s.simple[gpu::kMSrcBlend >> 2] << 16 | s.simple[gpu::kMDestBlend >> 2]) : 0;
        d_.zwrite = s.simple[gpu::kMZWriteEnable >> 2] != 0;
    }

    void Vertices(const gpu::State& s, const gpu::Assembled& a) override {
        d_.squeeze = true;
        bool prog = (s.vs_handle & 1) && s.vs && !s.vs->program.empty();
        d_.textured = s.textures[0] != 0;
        d_.count = a.vertex_count;
        // fixed function transform: a camera of its own
        d_.world = !prog && !a.pretransformed;
        if (d_.world) return;
        Program p = prog ? Classify(*s.vs) : Program{};
        if (prog) d_.program = gpu::Hash(s.vs->program.data(), s.vs->program.size() * 4);
        d_.world = p.camera;
        d_.skinned = p.skinned;
        if (p.camera) d_.matrix = p.world ? kCameraWorld : kCamera;
        if (p.camera) {
            Distance(s, a, p);
            // frame generation's view (View): the draws of the world matrix, found again in the next frame by what
            // they draw: their first vertices' positions, their first indices, their counts (the texture is added in
            // SeeWorld). The game streams its meshes into buffers that move from frame to frame, so not by address
            if (p.world && !p.skinned) {
                uint64_t h = 1469598103934665603ull;
                auto mix = [&](const void* v, size_t n) {
                    const uint8_t* b = static_cast<const uint8_t*>(v);
                    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 1099511628211ull;
                };
                const gpu::VertexElement& e = a.elements[0];
                if ((a.regs & 1) && e.buffer < a.buffer_count) {
                    const gpu::VertexBuffer& b = a.buffers[e.buffer];
                    uint32_t n = std::min<uint32_t>(a.vertex_count, 4);
                    for (uint32_t i = 0; b.src && i < n; i++)
                        if (uint64_t(i) * b.stride + e.offset + 12 <= b.bytes) mix(b.src + size_t(i) * b.stride + e.offset, 12);
                }
                if (a.index_src && a.index_count) mix(a.index_src, std::min<size_t>(a.index_count, 8) * (a.index16 ? 2 : 4));
                mix(&a.vertex_count, sizeof(a.vertex_count));
                mix(&a.index_count, sizeof(a.index_count));
                mix(&a.draw_count, sizeof(a.draw_count));
                d_.mesh = h | 1;
                zscale_ = s.zscale;
            }
            return;
        }
        // on the console frame
        auto screen = [&](int axis) {
            return Read(a, 0, [&](const float* v, uint32_t comps) {
                if (!p.screen) return v[axis];
                uint32_t decode = a.decode & 0xF;
                float w = comps == 4 && decode != gpu::kDecodeW1 ? v[3] : decode == gpu::kDecodeFloat2H ? v[2] : 1.0f;
                return v[axis] * s.vsc[kScreenScale][axis] * (w != 0 ? 1 / w : 1) + s.vsc[kScreenOffset][axis];
            });
        };
        d_.x = screen(0);
        d_.y = screen(1);
        const Span& x = d_.x;
        const Span& y = d_.y;
        float gw = float(s.viewport.w ? s.viewport.w : 640), gh = float(s.viewport.h ? s.viewport.h : 480);
        d_.gw = gw;
        d_.gh = gh;
        d_.whole = !x.Empty() && x.lo <= 1 && x.hi >= gw - 1;
        bool inside = !x.Empty() && !y.Empty() && x.lo >= -2 && x.hi <= gw + 2 && y.lo >= -2 && y.hi <= gh + 2;
        // a backdrop's piece at the frame's left or right edge (on the PS2's grid, a pixel to the right), no speck
        d_.edge = inside && !d_.whole && x.hi - x.lo >= 64 &&
                  (std::fabs(x.lo - 0.5f) <= 1.5f || std::fabs(x.hi - gw - 0.5f) <= 1.5f);
        // a quad on the frame: across it, how far its texture coordinates go
        if (inside && a.vertex_count == 4) d_.u = Read(a, kTexCoord, [](const float* v, uint32_t) { return v[0]; });
        // any quad's, for TexelBox
        if (a.vertex_count == 4) {
            d_.box_u = Read(a, kTexCoord, [](const float* v, uint32_t) { return v[0]; });
            d_.box_v = Read(a, kTexCoord, [](const float* v, uint32_t) { return v[1]; });
        }
        // the frame pass: screen positions, its copy's rows read over a taller span than it covers
        if (prog && !p.screen && d_.whole && a.vertex_count == 4) {
            Span t = Read(a, kFrameTexCoord, [](const float* v, uint32_t) { return v[1]; });
            if (!t.Empty() && y.hi > y.lo && t.lo < y.lo - 0.5f && t.hi > y.hi + 0.5f)
                d_.stretch = (t.hi - t.lo) / (y.hi - y.lo);
        }
    }

    bool Bound(gpu::Context& c, const gpu::State& s, gpu::Target* rt, gpu::Target*, bool) override {
        uint32_t w = 0, h = 0;
        c.GuestSize(w, h);
        // the device's own: the game also composes in a target the size of it and copies pieces out (the name card
        // of an area: CopyRects from 640x480 to its texture), which a squeeze there would cut at the left
        back_buffer_ = rt && s.rt_back_buffer && rt->gw == w && rt->gh == h;
        guest_w_ = w;
        guest_h_ = h;
        trace_rt_ = rt ? rt->data : 0;
        trace_device_ = s.rt_back_buffer;
        if (c.Presents() != frame_) {  // the frame's first draw
            frame_ = c.Presents();
            world_ = 0;
            picture_ = false;
            hud_ = false;
            ui_ = false;
            menu_ = PauseMenuUp();
            screen_ = menu_ || CameraSpecial();
            modal_ = ModalUp();
            card_free_ = g_hud_edges && !menu_ && !modal_;
            trace_now_ = g_wide_trace_frame.exchange(false) || (kTrace && frame_ % kTrace == 0) ||
                         frame_ <= odd_trace_until_;
            if (trace_now_) LOGR("wide %llu: the frame's draws (menu %d, modal %d, scene %08x)", (unsigned long long)frame_,
                                 int(menu_), int(ModalUp()), SceneWord());
            MenuGroups();
            // the frame before's world draws, to find the view's move from (View)
            std::swap(seen_prev_, seen_now_);
            seen_now_.clear();
            cam_now_ = {};
            card_before_ = card_now_;
            card_before_seen_ = card_now_seen_;
            card_now_seen_ = false;
            hud_free_ = g_hud_edges && !menu_ && !(SceneWord() & 0x20000);
            HudPieces();
            if (TexturePackCensus() && frame_ % 300 == 0) TexturePackCensusFlush();
            MoviesFrame(frame_);
        }
        return false;
    }

    void Sampled(const gpu::TextureDesc& d, const gpu::Target* t, uint8_t, gpu::TexInfo&) override {
        d_.target |= t != nullptr;
        if (t && !d_.src_data) {
            d_.src_data = t->data;
            d_.src_w = t->gw;
            d_.src_h = t->gh;
        }
        // a picture the game composed in a target of its own (an area's name card: in a 640x480 one, then drawn to
        // the frame as a mesh), not the one drawn into
        d_.card |= t && (t->gw != guest_w_ || t->gh != guest_h_ || t->data != trace_rt_);
        if (!t && d_.ntex < 4) d_.texs[d_.ntex++] = d.data;
        if (!d_.sampled) {
            d_.tex = d;
            auto it = t ? names_.end() : names_.find(d.data);
            d_.name = it != names_.end() ? it->second : 0;
        }
        d_.sampled = true;
    }

    // a movie's frames (movies.cpp), else HD texture packs (texpack.cpp)
    uint64_t Replace(const gpu::TextureDesc& d, const uint8_t* guest, const uint32_t* palette) override {
        if (guest) names_[d.data] = Name(d, guest);
        if (uint64_t id = MoviesReplace(d, guest)) return id;
        return TexturePackReplace(d, guest, palette);
    }
    std::shared_ptr<const gpu::Replacement> Replaced(uint64_t id) override {
        return MoviesOwn(id) ? MoviesReplaced(id) : TexturePackReplaced(id);
    }

    // the 3D world drawn into the frame takes anti-aliasing's sub-pixel offset (the SDK's Jitter), which also puts it
    // in ray tracing's scene; its 2D, the HUD, menus and the frame pass, never. Nor the 3D of a screen of its own (the
    // map screen's rooms, the pause menu's turning item and Innocent Devil, the title's castle): held still, they
    // shook by the offset, and ray tracing's light and shade moved over them
    bool Jitter() override { return back_buffer_ && d_.world && !screen_; }
    // a tip's page to the screen's sides (Constants): its texture read clamped, its edges' black past them
    bool Clamp() override { return clamp_; }

    // ray tracing's lights drawn flat (the SDK's ScreenLight): a flame, a torch's fire, a glow the game draws as a 2D
    // sprite adding its light over the 3D world (before its UI), which ray tracing puts back into the room by its depth
    bool ScreenLight() override {
        return back_buffer_ && !d_.world && d_.additive && !d_.whole && !d_.target && !ui_ && world_ >= kWorld;
    }

    // ray tracing's characters (the SDK's Character): a 3D draw by a skinned program (Program::skinned), Hector, the
    // enemies and the Innocent Devils, whose shadows it casts and whose armour and blades it lets shine
    bool Character() override { return d_.world && d_.skinned; }

    // ray tracing's water (the SDK's Water): a 3D draw of the pools, fountains and moats, by its texture's name (kWater,
    // and CV_WATER's: names in hex by commas, to try others), which ray tracing reflects the room in
    bool Water() override {
        if (!d_.world || !d_.name) return false;
        // CV_WATER_LIST: each blended 3D draw (not adding light), once by its texture, to find the water's among
        // (CV_WATER_LIST=2: each 3D draw)
        static const int list = std::getenv("CV_WATER_LIST") ? std::atoi(std::getenv("CV_WATER_LIST")) : 0;
        if (list && (list == 2 || (d_.blend && !d_.additive)) && listed_.insert(d_.name).second)
            LOGR("water? %016llx tex %ux%u blend %03x %03x%s, %u vertices, program %016llx, %d textures, room %d.%d, "
                 "pack %s", (unsigned long long)d_.name, d_.tex.width, d_.tex.height, d_.blend >> 16, d_.blend & 0xFFFF,
                 d_.zwrite ? " writes depth" : "", d_.count, (unsigned long long)d_.program, d_.ntex, RoomId() >> 8,
                 RoomId() & 0xFF, d_.ntex ? TexturePackName(d_.texs[0]).c_str() : "");
        static const std::vector<uint64_t> extra = [] {
            std::vector<uint64_t> v;
            if (const char* e = std::getenv("CV_WATER"))
                for (const char* c = e; *c;) {
                    char* end = nullptr;
                    uint64_t n = std::strtoull(c, &end, 16);
                    if (end == c) break;
                    v.push_back(n);
                    c = *end == ',' ? end + 1 : end;
                }
            return v;
        }();
        for (uint64_t n : kWater)
            if (n == d_.name) return true;
        return std::find(extra.begin(), extra.end(), d_.name) != extra.end();
    }

    // the anti-aliasing's reactive mask (the SDK's Reactive), for a blended draw that writes no depth: the frame's
    // effects, sparks, fire, magic and the light flares drawn as 2D sprites over the world, which have no motion of their
    // own and would smear. Not a tint over the whole frame, nor the frame drawn back over itself
    bool Reactive() override { return back_buffer_ && !d_.whole && !d_.target; }

    // a 2D quad reads its texture only inside its coordinates (the SDK's TexelBox): sprites, words and figures cut
    // from a sheet, whose edges upscaled blend in the sheet's next picture (a faint line under each of the pause
    // menu's figures, beside "Weapon"). Not a whole frame quad (a fill, the frame pass, a picture widened to the
    // sides past its coordinates), nor one reading a target, nor the world
    bool TexelBox(float* box) override {
        if (!d_.squeeze || d_.world || d_.whole || d_.target || !d_.textured || d_.count != 4 || d_.box_u.Empty() ||
            d_.box_v.Empty())
            return false;
        box[0] = d_.box_u.lo;
        box[1] = d_.box_v.lo;
        box[2] = d_.box_u.hi;
        box[3] = d_.box_v.hi;
        return true;
    }

    // HDR output, anti-aliasing and upscaling (the SDK's gpu/d3d12.cpp): the frame's UI starts at its first 2D draw
    // into the back buffer after its last 3D draw that does not add to what is there (a glow or a spark adds, and stays
    // a highlight). What it and the draws after it change, the HUD, text, menus, their backdrops and movies, keeps
    // scene brightness, and is drawn at the screen's resolution over the anti-aliased 3D. A tint over the whole frame,
    // untextured, is the scene's: a room tints its sky and then draws the rest of its world (the Abandoned Castle's
    // throne room, after 120 of its 1250 3D draws), which would otherwise be left out of the anti-aliasing
    bool UiStart(bool additive) override {
        if (!back_buffer_) return false;
        if (d_.world) {
            ui_ = false;
            return false;
        }
        if (ui_ || additive || !d_.squeeze || d_.target || (d_.whole && !d_.textured)) return false;
        // a sprite the game draws wholly off the frame is no UI: an effect of the world's (10.5's haze, thousands of
        // pixels to the side) closed the 3D before the torches' glows were drawn, and ray tracing never saw them
        const Span& x = d_.x;
        const Span& y = d_.y;
        if (!x.Empty() && !y.Empty() && (x.hi < 0 || x.lo > d_.gw || y.hi < 0 || y.lo > d_.gh)) return false;
        ui_ = true;
        return true;
    }

    void Constants(float (*vsc)[4], float* ext, const gpu::Target* rt, uint32_t src_blend) override {
        bool squeeze = d_.squeeze && back_buffer_ && app::Widescreen();
        // whole width: an untextured fade or tint, a panel whose texture does not change across it (a few texels
        // stretched over the frame: a message's dimmed panel), or the frame drawn back over itself
        float texels = (d_.u.hi - d_.u.lo) * (d_.tex.linear ? 1.0f : float(d_.tex.width));
        bool flat = !d_.textured || (d_.sampled && !d_.u.Empty() && texels <= 4);
        // the 3D drawn into a target the console frame's size other than the frame (an enemy's after-images as it
        // appears or swings: the room's armoured knights drawn there, then that picture laid over the frame a few
        // times over, faded and moved): drawn as the game drew it, at the console's 4:3, so a picture of it laid
        // over the whole frame is squeezed as the frame's own 3D is, or the copies stood a third wider than the
        // knight (Julian: "it spawns 4:3, then 16:9 a moment later"). CV_WIDE_AFTERIMAGE=0: laid over whole, as before
        static const bool afterimages =
            !(std::getenv("CV_WIDE_AFTERIMAGE") && std::atoi(std::getenv("CV_WIDE_AFTERIMAGE")) == 0);
        if (d_.world && !back_buffer_ && rt && rt->gw == guest_w_ && rt->gh == guest_h_ && !rt->depth)
            offscreen3d_[rt->data] = frame_;
        auto drawn3d = d_.target && d_.src_data ? offscreen3d_.find(d_.src_data) : offscreen3d_.end();
        bool afterimage = afterimages && drawn3d != offscreen3d_.end() && frame_ - drawn3d->second <= 2;
        if (d_.whole && (flat || (d_.target && !afterimage))) squeeze = false;
        static int afterimages_said = 0;
        if (afterimage && d_.whole && afterimages_said < 4 && ++afterimages_said)
            LOGR("wide: a picture of the 3D drawn apart (%08x) laid over the frame, squeezed as the frame's 3D",
                 d_.src_data);
        // a backdrop's edge pieces reach the screen's edges on a screen of their own: not over the 3D world or a
        // 4:3 picture drawn before them in the frame. In the pause menu the world is not drawn, and the 3D there is
        // the menu's own (an Innocent Devil's model on its page, 1600 vertices before the backdrop)
        bool edges = squeeze && d_.edge && !d_.target && (world_ < kWorld || menu_ || (modal_ && !hud_)) && !picture_;
        if (back_buffer_) {
            // the world's 3D is textured; the map screen's rooms are flat colour, drawn before its backdrop (126 of
            // them, 3000 vertices, with half the castle explored: its backdrop stood as a 4:3 box)
            world_ += d_.world && d_.textured ? d_.count : 0;
            picture_ |= squeeze && d_.whole && d_.textured && !d_.target;
        }
        // the HUD, from its first draw to the frame pass, to the screen's edges
        float shift = 0;
        if (back_buffer_ && d_.target && d_.whole) hud_ = false;
        // a window's page over the world while one is up (a tip: one picture over the whole frame, its words in it):
        // to the screen's sides as a kSides picture is, its edges' black reaching them; it covers the HUD drawn before
        // it, as on a 4:3 screen, and what is drawn on it after (its "Next") stays on it rather than going with the HUD
        bool page = back_buffer_ && modal_ && squeeze && d_.whole && d_.textured && !d_.target && d_.sampled &&
                    !d_.u.Empty() && world_ >= kWorld;
        if (page) hud_ = false;
        // the rest of a window over the world while one is up: a whole-frame draw after the HUD (the dimmed panel an
        // item's name and description lie on) ends it, so what is drawn on the window stays on it rather than going
        // with a piece of the HUD it touches (an item's description, its first 32 letters a draw of their own that
        // touched the HUD's left column, went to the left edge and the rest of the line stayed)
        if (back_buffer_ && modal_ && d_.whole && !d_.target && world_ >= kWorld) hud_ = false;
        bool card_fx = (card_now_seen_ || card_before_seen_) && !d_.target && d_.name &&
                       std::find(std::begin(kCardFx), std::end(kCardFx), d_.name) != std::end(kCardFx);
        if (squeeze && hud_free_ && !d_.world && !d_.whole && !d_.target && !card_fx) {
            bool named = std::find(std::begin(kHud), std::end(kHud), d_.name) != std::end(kHud);
            hud_ |= named;
            bool inside = !d_.x.Empty() && !d_.y.Empty() && d_.x.lo >= -2 && d_.x.hi <= d_.gw + 2;
            if (hud_ && inside) {
                // the draw's own key from frame to frame: its texture, and where its top left lies (a bar's fill
                // changes its width, a figure its value, in place)
                uint64_t key = (d_.name ^ 0x9E3779B97F4A7C15ull) * 1099511628211ull ^
                               uint64_t(int64_t(std::lround(d_.x.lo / 8)) & 0xFFFF) << 16 ^
                               uint64_t(int64_t(std::lround(d_.y.lo / 8)) & 0xFFFF) << 32 ^ (d_.count == 4 ? 1 : 2);
                shift = HudShift({d_.x.lo, d_.x.hi, d_.y.lo, d_.y.hi, named, 0}, d_.count == 4, key);
            }
        } else if (squeeze && card_free_ && !d_.world && !d_.whole && (d_.card || card_fx)) {
            // an area's name card, drawn from the picture the game composed of it, and its effects (kCardFx), to its
            // side's screen edge as the HUD goes; it comes up as an area's opening scene ends, so a story scene does
            // not hold it. As it burns away the game draws the picture as a wavering mesh that reaches a few pixels
            // past the frame: it goes as the card does, though only what lies on the frame says where the card is
            bool inside = !d_.x.Empty() && !d_.y.Empty() && d_.x.lo >= -2 && d_.x.hi <= d_.gw + 2;
            Rect r{d_.x.lo, d_.x.hi, d_.y.lo, d_.y.hi, true, 0};
            if (inside) {
                if (card_now_seen_) {
                    card_now_.x0 = std::min(card_now_.x0, r.x0);
                    card_now_.x1 = std::max(card_now_.x1, r.x1);
                    card_now_.y0 = std::min(card_now_.y0, r.y0);
                    card_now_.y1 = std::max(card_now_.y1, r.y1);
                } else {
                    card_now_ = r;
                    card_now_seen_ = true;
                }
            }
            gw_ = d_.gw;
            if (card_before_seen_ || card_now_seen_) shift = Side(card_before_seen_ ? card_before_ : card_now_);
            static int logged = 0;
            if (logged < 6 && ++logged)
                LOGR("wide: a composed picture (an area's name card) at x %.0f..%.0f y %.0f..%.0f, %s", d_.x.lo,
                     d_.x.hi, d_.y.lo, d_.y.hi, shift > 0 ? "to the right edge" : shift < 0 ? "to the left edge" : "kept");
        }
        // the pause menu's columns toward the sides; its backdrop's edge pieces, which the backdrop's other large
        // pieces join, stay
        MenuMove menu;
        if (squeeze && menu_ && g_menu_spread && !d_.world && !d_.whole && !d_.target && shift == 0) {
            bool inside = !d_.x.Empty() && !d_.y.Empty() && d_.x.lo >= -48 && d_.x.hi <= d_.gw + 48;
            Rect r{d_.x.lo, d_.x.hi, d_.y.lo, d_.y.hi, false, 0};
            if (edges && inside) menu_back_.push_back(r);
            else if (!edges && inside) menu = MenuPlace(r, d_.name);
        }
        if (!menu.strip && menu.shift != 0) shift = menu.shift;
        if (TexturePackCensus())
            for (int i = 0; i < d_.ntex; i++) TexturePackUsed(d_.texs[i], d_.world, d_.skinned, d_.additive);
        bool sides = squeeze && d_.whole && d_.textured && !d_.target && d_.sampled && !d_.u.Empty() &&
                     (page || std::find(std::begin(kSides), std::end(kSides), d_.name) != std::end(kSides));
        clamp_ = page && sides;
        ext[0] = squeeze ? Squeeze() : 1.0f;
        ext[1] = menu.strip ? menu.left : back_buffer_ && d_.target ? d_.stretch : 1.0f;
        // a fade or tint over the whole frame, the whole height too: to the screen's edges exactly
        bool fill = back_buffer_ && d_.whole && flat && !d_.target && !d_.y.Empty() && d_.y.lo <= 1 &&
                    d_.y.hi >= d_.gh - 1;
        ext[2] = sides ? 2.0f : edges ? 1.0f : menu.strip ? 3.0f : fill ? 4.0f : 0.0f;
        ext[3] = sides ? (d_.u.lo + d_.u.hi) * 0.5f : menu.strip ? menu.right : shift;
        // a movie's frame (movies.cpp), its picture larger about the screen's middle at its own shape, its bars off
        // the screen. The quad's pixels a texel across and down: the game reads its frame's rows -17 to 497 over the
        // frame's 480, squashing it
        float mx, my;
        auto per = [](const Span& px, const Span& tex) {
            float k = !px.Empty() && !tex.Empty() && tex.hi > tex.lo ? (px.hi - px.lo) / (tex.hi - tex.lo) : 1.0f;
            return k > 0.8f && k < 1.25f ? k : 1.0f;
        };
        if (back_buffer_ && d_.whole && d_.textured && d_.sampled && !d_.target && !sides && !edges && !menu.strip &&
            MoviesScale(d_.tex, squeeze ? Squeeze() : 1.0f, per(d_.x, d_.box_u), per(d_.y, d_.box_v), mx, my)) {
            ext[0] *= mx;
            ext[1] *= my;
        }
        if (Backdrop(vsc)) ext[0] = ext[1] = 0;  // not drawn: every vertex at the centre
        // CV_WATER_HIDE: the water's draws (Water) left out, to see which they are
        static const bool hide_water = std::getenv("CV_WATER_HIDE") != nullptr;
        if (hide_water && Water()) ext[0] = ext[1] = 0;
        else if (back_buffer_ && d_.world && d_.mesh && vsc) SeeWorld(vsc, ext[0]);
        // CV_WIDE_TRACE=<n>: every draw of a frame in n, the target it goes into ("device": the back buffer)
        if (trace_now_ && d_.world)
            LOGR("wide %llu: into %08x%s 3D, n %u tex %ux%u %016llx blend %08x%s%s%s", (unsigned long long)frame_,
                 trace_rt_, trace_device_ ? " device" : "", d_.count, d_.tex.width, d_.tex.height,
                 (unsigned long long)d_.name, d_.blend, d_.zwrite ? " zwrite" : "", d_.ntex ? " pack " : "",
                 d_.ntex ? TexturePackName(d_.texs[0]).c_str() : "");
        else if (trace_now_ && d_.squeeze)
            LOGR("wide %llu: into %08x%s n %u x %.0f..%.0f y %.0f..%.0f tex %ux%u at %08x %016llx u %g..%g%s blend %u%s%s: %s%s",
                 (unsigned long long)frame_, trace_rt_, trace_device_ ? " device" : "", d_.count, d_.x.lo, d_.x.hi,
                 d_.y.lo, d_.y.hi, d_.tex.width, d_.tex.height, d_.tex.data, (unsigned long long)d_.name, d_.u.lo, d_.u.hi,
                 d_.target ? " target" : "", src_blend, d_.ntex ? " pack " : "",
                 d_.ntex ? TexturePackName(d_.texs[0]).c_str() : "", d_.whole ? "whole, " : "",
                 sides ? "to the sides" : edges ? "edges out" : menu.strip ? "menu strip" : menu.backdrop ? "menu backdrop"
                 : shift < 0 ? (menu_ ? "menu left" : "hud left") : shift > 0 ? (menu_ ? "menu right" : "hud right")
                 : squeeze ? "squeezed" : "kept");
        // CV_WIDE_ODD: the draws drawn at the console's shape, each kind once (an enemy's spawn: the 4:3 shape it
        // showed before taking the screen's)
        static const bool odd = std::getenv("CV_WIDE_ODD") != nullptr;
        if (odd) Odd(rt, ext[0]);
    }

    // CV_WIDE_ODD's: the 3D drawn into a target of its own; a mesh drawn onto the console frame by a program without
    // the camera; a draw into the back buffer left unsqueezed; one that reads a target into it
    std::set<uint64_t> odd_;
    uint64_t odd_trace_until_ = 0;
    int odd_traces_ = 0;
    // the targets the console frame's size, not the frame, that the 3D was drawn into, by address, and the frame
    std::unordered_map<uint32_t, uint64_t> offscreen3d_;
    void Odd(const gpu::Target* rt, float sx) {
        int kind = d_.world && !back_buffer_                                     ? 1
                   : back_buffer_ && !d_.world && d_.program && d_.count > 32    ? 2
                   : back_buffer_ && d_.squeeze && app::Widescreen() && sx == 1.0f ? 3
                   : back_buffer_ && d_.target                                   ? 4
                                                                                 : 0;
        if (!kind) return;
        // CV_WIDE_ODD=2: the frames after the 3D drawn apart traced whole (CV_WIDE_TRACE's lines)
        static const int odd_level = std::atoi(std::getenv("CV_WIDE_ODD"));
        if (kind == 1 && odd_level >= 2 && odd_traces_ < 6 && frame_ > odd_trace_until_) {
            odd_traces_++;
            odd_trace_until_ = frame_ + 2;
            trace_now_ = true;
        }
        uint64_t key = uint64_t(kind) << 60 ^ d_.program * 31 ^ d_.name * 7 ^
                       (rt ? uint64_t(rt->gw) << 40 ^ uint64_t(rt->gh) << 20 : 0) ^ (d_.target ? 1 : 0) ^
                       uint64_t(d_.src_w) << 8 ^ uint64_t(d_.src_h) << 24 ^ (d_.skinned ? 2 : 0) ^ (d_.zwrite ? 4 : 0);
        if (odd_.size() > 600 || !odd_.insert(key).second) return;
        int room = RoomId();
        LOGR("wide odd %d, frame %llu room %d.%d: into %08x %ux%u%s (guest %ux%u), n %u, program %016llx%s%s, tex "
             "%ux%u %016llx%s%s, from target %08x %ux%u, blend %08x, x %.0f..%.0f y %.0f..%.0f, squeeze %.3f%s",
             kind, (unsigned long long)frame_, room >> 8, room & 0xFF, rt ? rt->data : 0, rt ? rt->gw : 0,
             rt ? rt->gh : 0, trace_device_ ? " device" : "", guest_w_, guest_h_, d_.count,
             (unsigned long long)d_.program, d_.world ? " 3D" : "", d_.skinned ? " skinned" : "", d_.tex.width,
             d_.tex.height, (unsigned long long)d_.name, d_.textured ? "" : " untextured", d_.zwrite ? " zwrite" : "",
             d_.src_data, d_.src_w, d_.src_h, d_.blend, d_.x.lo, d_.x.hi, d_.y.lo, d_.y.hi, sx,
             d_.whole ? " whole" : "");
    }

    // Frame generation's view (gpu::FrameView): the world drawn with the camera's view screen matrix and an object's
    // world matrix (c85..c88: PS2 screen coordinates t = M v, then p = c94 * t.xyz / t.w + c95, console pixels). For a
    // mesh that held still, M before times M now inverted is the camera's move in t, the same for every such mesh
    // whatever its world matrix: of the meshes drawn in both frames, the move most of them agree on. Then the clip
    // space the renderer draws (x times the squeeze, the depth buffer's reversed z) round it, from this frame's clip
    // to the frame before's
    bool View(gpu::FrameView& v) override {
        static const bool trace = std::getenv("XBE_FG_TRACE") != nullptr;
        auto no = [&](const char* why, size_t a = 0, size_t b = 0) {
            if (trace) LOGR("wide: frame %llu no view: %s %zu %zu", (unsigned long long)frame_, why, a, b);
            return false;
        };
        if (!cam_now_.have || seen_now_.empty() || seen_prev_.empty())
            return no("no world draws now or before", seen_now_.size(), seen_prev_.size());
        struct Cand {
            uint32_t count;
            float r[16];
        };
        std::vector<Cand> cands;
        for (const auto& [key, now] : seen_now_) {
            if (now.dup) continue;
            auto it = seen_prev_.find(key);
            if (it == seen_prev_.end() || it->second.dup) continue;
            float inv[16];
            if (!Invert(now.m, inv)) continue;
            Cand c{now.count, {}};
            Mul(it->second.m, inv, c.r);
            cands.push_back(c);
        }
        if (cands.empty()) return no("no mesh in both frames", seen_now_.size(), seen_prev_.size());
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.count > b.count; });
        if (cands.size() > 24) cands.resize(24);
        auto close = [](const float* a, const float* b) {
            float big = 0, diff = 0;
            for (int i = 0; i < 16; i++) {
                big = std::max(big, std::fabs(a[i]));
                diff = std::max(diff, std::fabs(a[i] - b[i]));
            }
            return diff <= 2e-3f * std::max(big, 1e-6f);
        };
        size_t best = 0, support = 0;
        for (size_t i = 0; i < cands.size(); i++) {
            size_t n = 0;
            for (size_t j = 0; j < cands.size(); j++) n += close(cands[i].r, cands[j].r);
            if (n > support) {
                support = n;
                best = i;
            }
        }
        // one mesh alone is believed only as the only one there is
        if (support < 2 && cands.size() > 1) return no("the meshes disagree", support, cands.size());
        const Cam& c = cam_now_;
        float gw = float(guest_w_), gh = float(guest_h_), z = std::max(zscale_, 1.0f);
        if (std::fabs(c.s[0]) < 1e-12f || std::fabs(c.s[1]) < 1e-12f || std::fabs(c.s[2]) < 1e-12f) return false;
        // t to the renderer's clip space (shaders.cpp's epilogue, ReversedClip, the squeeze of Position)
        float to_clip[16] = {c.sq * 2 * c.s[0] / gw, 0, 0, c.sq * (2 * c.o[0] / gw - 1),
                             0, -2 * c.s[1] / gh, 0, 1 - 2 * c.o[1] / gh,
                             0, 0, -c.s[2] / z, 1 - c.o[2] / z,
                             0, 0, 0, 1};
        float from_clip[16], tmp[16];
        if (!Invert(to_clip, from_clip)) return false;
        Mul(cands[best].r, from_clip, tmp);
        Mul(to_clip, tmp, v.clip_to_prev);
        // depth: t.z = a t.w + b (the projection's z row), so the buffer holds k0 + k1 / w
        const float* m = c.m;
        float l2 = m[12] * m[12] + m[13] * m[13] + m[14] * m[14];
        if (l2 > 0) {
            float a = (m[8] * m[12] + m[9] * m[13] + m[10] * m[14]) / l2, b = m[11] - a * m[15];
            float k0 = 1 - (c.s[2] * a + c.o[2]) / z, k1 = -c.s[2] * b / z;
            float n = k0 != 0 ? -k1 / k0 : 0, f = k0 != 1 ? -k1 / (k0 - 1) : 0;
            if (n > 0 && f > n) {
                v.near_w = n;
                v.far_w = f;
            }
        }
        // the camera, as the largest mesh's matrix has it (a level's, with no world matrix of its own)
        float fwd[3] = {m[12], m[13], m[14]};
        if (Normalize(fwd)) {
            std::copy(fwd, fwd + 3, v.forward);
            float d = m[4] * fwd[0] + m[5] * fwd[1] + m[6] * fwd[2];
            float up[3] = {-(m[4] - d * fwd[0]), -(m[5] - d * fwd[1]), -(m[6] - d * fwd[2])};  // screen y runs down
            if (Normalize(up)) {
                std::copy(up, up + 3, v.up);
                // the screen's x as the matrix has it, square to the others (ray tracing turns its normals into the
                // world by these three); else the one square to both
                float dx = m[0] * fwd[0] + m[1] * fwd[1] + m[2] * fwd[2];
                float rx[3] = {m[0] - dx * fwd[0], m[1] - dx * fwd[1], m[2] - dx * fwd[2]};
                float ux = rx[0] * up[0] + rx[1] * up[1] + rx[2] * up[2];
                for (int i = 0; i < 3; i++) rx[i] -= ux * up[i];
                if (Normalize(rx)) {
                    std::copy(rx, rx + 3, v.right);
                } else {
                    v.right[0] = up[1] * fwd[2] - up[2] * fwd[1];
                    v.right[1] = up[2] * fwd[0] - up[0] * fwd[2];
                    v.right[2] = up[0] * fwd[1] - up[1] * fwd[0];
                }
            }
            // where rows 0, 1 and 3 are all 0: the camera (a 3 x 3 solve, written as a 4 x 4 one)
            float inv[16];
            float rows[16] = {m[0], m[1], m[2], 0, m[4], m[5], m[6], 0, m[12], m[13], m[14], 0, 0, 0, 0, 1};
            if (Invert(rows, inv)) {
                float rhs[3] = {-m[3], -m[7], -m[15]};
                for (int i = 0; i < 3; i++) v.position[i] = inv[i * 4] * rhs[0] + inv[i * 4 + 1] * rhs[1] + inv[i * 4 + 2] * rhs[2];
            }
        }
        // the projection: c68..c71 when it is one (its w row 0 0 1 0), into the same clip space
        if (c.p_ok) {
            Mul(to_clip, c.p, v.view_to_clip);
            float fy = c.p[5];
            if (fy != 0) v.fov_y = 2 * std::atan(gh * 0.5f / std::fabs(c.s[1] * fy));
        } else {
            for (int i = 0; i < 16; i++) v.view_to_clip[i] = i % 5 == 0 ? 1.0f : 0.0f;
        }
        v.valid = true;
        v.cut = false;
        return true;
    }

private:
    // a world matrix draw of this frame: its matrix (c85..c88, rows) and how many vertices it has; dup when the same
    // mesh was drawn twice, which then tells nothing
    struct Mesh {
        float m[16];
        uint32_t count;
        bool dup;
    };
    // the frame's camera constants, as its last world draw had them
    struct Cam {
        bool have = false;
        float s[3] = {}, o[3] = {};  // c94, c95
        float m[16] = {};            // the largest mesh's c85..c88
        uint32_t count = 0;
        float p[16] = {};            // c68..c71
        bool p_ok = false;
        float sq = 1;                // the squeeze in x the world draws had
    };
    void SeeWorld(float (*vsc)[4], float squeeze) {
        Mesh n{};
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++) n.m[r * 4 + c] = vsc[kCameraWorld + r][c];
        n.count = d_.count;
        auto [it, fresh] = seen_now_.try_emplace(d_.mesh ^ d_.name * 0x100000001B3ull, n);
        if (!fresh) it->second.dup = true;
        Cam& c = cam_now_;
        for (int i = 0; i < 3; i++) {
            c.s[i] = vsc[kScreenScale][i];
            c.o[i] = vsc[kScreenOffset][i];
        }
        c.sq = squeeze;
        if (!c.have || n.count > c.count) {
            std::copy(n.m, n.m + 16, c.m);
            c.count = n.count;
        }
        for (int r = 0; r < 4; r++)
            for (int k = 0; k < 4; k++) c.p[r * 4 + k] = vsc[kCamera + r][k];
        c.p_ok = c.p[12] == 0 && c.p[13] == 0 && c.p[14] != 0 && c.p[15] == 0;
        c.have = true;
    }
    static void Mul(const float* a, const float* b, float* out) {
        float r[16];
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
        std::copy(r, r + 16, out);
    }
    static bool Normalize(float* v) {
        float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (!(l > 0)) return false;
        for (int i = 0; i < 3; i++) v[i] /= l;
        return true;
    }
    // a 4 x 4 inverse, in double for the PS2 matrices' range (screen scale with depth in the millions)
    static bool Invert(const float* f, float* out) {
        double m[16], inv[16];
        for (int i = 0; i < 16; i++) m[i] = f[i];
        inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
        inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
        inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
        inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
        inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
        inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
        inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
        inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
        inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
        inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
        inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
        inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
        inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
        inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
        double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
        if (!(std::fabs(det) > 1e-300)) return false;
        for (int i = 0; i < 16; i++) out[i] = float(inv[i] / det);
        return true;
    }

    // a HUD draw on the console frame, and where it went: a part of the frame's width
    struct Rect {
        float x0, x1, y0, y1;
        bool named;  // with one of the HUD's textures
        float shift;
    };
    static bool Touch(const Rect& a, const Rect& b) {
        constexpr float kGap = 2;
        return a.x0 <= b.x1 + kGap && b.x0 <= a.x1 + kGap && a.y0 <= b.y1 + kGap && b.y0 <= a.y1 + kGap;
    }
    // a piece of the HUD at least twice as far from one side of the frame as from the other, to that side
    float Side(const Rect& r) const {
        float left = std::max(r.x0, 0.0f), right = std::max(gw_ - r.x1, 0.0f);
        float k = Squeeze();
        return left * 2 < right ? k - 1 : right * 2 < left ? 1 - k : 0.0f;
    }

    // at a frame's start: the frame before's HUD draws, gathered into the pieces they touch one another in
    void HudPieces() {
        pieces_.clear();
        size_t n = rects_.size();
        std::vector<size_t> up(n);
        for (size_t i = 0; i < n; i++) up[i] = i;
        auto root = [&](size_t i) {
            while (up[i] != i) i = up[i] = up[up[i]];
            return i;
        };
        for (size_t i = 0; i < n; i++)
            for (size_t j = i + 1; j < n; j++)
                if (Touch(rects_[i], rects_[j])) up[root(i)] = root(j);
        std::vector<int> piece(n, -1);
        for (size_t i = 0; i < n; i++) {
            size_t r = root(i);
            if (piece[r] < 0) {
                piece[r] = int(pieces_.size());
                pieces_.push_back(rects_[i]);
            }
            Rect& p = pieces_[size_t(piece[r])];
            p.x0 = std::min(p.x0, rects_[i].x0);
            p.x1 = std::max(p.x1, rects_[i].x1);
            p.y0 = std::min(p.y0, rects_[i].y0);
            p.y1 = std::max(p.y1, rects_[i].y1);
            p.named |= rects_[i].named;
        }
        for (Rect& p : pieces_) p.shift = p.named ? Side(p) : 0.0f;
        rects_.clear();
        if (frame_ % 60 == 0) std::erase_if(hud_kept_, [&](const auto& k) { return frame_ - k.second.seen > kHudKeep; });
    }

    // a HUD draw's move: as it moved the frame before, while it is drawn frame after frame in its place (key); a draw
    // new to the HUD as the piece of the frame before it is in, else as a draw of this frame it touches, else a quad
    // of the HUD's own textures by itself (a piece's first frame). A part of the HUD keeps its side while it is up: a
    // boss's health bar coming up against the Innocent Devils' panel at the bottom left joined them into one piece
    // across the frame, which went back to the 4:3 middle, and as the bar's parts came and went the panel jumped
    // between the two from frame to frame
    float HudShift(Rect r, bool quad, uint64_t key) {
        gw_ = d_.gw;
        r.shift = 0;
        auto kept = hud_kept_.find(key);
        bool found = kept != hud_kept_.end() && frame_ - kept->second.seen <= kHudKeep;
        if (found) r.shift = kept->second.shift;
        for (size_t i = 0; !found && i < pieces_.size(); i++)
            if (Touch(pieces_[i], r)) {
                r.shift = pieces_[i].shift;
                found = true;
            }
        for (size_t i = 0; !found && i < rects_.size(); i++)
            if (Touch(rects_[i], r)) {
                r.shift = rects_[i].shift;
                found = true;
            }
        if (!found && r.named && quad) r.shift = Side(r);
        if (hud_kept_.size() < 1024) hud_kept_[key] = {r.shift, frame_};
        if (rects_.size() < 256) rects_.push_back(r);
        return r.shift;
    }
    // each HUD draw's move by its key (HudShift), and the frame it was last drawn: kept while it is drawn at least
    // every kHudKeep frames
    struct HudKept {
        float shift;
        uint64_t seen;
    };
    std::unordered_map<uint64_t, HudKept> hud_kept_;
    static constexpr uint64_t kHudKeep = 90;

    // the pause menu (menu_spread). Its sides: -1 left, 0 stays (the page buttons), 1 right, 2 a strip across it,
    // stretched (its title bar: the page's name, a bar and a line under them, drawn as tiles that meet)
    static constexpr int kMenuStrip = 2;
    static constexpr float kMenuBackdrop = 100;  // console pixels: a draw this wide and tall can be the backdrop's
    static constexpr size_t kMenuMost = 512;      // menu draws kept a frame
    static constexpr float kMenuTitleBand = 0.23f;  // the title bar's bottom, a part of the frame's height, and more
    struct MenuRect {
        Rect r;
        uint64_t name;
        int side;
    };
    struct MenuMove {
        bool strip = false, backdrop = false;
        float shift = 0;            // clip space
        float left = 0, right = 0;  // a strip's moves left and right of the frame's middle, clip space
    };
    static float Area(const Rect& a) { return std::max(a.x1 - a.x0, 0.0f) * std::max(a.y1 - a.y0, 0.0f); }
    static float Overlap(const Rect& a, const Rect& b) {
        return std::max(std::min(a.x1, b.x1) - std::max(a.x0, b.x0), 0.0f) *
               std::max(std::min(a.y1, b.y1) - std::max(a.y0, b.y0), 0.0f);
    }
    // side by side, or one above the other, edge on edge
    static bool Abut(const Rect& a, const Rect& b) {
        constexpr float kGap = 1.5f;
        bool rows = std::min(a.y1, b.y1) > std::max(a.y0, b.y0), cols = std::min(a.x1, b.x1) > std::max(a.x0, b.x0);
        return (rows && (std::fabs(a.x1 - b.x0) <= kGap || std::fabs(a.x0 - b.x1) <= kGap)) ||
               (cols && (std::fabs(a.y1 - b.y0) <= kGap || std::fabs(a.y0 - b.y1) <= kGap));
    }
    static bool Same(const Rect& a, const Rect& b) {
        return std::fabs(a.x0 - b.x0) <= 0.5f && std::fabs(a.x1 - b.x1) <= 0.5f && std::fabs(a.y0 - b.y0) <= 0.5f &&
               std::fabs(a.y1 - b.y1) <= 0.5f;
    }
    // a group by its span alone: half the frame wide, a strip; else by its middle, the frame's middle tenth stays
    static int MenuSide(float x0, float x1, float gw) {
        if (x1 - x0 >= 0.5f * gw) return kMenuStrip;
        float mid = (x0 + x1) * 0.5f;
        return mid < 0.45f * gw ? -1 : mid > 0.55f * gw ? 1 : 0;
    }

    // at a frame's start: the frame before's menu draws gathered into groups, each group's side, and each side's
    // move, the most every group on it has room for (a strip has both sides). A group: draws of one texture that
    // touch (a word's letters, a list's rows) or share a row close by (a row of figures, icons and their counts);
    // and a draw onto the one before it it covers most (the cursor on its button, a title on its bar). A strip is
    // not joined by its texture, only by what is drawn onto it
    void MenuGroups() {
        menu_before_.swap(menu_rects_);
        menu_rects_.clear();
        menu_back_.clear();
        menu_move_[0] = menu_move_[1] = 0;
        menu_plain_before_ = menu_plain_;
        menu_plain_ = false;
        if (menu_plain_before_) {
            if (trace_now_) LOGR("wide %llu: menu plain: a sub-menu, its panels across it", (unsigned long long)frame_);
            return;
        }
        size_t n = menu_before_.size();
        if (!n) return;
        float gw = menu_gw_;
        std::vector<size_t> up(n);
        for (size_t i = 0; i < n; i++) up[i] = i;
        auto root = [&](size_t i) {
            while (up[i] != i) i = up[i] = up[up[i]];
            return i;
        };
        for (size_t i = 0; i < n; i++)
            for (size_t j = i + 1; j < n; j++) {
                const MenuRect& a = menu_before_[i];
                const MenuRect& b = menu_before_[j];
                bool same = a.name && a.name == b.name && a.r.x1 - a.r.x0 < 0.5f * gw && b.r.x1 - b.r.x0 < 0.5f * gw;
                bool row = std::fabs(a.r.y0 - b.r.y0) <= 1.5f && std::fabs(a.r.y1 - b.r.y1) <= 1.5f &&
                           std::max(a.r.x0, b.r.x0) - std::min(a.r.x1, b.r.x1) <= 0.1f * gw;
                if (same && (Touch(a.r, b.r) || row)) up[root(i)] = root(j);
            }
        // a draw onto those before it joins the one it covers most, only: the cursor on the Options button also lies
        // over the figures of EXP and Next, which would join the stats column to the buttons
        for (size_t j = 1; j < n; j++) {
            const Rect& b = menu_before_[j].r;
            size_t most = n;
            float best = 0.2f * Area(b);
            for (size_t i = 0; i < j; i++)
                if (float o = Overlap(menu_before_[i].r, b); o > 0 && o >= best) {
                    best = o;
                    most = i;
                }
            if (most < n) up[root(most)] = root(j);
        }
        std::vector<float> lo(n, 1e30f), hi(n, -1e30f);
        for (size_t i = 0; i < n; i++) {
            size_t r = root(i);
            lo[r] = std::min(lo[r], menu_before_[i].r.x0);
            hi[r] = std::max(hi[r], menu_before_[i].r.x1);
        }
        // the screen's extra width at each side, in console pixels, and the room each side's groups leave: of what
        // lies on the frame (a list's rows that run off its right edge run off the screen's)
        float k = Squeeze();
        float margin = (1 - k) * gw / (2 * k);
        float most = 0.7f * margin, pad = 0.12f * margin;
        float room[2] = {most, most};
        for (size_t i = 0; i < n; i++) {
            size_t r = root(i);
            int side = MenuSide(lo[r], hi[r], gw);
            menu_before_[i].side = side;
            if (r != i) continue;
            if (side < 0 || side == kMenuStrip) room[0] = std::min(room[0], std::max(lo[r], 0.0f) + margin - pad);
            if (side == 1 || side == kMenuStrip) room[1] = std::min(room[1], gw + margin - pad - std::min(hi[r], gw));
            if (trace_now_)
                LOGR("wide %llu: menu group x %.0f..%.0f: %s", (unsigned long long)frame_, lo[r], hi[r],
                     side == kMenuStrip ? "strip" : side < 0 ? "left" : side > 0 ? "right" : "stays");
        }
        menu_move_[0] = std::max(room[0], 0.0f);
        menu_move_[1] = std::max(room[1], 0.0f);
        if (trace_now_)
            LOGR("wide %llu: menu moves %.1f left, %.1f right (console pixels)", (unsigned long long)frame_,
                 menu_move_[0], menu_move_[1]);
    }

    // a menu draw's move: the backdrop's large pieces stay; else as its group of the frame before (the same texture
    // in the same place), or as the draw of this frame it lies on, or as a draw of its texture it touches the frame
    // before (a figure that changed, the cursor moved on), else by its own span
    MenuMove MenuPlace(const Rect& r, uint64_t name) {
        float gw = menu_gw_ = d_.gw;
        MenuMove m;
        // a sub-menu (Item, Equip, Skill, Tips, Options, Summon): a panel across it below the title bar (its
        // description's, a list's frame), drawn before what is on it. Its page stays as the game draws it, this frame
        // on and the next
        if (r.x1 - r.x0 >= 0.5f * gw && r.y1 > kMenuTitleBand * d_.gh) menu_plain_ = true;
        if (menu_plain_ || menu_plain_before_) {
            if (menu_rects_.size() < kMenuMost) menu_rects_.push_back({r, name, 0});
            return m;
        }
        if (r.x1 - r.x0 >= kMenuBackdrop && r.y1 - r.y0 >= kMenuBackdrop)
            for (const Rect& b : menu_back_)
                if (Abut(r, b)) {
                    if (menu_back_.size() < kMenuMost) menu_back_.push_back(r);
                    m.backdrop = true;
                    return m;
                }
        int side = 0;
        bool found = false;
        for (const MenuRect& p : menu_before_)
            if (p.name == name && Same(p.r, r)) {
                side = p.side;
                found = true;
                break;
            }
        if (!found) {
            float best = 0.2f * Area(r);
            for (const MenuRect& p : menu_rects_)
                if (float o = Overlap(p.r, r); o > 0 && o >= best) {
                    best = o;
                    side = p.side;
                    found = true;
                }
        }
        for (const MenuRect& p : menu_before_)
            if (!found && name && p.name == name && Touch(p.r, r)) {
                side = p.side;
                found = true;
            }
        if (!found) side = MenuSide(r.x0, r.x1, gw);
        if (menu_rects_.size() < kMenuMost) menu_rects_.push_back({r, name, side});
        // console pixels to clip space as the squeeze draws them: 2 / gw a pixel, times the squeeze
        float px = 2 * Squeeze() / gw;
        if (side == kMenuStrip) {
            m.strip = true;
            m.left = -menu_move_[0] * px;
            m.right = menu_move_[1] * px;
        } else {
            m.shift = (side < 0 ? -menu_move_[0] : side > 0 ? menu_move_[1] : 0.0f) * px;
        }
        return m;
    }

    // a texture by its first bytes (its indices), size and format: the same picture wherever the game loads it
    static uint64_t Name(const gpu::TextureDesc& d, const uint8_t* guest) {
        uint32_t n = std::min<uint32_t>(gpu::LevelSize(d, 0), 16384);
        uint64_t shape[2] = {uint64_t(d.width) << 32 | d.height, d.format};
        return gpu::Hash(guest, n, gpu::Hash(shape, sizeof(shape)));
    }

    // a 3D draw's depth through the camera, while the view is pushed out (Backdrop)
    void Distance(const gpu::State& s, const gpu::Assembled& a, const Program& p) {
        uint32_t room = 0;
        backdrop_ = g_sky_backdrops && ViewBackdrop(fog_game_, fog_drawn_, far_z_, room);
        if (!backdrop_) return;
        if (room != room_) {
            SkyCandidates();
            room_ = room;
            room_far_ = far_z_;
            room_frame_ = frame_;
        }
        uint32_t m = p.world ? kCameraWorld : kCamera;
        const auto& c = s.vsc[m + 3];
        uint32_t ahead = 0, past = 0;
        d_.w = Read(a, 0, [&](const float* v, uint32_t comps) {
            float w = v[0] * c[0] + v[1] * c[1] + v[2] * c[2] + (comps == 4 ? v[3] : 1.0f) * c[3];
            ahead += w > 0;
            past += w > far_z_;
            return w;
        });
        d_.past = ahead ? float(past) / float(ahead) : 0.0f;
    }

    // CV_SKY_TRACE: the textures of a room none of whose draws came nearer than mostly past its far plane, the
    // candidates for kSky, logged as the next room starts
    void SkyCandidates() {
        if (kSkyTrace && room_)
            for (const auto& [name, seen] : seen_)
                if (seen.min_past >= 0.3f && !Sky(name))
                    LOGR("wide: sky candidate in room %u (from frame %llu, far %.0f): %016llx, %u draws, all %.0f%% or "
                         "more past it, out to %.0f",
                         room_, (unsigned long long)room_frame_, room_far_, (unsigned long long)name, seen.draws,
                         seen.min_past * 100, seen.max_w);
        seen_.clear();
    }

    static bool Sky(uint64_t name) { return std::find(std::begin(kSky), std::end(kSky), name) != std::end(kSky); }

    // A sky's draw (kSky) gets the room's own fog, and one wholly past the room's own far plane is not drawn (true):
    // the sky is as the game shows it, and the rest of the room has the fog and far plane as set.
    bool Backdrop(float (*vsc)[4]) {
        if (!backdrop_ || !vsc || !d_.world || d_.w.Empty() || !d_.name) return false;
        if (kSkyTrace && d_.past > 0) {
            Seen& seen = seen_[d_.name];
            seen.draws++;
            seen.min_past = std::min(seen.min_past, d_.past);
            seen.max_w = std::max(seen.max_w, d_.w.hi);
        } else if (kSkyTrace) {
            seen_[d_.name].min_past = 0;
        }
        // CV_SKY_HIDE: every candidate's draw hidden, to see what they are
        if (kSkyHide) return d_.past >= 0.3f;
        if (!Sky(d_.name)) return false;
        auto same = [](float a, float b) { return b != 0 && std::fabs(a - b) <= 1e-4f * std::fabs(b); };
        float* fog = vsc[kFog];
        // c91 holds them over 128 (the PS2's fog units): (A / 128, B / 128, 0, 0)
        for (int i = 0; i < 4; i++)
            for (int k = 0; k < 2; k++)
                if (same(fog[i], fog_drawn_[k] / 128)) {
                    fog[i] = fog_game_[k] / 128;
                    break;
                }
        return d_.w.lo > far_z_;
    }

    Program Classify(const gpu::VertexShader& vs) {
        uint64_t key = gpu::Hash(vs.program.data(), vs.program.size() * 4);
        auto it = programs_.find(key);
        if (it != programs_.end()) return it->second;
        Program p;
        for (uint32_t r = 0; r < 4; r++)
            p.camera |= gpu::ReadsConstant(vs.program, kCamera + r) || gpu::ReadsConstant(vs.program, kCameraWorld + r);
        p.world = gpu::ReadsConstant(vs.program, kCameraWorld);
        p.screen = gpu::ReadsConstant(vs.program, kScreenScale) && gpu::ReadsConstant(vs.program, kScreenOffset);
        // an instruction's MAC op, bits 21 to 24 of its second word: 13 loads a0
        for (size_t n = 0; n + 4 <= vs.program.size(); n += 4) p.skinned |= ((vs.program[n + 1] >> 21) & 0xF) == 13;
        programs_.emplace(key, p);
        return p;
    }

    // f(element, components) over the draw's vertices of input register reg, when it is 2 to 4 floats
    template <typename F>
    static Span Read(const gpu::Assembled& a, int reg, F f) {
        Span span;
        const gpu::VertexElement& e = a.elements[size_t(reg)];
        if (!(a.regs >> reg & 1) || e.buffer >= a.buffer_count) return span;
        const gpu::VertexBuffer& b = a.buffers[e.buffer];
        uint32_t comps = e.format == DXGI_FORMAT_R32G32B32A32_FLOAT ? 4
                         : e.format == DXGI_FORMAT_R32G32B32_FLOAT  ? 3
                         : e.format == DXGI_FORMAT_R32G32_FLOAT     ? 2
                                                                    : 0;
        if (!comps || !b.src) return span;
        for (uint32_t v = 0; v < a.vertex_count; v++) {
            uint64_t at = uint64_t(v) * b.stride + e.offset;
            if (at + comps * 4 > b.bytes) break;
            span.Add(f(reinterpret_cast<const float*>(b.src + at), comps));
        }
        return span;
    }

    bool trace_now_ = false;  // this frame's draws logged
    std::set<uint64_t> listed_;  // CV_WATER_LIST's, logged
    static inline const uint64_t kTrace =
        std::getenv("CV_WIDE_TRACE") ? std::max(1, std::atoi(std::getenv("CV_WIDE_TRACE"))) : 0;

    // the draw at hand
    struct Draw {
        bool squeeze = false, world = false, whole = false, edge = false, textured = false, target = false;
        bool sampled = false;
        bool card = false;     // samples a target smaller than the frame
        uint32_t src_data = 0, src_w = 0, src_h = 0;  // the first target it samples (CV_WIDE_ODD)
        bool skinned = false;  // a skinned program's (Program::skinned)
        bool additive = false;
        uint64_t program = 0;  // its vertex program's (Classify's key), for CV_WATER_LIST
        uint32_t matrix = 0;  // the first of the constants its vertices are put on the PS2's screen by (0: none)
        uint32_t blend = 0;   // its blend's source and destination factors (0: none), for the trace
        bool zwrite = false;  // it writes depth, for the trace
        uint32_t texs[4] = {};  // the textures it samples, for the census (texpack.cpp)
        int ntex = 0;
        uint64_t name = 0;  // the first texture's (Name), 0 for a target or one not seen uploaded
        uint64_t mesh = 0;  // a world matrix draw's indices or vertices, count and base, for View
        Span w;             // a 3D draw's depth, while the view is pushed out
        float past = 0;     // the part of its vertices ahead past the room's own far plane
        float stretch = 1;
        float gw = 640;     // the console frame's width
        float gh = 480;     // and height
        uint32_t count = 0;
        Span x, y, u;  // on the console frame; the first texture coordinate, for one quad on it
        Span box_u, box_v;  // a quad's texture coordinates, on the frame or not (TexelBox)
        gpu::TextureDesc tex;
    };
    Draw d_;
    bool back_buffer_ = false;
    // the frame so far: vertices of textured 3D drawn (the world's thousands; the map screen's rooms are flat colour),
    // a 4:3 picture over the whole frame
    static constexpr uint32_t kWorld = 1000;
    uint64_t frame_ = ~0ull;
    uint32_t guest_w_ = 640, guest_h_ = 480;
    uint32_t trace_rt_ = 0;  // the target's address, for the trace
    bool trace_device_ = false;
    uint32_t world_ = 0;
    bool picture_ = false;
    bool hud_ = false;       // the frame's HUD has begun
    bool hud_free_ = false;  // the HUD may go to the edges this frame
    bool modal_ = false;     // a window is up over the world this frame (ModalUp)
    bool clamp_ = false;     // the draw at hand reads its texture clamped (Clamp)
    bool card_free_ = false;  // an area's name card may
    bool menu_ = false;       // the pause menu is up this frame
    bool screen_ = false;     // a screen of its own: the pause menu, the map screen, the title (CameraSpecial)
    std::vector<MenuRect> menu_rects_, menu_before_;  // the pause menu's draws, this frame's and the frame before's
    std::vector<Rect> menu_back_;  // this frame's backdrop: its edge pieces, the large pieces against them
    float menu_move_[2] = {};      // from the frame before: the moves left and right, in console pixels
    bool menu_plain_ = false, menu_plain_before_ = false;  // a sub-menu's page, kept as drawn: this frame's, the one before's
    float menu_gw_ = 640;
    bool ui_ = false;        // the frame's UI has begun, for HDR (UiStart)
    std::vector<Rect> rects_, pieces_;  // this frame's HUD draws; the pieces of the frame before's
    // an area's name card, its crest, title and line drawn apart: all of it this frame, and the frame before's, which
    // goes to a side as one
    Rect card_now_{}, card_before_{};
    bool card_now_seen_ = false, card_before_seen_ = false;
    // frame generation's view: this frame's and the frame before's world draws, by mesh and texture; their camera
    std::unordered_map<uint64_t, Mesh> seen_now_, seen_prev_;
    Cam cam_now_;
    float zscale_ = 16777215.0f;
    float gw_ = 640;
    // backdrops (Backdrop): the room's own fog coefficients and far plane, the fog drawn, the backdrops' textures
    bool backdrop_ = false;
    float fog_game_[2] = {}, fog_drawn_[2] = {}, far_z_ = 0;
    uint32_t room_ = 0;
    float room_far_ = 0;
    uint64_t room_frame_ = 0;
    struct Seen {
        uint32_t draws = 0;
        float min_past = 1, max_w = 0;
    };
    std::unordered_map<uint64_t, Seen> seen_;  // CV_SKY_TRACE: each texture's draws in the room
    static inline const bool kSkyTrace = std::getenv("CV_SKY_TRACE") != nullptr;
    static inline const bool kSkyHide = std::getenv("CV_SKY_HIDE") != nullptr;
    Hooks hooks_;
    std::unordered_map<uint64_t, Program> programs_;
    std::unordered_map<uint32_t, uint64_t> names_;  // each texture's Name, by its guest address, as last uploaded
};

}  // namespace

// GameInfo::init: before the renderer is made
void WidescreenInit() { gpu::SetExtension(std::make_unique<Widescreen>()); }
