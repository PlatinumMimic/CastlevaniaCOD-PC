// Game state for tests: the frame, map and room, where Hector is and which way he faces, his HP, MP, EXP and gold,
// where the camera is and looks, and whether a menu is up, as one line of JSON. Every read checks its pages first,
// so a pointer that is not there leaves its fields out instead of faulting.
//
// Addresses in the USA default.xbe, found by Neguchads (Castlevania-COD-Recomp: camera.cpp, minimap.cpp, hooks.cpp,
// game_cheat_rules.h) and checked in play: stick up with the camera heading 0 moves Hector along +z with yaw 0,
// the pause menu shows the same HP, EXP and gold:
//   map, room            int8 0xB234F4, 0xB234F5 (the game's getter at 0x2F6970 returns 0xB234F4)
//   player               handle u32 0xB41394; in play when int16 [h+0x144] is an index i under 128, int16
//                        [h+0x146+i*2] is 2 and u8 [h+0xEE] has both bits of 0x18
//   position, facing     record 0xB4129C + u32 [h] * 0x1210: float3 at +0x10 and the yaw in radians at +0x2C, what
//                        the game's getters sub_00334640 and sub_00334510 return
//   Hector               int32 attributes at 0xB2CCC0 + n * 4: 3 EXP, 5 max HP, 6 HP, 31 max MP, 32 MP; gold 0xB35204
//   camera               view u32 0xC36B74: angles float3 +0x20, target +0x30, position +0x40. The title, the map
//                        screen and debug views run callback 0x345020 or 0x4E7D0, at [u32 0xC36B70] + 0x1D0
//   menus                u32 0xB25518 is not 0 while the pause or inventory menu is up; a modal box sets bit 16 of the
//                        word at u32 [0x548C10]
//
// CV_TELEMETRY=<file.jsonl> appends the state every CV_TELEMETRY_EVERY frames (30 by default) while the game reads
// the pad; turn by turn play (keyboard.cpp) writes it beside each done file.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>

#include "kernel/memory.h"
#include "xdk/d3d8.h"

namespace {

template <class T>
bool Read(uint32_t va, T& out) {
    if (va < 0x10000 || va > 0xFFFFFFFFu - sizeof(T)) return false;
    if (!mem::IsCommitted(va) || !mem::IsCommitted(va + uint32_t(sizeof(T)) - 1)) return false;
    std::memcpy(&out, mem::Base() + va, sizeof(T));
    return true;
}

struct Vec3 {
    float x, y, z;
};

bool Finite(const Vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// names of the maps seen in play. A new game starts on map 5, the Abandoned Castle; the names Neguchads read from the
// teleporter table (0x4E91F0) call map 5 the Boss Rush, so they wait to be seen before they go here
const char* AreaName(int map) {
    switch (map) {
        case 5: return "Abandoned Castle";
        default: return nullptr;
    }
}

class Json {
public:
    Json() { s_ = "{"; }
    void Key(const char* k) {
        if (s_.size() > 1 && s_.back() != '{' && s_.back() != '[') s_ += ',';
        s_ += '"';
        s_ += k;
        s_ += "\":";
    }
    void Int(const char* k, long long v) {
        Key(k);
        s_ += std::to_string(v);
    }
    void Num(const char* k, float v) {
        Key(k);
        char b[32];
        snprintf(b, sizeof(b), "%.3f", v);
        s_ += b;
    }
    void Bool(const char* k, bool v) {
        Key(k);
        s_ += v ? "true" : "false";
    }
    void Str(const char* k, const char* v) {
        Key(k);
        s_ += '"';
        s_ += v;
        s_ += '"';
    }
    void Vec(const char* k, const Vec3& v) {
        Key(k);
        char b[96];
        snprintf(b, sizeof(b), "[%.3f,%.3f,%.3f]", v.x, v.y, v.z);
        s_ += b;
    }
    void Open(const char* k) {
        Key(k);
        s_ += '{';
    }
    void Close() { s_ += '}'; }
    std::string Done() { return s_ + '}'; }

private:
    std::string s_;
};

void Player(Json& j) {
    uint32_t h = 0;
    int16_t index = -1, state = 0;
    uint8_t flags = 0;
    bool active = Read(0xB41394u, h) && h && Read(h + 0x144, index) && index >= 0 && index < 128 &&
                  Read(h + 0x146 + uint32_t(index) * 2, state) && state == 2 && Read(h + 0xEE, flags) &&
                  (flags & 0x18) == 0x18;
    j.Open("player");
    j.Bool("active", active);
    uint32_t record = 0;
    Vec3 pos;
    float yaw = 0;
    if (h && Read(h, record) && record < 128 && Read(0xB4129Cu + record * 0x1210 + 0x10, pos) && Finite(pos) &&
        Read(0xB4129Cu + record * 0x1210 + 0x2C, yaw) && std::isfinite(yaw)) {
        j.Vec("pos", pos);
        j.Num("yaw", yaw);
    }
    struct Stat {
        const char* name;
        uint32_t va;
    };
    static const Stat kStats[] = {{"hp", 0xB2CCC0 + 6 * 4},     {"hp_max", 0xB2CCC0 + 5 * 4}, {"mp", 0xB2CCC0 + 32 * 4},
                                  {"mp_max", 0xB2CCC0 + 31 * 4}, {"exp", 0xB2CCC0 + 3 * 4},   {"gold", 0xB35204}};
    for (const Stat& s : kStats) {
        int32_t v;
        if (Read(s.va, v)) j.Int(s.name, v);
    }
    j.Close();
}

void Camera(Json& j) {
    uint32_t view = 0, descriptor = 0, callback = 0;
    Vec3 angles, target, pos;
    if (!Read(0xC36B74u, view) || !view || !Read(view + 0x20, angles) || !Read(view + 0x30, target) ||
        !Read(view + 0x40, pos) || !Finite(angles) || !Finite(target) || !Finite(pos))
        return;
    j.Open("camera");
    j.Vec("pos", pos);
    j.Vec("target", target);
    j.Vec("angles", angles);
    // the direction the camera looks across the floor, as Hector's yaw: atan2 of x over z
    j.Num("heading", std::atan2(target.x - pos.x, target.z - pos.z));
    if (Read(0xC36B70u, descriptor) && descriptor && Read(descriptor + 0x1D0, callback))
        j.Bool("special", callback == 0x345020 || callback == 0x4E7D0);  // title, map screen or a debug view
    j.Close();
}

}  // namespace

// the state now, one JSON object on one line
std::string GameStateJson() {
    Json j;
    j.Int("frame", d3d::Swaps());
    int8_t map = 0, room = 0;
    if (Read(0xB234F4u, map) && Read(0xB234F5u, room)) {
        j.Int("map", map);
        j.Int("room", room);
        if (const char* name = AreaName(map)) j.Str("area", name);
    }
    Player(j);
    Camera(j);
    uint32_t menu = 0, flags_at = 0, flags = 0;
    if (Read(0xB25518u, menu)) j.Bool("menu", menu != 0);
    if (Read(0x548C10u, flags_at) && flags_at && Read(flags_at, flags)) j.Bool("modal", (flags >> 16) & 1);
    return j.Done();
}

// CV_TELEMETRY: at a pad read, the state once every CV_TELEMETRY_EVERY frames
void TelemetryFrame() {
    static const std::string path = [] {
        const char* v = std::getenv("CV_TELEMETRY");
        return std::string(v ? v : "");
    }();
    if (path.empty()) return;
    static const uint32_t every = [] {
        const char* v = std::getenv("CV_TELEMETRY_EVERY");
        long n = v ? std::atol(v) : 30;
        return uint32_t(n > 0 ? n : 30);
    }();
    static std::mutex m;
    static uint32_t last = ~0u;
    uint32_t frame = d3d::Swaps();
    std::lock_guard lock(m);  // the game reads the pad on more than one thread
    if (last != ~0u && frame - last < every) return;
    last = frame;
    std::ofstream(path, std::ios::app) << GameStateJson() << "\n";
}
