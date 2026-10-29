// Keyboard play on port 1, and a scripted pad for unattended test runs.
//
// The keyboard is read only while the game window is in front, and adds to a real controller on the same port
// rather than replacing it. Port 1 always reports a controller, so the game can be played with the keyboard alone.
//
//   WASD left stick         arrows right stick (camera)    numpad 8 2 4 6 d-pad
//   K or Space  A (jump)    J  X (attack)                  L  B         I  Y
//   Q  left trigger         E  right trigger               Z  white     C  black
//   Enter  Start            Backspace  Back
//
// CV_PAD_SCRIPT="<ms>:<button>[+<button>...][:<hold ms>],..." presses buttons at times measured from the game's
// first read of the pad (default hold 150 ms). A time ending in f counts the game's frames instead, its hold too
// ("120f:a:10f", default hold 9 frames), so a run does not drift when frames take longer. Buttons: up down left
// right start back a b x y black white lt rt lsu lsd lsl lsr (left stick) rsu rsd rsl rsr (right stick). "@file"
// reads the same entries from a file, one per line or comma separated.
//
// CV_PAD_LIVE=<folder> plays turn by turn: the game waits at its pad read for <folder>\cmd-<n>.txt (n from 1), presses
// its entries as CV_PAD_SCRIPT does, times from the command's start ("<ms>:wait" only waits), then writes
// state-<n>.json (telemetry.cpp) and done-<n>.txt and waits for the next. The window need not be in front. An entry
// "0:warp=<map>.<room>[.<entry>]" sends Hector to that room at the command's start (game/warp.cpp).
// <va>:<bytes>[,...] in <folder>\dump.txt (or CV_PAD_LIVE_DUMP) also saves those guest memory ranges at each
// command's end, as mem-<n>-<va>.bin.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "core/input.h"
#include "kernel/memory.h"
#include "xdk/d3d8.h"

extern std::atomic<bool> g_closing;  // below: the window is closing
std::string GameStateJson();         // telemetry.cpp
void TelemetryFrame();               // telemetry.cpp
void RequestWarp(int map, int room, int entry);  // game/warp.cpp

namespace {

HWND g_hwnd = nullptr;

enum : uint16_t { kUp = 1, kDown = 2, kLeft = 4, kRight = 8, kStart = 0x10, kBack = 0x20 };
enum { kA, kB, kX, kY, kBlack, kWhite, kLT, kRT };

bool Down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

bool Focused() {
    HWND fg = GetForegroundWindow();
    if (!fg || !g_hwnd) return false;
    DWORD a = 0, b = 0;
    GetWindowThreadProcessId(fg, &a);
    GetWindowThreadProcessId(g_hwnd, &b);
    return a == b;
}

// one stick axis from two keys, full deflection
int16_t Axis(int neg, int pos) {
    int v = (Down(pos) ? 32767 : 0) - (Down(neg) ? 32767 : 0);
    return int16_t(v);
}

void Keyboard(input::Pad& p) {
    if (!Focused()) return;
    if (Down(VK_NUMPAD8)) p.buttons |= kUp;
    if (Down(VK_NUMPAD2)) p.buttons |= kDown;
    if (Down(VK_NUMPAD4)) p.buttons |= kLeft;
    if (Down(VK_NUMPAD6)) p.buttons |= kRight;
    if (Down(VK_RETURN)) p.buttons |= kStart;
    if (Down(VK_BACK)) p.buttons |= kBack;
    auto press = [&](int slot, bool on) { if (on) p.analog[slot] = 0xFF; };
    press(kA, Down('K') || Down(VK_SPACE));
    press(kX, Down('J'));
    press(kB, Down('L'));
    press(kY, Down('I'));
    press(kWhite, Down('Z'));
    press(kBlack, Down('C'));
    press(kLT, Down('Q'));
    press(kRT, Down('E'));
    // the keyboard wins over a centred stick, a moved stick keeps its value
    int16_t lx = Axis('A', 'D'), ly = Axis('S', 'W'), rx = Axis(VK_LEFT, VK_RIGHT), ry = Axis(VK_DOWN, VK_UP);
    if (lx) p.lx = lx;
    if (ly) p.ly = ly;
    if (rx) p.rx = rx;
    if (ry) p.ry = ry;
}

struct Step {
    int64_t at, until;  // ms, or frames
    bool frames;
    uint16_t buttons;
    uint32_t analog;  // bit per analog slot
    int16_t lx, ly, rx, ry;
    int map = -1, room = 0, entry = 0;  // warp=<map>.<room>[.<entry>]: live commands send Hector there at their start
};

std::vector<Step> ParseScript(std::string text) {
    std::vector<Step> steps;
    for (char& c : text)
        if (c == '\n' || c == '\r') c = ',';
    std::stringstream all(text);
    std::string entry;
    while (std::getline(all, entry, ',')) {
        if (entry.empty() || entry[0] == '#') continue;
        std::stringstream e(entry);
        std::string at, names, hold;
        std::getline(e, at, ':');
        std::getline(e, names, ':');
        std::getline(e, hold, ':');
        if (at.empty() || names.empty()) continue;
        Step s{std::atoll(at.c_str()), 0, at.back() == 'f', 0, 0, 0, 0, 0, 0};
        s.until = s.at + (hold.empty() ? (s.frames ? 9 : 150) : std::atoll(hold.c_str()));  // atoll stops at the f
        std::stringstream n(names);
        std::string b;
        while (std::getline(n, b, '+')) {
            if (b == "up") s.buttons |= kUp;
            else if (b == "down") s.buttons |= kDown;
            else if (b == "left") s.buttons |= kLeft;
            else if (b == "right") s.buttons |= kRight;
            else if (b == "start") s.buttons |= kStart;
            else if (b == "back") s.buttons |= kBack;
            else if (b == "a") s.analog |= 1u << kA;
            else if (b == "b") s.analog |= 1u << kB;
            else if (b == "x") s.analog |= 1u << kX;
            else if (b == "y") s.analog |= 1u << kY;
            else if (b == "black") s.analog |= 1u << kBlack;
            else if (b == "white") s.analog |= 1u << kWhite;
            else if (b == "lt") s.analog |= 1u << kLT;
            else if (b == "rt") s.analog |= 1u << kRT;
            else if (b == "lsu") s.ly = 32767;
            else if (b == "lsd") s.ly = -32767;
            else if (b == "lsl") s.lx = -32767;
            else if (b == "lsr") s.lx = 32767;
            else if (b == "rsu") s.ry = 32767;
            else if (b == "rsd") s.ry = -32767;
            else if (b == "rsl") s.rx = -32767;
            else if (b == "rsr") s.rx = 32767;
            else if (b.rfind("warp=", 0) == 0) std::sscanf(b.c_str() + 5, "%d.%d.%d", &s.map, &s.room, &s.entry);
        }
        steps.push_back(s);
    }
    return steps;
}

const std::vector<Step>& Script() {
    static const std::vector<Step> steps = [] {
        const char* v = std::getenv("CV_PAD_SCRIPT");
        if (!v || !*v) return std::vector<Step>{};
        std::string text = v;
        if (text[0] == '@') {
            std::ifstream f(text.substr(1));
            std::stringstream ss;
            ss << f.rdbuf();
            text = ss.str();
        }
        return ParseScript(text);
    }();
    return steps;
}

// ms and frames since the script or command started
void Press(input::Pad& p, const std::vector<Step>& steps, int64_t ms, int64_t frames) {
    for (const Step& s : steps) {
        int64_t now = s.frames ? frames : ms;
        if (now < s.at || now >= s.until) continue;
        p.buttons |= s.buttons;
        for (int i = 0; i < 8; i++)
            if (s.analog >> i & 1) p.analog[i] = 0xFF;
        if (s.lx) p.lx = s.lx;
        if (s.ly) p.ly = s.ly;
        if (s.rx) p.rx = s.rx;
        if (s.ry) p.ry = s.ry;
    }
}

using Clock = std::chrono::steady_clock;

int64_t MsSince(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}

void Scripted(input::Pad& p) {
    const auto& steps = Script();
    if (steps.empty()) return;
    static const Clock::time_point start = Clock::now();
    static const uint32_t start_frame = d3d::Swaps();
    Press(p, steps, MsSince(start), int64_t(d3d::Swaps() - start_frame));
}

// CV_PAD_LIVE: the command running, its entries and when it is over
struct Live {
    std::filesystem::path dir;
    int n = 0;
    bool running = false;
    std::vector<Step> steps;
    Clock::time_point start;
    uint32_t start_frame = 0;
    int64_t end_ms = 0, end_frames = 0;  // over when both are reached
};

Live& LiveState() {
    static Live live = [] {
        Live l;
        if (const char* v = std::getenv("CV_PAD_LIVE"); v && *v) l.dir = v;
        return l;
    }();
    return live;
}

bool LiveOn() { return !LiveState().dir.empty(); }

// the ranges in <folder>\dump.txt (read at each command's end, so they can change while the game runs) or
// CV_PAD_LIVE_DUMP, written beside the done file
void LiveDump(const Live& l) {
    std::string spec;
    if (std::ifstream f(l.dir / "dump.txt"); f) std::getline(f, spec);
    else if (const char* v = std::getenv("CV_PAD_LIVE_DUMP")) spec = v;
    std::stringstream all(spec);
    std::string e;
    while (std::getline(all, e, ',')) {
        size_t colon = e.find(':');
        if (colon == std::string::npos) continue;
        uint32_t va = uint32_t(std::stoul(e.substr(0, colon), nullptr, 0));
        uint32_t bytes = uint32_t(std::stoul(e.substr(colon + 1), nullptr, 0));
        if (!mem::IsCommitted(va)) continue;
        char name[48];
        snprintf(name, sizeof(name), "mem-%d-%08X.bin", l.n, va);
        std::ofstream(l.dir / name, std::ios::binary).write(reinterpret_cast<const char*>(mem::Base() + va), bytes);
    }
}

// the game reads the pad on more than one thread: one at a time takes a command, the others wait with it
std::mutex g_live_m;

// at each pad read: the command over, its done file; no command, wait here for the next
void LiveWait() {
    std::lock_guard lock(g_live_m);
    Live& l = LiveState();
    if (l.running && MsSince(l.start) >= l.end_ms && int64_t(d3d::Swaps() - l.start_frame) >= l.end_frames) {
        LiveDump(l);
        std::ofstream(l.dir / ("state-" + std::to_string(l.n) + ".json")) << GameStateJson() << "\n";
        std::ofstream(l.dir / ("done-" + std::to_string(l.n) + ".txt")) << l.n << "\n";
        l.running = false;
    }
    while (!l.running && !g_closing && !(g_hwnd && !IsWindow(g_hwnd))) {
        std::filesystem::path cmd = l.dir / ("cmd-" + std::to_string(l.n + 1) + ".txt");
        std::error_code ec;
        if (std::filesystem::exists(cmd, ec)) {
            std::ifstream f(cmd);
            std::stringstream ss;
            ss << f.rdbuf();
            l.steps = ParseScript(ss.str());
            l.end_ms = l.end_frames = 0;
            for (const Step& s : l.steps) {
                int64_t& end = s.frames ? l.end_frames : l.end_ms;
                end = (std::max)(end, s.until);  // windows.h defines max
            }
            for (const Step& s : l.steps)
                if (s.map >= 0) RequestWarp(s.map, s.room, s.entry);
            l.n++;
            l.start = Clock::now();
            l.start_frame = d3d::Swaps();
            l.running = true;
            break;
        }
        Sleep(20);
    }
}

}  // namespace

bool MenuOpen();                     // menu.cpp
void MenuToggle();                   // menu.cpp
void MenuWindow();                   // menu.cpp
extern bool g_pause_on_focus_loss;   // menu.cpp
std::atomic<bool> g_closing{false};  // the window is closing: a paused game lets go

void KeyboardWindow(void* hwnd) {
    g_hwnd = static_cast<HWND>(hwnd);
    MenuWindow();
}

void KeyboardPad(int port, input::Pad& p) {
    if (port != 0) return;
    // pause on focus loss: the game stops at its pad read until the window is in front again; not in scripted runs,
    // which go on behind other windows
    if (g_pause_on_focus_loss && Script().empty() && !LiveOn())
        while (!Focused() && !g_closing && IsWindow(g_hwnd)) Sleep(50);
    if (LiveOn()) LiveWait();
    TelemetryFrame();
    p.connected = true;
    // both sticks clicked in together: the menu from a controller
    static bool sticks = false;
    bool both = (p.buttons & 0xC0) == 0xC0;
    if (both && !sticks) MenuToggle();
    sticks = both;
    if (MenuOpen()) {
        // the menu has the keyboard and controller
        p = input::Pad{};
        p.connected = true;
        return;
    }
    Keyboard(p);
    Scripted(p);
    if (LiveOn()) {
        std::lock_guard lock(g_live_m);
        const Live& l = LiveState();
        if (l.running) Press(p, l.steps, MsSince(l.start), int64_t(d3d::Swaps() - l.start_frame));
    }
}
