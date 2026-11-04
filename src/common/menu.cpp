// The in-game settings menu (F10) and the FPS counter, drawn with ImGui over the game.
//
// Every change applies at once and is saved to the settings file. While the menu is open the game gets no input:
// the keyboard and controller drive the menu instead. It stands at the screen's right, so the picture beside it shows
// each change as it is made; the setting in focus says what it does at its foot.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "app/app.h"
#include "core/input.h"
#include "gpu/backend.h"
#include "gpu/raytrace.h"

// game settings beside the runtime's, in the settings file (game_info.cpp lists them)
bool g_fps_counter = false;
bool g_pause_on_focus_loss = true;
uint32_t g_menu_key = VK_F10;  // virtual key that opens the menu
bool g_launcher_keep_open = false;  // the launcher's, kept when the game writes the file
bool g_lament_save = false;  // as with a Lament of Innocence save on the PS2's memory card (hooks.cpp)
bool g_ps2_stick = true;     // the sticks as the PS2 read them, without the Xbox's dead zone by axis (hooks.cpp)
bool g_invert_camera_x = false;  // the right stick turns the camera the other way across (keyboard.cpp)
bool g_invert_camera_y = false;  // and up and down
uint32_t g_fov = 100;            // percent of the game's field of view in play (game/view.cpp)
uint32_t g_draw_distance = 100;  // percent of the game's far plane (game/view.cpp)
uint32_t g_fog_distance = 100;   // percent of the game's fog distance, 0 for none (game/view.cpp)
bool g_hud_edges = true;         // widescreen: the HUD at the screen's corners rather than the 4:3 middle (widescreen.cpp)
// surfaces with an HD texture pack's material maps seen into: 0 off, 1 low, 2 high (the SDK's gpu::g_surface_depth)
uint32_t g_surface_depth = 2;
// the frame's 3D anti-aliased before its UI: 0 off, 1 AMD FSR, 2 NVIDIA DLSS (the SDK's gpu::g_antialias)
uint32_t g_antialias = 1;
// and drawn below the frame's size, made up to it: 0 native, 1 quality, 2 balanced, 3 performance (gpu::g_upscale)
uint32_t g_upscale = 0;
// AMD FSR's sharpening after it, percent (gpu::g_aa_sharpness)
uint32_t g_sharpening = 30;
// the 3D darkened where surfaces meet: 0 off, 1 low, 2 high (gpu::g_ambient_occlusion)
uint32_t g_ambient_occlusion = 1;
// ray tracing over the frame's 3D: 0 off, 1 its ambient occlusion ray traced (gpu::g_ray_tracing)
uint32_t g_ray_tracing = 0;
extern bool g_menu_spread;       // widescreen.cpp: the pause menu's columns toward the screen's sides
extern bool g_hd_movies;         // movies.cpp: a movie pack's HD movies drawn
extern uint32_t g_movie_view;    // movies.cpp: 0 the game's 4:3, 1 the picture whole as large as it goes, 2 filled
extern uint32_t g_button_layout;    // texpack.cpp
bool ViewFov(double& game, double& drawn);  // game/view.cpp: the field of view in degrees, the game's and as drawn
extern std::atomic<bool> g_wide_trace_frame;  // widescreen.cpp: log the next frame's draws

namespace {

std::atomic<bool> g_open{false};
bool g_scaled = false;
app::FrameGenState g_fg_seen;  // frame generation's last state while it was generating (the menu pauses it)

std::string KeyName(uint32_t vk);  // the key as the keyboard labels it

struct Res {
    uint32_t w, h;
};

// the window's monitor's modes, largest first, with 0x0 for the desktop's own, and 1920x1200 for a 16:10 window
// (or a 16:10 screen streamed to) on a monitor without that mode
std::vector<Res> Resolutions() {
    std::vector<Res> list{{0, 0}};
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(static_cast<HWND>(app::Window()), MONITOR_DEFAULTTOPRIMARY), &mi);
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    auto add = [&](uint32_t w, uint32_t h) {
        bool seen = false;
        for (const Res& r : list) seen |= r.w == w && r.h == h;
        if (!seen) list.push_back({w, h});
    };
    for (DWORD i = 0; EnumDisplaySettingsW(mi.szDevice, i, &dm); i++)
        if (dm.dmPelsWidth >= 640 && dm.dmPelsHeight >= 480) add(dm.dmPelsWidth, dm.dmPelsHeight);
    add(1920, 1200);
    std::sort(list.begin() + 1, list.end(), [](Res a, Res b) { return a.w != b.w ? a.w > b.w : a.h > b.h; });
    return list;
}

std::string ResName(Res r) {
    if (!r.w) {
        app::Mode d = app::Desktop();
        return "Desktop (" + std::to_string(d.width) + "x" + std::to_string(d.height) + ")";
    }
    return std::to_string(r.w) + "x" + std::to_string(r.h);
}

// what the setting in focus (a controller's or the keyboard's) or under the mouse does, for the menu's foot: the
// row's help as Row set it, taken by its control (Track) or its name
const char* g_row_help = nullptr;   // the row being drawn
const char* g_focus_help = nullptr;  // this frame's, if any
std::string g_shown_help;            // the last one, kept while a drop-down's list is open

void Track() {
    if (ImGui::IsItemFocused() || ImGui::IsItemHovered()) g_focus_help = g_row_help;
}

bool Check(const char* id, bool* v) {
    bool changed = ImGui::Checkbox(id, v);
    Track();
    return changed;
}

bool SliderI(const char* id, int* v, int lo, int hi, const char* fmt, ImGuiSliderFlags flags) {
    bool changed = ImGui::SliderInt(id, v, lo, hi, fmt, flags);
    Track();
    return changed;
}

// a combo over a list of values; true when the choice changed
template <typename T>
bool Choice(const char* label, T& value, const std::vector<T>& values, const std::vector<std::string>& names) {
    size_t cur = 0;
    for (size_t i = 0; i < values.size(); i++)
        if (values[i] == value) cur = i;
    bool changed = false;
    bool open = ImGui::BeginCombo(label, names[cur].c_str());
    Track();
    if (open) {
        for (size_t i = 0; i < values.size(); i++) {
            if (ImGui::Selectable(names[i].c_str(), i == cur) && i != cur) {
                value = values[i];
                changed = true;
            }
            if (i == cur) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

void Row(const char* name, const char* help) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(name);
    g_row_help = help;
    if (ImGui::IsItemHovered()) g_focus_help = help;
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
}

bool BeginRows(const char* id) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
    ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch, 0.45f);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
    return true;
}

// HDR: the switch with the display's state beside it, then the two brightness levels, which take effect at once
void HdrRows(bool& save) {
    app::Settings& s = app::Get();
    app::HdrState h = app::Hdr();
    Row("HDR", "On a display with HDR on in Windows: fire, magic, light and the other brightest parts of the picture "
               "shine brighter than the rest, up to the peak brightness. The game's HUD is part of its picture, so its "
               "white text rises with them. Off sends the usual picture.");
    save |= Check("##hdr", &s.hdr);
    ImGui::SameLine();
    if (h.on)
        ImGui::TextDisabled("HDR output");
    else if (!h.display)
        ImGui::TextDisabled("HDR is off in Windows for this display");
    else
        ImGui::TextDisabled("This display is in HDR");
    ImGui::BeginDisabled(!h.on);
    Row("Peak brightness", "The brightest highlights, in nits. The display's own peak suits most; lower it if highlights "
                           "glare. Right-click for the display's own again.");
    {
        int peak = int(app::HdrPeak());
        const char* fmt = s.hdr_peak_nits ? "%d nits" : "%d nits (the display's)";
        if (SliderI("##peak", &peak, 400, 4000, fmt, ImGuiSliderFlags_AlwaysClamp)) {
            s.hdr_peak_nits = uint32_t(peak + 5) / 10 * 10;
            save = true;
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && s.hdr_peak_nits) {
            s.hdr_peak_nits = 0;
            save = true;
        }
    }
    Row("Scene brightness", "How bright the picture is apart from its highlights, and this menu, in nits. 200 suits "
                            "most rooms; more for a bright room, less for a dark one.");
    {
        int scene = int(s.hdr_scene_nits);
        if (SliderI("##scene", &scene, 80, 500, "%d nits", ImGuiSliderFlags_AlwaysClamp)) {
            s.hdr_scene_nits = uint32_t(scene);
            save = true;
        }
    }
    ImGui::EndDisabled();
}

void DisplayTab(bool& save) {
    app::Settings& s = app::Get();
    if (!BeginRows("display")) return;
    Row("Display mode", "Borderless fills the monitor without changing its mode and switches away instantly. "
                        "Fullscreen switches the monitor to the resolution below.");
    if (Choice("##mode", s.display_mode,
               {app::DisplayMode::Fullscreen, app::DisplayMode::Borderless, app::DisplayMode::Windowed},
               {"Fullscreen", "Borderless", "Windowed"})) {
        app::ApplyDisplay();
        save = true;
    }
    Row("Resolution", "The output size. Fullscreen switches the monitor to it; borderless always fills the monitor.");
    static std::vector<Res> modes = Resolutions();
    {
        size_t cur = 0;
        for (size_t i = 0; i < modes.size(); i++)
            if (modes[i].w == s.width && modes[i].h == s.height) cur = i;
        bool open = ImGui::BeginCombo("##res", ResName(modes[cur]).c_str());
        Track();
        if (open) {
            for (size_t i = 0; i < modes.size(); i++)
                if (ImGui::Selectable(ResName(modes[i]).c_str(), i == cur) && i != cur) {
                    s.width = modes[i].w;
                    s.height = modes[i].h;
                    app::ApplyDisplay();
                    save = true;
                }
            ImGui::EndCombo();
        }
    }
    // no frame rate cap here: the game's logic steps once a frame, so it is held at the 60 it was made for
    Row("V-Sync", "Off can tear, and lowers input latency a little.");
    save |= Check("##vsync", &s.vsync);
    HdrRows(save);
    ImGui::EndTable();
}

void GraphicsTab(bool& save) {
    app::Settings& s = app::Get();
    if (!BeginRows("graphics")) return;
    Row("Render scale", "Of the output height. Above 100% supersamples for a sharper, smoother picture.");
    save |= Choice<uint32_t>("##scale", s.render_percent, {50, 75, 100, 150, 200},
                             {"50%", "75%", "100%", "150%", "200%"});
    Row("Anisotropic filtering", "Sharper textures at an angle.");
    save |= Choice<uint32_t>("##aniso", s.aniso, {0, 2, 4, 8, 16}, {"Off", "2x", "4x", "8x", "16x"});
    Row("Anti-aliasing", "Smooth, steady edges on the 3D world, gathered over the frames: AMD FSR on any graphics card, "
                         "NVIDIA DLSS on a GeForce RTX (from the next start once chosen). Their files come from AMD and "
                         "NVIDIA: the launcher's Graphics page downloads them with frame generation's.");
    {
        static const uint32_t started = g_antialias;  // as this run started: DLSS needs it then
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        save |= Choice<uint32_t>("##aa", g_antialias, {0, 1, 2}, {"Off", "AMD FSR", "NVIDIA DLSS"});
        uint32_t running = gpu::g_antialias_running.load(std::memory_order_relaxed);
        ImGui::SameLine();
        if (g_antialias == 2 && running != 2 && started != 2)
            ImGui::TextDisabled("from the next start, AMD FSR until then");
        else if (g_antialias && running != g_antialias)
            ImGui::TextDisabled(running ? "AMD FSR in its place (logs\\castlevania.log)" : "not running (logs\\castlevania.log)");
    }
    Row("Upscaling", "The 3D world drawn below the screen's resolution and rebuilt to it by the anti-aliasing above, "
                     "for a higher frame rate on a slower graphics card: Quality draws two thirds of it across with "
                     "textures as sharp as the full resolution's, Balanced a little less of both, Performance half "
                     "with textures at that size's own detail, the lightest on a handheld. The HUD, menus and text "
                     "stay at the screen's resolution. Native draws it whole.");
    {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        save |= Choice<uint32_t>("##up", g_upscale, {0, 1, 2, 3}, {"Native", "Quality", "Balanced", "Performance"});
        ImGui::SameLine();
        uint32_t drawn = gpu::g_antialias_drawn.load(std::memory_order_relaxed);
        uint32_t out = gpu::g_antialias_output.load(std::memory_order_relaxed);
        if (!g_antialias) {
            ImGui::TextDisabled("needs anti-aliasing");
        } else if (g_upscale && drawn && drawn != out) {
            char text[64];
            std::snprintf(text, sizeof(text), "3D at %ux%u of %ux%u", drawn >> 16, drawn & 0xFFFF, out >> 16, out & 0xFFFF);
            ImGui::TextDisabled("%s", text);
        }
    }
    Row("Sharpening", "AMD FSR's sharpening of the 3D after it is smoothed: more brings back fine texture detail, too much "
                      "outlines edges. NVIDIA DLSS has no sharpening of its own.");
    {
        int sharp = int(g_sharpening);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        if (SliderI("##sharp", &sharp, 0, 100, sharp ? "%d%%" : "Off", ImGuiSliderFlags_AlwaysClamp)) {
            g_sharpening = uint32_t(sharp);
            save = true;
        }
        if (gpu::g_antialias_running.load(std::memory_order_relaxed) == 2) {
            ImGui::SameLine();
            ImGui::TextDisabled("not used by DLSS");
        }
    }
    Row("Ambient occlusion", "Soft shadow where surfaces meet: corners, the gaps between stones, under railings, where "
                             "feet touch the floor. Worked out from the 3D's depth before the anti-aliasing smooths it, "
                             "so it needs anti-aliasing on. Low is lighter on the graphics card.");
    {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        save |= Choice<uint32_t>("##ao", g_ambient_occlusion, {0, 1, 2}, {"Off", "Low", "High"});
        if (g_ambient_occlusion && !g_antialias) {
            ImGui::SameLine();
            ImGui::TextDisabled("needs anti-aliasing");
        }
    }
    Row("Ray tracing", "On a graphics card with ray tracing (DXR 1.1: NVIDIA RTX, AMD Radeon RX 6000 and later): the "
                       "ambient occlusion traced against the room's own 3D rather than worked out from the screen, so "
                       "what is off screen or behind something shades too. Needs ambient occlusion and anti-aliasing "
                       "on.");
    {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        save |= Choice<uint32_t>("##rt", g_ray_tracing, {0, 1}, {"Off", "Ambient occlusion"});
        if (g_ray_tracing && !gpu::g_ray_tracing_ready.load(std::memory_order_relaxed)) {
            ImGui::SameLine();
            ImGui::TextDisabled("not on this graphics card");
        } else if (g_ray_tracing && (!g_ambient_occlusion || !g_antialias)) {
            ImGui::SameLine();
            ImGui::TextDisabled("needs ambient occlusion and anti-aliasing");
        }
    }
    // DLSS turned into multi frame generation by the NVIDIA app slows the game (FrameGenSeen): said in its help too
    static std::string fg_help;
    bool fg_slow = g_fg_seen.kind == 2 && g_fg_seen.game_fps > 1 && g_fg_seen.game_fps < 57 &&
                   g_fg_seen.shown_fps > g_fg_seen.game_fps * 2.3;
    fg_help = "A frame made between each two the game draws, for twice the frame rate on a 120 Hz or faster screen: "
              "AMD FSR 3 on any graphics card, NVIDIA DLSS on a GeForce RTX 40 or 50. Their files come from AMD and "
              "NVIDIA: the launcher's Graphics page downloads them. From the next start.";
    if (fg_slow) {
        char line[256];
        std::snprintf(line, sizeof(line),
                      " Right now the NVIDIA app is making %.1f frames for each of the game's and holding the game at "
                      "%.0f a second, so it plays slowly: in the NVIDIA app, set Frame Generation to 2x for this game "
                      "(or use the app's setting), or use AMD FSR 3 here.",
                      g_fg_seen.shown_fps / g_fg_seen.game_fps, g_fg_seen.game_fps);
        fg_help += line;
    }
    Row("Frame generation", fg_help.c_str());
    {
        static const uint32_t running = s.frame_generation;  // as this run started
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        save |= Choice<uint32_t>("##fg", s.frame_generation, {0, 1, 2}, {"Off", "AMD FSR 3", "NVIDIA DLSS"});
        ImGui::SameLine();
        app::FrameGenState fg = app::FrameGen();
        if (s.frame_generation != running)
            ImGui::TextDisabled("from the next start");
        else if (running && fg.kind != running)
            ImGui::TextDisabled("not running (logs\\castlevania.log)");
        else if (fg_slow)
            ImGui::TextColored({1.0f, 0.65f, 0.25f, 1}, "slows the game: see below");
        else if (running)
            // the menu over the frame stops it: what counts is the frame rate once the menu is closed
            ImGui::TextDisabled("on (paused in this menu)");
    }
    Row("Widescreen", "More of the world at the sides, nothing stretched: 16:9 for a 16:9 screen, 16:10 for a 16:10 "
                      "one (1920x1200). The HUD and menus stay 4:3 in the middle. Off shows the game 4:3, as on a "
                      "4:3 TV.");
    {
        uint32_t wide = s.widescreen ? s.widescreen_aspect : 0;
        if (Choice<uint32_t>("##wide", wide, {0, 169, 1610}, {"Off (4:3)", "16:9", "16:10"})) {
            s.widescreen = wide != 0;
            if (wide) s.widescreen_aspect = wide;
            save = true;
        }
    }
    Row("HUD at the screen edges", "Widescreen: the health and magic bars and the rest of the HUD in the screen's "
                                   "corners, as on a 4:3 screen, rather than at the corners of the 4:3 middle.");
    save |= Check("##hud", &g_hud_edges);
    Row("Pause menu across the screen", "Widescreen: the pause menu's left column (Hector's stats, lists) toward the "
                                        "screen's left side and its right column (status, equipment) toward the right, "
                                        "the page buttons staying in the middle, rather than all of it in the 4:3 middle.");
    save |= Check("##menuspread", &g_menu_spread);
    Row("HD movies", "The story's movies from an HD movie pack (a movies folder beside the game), where it has them. "
                     "Off plays the game's own.");
    save |= Check("##hdmovies", &g_hd_movies);
    Row("Movie size", "Fit: the movie's picture as large as the screen shows it whole, without the black bars above "
                      "and below it. Fill: the whole screen, a little of the picture's top and bottom cut off on a "
                      "16:9 screen. Original: the game's 4:3 frame with its bars.");
    save |= Choice<uint32_t>("##movieview", g_movie_view, {1, 2, 0}, {"Fit", "Fill", "Original"});
    Row("Surface depth", "HD texture pack: walls, floors and rock seen into, their mortar, cracks and crevices sunk "
                         "below the surface and moving with the camera (parallax). Low is shallower; Off shades them "
                         "only, flat.");
    save |= Choice<uint32_t>("##depth", g_surface_depth, {0, 1, 2}, {"Off", "Low", "High"});
    Row("Field of view", "How much of the world the camera takes in: 100% is the game's own, which each room's camera "
                         "sets; more is wider. Story scenes keep the game's.");
    {
        int fov = int(g_fov);
        char fmt[64] = "%d%% (the game's)";
        double game = 0, drawn = 0;
        if (fov != 100 && ViewFov(game, drawn))
            std::snprintf(fmt, sizeof(fmt), "%%d%%%% (%.0f\xC2\xB0 up and down here, the game's %.0f\xC2\xB0)", drawn, game);
        else if (fov != 100)
            std::snprintf(fmt, sizeof(fmt), "%%d%%%%");
        if (SliderI("##fov", &fov, 100, 200, fmt, ImGuiSliderFlags_AlwaysClamp)) {
            g_fov = uint32_t(fov);
            save = true;
        }
    }
    Row("Far view", "The fog and the draw distance both 4x as far out. Off puts both back to the game's.");
    {
        bool on = g_draw_distance == 400 && g_fog_distance == 400;
        if (Check("##farview", &on)) {
            g_draw_distance = g_fog_distance = on ? 400 : 100;
            save = true;
        }
    }
    Row("Draw distance", "How far the world is drawn. The game's ends where its fog has hidden everything; farther shows "
                         "more once there is less fog, and may show where a room's scenery ends.");
    save |= Choice<uint32_t>("##far", g_draw_distance, {100, 150, 200, 400}, {"The game's", "1.5x", "2x", "4x"});
    Row("Fog", "The fog and the darkness that hides the distance in most rooms: as far out as the game puts it, "
               "farther, or none.");
    save |= Choice<uint32_t>("##fog", g_fog_distance, {100, 150, 200, 400, 0},
                             {"The game's", "1.5x farther", "2x farther", "4x farther", "None"});
    ImGui::EndTable();
}

void GameTab(bool& save) {
    if (!BeginRows("game")) return;
    Row("FPS counter", "The frame rate in the top left corner; with frame generation, the frames reaching the "
                       "screen with the game's own beside them.");
    save |= Check("##fpsc", &g_fps_counter);
    Row("Pause on focus loss", "Stops the game while another window is in front.");
    save |= Check("##pause", &g_pause_on_focus_loss);
    Row("Lament of Innocence save", "As the PS2 game with a Lament of Innocence save on the memory card: a Moai "
                                    "at the far end of the room behind the first courtyard, beside the potion.");
    save |= Check("##lament", &g_lament_save);
    ImGui::EndTable();
}

void ControlsTab(bool& save) {
    if (BeginRows("layout")) {
        Row("Button layout", "PS2: the PS2's button icons (from an HD texture pack made from the PS2 game) and its "
                             "shoulders: L1 resets the camera, R1 guards, L2 and R2 lock on. From the next start.");
        save |= Choice<uint32_t>("##layout", g_button_layout, {0, 1}, {"Xbox", "PS2"});
        Row("Stick response", "PS2: Hector walks any way the stick points and walks slowly when it is tilted a "
                              "little, as on the PS2. Xbox: the Xbox release's, where a stick near up, down, left or "
                              "right walks straight along it and a walk starts at a third of his speed.");
        save |= Choice<bool>("##stick", g_ps2_stick, {true, false}, {"PS2", "Xbox"});
        Row("Invert camera left / right", "The right stick (and the arrow keys) turn the camera the other way across.");
        save |= Check("##invx", &g_invert_camera_x);
        Row("Invert camera up / down", "The right stick (and the arrow keys) move the camera the other way up and down.");
        save |= Check("##invy", &g_invert_camera_y);
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::TextWrapped("Xbox, PlayStation and Switch controllers work as player 1. The keyboard plays alongside:");
    ImGui::Spacing();
    static const char* kKeys[][2] = {
        {"W A S D", "Move"},           {"Arrow keys", "Camera"},   {"K / Space", "A (jump)"},
        {"J", "X (attack)"},           {"L", "B"},                 {"I", "Y"},
        {"Q / E", "Left / right trigger"}, {"Z / C", "White / black"}, {"Enter", "Start"},
        {"Backspace", "Back"},         {"Numpad 8 2 4 6", "D-pad"},
        {"L3 + R3 (controller)", "This menu"},
    };
    if (ImGui::BeginTable("keys", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(KeyName(g_menu_key).c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("This menu (change it in the launcher)");
        for (auto& k : kKeys) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(k[0]);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(k[1]);
        }
        ImGui::EndTable();
    }
}

// for looking into how a screen is drawn: a capture of the frame behind the menu
void ToolsTab() {
    if (!BeginRows("tools")) return;
    Row("Frame capture", "Saves the game's next frame, the one behind this menu: how each part of it was drawn and "
                         "the pictures it used, in the captures folder in the game's folder. Open the menu over the "
                         "screen to look into, then press this.");
    {
        static bool asked = false;
        static uint32_t asked_at = 0;  // captures saved when it was asked for
        uint32_t saved = gpu::g_captures_saved.load();
        bool pressed = ImGui::Button("Capture this frame");
        Track();
        if (pressed) {
            asked = true;
            asked_at = saved;
            gpu::g_capture_asked = true;
            gpu::g_capture = true;
            g_wide_trace_frame = true;
        }
        if (asked) {
            ImGui::SameLine();
            if (saved > asked_at)
                ImGui::TextDisabled("saved (%u this run) to the captures folder", saved);
            else
                ImGui::TextDisabled("saving...");
        }
    }
    ImGui::EndTable();
}

void Defaults() {
    app::Settings d;
    app::Settings& s = app::Get();
    d.join_ip = s.join_ip;
    s = d;
    g_fps_counter = false;
    g_pause_on_focus_loss = true;
    g_button_layout = 0;
    g_lament_save = false;
    g_ps2_stick = true;
    g_invert_camera_x = g_invert_camera_y = false;
    g_fov = 100;
    g_draw_distance = g_fog_distance = 100;
    g_hud_edges = true;
    g_menu_spread = true;
    g_hd_movies = true;
    g_movie_view = 1;
    g_surface_depth = 2;
    g_antialias = 1;
    g_upscale = 0;
    g_sharpening = 30;
    g_ambient_occlusion = 1;
    g_ray_tracing = 0;
    app::ApplyDisplay();
}

std::string KeyName(uint32_t vk) {
    wchar_t name[64] = {};
    LONG scan = LONG(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)) << 16;
    // keys of the navigation block share scan codes with the numpad: the extended bit tells them apart
    if ((vk >= VK_PRIOR && vk <= VK_DOWN) || vk == VK_INSERT || vk == VK_DELETE) scan |= 1 << 24;
    if (!GetKeyNameTextW(scan, name, 64)) return "F10";
    char out[64];
    WideCharToMultiByte(CP_UTF8, 0, name, -1, out, sizeof(out), nullptr, nullptr);
    return out;
}

bool Foreground() {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return fg && pid == GetCurrentProcessId();
}

bool Down(uint32_t vk) { return (GetAsyncKeyState(int(vk)) & 0x8000) != 0; }

// render thread, each frame: the menu key's state is read rather than its messages, which an overlay or another
// program's hotkey can take before the window sees them
void PollKeys() {
    static bool menu_was = false, escape_was = false;
    bool fg = Foreground();
    bool menu = fg && Down(g_menu_key ? g_menu_key : VK_F10);
    if (menu && !menu_was) g_open = !g_open;
    menu_was = menu;
    bool escape = fg && Down(VK_ESCAPE);
    if (escape && !escape_was) g_open = false;
    escape_was = escape;
}

void Style(float scale) {
    ImGuiStyle& st = ImGui::GetStyle();
    ImGui::StyleColorsDark(&st);
    st.WindowRounding = 6;
    st.FrameRounding = 4;
    st.TabRounding = 4;
    st.WindowPadding = {14, 12};
    st.FramePadding = {8, 5};
    st.ItemSpacing = {10, 8};
    ImVec4* c = st.Colors;
    // Castlevania red over near black
    ImVec4 red{0.55f, 0.08f, 0.10f, 1}, red_hi{0.72f, 0.12f, 0.14f, 1}, red_lo{0.40f, 0.06f, 0.08f, 1};
    c[ImGuiCol_WindowBg] = {0.06f, 0.05f, 0.06f, 0.94f};
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = red_lo;
    c[ImGuiCol_Tab] = red_lo;
    c[ImGuiCol_TabHovered] = red_hi;
    c[ImGuiCol_TabSelected] = red;
    c[ImGuiCol_Button] = red_lo;
    c[ImGuiCol_ButtonHovered] = red_hi;
    c[ImGuiCol_ButtonActive] = red;
    c[ImGuiCol_FrameBg] = {0.16f, 0.13f, 0.14f, 1};
    c[ImGuiCol_FrameBgHovered] = {0.26f, 0.16f, 0.18f, 1};
    c[ImGuiCol_FrameBgActive] = {0.32f, 0.18f, 0.20f, 1};
    c[ImGuiCol_Header] = red_lo;
    c[ImGuiCol_HeaderHovered] = red_hi;
    c[ImGuiCol_HeaderActive] = red;
    c[ImGuiCol_CheckMark] = {0.95f, 0.80f, 0.45f, 1};
    c[ImGuiCol_NavHighlight] = {0.95f, 0.80f, 0.45f, 1};
    st.ScaleAllSizes(scale);
}

}  // namespace

bool MenuOpen() { return g_open; }
void MenuToggle() { g_open = !g_open; }

// the game window is up, before the first frame: the font, sized for the monitor
void MenuWindow() {
    std::lock_guard lock(app::UiLock());
    float scale = std::max(1.0f, app::Desktop().height / 1080.0f);
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 20.0f * scale)) {
        ImFontConfig cfg;
        cfg.SizePixels = 13.0f * scale;
        io.Fonts->AddFontDefault(&cfg);
    }
    Style(scale);
    g_scaled = true;
}

// window thread: the menu key and Escape are taken here so the host does nothing with them (F10 alone would put
// the window in menu mode); PollKeys opens and closes the menu
extern std::atomic<bool> g_closing;  // keyboard.cpp

bool MenuMessage(void*, uint32_t msg, uintptr_t wp, intptr_t, intptr_t* result) {
    if (msg == WM_CLOSE) g_closing = true;
    bool key = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP;
    if (key && (wp == VK_F10 || wp == g_menu_key || (wp == VK_ESCAPE && g_open))) {
        *result = 0;
        return true;
    }
    return false;
}

void SettingsWatch();  // keyboard.cpp

// the settings the SDK's renderer reads: each frame, and before it starts (GameInit), as DLSS is chosen then
void MenuToRenderer() {
    gpu::g_surface_depth.store(std::min<uint32_t>(g_surface_depth, 2), std::memory_order_relaxed);
    gpu::g_antialias.store(std::min<uint32_t>(g_antialias, 2), std::memory_order_relaxed);
    gpu::g_upscale.store(std::min<uint32_t>(g_upscale, 3), std::memory_order_relaxed);
    gpu::g_aa_sharpness.store(std::min<uint32_t>(g_sharpening, 100), std::memory_order_relaxed);
    gpu::g_ambient_occlusion.store(std::min<uint32_t>(g_ambient_occlusion, 2), std::memory_order_relaxed);
    gpu::g_ray_tracing.store(g_ray_tracing ? gpu::kRtAmbientOcclusion : 0, std::memory_order_relaxed);
}

void MenuUi(double fps) {
    SettingsWatch();
    MenuToRenderer();
    if (!g_scaled) return;
    PollKeys();
    ImGuiIO& io = ImGui::GetIO();
    if (g_fps_counter) {
        // with frame generation, the frames reaching the screen, and the game's own beside them
        char text[64];
        app::FrameGenState fg = app::FrameGen();
        if (fg.active && fg.shown_fps > fps + 1)
            std::snprintf(text, sizeof(text), "%.0f FPS (game %.0f)", fg.shown_fps, fps);
        else
            std::snprintf(text, sizeof(text), "%.0f FPS", fps);
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 at{io.DisplaySize.x * 0.01f, io.DisplaySize.y * 0.01f};
        dl->AddText({at.x + 2, at.y + 2}, IM_COL32(0, 0, 0, 200), text);
        dl->AddText(at, IM_COL32(255, 230, 140, 255), text);
    }
    io.MouseDrawCursor = false;
    {
        // the last frame generation state while it generated: the menu pauses it, and its row speaks of that
        app::FrameGenState fg = app::FrameGen();
        if (fg.active) g_fg_seen = fg;
    }
    // the first controller, for what ImGui's navigation leaves to the menu (the renderer gives it the D-pad, the
    // sticks, A and B): the shoulders turn the pages, Start closes, and B closes when it has nothing else to back out
    // of (a list open or a slider held the frame before)
    input::Pad pad = input::Raw(0);
    static bool lb_was = false, rb_was = false, start_was = false, b_was = false, busy_was = false;
    bool lb = pad.analog[5] > 0x40, rb = pad.analog[4] > 0x40, start = (pad.buttons & 0x10) != 0,
         b = pad.analog[1] > 0x40;
    bool lb_hit = lb && !lb_was, rb_hit = rb && !rb_was, start_hit = start && !start_was, b_hit = b && !b_was;
    lb_was = lb;
    rb_was = rb;
    start_was = start;
    b_was = b;
    if (!g_open) {
        busy_was = false;
        return;
    }
    if (start_hit || (b_hit && !busy_was)) {
        g_open = false;
        return;
    }

    // at the screen's right, nearly its height: the game's picture stays in view beside it
    float margin = io.DisplaySize.y * 0.03f;
    ImVec2 size{std::min(io.DisplaySize.y * 0.8f, io.DisplaySize.x * 0.45f), io.DisplaySize.y - 2 * margin};
    ImGui::SetNextWindowPos({io.DisplaySize.x - margin, io.DisplaySize.y * 0.5f}, ImGuiCond_Always, {1.0f, 0.5f});
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    bool open = true;
    g_focus_help = nullptr;
    if (ImGui::Begin("Castlevania: Curse of Darkness - Settings", &open,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoSavedSettings)) {
        // the menu keeps the keyboard and controller: focused when it has lost the focus, not each frame, which put
        // the D-pad's and arrow keys' place back to the window itself every frame (its pages are a child window), so
        // they moved nothing; nor while a drop-down's list is open, which focusing the menu would close
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup))
            ImGui::SetWindowFocus();
        bool save = false;
        const ImGuiStyle& st = ImGui::GetStyle();
        float line = ImGui::GetTextLineHeightWithSpacing();
        float help_h = line * 3 + st.WindowPadding.y * 2;
        float footer = help_h + st.ItemSpacing.y + line + st.ItemSpacing.y * 2 + ImGui::GetFrameHeightWithSpacing();
        // the pages; the shoulders turn them as a tab bar's arrows would
        static int page = 0;
        int want = -1;
        constexpr int kPages = 5;
        if (lb_hit) want = (page + kPages - 1) % kPages;
        if (rb_hit) want = (page + 1) % kPages;
        auto flags = [&](int i) { return want == i ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None; };
        // its items one list with the buttons below for the D-pad (NavFlattened)
        if (ImGui::BeginChild("tabs", {0, -footer}, ImGuiChildFlags_NavFlattened)) {
            if (ImGui::BeginTabBar("tabbar")) {
                if (ImGui::BeginTabItem("Display", nullptr, flags(0))) {
                    page = 0;
                    DisplayTab(save);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Graphics", nullptr, flags(1))) {
                    page = 1;
                    GraphicsTab(save);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Game", nullptr, flags(2))) {
                    page = 2;
                    GameTab(save);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Controls", nullptr, flags(3))) {
                    page = 3;
                    ControlsTab(save);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Tools", nullptr, flags(4))) {
                    page = 4;
                    ToolsTab();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();
        // what the setting in focus does; another page's is not kept
        static int help_page = -1;
        if (page != help_page) g_shown_help.clear();
        help_page = page;
        if (g_focus_help) g_shown_help = g_focus_help;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.11f, 0.09f, 0.10f, 1});
        if (ImGui::BeginChild("help", {0, help_h}, ImGuiChildFlags_AlwaysUseWindowPadding)) {
            if (g_shown_help.empty())
                ImGui::TextDisabled("Move to a setting to see what it does here.");
            else
                ImGui::TextWrapped("%s", g_shown_help.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        // the controller's buttons, by the layout's names
        ImGui::TextDisabled(g_button_layout == 1
                                ? "Controller: D-pad move, Cross choose, Circle back, L1 / R1 page, Options close"
                                : "Controller: D-pad move, A choose, B back, LB / RB page, Start close");
        ImGui::Separator();
        if (ImGui::Button("Reset to defaults")) {
            Defaults();
            save = true;
        }
        ImGui::SameLine();
        std::string close = "Close (" + KeyName(g_menu_key) + ")";
        float close_w = ImGui::CalcTextSize(close.c_str()).x + st.FramePadding.x * 2;
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - close_w);
        if (ImGui::Button(close.c_str())) open = false;
        if (save) app::Save();
    }
    ImGui::End();
    busy_was = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup) || ImGui::IsAnyItemActive();
    if (!open) g_open = false;
}
