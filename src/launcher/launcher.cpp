// The launcher: checks the game files, edits the settings file the game reads, and starts the game.
//
// It sits beside Castlevania.exe and writes the same castlevania.toml, keeping any keys it does not know. The game
// files are checked as the game checks them: assets beside the exe or in a folder above it, and default.xbe there or
// on a disc image (.iso) in it must have the SHA-1 the exe was recompiled from, once a patched certificate is put back
// (the SDK's runtime/app/game_files.cpp).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef _WIN32
#include <bcrypt.h>
#include <commdlg.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dwmapi.h>
#include <dxgi1_6.h>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>
#else
// Linux: an SDL window drawn with OpenGL, the graphics cards Vulkan's, pictures stb's (the SDK's linux/)
#include <SDL3/SDL.h>
#include <spawn.h>
#include <sys/wait.h>
#include <vkd3d.h>
#include <vulkan/vulkan.h>

#include <mutex>

#include "kernel/crypto.h"
#include "linux/image.h"
#include "linux/sdl_app.h"
extern char** environ;
#endif

#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cwchar>
#include <string>
#include <thread>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#ifdef _WIN32
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#else
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_opengl3_loader.h>
#endif
#include <toml++/toml.hpp>

#include "app/app.h"
#include "kernel/disc.h"
#include "../common/marks.h"
#include "words.h"

#ifdef _WIN32
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#endif
// each release's xbe (its SHA-1, its certificate's title name, media, region and ratings) and the executable it was
// recompiled into, written by CMake: the USA one first, then the European one
extern const app::GameInfo::Release kReleases[];
extern const unsigned kReleaseCount;

namespace {

namespace fs = std::filesystem;
using words::F;
using words::T;
// a phrase as each system has it: Windows's, Linux's (Vulkan for Direct3D 12, no .exe, / in paths)
#ifdef _WIN32
#define CV_WIN_LINUX(win, linux_text) win
#else
#define CV_WIN_LINUX(win, linux_text) linux_text
#endif

constexpr char kSettings[] = "castlevania.toml";
constexpr char kXbe[] = "default.xbe";
#ifdef _WIN32
constexpr wchar_t kSaves[] = L"hdd\\E\\UDATA\\4b4e002d";
#else
constexpr char kSaves[] = "hdd/E/UDATA/4b4e002d";
#endif

#ifdef _WIN32
// D3D11 and the window

ID3D11Device* g_dev = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;
IDXGISwapChain* g_swap = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
HWND g_hwnd = nullptr;
bool g_activated = false;  // the window came back to the front: the files are looked at again (a pack added meanwhile)

void MakeTarget() {
    ID3D11Texture2D* bb = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
    bb->Release();
}

bool CreateDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;  // 10 bits: no bands in the gradients
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    D3D_FEATURE_LEVEL got;
    const D3D_FEATURE_LEVEL want[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, want, 2, D3D11_SDK_VERSION,
                                               &sd, &g_swap, &g_dev, &got, &g_ctx);
    // no GPU driver: the software rasterizer still draws a launcher
    if (FAILED(hr))
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, want, 2, D3D11_SDK_VERSION, &sd,
                                           &g_swap, &g_dev, &got, &g_ctx);
    if (FAILED(hr)) return false;
    MakeTarget();
    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
    switch (msg) {
        case WM_SIZE:
            if (g_dev && wp != SIZE_MINIMIZED) {
                if (g_rtv) g_rtv->Release(), g_rtv = nullptr;
                g_swap->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
                MakeTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_KEYMENU) return 0;  // Alt alone
            break;
        case WM_ACTIVATEAPP:
            if (wp) g_activated = true;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
#else
// Linux: SDL's window and an OpenGL context
SDL_Window* g_window = nullptr;
bool g_activated = false;  // the window came back to the front: the files are looked at again (a pack added meanwhile)
#endif

// files

fs::path ExeDir() {
#ifdef _WIN32
    wchar_t p[MAX_PATH];
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    return fs::path(p).parent_path();
#else
    std::error_code ec;
    return fs::read_symlink("/proc/self/exe", ec).parent_path();
#endif
}

// the game's own search: assets beside the exe or in any folder above it
fs::path FindAssets() {
    fs::path dir = ExeDir();
    std::error_code ec;
    for (int i = 0; i < 6; i++) {
        if (fs::is_directory(dir / "assets", ec)) return dir / "assets";
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return {};
}

std::string Sha1(const std::vector<uint8_t>& data) {
    uint8_t digest[20];
#ifdef _WIN32
    if (BCryptHash(BCRYPT_SHA1_ALG_HANDLE, nullptr, 0, const_cast<PUCHAR>(data.data()), ULONG(data.size()), digest, 20) != 0)
        return {};
#else
    alignas(8) uint8_t ctx[128];  // the SDK's, as the game hashes it (runtime/kernel/crypto.cpp)
    crypto::ShaInit(ctx);
    crypto::ShaUpdate(ctx, data.data(), uint32_t(data.size()));
    crypto::ShaFinal(ctx, digest);
#endif
    std::string hex;
    for (uint8_t b : digest) {
        hex += "0123456789abcdef"[b >> 4];
        hex += "0123456789abcdef"[b & 15];
    }
    return hex;
}

std::string Utf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(size_t(std::max(n, 1)) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring Wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(size_t(std::max(n, 1)) - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

void Open(const fs::path& p) { ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

// The graphics the game draws with, as the SDK picks them (runtime/gpu/d3d12.cpp): the card the settings name (gpu)
// if it has Direct3D 12, else the fastest, else the default one, at feature level 11_0. Windows 10 and 11 all have
// Direct3D 12, but only graphics with a driver for it run it. CV_NO_D3D12=1 pretends none does, to see the warning.
struct Gpu {
    std::string name;
    bool d3d12 = false;
    bool software = false;  // Windows' own renderer on the CPU, no graphics driver installed
};

#ifdef _WIN32
// a null device: only whether one would be made; a null adapter is the default one
bool RunsD3D12(IDXGIAdapter1* a) {
    static const bool none = GetEnvironmentVariableW(L"CV_NO_D3D12", nullptr, 0) != 0;
    return !none && SUCCEEDED(D3D12CreateDevice(a, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr));
}

Gpu Describe(IDXGIAdapter1* a, bool d3d12) {
    Gpu g{{}, d3d12};
    DXGI_ADAPTER_DESC1 d{};
    if (a && SUCCEEDED(a->GetDesc1(&d))) {
        g.name = Utf8(d.Description);
        g.software = (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    }
    return g;
}

// the graphics cards to choose from, the fastest first, without Windows' software renderer
std::vector<Gpu> Gpus() {
    std::vector<Gpu> list;
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    Microsoft::WRL::ComPtr<IDXGIFactory6> f6;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> a;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return list;
    factory.As(&f6);
    for (UINT i = 0;; i++) {
        HRESULT hr = f6 ? f6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&a))
                        : factory->EnumAdapters1(i, &a);
        if (FAILED(hr)) break;
        Gpu g = Describe(a.Get(), RunsD3D12(a.Get()));
        if (!g.software) list.push_back(g);
    }
    return list;
}

// what the game takes with no card named: the fastest, else the default adapter
Gpu Automatic() {
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    Microsoft::WRL::ComPtr<IDXGIFactory6> f6;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return {};
    if (SUCCEEDED(factory.As(&f6))) f6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
    if (RunsD3D12(adapter.Get())) return Describe(adapter.Get(), true);
    bool d3d12 = RunsD3D12(nullptr);  // the game's second try
    adapter.Reset();
    factory->EnumAdapters1(0, &adapter);
    return Describe(adapter.Get(), d3d12);
}
#else
// Linux: the Vulkan devices, as the game takes them (the SDK's linux/present.cpp): the card the settings name, else a
// discrete one over an integrated one over the rest. A card runs the game when vkd3d makes a D3D12 device on it;
// CV_NO_D3D12=1 pretends none does, to see the warning.
struct VulkanCards {
    vkd3d_instance* instance = nullptr;
    std::vector<VkPhysicalDevice> cards;
    VulkanCards() {
        vkd3d_instance_create_info ici{};
        ici.type = VKD3D_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ici.pfn_signal_event = [](HANDLE) -> HRESULT { return S_OK; };
        ici.wchar_size = sizeof(WCHAR);
        if (FAILED(vkd3d_create_instance(&ici, &instance))) return;
        VkInstance vki = vkd3d_instance_get_vk_instance(instance);
        uint32_t n = 0;
        vkEnumeratePhysicalDevices(vki, &n, nullptr);
        cards.resize(n);
        vkEnumeratePhysicalDevices(vki, &n, cards.data());
    }
    ~VulkanCards() {
        if (instance) vkd3d_instance_decref(instance);
    }
    bool Runs(VkPhysicalDevice pd) const {
        static const bool none = std::getenv("CV_NO_D3D12") != nullptr;
        if (none || !instance) return false;
        vkd3d_device_create_info dci{};
        dci.type = VKD3D_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.minimum_feature_level = D3D_FEATURE_LEVEL_11_0;
        dci.instance = instance;
        dci.vk_physical_device = pd;
        ID3D12Device* dev = nullptr;
        if (FAILED(vkd3d_create_device(&dci, __uuidof(ID3D12Device), reinterpret_cast<void**>(&dev)))) return false;
        dev->Release();
        return true;
    }
    static Gpu Describe(VkPhysicalDevice pd, bool runs) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(pd, &p);
        return {p.deviceName, runs, p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU};
    }
    static int Rank(VkPhysicalDevice pd) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(pd, &p);
        return p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? 3
               : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2
               : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU            ? 0
                                                                        : 1;
    }
};

// the graphics cards to choose from, the fastest first, without a renderer on the processor
std::vector<Gpu> Gpus() {
    VulkanCards v;
    std::vector<VkPhysicalDevice> cards = v.cards;
    std::stable_sort(cards.begin(), cards.end(), [](VkPhysicalDevice a, VkPhysicalDevice b) {
        return VulkanCards::Rank(a) > VulkanCards::Rank(b);
    });
    std::vector<Gpu> list;
    for (VkPhysicalDevice pd : cards) {
        Gpu g = VulkanCards::Describe(pd, v.Runs(pd));
        if (!g.software) list.push_back(g);
    }
    return list;
}

// what the game takes with no card named: the fastest, the processor's renderer last
Gpu Automatic() {
    VulkanCards v;
    VkPhysicalDevice best = VK_NULL_HANDLE;
    for (VkPhysicalDevice pd : v.cards)
        if (!best || VulkanCards::Rank(pd) > VulkanCards::Rank(best)) best = pd;
    if (!best) return {};
    return VulkanCards::Describe(best, v.Runs(best));
}

#endif

// The game's check: an xbe one of the executables was made from, or that with the certificate's title, media and
// region patched; which release it is (kReleases), and so which executable plays it
enum class Release { Missing, Match, Patched, Other };
struct Copy {
    Release release = Release::Missing;  // Missing: no Xbox disc, or no default.xbe on it
    int which = -1;                       // the release in kReleases
    bool europe() const { return which > 0; }
};

Copy Check(const std::vector<uint8_t>& xbe) {
    std::string sha = Sha1(xbe);
    for (unsigned i = 0; i < kReleaseCount; i++)
        if (sha == kReleases[i].sha1) return {Release::Match, int(i)};
    uint32_t base = 0, cert = 0;
    if (xbe.size() >= 0x11C) {
        memcpy(&base, &xbe[0x104], 4);
        memcpy(&cert, &xbe[0x118], 4);
    }
    if (cert < base || uint64_t(cert - base) + 0xA8 > xbe.size()) return {Release::Other};
    for (unsigned i = 0; i < kReleaseCount; i++) {
        std::vector<uint8_t> copy = xbe;
        memcpy(&copy[cert - base + 0x0C], kReleases[i].certificate, 80);
        memcpy(&copy[cert - base + 0x9C], kReleases[i].certificate + 80, 12);
        if (Sha1(copy) == kReleases[i].sha1) return {Release::Patched, int(i)};
    }
    return {Release::Other};
}

fs::path ReleaseExe(int which) { return ExeDir() / app::ExeFile(kReleases[which].exe); }

// what the game will find
// the textures folder the game looks for, found the same way as assets
fs::path FindTextures() {
    fs::path dir = ExeDir();
    std::error_code ec;
    for (int i = 0; i < 6; i++) {
        if (fs::is_directory(dir / "textures", ec)) return dir / "textures";
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return {};
}

struct Files {
    fs::path assets;
    Copy xbe;
    struct Image {
        fs::path path;
        Copy copy;
    };
    std::vector<Image> isos;
    bool exe = false;  // Castlevania.exe, the USA release's, which the other starts when it has no game of its own
    fs::path textures;
    size_t pictures = 0;      // .png and .dds in textures and the folders inside it
    bool ps2_buttons = false;  // the PS2's button icons among them (texpack.cpp, the PS2 button layout)
    fs::path logo;             // the title logo among them, for the home page

    void Scan() {
        textures = FindTextures();
        pictures = 0;
        ps2_buttons = false;
        logo.clear();
        if (!textures.empty()) {
            std::error_code ec;
            for (auto it = fs::recursive_directory_iterator(textures, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
                std::wstring ext = it->path().extension().wstring();
                std::transform(ext.begin(), ext.end(), ext.begin(), towlower);
                if (ext != L".png" && ext != L".dds") continue;
                pictures++;
                std::wstring name = it->path().filename().wstring();
                ps2_buttons |= name.rfind(L"27818fa40a1117af-66279948575c23d7-", 0) == 0;
                // the SLUS-21168 pack has it twice, the same picture
                if (logo.empty() && (name.rfind(L"b0f0108366aad095-5623c99b0b0ecf41-", 0) == 0 ||
                                     name.rfind(L"fe15d42446e1b4e5-5623c99b0b0ecf41-", 0) == 0))
                    logo = it->path();
            }
        }
        assets = FindAssets();
        xbe = {};
        isos.clear();
        exe = fs::exists(ReleaseExe(0));
        if (assets.empty()) return;
        std::error_code ec;
        if (std::ifstream f{assets / kXbe, std::ios::binary}) xbe = Check({std::istreambuf_iterator<char>(f), {}});
        for (auto& e : fs::directory_iterator(assets, ec)) {
            std::wstring ext = e.path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), towlower);
            if (ext != L".iso" || !e.is_regular_file(ec)) continue;
            Image image{e.path()};
            kernel::Disc disc;
            std::string error;
            std::vector<uint8_t> data;
            if (disc.Open(e.path(), error) && disc.Load(kXbe, data)) image.copy = Check(data);
            isos.push_back(image);
        }
        std::sort(isos.begin(), isos.end(), [](const Image& a, const Image& b) { return a.path < b.path; });
    }
    static bool Good(const Copy& c) { return c.release == Release::Match || c.release == Release::Patched; }
    // the releases a copy here plays, with their executables there: kReleases' order, the USA one first
    std::vector<int> Releases() const {
        std::vector<int> list;
        for (unsigned which = 0; which < kReleaseCount; which++) {
            bool found = (Good(xbe) && xbe.which == int(which)) ||
                         std::any_of(isos.begin(), isos.end(), [&](const Image& i) { return Good(i.copy) && i.copy.which == int(which); });
            std::error_code ec;
            if (found && fs::exists(ReleaseExe(int(which)), ec)) list.push_back(int(which));
        }
        return list;
    }
    // The release Play starts: the one chosen (Settings::release, by its executable's name) when there is a copy of it,
    // else the one each executable picks (the SDK's game_files.cpp), the USA release first; -1 with none.
    int Played(const std::string& chosen) const {
        std::vector<int> list = Releases();
        for (int which : list)
            if (chosen == kReleases[which].exe) return which;
        return list.empty() ? -1 : list[0];
    }
    fs::path Player(const std::string& chosen) const {
        int which = Played(chosen);
        return which < 0 ? fs::path() : ReleaseExe(which);
    }
    bool Playable() const { return !Releases().empty(); }
};

// a disc image into assets: a hard link on the same drive, a copy with progress otherwise
struct Import {
    std::atomic<bool> running{false};
    std::atomic<float> progress{0};
    std::string result;
    std::thread worker;

#ifdef _WIN32
    static DWORD CALLBACK Progress(LARGE_INTEGER total, LARGE_INTEGER done, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD,
                                  HANDLE, HANDLE, LPVOID self) {
        static_cast<Import*>(self)->progress = total.QuadPart ? float(double(done.QuadPart) / double(total.QuadPart)) : 0;
        return PROGRESS_CONTINUE;
    }
#else
    // Linux: a copy in 4 MB pieces, its progress as it goes; false with the error
    bool Copy(const fs::path& from, const fs::path& to, std::string& error) {
        std::error_code ec;
        if (fs::exists(to, ec)) {
            error = "exists";
            return false;
        }
        uint64_t total = fs::file_size(from, ec), done = 0;
        std::ifstream in(from, std::ios::binary);
        std::ofstream out(to, std::ios::binary);
        std::vector<char> buf(4 << 20);
        while (in && out) {
            in.read(buf.data(), std::streamsize(buf.size()));
            out.write(buf.data(), in.gcount());
            done += uint64_t(in.gcount());
            progress = total ? float(double(done) / double(total)) : 0;
        }
        if (!out || done != total) {
            error = std::strerror(errno);
            out.close();
            fs::remove(to, ec);
            return false;
        }
        return true;
    }
#endif

    void Start(fs::path from, fs::path to) {
        if (running) return;
        if (worker.joinable()) worker.join();
        running = true;
        progress = 0;
        result.clear();
        worker = std::thread([this, from, to] {
            std::error_code ec;
            fs::create_directories(to.parent_path(), ec);
#ifdef _WIN32
            if (CreateHardLinkW(to.c_str(), from.c_str(), nullptr)) {
                result = F("Linked {} into assets.", {Utf8(to.filename().wstring())});
            } else if (CopyFileExW(from.c_str(), to.c_str(), Progress, this, nullptr, COPY_FILE_FAIL_IF_EXISTS)) {
                result = F("Copied {} into assets.", {Utf8(to.filename().wstring())});
            } else {
                result = F("Could not copy the disc image (error {}).", {std::to_string(GetLastError())});
            }
#else
            std::string error;
            if (fs::create_hard_link(from, to, ec), !ec) {
                result = F("Linked {} into assets.", {to.filename().string()});
            } else if (Copy(from, to, error)) {
                result = F("Copied {} into assets.", {to.filename().string()});
            } else {
                result = F("Could not copy the disc image (error {}).", {error});
            }
#endif
            running = false;
        });
    }
    ~Import() {
        if (worker.joinable()) worker.join();
    }
};

// The European release's languages, as the console's (XGetLanguage), each by its name in itself; the USA release
// plays English
struct GameLanguage {
    uint32_t console;
    const char* name;
    const char* launcher;  // the launcher's language that picks it at first (words.h)
};
const GameLanguage kGameLanguages[] = {
    {1, "English", "en"}, {3, "Deutsch", "de"}, {5, "Español", "es"}, {4, "Français", "fr"}, {6, "Italiano", "it"},
};

// a player who reads the launcher in one of the game's languages plays in it, until they choose
uint32_t GameLanguageFor(const std::string& launcher) {
    for (const GameLanguage& g : kGameLanguages)
        if (launcher == g.launcher) return g.console;
    return 1;
}

// settings: the keys the game reads, the rest of the file kept

struct Settings {
    std::string display_mode = "borderless";
    uint32_t width = 0, height = 0;
    uint32_t render_percent = 100;
    uint32_t aniso = 0;
    bool vsync = true;
    bool widescreen = true;
    bool fps_counter = false;
    bool pause_on_focus_loss = true;
    uint32_t menu_key = VK_F10;
    bool keep_open = false;        // the launcher's own
    std::string language = "en";   // the launcher's own too, a code of words.h
    uint32_t game_language = 1;    // the European release's, as the console's (XGetLanguage): kGameLanguages
    bool texture_pack = true;
    uint32_t texture_pack_size = 2048;  // 0 full size
    uint32_t button_layout = 0;         // 0 Xbox, 1 PS2
    bool lament_save = false;           // the Moai of a Lament of Innocence save (hooks.cpp)
    bool ps2_stick = true;              // the sticks as the PS2 read them (hooks.cpp)
    bool invert_camera_x = false, invert_camera_y = false;  // the camera stick the other way (keyboard.cpp)
    uint32_t fov = 100;                 // percent of the game's field of view in play (game/view.cpp)
    uint32_t draw_distance = 100;       // percent of the game's far plane (game/view.cpp)
    uint32_t fog_distance = 100;        // percent of the game's fog distance, 0 none (game/view.cpp)
    bool hud_edges = true;              // widescreen: the HUD at the screen's corners (widescreen.cpp)
    std::string gpu;                    // the graphics card by its name, empty the fastest (the SDK's)
    std::string release;                // the release played, by its executable's name, when there are more (the SDK's)
    toml::table rest;

    fs::path File() const { return ExeDir() / kSettings; }

    void Load() {
        try {
            rest = toml::parse_file(File().string());
        } catch (...) {
            rest = {};
        }
        display_mode = rest["display_mode"].value_or(display_mode);
        width = rest["width"].value_or(width);
        height = rest["height"].value_or(height);
        render_percent = rest["render_percent"].value_or(render_percent);
        aniso = rest["anisotropic_filtering"].value_or(aniso);
        vsync = rest["vsync"].value_or(vsync);
        widescreen = rest["widescreen"].value_or(widescreen);
        fps_counter = rest["fps_counter"].value_or(fps_counter);
        pause_on_focus_loss = rest["pause_on_focus_loss"].value_or(pause_on_focus_loss);
        menu_key = rest["menu_key"].value_or(menu_key);
        keep_open = rest["launcher_keep_open"].value_or(keep_open);
        language = rest["launcher_language"].value_or(language);
        game_language = rest["game_language"].value_or(GameLanguageFor(language));
        texture_pack = rest["texture_pack"].value_or(texture_pack);
        texture_pack_size = rest["texture_pack_size"].value_or(texture_pack_size);
        button_layout = rest["button_layout"].value_or(button_layout);
        lament_save = rest["lament_save"].value_or(lament_save);
        ps2_stick = rest["ps2_stick"].value_or(ps2_stick);
        invert_camera_x = rest["invert_camera_x"].value_or(invert_camera_x);
        invert_camera_y = rest["invert_camera_y"].value_or(invert_camera_y);
        fov = rest["fov"].value_or(fov);
        draw_distance = rest["draw_distance"].value_or(draw_distance);
        fog_distance = rest["fog_distance"].value_or(fog_distance);
        hud_edges = rest["hud_edges"].value_or(hud_edges);
        gpu = rest["gpu"].value_or(gpu);
        release = rest["release"].value_or(release);
    }

    bool Save() {
        rest.insert_or_assign("display_mode", display_mode);
        rest.insert_or_assign("width", int64_t(width));
        rest.insert_or_assign("height", int64_t(height));
        rest.insert_or_assign("render_percent", int64_t(render_percent));
        rest.insert_or_assign("anisotropic_filtering", int64_t(aniso));
        rest.insert_or_assign("vsync", vsync);
        rest.insert_or_assign("widescreen", widescreen);
        rest.insert_or_assign("fps_counter", fps_counter);
        rest.insert_or_assign("pause_on_focus_loss", pause_on_focus_loss);
        rest.insert_or_assign("menu_key", int64_t(menu_key));
        rest.insert_or_assign("launcher_keep_open", keep_open);
        rest.insert_or_assign("launcher_language", language);
        rest.insert_or_assign("game_language", int64_t(game_language));
        rest.insert_or_assign("texture_pack", texture_pack);
        rest.insert_or_assign("texture_pack_size", int64_t(texture_pack_size));
        rest.insert_or_assign("button_layout", int64_t(button_layout));
        rest.insert_or_assign("lament_save", lament_save);
        rest.insert_or_assign("ps2_stick", ps2_stick);
        rest.insert_or_assign("invert_camera_x", invert_camera_x);
        rest.insert_or_assign("invert_camera_y", invert_camera_y);
        rest.insert_or_assign("fov", int64_t(fov));
        rest.insert_or_assign("draw_distance", int64_t(draw_distance));
        rest.insert_or_assign("fog_distance", int64_t(fog_distance));
        rest.insert_or_assign("hud_edges", hud_edges);
        rest.insert_or_assign("gpu", gpu);
        rest.insert_or_assign("release", release);
        std::ofstream f(File());
        f << rest << "\n";
        return bool(f);
    }

    void Defaults() {
        Settings d;
        d.rest = std::move(rest);
        d.keep_open = keep_open;
        d.language = language;
        d.game_language = game_language;
        d.release = release;  // which game, not how it plays
        *this = std::move(d);
    }
};

// UI pieces

struct Res {
    uint32_t w, h;
};

#ifndef _WIN32
// Linux: the primary display's, from SDL
std::vector<Res> Resolutions() {
    std::vector<Res> list{{0, 0}};
    int n = 0;
    if (SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &n)) {
        for (int i = 0; i < n; i++) {
            uint32_t w = uint32_t(modes[i]->w), h = uint32_t(modes[i]->h);
            if (w < 640 || h < 480) continue;
            bool seen = false;
            for (const Res& r : list) seen |= r.w == w && r.h == h;
            if (!seen) list.push_back({w, h});
        }
        SDL_free(modes);
    }
    std::sort(list.begin() + 1, list.end(), [](Res a, Res b) { return a.w != b.w ? a.w > b.w : a.h > b.h; });
    return list;
}

std::string ResName(Res r) {
    if (!r.w) {
        const SDL_DisplayMode* d = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
        return F("Desktop ({})", {d ? std::to_string(d->w) + "x" + std::to_string(d->h) : std::string("?")});
    }
    return std::to_string(r.w) + "x" + std::to_string(r.h);
}
#else
std::vector<Res> Resolutions() {
    std::vector<Res> list{{0, 0}};
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++) {
        if (dm.dmPelsWidth < 640 || dm.dmPelsHeight < 480) continue;
        bool seen = false;
        for (const Res& r : list) seen |= r.w == dm.dmPelsWidth && r.h == dm.dmPelsHeight;
        if (!seen) list.push_back({dm.dmPelsWidth, dm.dmPelsHeight});
    }
    std::sort(list.begin() + 1, list.end(), [](Res a, Res b) { return a.w != b.w ? a.w > b.w : a.h > b.h; });
    return list;
}

std::string ResName(Res r) {
    if (!r.w) {
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        EnumDisplaySettingsW(nullptr, ENUM_REGISTRY_SETTINGS, &dm);
        return F("Desktop ({})", {std::to_string(dm.dmPelsWidth) + "x" + std::to_string(dm.dmPelsHeight)});
    }
    return std::to_string(r.w) + "x" + std::to_string(r.h);
}
#endif

std::string KeyName(uint32_t vk) {
    wchar_t name[64] = {};
    LONG scan = LONG(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)) << 16;
    if ((vk >= VK_PRIOR && vk <= VK_DOWN) || vk == VK_INSERT || vk == VK_DELETE) scan |= 1 << 24;
    if (!GetKeyNameTextW(scan, name, 64)) return F("Key {}", {std::to_string(vk)});
    return Utf8(name);
}

// the look: the game's own. Its menus are deep navy, a teal band along the top and steel-blue fog at the right; a
// page's name is on an orange banner, the choice on a slanted glossy green bar, a box framed in thin light blue; its
// title screen is the gold logo over a purple highlight, the menus' cursor a silver arrowhead. Palatino Linotype
// stands in for its serif. The logo and the crest are the player's own when there (Art). Settings save as they change.

float g_scale = 1;
ImFont* g_body = nullptr;     // Palatino 16.5, icons merged
ImFont* g_small = nullptr;    // Palatino 14.5, descriptions
ImFont* g_semi = nullptr;     // Palatino Bold 16.5, icons merged
ImFont* g_italic = nullptr;   // Palatino Bold Italic 18: the sidebar, buttons, chosen options
ImFont* g_head = nullptr;     // Palatino Bold Italic 30, a page's name on its banner
ImFont* g_hero = nullptr;     // Palatino Bold Italic 58, the title on the home page without the pack's logo
ImFont* g_heroSub = nullptr;  // Palatino Bold Italic 28
ImFont* g_button = nullptr;   // Palatino Bold Italic 26, Play

float S(float v) { return v * g_scale; }

ImU32 Rgb(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }
// the menus' navy, their light blue frames, their white text
const ImU32 kBg = Rgb(0, 3, 18), kCard = Rgb(3, 12, 50, 232), kCardLo = Rgb(1, 6, 30, 232), kCardHi = Rgb(10, 26, 78);
const ImU32 kLine = Rgb(56, 130, 190), kLineHi = Rgb(132, 196, 236), kLineLo = Rgb(24, 52, 100);
const ImU32 kText = Rgb(242, 240, 232), kMuted = Rgb(142, 158, 186);
const ImU32 kTealHi = Rgb(92, 178, 172);
// the logo's gold and red, the banners' orange, the title screen's purple
const ImU32 kGoldHi = Rgb(255, 238, 176), kGoldLo = Rgb(150, 100, 34), kBlood = Rgb(184, 24, 28);
const ImU32 kOrange = Rgb(255, 166, 50), kOrangeLo = Rgb(238, 122, 20), kEmber = Rgb(230, 82, 74), kEmberLo = Rgb(204, 48, 72);
const ImU32 kPurple = Rgb(150, 50, 150);
// the menus' green, for what is chosen and the main action
const ImU32 kAccent = Rgb(34, 220, 64), kAccentHi = Rgb(120, 255, 140), kAccentLo = Rgb(8, 116, 30);
const ImU32 kGreen = Rgb(86, 234, 124), kRed = Rgb(242, 84, 72), kAmber = Rgb(252, 166, 66);
ImVec4 V(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

ImVec2 operator+(ImVec2 a, ImVec2 b) { return {a.x + b.x, a.y + b.y}; }
ImVec2 operator-(ImVec2 a, ImVec2 b) { return {a.x - b.x, a.y - b.y}; }

#ifdef _WIN32
// Segoe Fluent / MDL2 icons, for status lines and the language
enum : uint32_t {
    kIconCheck = 0xE73E, kIconGlobe = 0xE774, kIconWarn = 0xE7BA, kIconDisc = 0xE958,
};
const ImWchar kIconRanges[] = {0xE73E, 0xE73E, 0xE774, 0xE774, 0xE7BA, 0xE7BA, 0xE958, 0xE958, 0};
#else
// Linux: the same icons from Material Icons (a release's fonts folder): check, language, warning, album
enum : uint32_t {
    kIconCheck = 0xE5CA, kIconGlobe = 0xE894, kIconWarn = 0xE002, kIconDisc = 0xE019,
};
const ImWchar kIconRanges[] = {0xE002, 0xE002, 0xE019, 0xE019, 0xE5CA, 0xE5CA, 0xE894, 0xE894, 0};
#endif
// the letters of the launcher's languages: Latin with the accents of Portuguese, Spanish and Italian, Cyrillic, and
// dashes, quotes and the ellipsis
const ImWchar kTextRanges[] = {0x0020, 0x00FF, 0x0400, 0x045F, 0x2013, 0x2026, 0};

std::string Icon(uint32_t cp) {
    char s[5] = {};
    ImTextCharToUtf8(s, cp);
    return s;
}

void Text(ImFont* f, ImU32 col, const char* s) {
    ImGui::PushFont(f);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted(s);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void Wrapped(ImFont* f, ImU32 col, const char* s) {
    ImGui::PushFont(f);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextWrapped("%s", s);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

// an animated 0..1 per id, easing toward on
float Ease(ImGuiID id, bool on, float speed = 12) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    float t = st->GetFloat(id, on ? 1.0f : 0.0f);
    float dt = ImGui::GetIO().DeltaTime;
    t += ((on ? 1.0f : 0.0f) - t) * std::min(1.0f, dt * speed);
    st->SetFloat(id, t);
    return t;
}

ImU32 Mix(ImU32 a, ImU32 b, float t) {
    ImVec4 x = V(a), y = V(b);
    return ImGui::ColorConvertFloat4ToU32({x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t});
}

// c with its alpha times a
ImU32 Fade(ImU32 c, float a) {
    float alpha = float((c >> IM_COL32_A_SHIFT) & 0xFF) * std::clamp(a, 0.0f, 1.0f);
    return (c & ~IM_COL32_A_MASK) | (ImU32(alpha + 0.5f) << IM_COL32_A_SHIFT);
}

// 0..1, the same for the same i: where a streak or a torn strip ends
float Hash01(int i) {
    float v = std::sin(float(i) * 12.9898f + 4.1414f) * 43758.5453f;
    return v - std::floor(v);
}

// text with a dark edge around it, as the game letters its menus
void Outlined(ImDrawList* dl, ImFont* f, ImVec2 p, ImU32 col, ImU32 edge, const char* s, float px = 1.5f,
              float size = 0) {
    size = size ? size : f->FontSize;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx || dy) dl->AddText(f, size, p + ImVec2(S(px) * dx, S(px) * dy), edge, s);
    dl->AddText(f, size, p, col, s);
}

// text in the logo's gold, lit from above, its edge dark
void Gold(ImDrawList* dl, ImFont* f, ImVec2 p, const char* s) {
    Outlined(dl, f, p, Rgb(0, 0, 0, 0), Rgb(36, 18, 2, 230), s, 2);
    int v0 = dl->VtxBuffer.Size;
    dl->AddText(f, f->FontSize, p, IM_COL32_WHITE, s);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, dl->VtxBuffer.Size, p + ImVec2(0, f->FontSize * 0.25f),
                                                  p + ImVec2(0, f->FontSize * 0.95f), kGoldHi, kGoldLo);
}

// a quad shaded from its top edge to its bottom one: top left, top right, bottom right, bottom left
void Quad(ImDrawList* dl, ImVec2 tl, ImVec2 tr, ImVec2 br, ImVec2 bl, ImU32 top, ImU32 bottom) {
    ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    dl->PrimReserve(6, 4);
    ImDrawIdx i = ImDrawIdx(dl->_VtxCurrentIdx);
    for (int k : {0, 1, 2, 0, 2, 3}) dl->PrimWriteIdx(ImDrawIdx(i + k));
    dl->PrimWriteVtx(tl, uv, top);
    dl->PrimWriteVtx(tr, uv, top);
    dl->PrimWriteVtx(br, uv, bottom);
    dl->PrimWriteVtx(bl, uv, bottom);
}

// the menus' bar: its top edge leaning skew to the right, glossy, an edge drawn smooth
void Bar(ImDrawList* dl, ImVec2 a, ImVec2 b, float skew, ImU32 top, ImU32 bottom, ImU32 edge) {
    Quad(dl, {a.x + skew, a.y}, {b.x, a.y}, {b.x - skew, b.y}, {a.x, b.y}, top, bottom);
    float mid = a.y + (b.y - a.y) * 0.45f;
    Quad(dl, {a.x + skew, a.y}, {b.x, a.y}, {b.x - skew * 0.45f, mid}, {a.x + skew * 0.55f, mid}, Rgb(255, 255, 255, 64),
         Rgb(255, 255, 255, 8));
    dl->AddQuad({a.x + skew, a.y}, {b.x, a.y}, {b.x - skew, b.y}, {a.x, b.y}, edge, S(1));
}

// a soft light: col at c, nothing at r
void Glow(ImDrawList* dl, ImVec2 c, ImVec2 r, ImU32 col, int segments = 48) {
    ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    ImU32 edge = Fade(col, 0);
    dl->PrimReserve(segments * 3, segments + 1);
    ImDrawIdx base = ImDrawIdx(dl->_VtxCurrentIdx);
    dl->PrimWriteVtx(c, uv, col);
    for (int i = 0; i < segments; i++) {
        float t = 2 * IM_PI * float(i) / float(segments);
        dl->PrimWriteVtx({c.x + std::cos(t) * r.x, c.y + std::sin(t) * r.y}, uv, edge);
    }
    for (int i = 0; i < segments; i++) {
        dl->PrimWriteIdx(base);
        dl->PrimWriteIdx(ImDrawIdx(base + 1 + i));
        dl->PrimWriteIdx(ImDrawIdx(base + 1 + (i + 1) % segments));
    }
}

// a box as the game frames its messages: navy, a light blue line and a fainter one inside it, a point at each corner
void Box(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 top = kCard, ImU32 bottom = kCardLo, ImU32 line = kLine) {
    dl->AddRectFilledMultiColor(a, b, top, top, bottom, bottom);
    dl->AddRect(a, b, line);
    dl->AddRect(a + ImVec2(S(3), S(3)), b - ImVec2(S(3), S(3)), kLineLo);
    float d = S(3.5f);
    for (ImVec2 c : {a, ImVec2{b.x, a.y}, b, ImVec2{a.x, b.y}})
        dl->AddQuadFilled({c.x, c.y - d}, {c.x + d, c.y}, {c.x, c.y + d}, {c.x - d, c.y}, kLineHi);
}

// the menus' cursor: a silver arrowhead pointing right, its back at at
void Cursor(ImDrawList* dl, ImVec2 at, float len, float alpha = 1) {
    float hh = len * 0.3f;
    ImVec2 tip{at.x + len, at.y}, top{at.x, at.y - hh}, bottom{at.x, at.y + hh}, notch{at.x + len * 0.32f, at.y};
    dl->AddTriangleFilled(top, tip, notch, Fade(Rgb(238, 248, 248), alpha));
    dl->AddTriangleFilled(notch, tip, bottom, Fade(Rgb(112, 170, 178), alpha));
    dl->AddQuad(top, tip, bottom, notch, Fade(Rgb(10, 40, 52), alpha), S(1));
}

// a line fading out at both ends
void Rule(ImDrawList* dl, ImVec2 a, float w, ImU32 col) {
    float h = std::max(1.0f, S(1));
    dl->AddRectFilledMultiColor(a, {a.x + w * 0.5f, a.y + h}, Fade(col, 0), col, col, Fade(col, 0));
    dl->AddRectFilledMultiColor({a.x + w * 0.5f, a.y}, {a.x + w, a.y + h}, col, Fade(col, 0), Fade(col, 0), col);
}

// a page's banner, as the menus have theirs: orange burning out to ember red, drawn in strips that start and end
// unevenly as if torn, a white rule under it
void Banner(ImDrawList* dl, ImVec2 a, float w, float h) {
    float solid = w * 0.56f, warm = w * 0.78f;
    const int strips = 14;
    for (int i = 0; i < strips; i++) {
        float t0 = float(i) / strips, t1 = float(i + 1) / strips;
        float y0 = a.y + h * t0, y1 = a.y + h * t1;
        ImU32 o0 = Mix(kOrange, kOrangeLo, t0), o1 = Mix(kOrange, kOrangeLo, t1);
        ImU32 e0 = Mix(kEmber, kEmberLo, t0), e1 = Mix(kEmber, kEmberLo, t1);
        float x0 = a.x + S(5) * Hash01(i + 40), end = a.x + warm + (w - warm) * (0.3f + 0.7f * Hash01(i));
        dl->AddRectFilledMultiColor({x0, y0}, {a.x + solid, y1}, o0, o0, o1, o1);
        dl->AddRectFilledMultiColor({a.x + solid, y0}, {a.x + warm, y1}, o0, e0, e1, o1);
        dl->AddRectFilledMultiColor({a.x + warm, y0}, {end, y1}, e0, Fade(e0, 0), Fade(e1, 0), e1);
    }
    ImU32 white = Rgb(244, 244, 240, 230);
    float ry = a.y + h + S(3);
    dl->AddRectFilledMultiColor({a.x, ry}, {a.x + w * 0.92f, ry + S(2)}, white, Fade(white, 0), Fade(white, 0), white);
}

// an on/off switch; true when flipped
// CV_LAUNCHER_STATE=<file>, for tests (tools/cvtest/launcher_test.py): where the controls are, a switch, list or
// slider by its id (a list's choices and a row of choices by their index after it: "mode.2"), a button by its words,
// a page of the sidebar as page.<n>, a status line as status:<its words>. A control named in <file>.click is clicked
// (activated, Marks::Click) at the next frame, and the file goes.
Marks g_marks("CV_LAUNCHER_STATE");

// <CV_LAUNCHER_STATE>.click's control clicked, once read
void TestClick() {
    static const std::string file = [] {
        const char* f = std::getenv("CV_LAUNCHER_STATE");
        return f ? std::string(f) + ".click" : std::string();
    }();
    std::error_code ec;
    if (file.empty() || !fs::exists(file, ec)) return;
    std::string name;
    std::getline(std::ifstream(file), name);
    if (g_marks.Click(name)) fs::remove(file, ec);
}

bool Toggle(const char* id, bool& v) {
    ImVec2 size{S(46), S(22)};
    ImVec2 a = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(id, size);
    g_marks.Mark(id);
    if (pressed) v = !v;
    float t = Ease(ImGui::GetItemID(), v);
    bool hover = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float r = size.y * 0.5f;
    dl->AddRectFilled(a, a + size, Rgb(2, 8, 32), r);
    if (t > 0.01f) {
        dl->AddRectFilled(a, a + size, Fade(Mix(kAccentLo, hover ? kAccentHi : kAccent, 0.55f), t), r);
        dl->AddRectFilled(a + ImVec2(S(4), S(2)), {a.x + size.x - S(4), a.y + size.y * 0.45f}, Rgb(255, 255, 255, int(60 * t)), r);
    }
    dl->AddRect(a, a + size, hover ? kLineHi : kLine, r);
    ImVec2 c{a.x + r + t * (size.x - 2 * r), a.y + r};
    dl->AddCircleFilled(c, r - S(4), Rgb(230, 236, 240), 24);
    dl->AddCircle(c, r - S(4), Rgb(16, 40, 60), 24);
    return pressed;
}

// joined choices, the one taken on the menus' green bar
template <typename T>
bool Segmented(const char* id, T& value, const std::vector<T>& values, const std::vector<std::string>& names) {
    ImGui::PushID(id);
    float avail = ImGui::GetContentRegionAvail().x;
    float h = S(32), w = avail / float(values.size());
    ImVec2 a = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(a, a + ImVec2(avail, h), Rgb(1, 5, 24));
    dl->AddRect(a, a + ImVec2(avail, h), kLineLo);
    bool changed = false;
    for (size_t i = 0; i < values.size(); i++) {
        ImVec2 p{a.x + w * float(i), a.y};
        ImGui::SetCursorScreenPos(p);
        ImGui::PushID(int(i));
        if (ImGui::InvisibleButton("seg", {w, h}) && !(values[i] == value)) {
            value = values[i];
            changed = true;
        }
        g_marks.Mark(std::string(id) + "." + std::to_string(i));
        bool on = values[i] == value, hover = ImGui::IsItemHovered();
        float t = Ease(ImGui::GetItemID(), on, 16);
        ImVec2 in{S(3), S(3)};
        if (t > 0.01f)
            Bar(dl, p + in, p + ImVec2(w, h) - in, S(7), Fade(hover ? kAccentHi : kAccent, t), Fade(kAccentLo, t),
                Fade(Rgb(0, 60, 14), t));
        else if (hover)
            dl->AddRectFilled(p + in, p + ImVec2(w, h) - in, Rgb(255, 255, 255, 14));
        // a name longer than its part, as some languages' are, a little smaller
        ImFont* f = on ? g_italic : g_semi;
        float size = f->FontSize, room = w - S(14);
        ImVec2 ts = f->CalcTextSizeA(size, FLT_MAX, 0, names[i].c_str());
        if (ts.x > room) {
            size *= std::max(0.72f, room / ts.x);
            ts = f->CalcTextSizeA(size, FLT_MAX, 0, names[i].c_str());
        }
        ImVec2 tp = p + ImVec2((w - ts.x) * 0.5f, (h - ts.y) * 0.5f);
        if (on) Outlined(dl, f, tp, kText, Rgb(0, 50, 12, 220), names[i].c_str(), 1, size);
        else dl->AddText(f, size, tp, hover ? kText : kMuted, names[i].c_str());
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(a);
    ImGui::Dummy({avail, h});
    ImGui::PopID();
    return changed;
}

// a value along a track, the part up to it on the menus' green bar, the value's name on it; dragged, clicked, or
// stepped with the arrow keys while it has the focus
bool Slider(const char* id, uint32_t& value, uint32_t lo, uint32_t hi, const std::string& name) {
    ImGui::PushID(id);
    float avail = ImGui::GetContentRegionAvail().x, h = S(32);
    ImVec2 a = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("track", {avail, h});
    g_marks.Mark(id);
    bool hover = ImGui::IsItemHovered(), changed = false;
    uint32_t v = std::clamp(value, lo, hi);
    if (ImGui::IsItemActive()) {
        float t = std::clamp((ImGui::GetIO().MousePos.x - a.x) / avail, 0.0f, 1.0f);
        v = lo + uint32_t(std::lround(t * float(hi - lo)));
    }
    if (ImGui::IsItemFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && v > lo) v--;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && v < hi) v++;
    }
    if (v != value) {
        value = v;
        changed = true;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(a, a + ImVec2(avail, h), Rgb(1, 5, 24));
    dl->AddRect(a, a + ImVec2(avail, h), kLineLo);
    float t = hi > lo ? float(v - lo) / float(hi - lo) : 0.0f;
    ImVec2 in{S(3), S(3)};
    float right = std::max(avail * t, S(18));
    Bar(dl, a + in, a + ImVec2(right, h) - in, S(7), hover ? kAccentHi : kAccent, kAccentLo, Rgb(0, 60, 14));
    ImVec2 ts = g_italic->CalcTextSizeA(g_italic->FontSize, FLT_MAX, 0, name.c_str());
    Outlined(dl, g_italic, a + ImVec2((avail - ts.x) * 0.5f, (h - ts.y) * 0.5f), kText, Rgb(0, 50, 12, 220), name.c_str(), 1);
    ImGui::PopID();
    return changed;
}

template <typename T>
void Combo(const char* id, T& value, const std::vector<T>& values, const std::vector<std::string>& names) {
    size_t cur = 0;
    for (size_t i = 0; i < values.size(); i++)
        if (values[i] == value) cur = i;
    ImGui::SetNextItemWidth(-FLT_MIN);
    std::string name = id + (id[0] == '#' && id[1] == '#' ? 2 : 0);
    bool open = ImGui::BeginCombo(id, names[cur].c_str());
    g_marks.Mark(name);
    if (open) {
        for (size_t i = 0; i < values.size(); i++) {
            if (ImGui::Selectable(names[i].c_str(), i == cur)) value = values[i];
            g_marks.Mark(name + "." + std::to_string(i));
            if (i == cur) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

// Primary: the menus' green bar; Secondary: their grey one; Ghost: words, lit purple under the pointer
enum class Kind { Primary, Secondary, Ghost };

bool Button(const char* label, ImVec2 size, Kind kind = Kind::Secondary, ImFont* font = nullptr) {
    font = font ? font : g_italic;
    ImVec2 ts = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, label);
    if (size.y == 0) size.y = S(38);
    if (size.x == 0) size.x = ts.x + S(40) + size.y * 0.6f;
    if (size.x < 0) size.x = ImGui::GetContentRegionAvail().x;
    ImVec2 a = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(label, size);
    g_marks.Mark(label);
    bool disabled = ImGui::GetItemFlags() & ImGuiItemFlags_Disabled;
    float hv = Ease(ImGui::GetItemID(), ImGui::IsItemHovered() && !disabled, 14);
    bool held = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float skew = size.y * 0.3f;
    ImU32 top = 0, bottom = 0, edge = Rgb(0, 0, 0, 170), text_col = kText;
    switch (kind) {
        case Kind::Primary:
            top = Mix(kAccent, kAccentHi, hv * 0.5f);
            bottom = Mix(kAccentLo, kAccent, hv * 0.35f);
            edge = Rgb(0, 54, 12, 230);
            break;
        case Kind::Secondary:
            top = Mix(Rgb(84, 92, 96), Rgb(110, 120, 126), hv);
            bottom = Mix(Rgb(38, 42, 48), Rgb(52, 58, 66), hv);
            break;
        default: text_col = Mix(kMuted, kText, hv); break;
    }
    if (held && kind != Kind::Ghost) {
        top = Mix(top, bottom, 0.5f);
        bottom = Mix(bottom, Rgb(0, 0, 0), 0.3f);
    }
    if (disabled) {
        top = Rgb(48, 52, 58);
        bottom = Rgb(30, 32, 38);
        text_col = Rgb(118, 122, 132);
    }
    if (kind == Kind::Primary && !disabled)  // a green light under the main action
        Glow(dl, a + ImVec2(size.x * 0.5f, size.y * 0.5f), {size.x * 0.62f, size.y * 1.1f}, Rgb(34, 220, 64, int(26 + 40 * hv)));
    if (kind != Kind::Ghost) {
        Bar(dl, a, a + size, skew, top, bottom, disabled ? Rgb(0, 0, 0, 120) : edge);
    } else if (hv > 0.01f) {
        ImU32 p = Fade(kPurple, 0.55f * hv);
        dl->AddRectFilledMultiColor(a, a + size, Fade(p, 0), p, p, Fade(p, 0));
    }
    ImVec2 tp = a + ImVec2((size.x - ts.x) * 0.5f, (size.y - ts.y) * 0.5f);
    if (kind == Kind::Ghost) dl->AddText(font, font->FontSize, tp, text_col, label);
    else Outlined(dl, font, tp, text_col, disabled ? Rgb(0, 0, 0, 90) : edge, label);
    return pressed && !disabled;
}

// a box framed as the game frames its messages; its contents laid out inside
void BeginCard(const char* id, float pad_y = 0) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(22), pad_y ? pad_y : S(18)});
    ImGui::BeginChild(id, {0, 0}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
}
void EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    // the frame on the page's draw list, which ImGui draws before the card's own
    Box(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    ImGui::Dummy({0, S(6)});
}

// a setting: title and description at the left, its control in the column at the right
bool BeginSetting(const char* title, const char* desc, bool first = false) {
    if (!first) {
        Rule(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), ImGui::GetContentRegionAvail().x, kLine);
        ImGui::Dummy({0, S(7)});
    }
    float control = S(300);
    float w = ImGui::GetContentRegionAvail().x - control - S(24);
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
    Text(g_semi, kText, title);
    if (desc) Wrapped(g_small, kMuted, desc);
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    float text_h = ImGui::GetItemRectSize().y;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - control);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (text_h - S(32)) * 0.5f));
    ImGui::BeginGroup();
    return true;
}
void EndSetting() {
    ImGui::EndGroup();
    ImGui::Dummy({0, S(4)});
}

// a switch at the right end of the control column
void ToggleRight(const char* id, bool& v) {
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - S(46));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(5));
    Toggle(id, v);
}

// a page's name on its banner, what it is for under it
void PageTitle(const char* title, const char* desc) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos();
    float w = std::min(ImGui::GetContentRegionAvail().x, S(620)), h = S(42);
    Banner(dl, a, w, h);
    Outlined(dl, g_head, {a.x + S(18), a.y + (h - g_head->FontSize) * 0.5f}, kText, Rgb(80, 26, 4, 235), title);
    ImGui::Dummy({w, h + S(12)});
    if (desc) Wrapped(g_body, kMuted, desc);
    ImGui::Dummy({0, S(10)});
}

// a small heading in the band's teal
void Label(const char* s) {
    Text(g_small, kTealHi, s);
}

// a sidebar entry: the chosen one on the title screen's purple highlight, the menus' cursor at it
bool NavItem(const char* label, bool selected) {
    ImVec2 size{ImGui::GetContentRegionAvail().x, S(38)};
    ImVec2 a = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(label, size);
    float hv = Ease(ImGui::GetItemID(), ImGui::IsItemHovered(), 14);
    float on = Ease(ImGui::GetItemID() + 1, selected, 14);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float y0 = a.y + S(5), y1 = a.y + size.y - S(5);
    float lit = std::max(0.85f * on, 0.35f * hv);
    if (lit > 0.01f) {
        ImU32 p = Fade(kPurple, lit);
        dl->AddRectFilledMultiColor({a.x + S(20), y0}, {a.x + size.x, y1}, p, Fade(p, 0), Fade(p, 0), p);
    }
    if (on > 0.01f) Cursor(dl, {a.x + S(2) - S(8) * (1 - on), (y0 + y1) * 0.5f}, S(20), on);
    ImVec2 p{a.x + S(30), a.y + (size.y - g_italic->FontSize) * 0.5f};
    if (selected) Outlined(dl, g_italic, p, kText, Rgb(40, 0, 40, 200), label, 1);
    else dl->AddText(g_italic, g_italic->FontSize, p, Mix(kMuted, kText, hv), label);
    return pressed;
}

// a status line: coloured icon and text
void Status(uint32_t icon, ImU32 col, const std::string& text) {
    ImGui::PushFont(g_semi);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted(Icon(icon).c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine(0, S(10));
    ImGui::TextUnformatted(text.c_str());
    g_marks.Mark("status:" + text);
    ImGui::PopFont();
}

// a keyboard key drawn as a keycap
void Keycap(const char* key) {
    ImGui::PushFont(g_semi);
    ImVec2 ts = ImGui::CalcTextSize(key);
    ImVec2 a = ImGui::GetCursorScreenPos();
    ImVec2 size{ts.x + S(18), ts.y + S(6)};
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(a + ImVec2(0, S(2)), a + size + ImVec2(0, S(2)), Rgb(0, 2, 12));
    dl->AddRectFilledMultiColor(a, a + size, kCardHi, kCardHi, Rgb(4, 14, 50), Rgb(4, 14, 50));
    dl->AddRect(a, a + size, kLine);
    dl->AddText(a + ImVec2(S(9), S(3)), kText, key);
    ImGui::Dummy(size + ImVec2(0, S(2)));
    ImGui::PopFont();
}

// the menus' backdrop under everything: navy darkening upward, steel-blue fog and its streaks at the right, a teal
// band along the top of the pages from left on
void Backdrop(ImDrawList* dl, ImVec2 size, float left) {
    dl->AddRectFilledMultiColor({0, 0}, size, Rgb(0, 1, 10), Rgb(0, 1, 10), Rgb(0, 8, 38), Rgb(0, 8, 38));
    Glow(dl, {size.x * 1.02f, size.y * 0.58f}, {size.x * 0.46f, size.y * 0.52f}, Rgb(72, 88, 112, 84));
    Glow(dl, {size.x * 0.9f, size.y * 0.22f}, {size.x * 0.3f, size.y * 0.2f}, Rgb(90, 104, 126, 40));
    for (int i = 0; i < 30; i++) {
        float y = size.y * (0.1f + 0.85f * Hash01(i * 3 + 1));
        float x0 = size.x * (0.42f + 0.4f * Hash01(i * 3 + 2));
        float th = S(1 + 3 * Hash01(i * 3 + 3));
        ImU32 c = Rgb(124, 140, 162, int(8 + 30 * Hash01(i * 7 + 5)));
        dl->AddRectFilledMultiColor({x0, y}, {size.x, y + th}, Fade(c, 0), c, c, Fade(c, 0));
    }
    float bh = S(16);
    dl->AddRectFilledMultiColor({left, 0}, {size.x, bh}, Rgb(14, 54, 56), Rgb(32, 98, 96), Rgb(18, 64, 64), Rgb(8, 34, 36));
    // its ornament: a point every so often, scrolls either side of it
    ImU32 orn = Rgb(84, 164, 158, 120);
    for (float x = left + S(24); x < size.x; x += S(48)) {
        float c = bh * 0.5f, d = S(3.5f);
        dl->AddQuadFilled({x, c - d}, {x + d, c}, {x, c + d}, {x - d, c}, orn);
        dl->AddBezierCubic({x + S(6), c}, {x + S(12), c - S(6)}, {x + S(18), c + S(6)}, {x + S(24), c}, orn, S(1));
        dl->AddBezierCubic({x - S(6), c}, {x - S(12), c - S(6)}, {x - S(18), c + S(6)}, {x - S(24), c}, orn, S(1));
    }
    dl->AddRectFilled({left, bh}, {size.x, bh + S(1)}, Rgb(104, 192, 186, 150));
}

void Style() {
    ImGuiStyle& st = ImGui::GetStyle();
    ImGui::StyleColorsDark(&st);
    st.WindowRounding = 0;
    st.ChildRounding = 0;
    st.FrameRounding = 2;
    st.PopupRounding = 2;
    st.GrabRounding = 2;
    st.ScrollbarRounding = 2;
    st.WindowBorderSize = 0;
    st.ChildBorderSize = 1;
    st.FrameBorderSize = 1;
    st.PopupBorderSize = 1;
    st.WindowPadding = {0, 0};
    st.FramePadding = {12, 7};
    st.ItemSpacing = {10, 8};
    st.ScrollbarSize = 10;
    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg] = {0, 0, 0, 0};
    c[ImGuiCol_Text] = V(kText);
    c[ImGuiCol_TextDisabled] = V(kMuted);
    c[ImGuiCol_PopupBg] = V(Rgb(2, 10, 42, 250));
    c[ImGuiCol_Border] = V(kLine);
    c[ImGuiCol_FrameBg] = V(Rgb(1, 5, 24));
    c[ImGuiCol_FrameBgHovered] = V(kCardHi);
    c[ImGuiCol_FrameBgActive] = V(kCardHi);
    c[ImGuiCol_Button] = V(kCardHi);
    c[ImGuiCol_ButtonHovered] = V(Rgb(20, 44, 110));
    c[ImGuiCol_ButtonActive] = V(kCard);
    c[ImGuiCol_Header] = V(Fade(kPurple, 0.6f));
    c[ImGuiCol_HeaderHovered] = V(Fade(kPurple, 0.35f));
    c[ImGuiCol_HeaderActive] = V(Fade(kPurple, 0.8f));
    c[ImGuiCol_CheckMark] = V(kAccent);
    c[ImGuiCol_PlotHistogram] = V(kAccent);
    c[ImGuiCol_NavHighlight] = V(kLineHi);
    c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
    c[ImGuiCol_ScrollbarGrab] = V(Rgb(30, 62, 110));
    c[ImGuiCol_ScrollbarGrabHovered] = V(kLine);
    c[ImGuiCol_TableRowBg] = {0, 0, 0, 0};
    c[ImGuiCol_TableRowBgAlt] = {0, 0, 0, 0};
    st.ScaleAllSizes(g_scale);
}

void Fonts() {
    ImGuiIO& io = ImGui::GetIO();
    std::error_code ec;
#ifdef _WIN32
    auto path = [&](std::initializer_list<const char*> names) -> std::string {
        for (const char* n : names) {
            std::string p = std::string("C:\\Windows\\Fonts\\") + n;
            if (fs::exists(p, ec)) return p;
        }
        return {};
    };
    // Palatino Linotype, else Georgia: Windows has both
    std::string regular = path({"pala.ttf", "georgia.ttf", "segoeui.ttf"});
    std::string bold = path({"palab.ttf", "georgiab.ttf", "seguisb.ttf"});
    std::string italic = path({"palabi.ttf", "georgiaz.ttf", "seguisb.ttf"});
    std::string icons = path({"SegoeIcons.ttf", "segmdl2.ttf"});
#else
    // Linux: the fonts folder beside the launcher (a release's: URW's P052, a Palatino with the Cyrillic letters, and
    // Material Icons), else the system's P052, else its serif (TeX Gyre Pagella has no Cyrillic: Russian showed ?)
    auto path = [&](std::initializer_list<const char*> names) -> std::string {
        for (const char* n : names) {
            for (fs::path dir : {ExeDir() / "fonts", fs::path("/usr/share/fonts/opentype/urw-base35"),
                                 fs::path("/usr/share/fonts/urw-base35"),
                                 fs::path("/usr/share/fonts/truetype/dejavu"), fs::path("/usr/share/fonts/TTF"),
                                 fs::path("/usr/share/fonts/dejavu-serif-fonts"), fs::path("/usr/share/fonts/truetype/noto")})
                if (fs::exists(dir / n, ec)) return (dir / n).string();
        }
        return {};
    };
    std::string regular = path({"P052-Roman.otf", "DejaVuSerif.ttf", "NotoSerif-Regular.ttf"});
    std::string bold = path({"P052-Bold.otf", "DejaVuSerif-Bold.ttf", "NotoSerif-Bold.ttf"});
    std::string italic = path({"P052-BoldItalic.otf", "DejaVuSerif-BoldItalic.ttf", "NotoSerif-BoldItalic.ttf"});
    std::string icons = path({"MaterialIcons-Regular.ttf"});
#endif
    // the title's fonts write only its own words, in Latin letters: the rest every language's
    auto load = [&](const std::string& file, float size, bool with_icons, const ImWchar* ranges = kTextRanges) -> ImFont* {
        if (file.empty()) return io.Fonts->AddFontDefault();
        ImFont* f = io.Fonts->AddFontFromFileTTF(file.c_str(), size * g_scale, nullptr, ranges);
        if (with_icons && !icons.empty()) {
            ImFontConfig cfg;
            cfg.MergeMode = true;
            cfg.GlyphOffset = {0, S(2)};
            io.Fonts->AddFontFromFileTTF(icons.c_str(), size * g_scale, &cfg, kIconRanges);
        }
        return f;
    };
    g_body = load(regular, 16.5f, true);
    g_small = load(regular, 14.5f, false);
    g_semi = load(bold, 16.5f, true);
    g_italic = load(italic, 18, false);
    g_head = load(italic, 30, false);
    g_hero = load(italic, 58, false, nullptr);
    g_heroSub = load(italic, 28, false, nullptr);
    g_button = load(italic, 26, false);
}


// --list-gpus: the graphics cards the launcher offers, a line each: d3d12 (Vulkan on Linux) or none, then the name,
// as the game takes it in its gpu setting
int ListGpus() {
    for (const Gpu& g : Gpus())
        std::printf("%s %s%s\n", g.d3d12 ? "d3d12" : "none", g.name.c_str(), g.software ? " (software)" : "");
    std::fflush(stdout);
    return 0;
}

// --check-fonts: every letter of every phrase in every language, as each font that writes them has it (a font without
// a language's letters, or kTextRanges without them, shows ? for them); the title's fonts write only Latin words.
// Prints what is missing, and is 0 with nothing missing.
int CheckFonts() {
    ImGui::CreateContext();
    g_scale = 1;
    Fonts();
    ImGui::GetIO().Fonts->Build();
    std::vector<std::pair<std::string, int>> texts;  // a phrase, its language
    for (const words::Row& r : words::kRows)
        for (int l = 0; l < words::kLangs; l++) texts.push_back({r.text[l], l});
    for (int l = 0; l < words::kLangs; l++) {
        texts.push_back({words::kLanguages[l].name, l});
        words::g_lang = l;
        for (size_t n : {1, 2, 5}) texts.push_back({words::Pictures(n), l});
    }
    const std::pair<const char*, ImFont*> fonts[] = {{"body", g_body},     {"small", g_small}, {"semi", g_semi},
                                                     {"italic", g_italic}, {"head", g_head},   {"button", g_button}};
    int missing = 0;
    for (const auto& [role, f] : fonts) {
        std::vector<ImWchar> seen;  // a letter reported once per font
        for (const auto& [text, lang] : texts) {
            for (const char* s = text.c_str(); *s;) {
                unsigned c = 0;
                s += ImTextCharFromUtf8(&c, s, nullptr);
                if (c < 0x20 || f->FindGlyphNoFallback(ImWchar(c)) ||
                    std::find(seen.begin(), seen.end(), ImWchar(c)) != seen.end())
                    continue;
                seen.push_back(ImWchar(c));
                char ch[5] = {};
                ImTextCharToUtf8(ch, c);
                std::printf("missing U+%04X %s in the %s font (%s), as in %s \"%s\"\n", c, ch, role, f->GetDebugName(),
                            words::kLanguages[lang].code, text.c_str());
                missing++;
            }
        }
    }
    std::printf("%d letters missing\n", missing);
    std::fflush(stdout);
    ImGui::DestroyContext();
    return missing ? 1 : 0;
}

// a picture the launcher shows: a texture of its own
struct Picture {
#ifdef _WIN32
    ID3D11ShaderResourceView* view = nullptr;
#else
    void* view = nullptr;  // Linux: an OpenGL texture's name, as ImGui takes it
#endif
    float w = 0, h = 0;
    void Release() {
#ifdef _WIN32
        if (view) view->Release();
#else
        if (view) {
            GLuint tex = GLuint(reinterpret_cast<uintptr_t>(view));
            glDeleteTextures(1, &tex);
        }
#endif
        *this = {};
    }
};

// px: BGRA with straight alpha, as ImGui blends, the top row first
Picture Upload(const std::vector<uint8_t>& px, uint32_t w, uint32_t h) {
    Picture p;
#ifndef _WIN32
    std::vector<uint8_t> rgba = px;
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) std::swap(rgba[i], rgba[i + 2]);
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(w), GLsizei(h), 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    p.view = reinterpret_cast<void*>(uintptr_t(tex));
    p.w = float(w);
    p.h = float(h);
    return p;
#else
    D3D11_TEXTURE2D_DESC d{};
    d.Width = w;
    d.Height = h;
    d.MipLevels = 1;
    d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_IMMUTABLE;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{px.data(), w * 4, 0};
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(g_dev->CreateTexture2D(&d, &init, &tex))) return p;
    if (SUCCEEDED(g_dev->CreateShaderResourceView(tex, nullptr, &p.view))) {
        p.w = float(w);
        p.h = float(h);
    }
    tex->Release();
    return p;
#endif
}

#ifndef _WIN32
// Linux: premultiplied BGRA w x h to tw x th, each pixel the mean of the source area it covers (WIC's Fant scaler)
std::vector<uint8_t> Shrink(const uint8_t* px, uint32_t pitch, uint32_t w, uint32_t h, uint32_t tw, uint32_t th) {
    std::vector<uint8_t> out(size_t(tw) * th * 4);
    for (uint32_t y = 0; y < th; y++) {
        uint32_t y0 = uint32_t(uint64_t(y) * h / th), y1 = std::max(y0 + 1, uint32_t(uint64_t(y + 1) * h / th));
        for (uint32_t x = 0; x < tw; x++) {
            uint32_t x0 = uint32_t(uint64_t(x) * w / tw), x1 = std::max(x0 + 1, uint32_t(uint64_t(x + 1) * w / tw));
            uint64_t sum[4] = {};
            for (uint32_t sy = y0; sy < y1; sy++)
                for (uint32_t sx = x0; sx < x1; sx++)
                    for (int k = 0; k < 4; k++) sum[k] += px[size_t(sy) * pitch + sx * 4 + k];
            uint64_t n = uint64_t(y1 - y0) * (x1 - x0);
            for (int k = 0; k < 4; k++) out[(size_t(y) * tw + x) * 4 + k] = uint8_t((sum[k] + n / 2) / n);
        }
    }
    return out;
}
#endif

// the HD texture pack's title logo: cut to its letters (its alpha the PS2's, 128 opaque) and shrunk to at most max_w
// wide, premultiplied while it is scaled so that its edges keep their colour
Picture LoadLogo(const fs::path& file, uint32_t max_w) {
#ifndef _WIN32
    std::vector<uint8_t> px;
    uint32_t w = 0, h = 0;
    if (!linuximage::ReadFile(file, px, w, h)) return {};
#else
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> wic;
    ComPtr<IWICBitmapDecoder> dec;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> conv;
    UINT w = 0, h = 0;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) ||
        FAILED(wic->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec)) ||
        FAILED(dec->GetFrame(0, &frame)) || FAILED(wic->CreateFormatConverter(&conv)) ||
        FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                WICBitmapPaletteTypeCustom)) ||
        FAILED(conv->GetSize(&w, &h)))
        return {};
    std::vector<uint8_t> px(size_t(w) * h * 4);
    if (FAILED(conv->CopyPixels(nullptr, w * 4, UINT(px.size()), px.data()))) return {};
#endif
    UINT x0 = w, y0 = h, x1 = 0, y1 = 0;
    for (UINT y = 0; y < h; y++)
        for (UINT x = 0; x < w; x++) {
            uint8_t* p = &px[(size_t(y) * w + x) * 4];
            unsigned a = std::min(255u, p[3] * 2u);
            for (int k = 0; k < 3; k++) p[k] = uint8_t(p[k] * a / 255);
            p[3] = uint8_t(a);
            if (a > 24) {
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x);
                y1 = std::max(y1, y);
            }
        }
    if (x1 <= x0 || y1 <= y0) return {};
    UINT cw = x1 - x0 + 1, ch = y1 - y0 + 1, tw = std::min(cw, max_w), th = UINT(uint64_t(ch) * tw / cw);
    size_t at = (size_t(y0) * w + x0) * 4;
#ifndef _WIN32
    std::vector<uint8_t> out = Shrink(&px[at], w * 4, cw, ch, tw, th);
#else
    ComPtr<IWICBitmap> cut;
    ComPtr<IWICBitmapScaler> sc;
    std::vector<uint8_t> out(size_t(tw) * th * 4);
    if (FAILED(wic->CreateBitmapFromMemory(cw, ch, GUID_WICPixelFormat32bppPBGRA, w * 4, UINT(px.size() - at), &px[at], &cut)) ||
        FAILED(wic->CreateBitmapScaler(&sc)) || FAILED(sc->Initialize(cut.Get(), tw, th, WICBitmapInterpolationModeFant)) ||
        FAILED(sc->CopyPixels(nullptr, tw * 4, UINT(out.size()), out.data())))
        return {};
#endif
    for (size_t i = 0; i < out.size(); i += 4)
        if (out[i + 3])
            for (int k = 0; k < 3; k++) out[i + k] = uint8_t(std::min(255u, out[i + k] * 255u / out[i + 3]));
    return Upload(out, tw, th);
}

// the crest the build put in the launcher's icon (resource 1), at its largest; none in a release
Picture LoadCrest() {
#ifndef _WIN32
    return {};  // Linux: an executable has no icon resource
#else
    HICON icon = HICON(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, 256, 256, 0));
    if (!icon) return {};
    Picture p;
    ICONINFO ii{};
    if (GetIconInfo(icon, &ii)) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = 256;
        bi.bmiHeader.biHeight = -256;  // the top row first
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        std::vector<uint8_t> px(256 * 256 * 4);
        HDC dc = GetDC(nullptr);
        if (ii.hbmColor && GetDIBits(dc, ii.hbmColor, 0, 256, px.data(), &bi, DIB_RGB_COLORS) == 256) p = Upload(px, 256, 256);
        ReleaseDC(nullptr, dc);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
    }
    DestroyIcon(icon);
    return p;
#endif
}

// the launcher's state and its one screen
class Launcher {
public:
    Launcher() {
        settings_.Load();
        saved_ = Snapshot();
        files_.Scan();
        g_activated = false;  // the window's first time in front, just looked at
        gpus_ = Gpus();
        auto_gpu_ = Automatic();
        modes_ = Resolutions();
        LoadArt();
    }

    // the page shown, by the sidebar's order: --page on the command line, for screenshots of a page
    void ShowPage(int page) { page_ = page; }

    // false: the launcher closes
    bool Frame() {
#ifndef _WIN32
        Picked();  // a disc image the file dialog chose since the frame before
#endif
        ShowLanguage();
        if (g_activated) {
            g_activated = false;
            rescan_ = true;
        }
        if (rescan_ && !import_.running) {
            rescan_ = false;
            files_.Scan();
            LoadArt();
            if (!import_.result.empty()) Note(import_.result);
        }
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        Backdrop(bg, io.DisplaySize, S(232));
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        TestClick();
        ImGui::Begin("launcher", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground);
        bool keep = true;
        Sidebar();
        ImGui::SameLine(0, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(36), S(28)});
        ImGui::BeginChild("page", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PushFont(g_body);
        switch (page_) {
            case 0: keep = HomePage(); break;
            case 1: DisplayPage(); break;
            case 2: GraphicsPage(); break;
            case 3: GamePage(); break;
            case 4: ControlsPage(); break;
            default: FilesPage(); break;
        }
        ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::End();
        AutoSave();
        g_marks.Write(page_);
        return keep;
    }

private:
    // every setting the launcher shows, to see when one changed
    std::string Snapshot() const {
        const Settings& s = settings_;
        return s.display_mode + "|" + std::to_string(s.width) + "x" + std::to_string(s.height) + "|" +
               std::to_string(s.render_percent) + "|" + std::to_string(s.aniso) + "|" + std::to_string(s.vsync) +
               std::to_string(s.widescreen) + std::to_string(s.fps_counter) + std::to_string(s.pause_on_focus_loss) +
               std::to_string(s.keep_open) + "|" + std::to_string(s.menu_key) + "|" + std::to_string(s.texture_pack) +
               std::to_string(s.texture_pack_size) + "|" + std::to_string(s.button_layout) +
               std::to_string(s.lament_save) + "|" + s.gpu + "|" + s.language + "|" + std::to_string(s.game_language) +
               "|" + std::to_string(s.ps2_stick) + std::to_string(s.invert_camera_x) + std::to_string(s.invert_camera_y) +
               std::to_string(s.hud_edges) + "|" + std::to_string(s.fov) + "|" + std::to_string(s.draw_distance) + "|" +
               std::to_string(s.fog_distance) + "|" + s.release;
    }

    void AutoSave() {
        std::string now = Snapshot();
        if (now == saved_) return;
        saved_ = now;
        ShowLanguage();  // a language just chosen: the note saying so in it
        Note(settings_.Save() ? T("Settings saved") : T("Could not write the settings file"));
    }

    // the words and the window's title in the language set
    void ShowLanguage() {
        if (settings_.language == shown_language_) return;
        shown_language_ = settings_.language;
        words::Set(shown_language_);
#ifdef _WIN32
        SetWindowTextW(g_hwnd, Wide(T("Castlevania: Curse of Darkness - Launcher")).c_str());
#else
        SDL_SetWindowTitle(g_window, T("Castlevania: Curse of Darkness - Launcher"));
#endif
    }

    void Note(const std::string& s) {
        status_ = s;
        status_time_ = ImGui::GetTime();
    }

    void Sidebar() {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 a = ImGui::GetCursorScreenPos();
        float w = S(232), h = ImGui::GetIO().DisplaySize.y;
        dl->AddRectFilledMultiColor(a, a + ImVec2(w, h), Rgb(1, 6, 28, 244), Rgb(1, 6, 28, 244), Rgb(0, 2, 14, 248),
                                    Rgb(0, 2, 14, 248));
        // its edge framed as the game frames a box
        dl->AddRectFilled(a + ImVec2(w - S(1), 0), a + ImVec2(w, h), kLine);
        dl->AddRectFilled(a + ImVec2(w - S(4), 0), a + ImVec2(w - S(3), h), kLineLo);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(14), S(20)});
        ImGui::BeginChild("side", {w, h}, ImGuiChildFlags_AlwaysUseWindowPadding);
        // the crest of the icon and the name in the logo's gold
        dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos() + ImVec2(S(2), 0);
        float cs = S(44), tx = p.x;
        if (crest_.view) {
            dl->AddImage(crest_.view, p, p + ImVec2(cs, cs));
            tx += cs + S(9);
        }
        Gold(dl, g_heroSub, {tx, p.y - S(3)}, "Castlevania");
        dl->AddText(g_small, g_small->FontSize, {tx + S(2), p.y + S(27)}, Rgb(226, 78, 70), "Curse of Darkness");
        ImGui::Dummy({0, cs + S(22)});

        static const words::Key kPages[] = {"Home", "Display", "Graphics", "Game", "Controls", "Game files"};
        for (int i = 0; i < 6; i++) {
            if (i == 1) {
                ImGui::Dummy({0, S(4)});
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + S(30));
                Label(T("SETTINGS"));
            }
            if (i == 5) ImGui::Dummy({0, S(6)});
            if (NavItem(T(kPages[i]), page_ == i)) page_ = i;
            g_marks.Mark("page." + std::to_string(i));
        }

        // at the bottom: the language, the save note (two lines at most, a language's can be long) and a reset
        float x = ImGui::GetCursorPosX(), line = g_small->FontSize;
        float reset = ImGui::GetWindowHeight() - S(20) - S(36);
        float language = reset - S(4) - 2 * line - S(14) - ImGui::GetFrameHeight();
        if (ImGui::GetCursorPosY() < language) ImGui::SetCursorPosY(language);
        LanguageChoice();
        float age = float(ImGui::GetTime() - status_time_);
        if (!status_.empty() && age < 4) {
            float alpha = std::clamp(4 - age, 0.0f, 1.0f), wrap = ImGui::GetContentRegionAvail().x - S(16);
            float h = g_small->CalcTextSizeA(line, FLT_MAX, wrap, status_.c_str()).y;
            ImGui::SetCursorPos({x + S(16), reset - S(4) - h});
            ImGui::PushTextWrapPos(x + S(16) + wrap);
            Text(g_small, Fade(kMuted, alpha), status_.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::SetCursorPos({x, reset});
        if (Button(T("Reset to defaults"), {-1, S(36)}, Kind::Ghost)) {
            settings_.Defaults();
            Note(T("Defaults restored"));
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    // the launcher's language, each by its name in itself (ShowLanguage puts it on)
    void LanguageChoice() {
        int cur = words::Current();
        std::string preview = Icon(kIconGlobe) + "  " + words::kLanguages[cur].name;
        ImGui::PushFont(g_body);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool open = ImGui::BeginCombo("##language", preview.c_str());
        g_marks.Mark("language");
        if (open) {
            for (int i : words::kMenu) {
                if (ImGui::Selectable(words::kLanguages[i].name, i == cur)) settings_.language = words::kLanguages[i].code;
                g_marks.Mark(std::string("language.") + words::kLanguages[i].code);
                if (i == cur) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopFont();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Language"));
    }

    // which release Play starts, when the game files have more than one: a list beside Play, its middle at left_mid
    void ReleaseChoice(ImVec2 left_mid) {
        auto name = [](int which) { return which == 0 ? T("USA release") : T("European release"); };
        int cur = files_.Played(settings_.release);
        ImGui::PushFont(g_body);
        ImGui::SetCursorScreenPos({left_mid.x, left_mid.y - ImGui::GetFrameHeight() * 0.5f});
        ImGui::SetNextItemWidth(S(200));
        bool open = ImGui::BeginCombo("##release", name(cur));
        g_marks.Mark("release");
        if (open) {
            for (int which : files_.Releases()) {
                if (ImGui::Selectable(name(which), which == cur)) settings_.release = kReleases[which].exe;
                g_marks.Mark(std::string("release.") + kReleases[which].exe);
                if (which == cur) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopFont();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("The release Play starts, of those in your game files"));
    }

    bool HomePage() {
        // the title screen: night and fog, the crest faint behind, the logo
        float w = ImGui::GetContentRegionAvail().x, h = S(322);
        ImVec2 a = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        Box(dl, a, a + ImVec2(w, h), Rgb(1, 4, 22, 238), Rgb(0, 1, 8, 242));
        dl->PushClipRect(a + ImVec2(S(4), S(4)), a + ImVec2(w - S(4), h - S(4)), true);
        Glow(dl, a + ImVec2(w * 0.82f, h * 0.55f), {w * 0.5f, h * 0.95f}, Rgb(72, 88, 116, 84));
        Glow(dl, a + ImVec2(w * 0.32f, h * 0.36f), {w * 0.42f, h * 0.5f}, Rgb(130, 76, 22, 34));
        if (crest_.view) {
            float cs = h * 1.2f, cx = a.x + w - cs * 0.34f, cy = a.y + h * 0.5f;
            dl->AddImage(crest_.view, {cx - cs * 0.5f, cy - cs * 0.5f}, {cx + cs * 0.5f, cy + cs * 0.5f}, {0, 0}, {1, 1},
                         Rgb(206, 220, 240, 44));
        }
        dl->PopClipRect();
        ImVec2 at = a + ImVec2(S(30), S(24));
        float below;
        if (logo_.view) {
            float lw = std::min(w * 0.6f, S(470)), lh = lw * logo_.h / logo_.w;
            dl->AddImage(logo_.view, at, at + ImVec2(lw, lh));
            below = at.y + lh;
        } else {  // its words, without the pack
            Gold(dl, g_hero, at, "Castlevania");
            float tw = g_hero->CalcTextSizeA(g_hero->FontSize, FLT_MAX, 0, "Castlevania").x;
            float sw = g_heroSub->CalcTextSizeA(g_heroSub->FontSize, FLT_MAX, 0, "Curse of Darkness").x;
            ImVec2 sp{at.x + std::max(0.0f, tw - sw), at.y + g_hero->FontSize * 0.95f};
            Outlined(dl, g_heroSub, sp, kBlood, Rgb(24, 0, 0, 235), "Curse of Darkness");
            below = sp.y + g_heroSub->FontSize;
        }
        ImGui::SetCursorScreenPos({at.x + S(2), below + S(10)});
        Label(T("ORIGINAL XBOX  |  NATIVE PC PORT"));

        ImGui::SetCursorScreenPos(a + ImVec2(S(30), h - S(26) - S(54)));
        bool keep = true, ready = files_.Playable();
        ImGui::BeginDisabled(!ready || import_.running);
        if (Button(T("Play"), {S(230), S(54)}, Kind::Primary, g_button)) keep = Play();
        g_marks.Mark("play");
        ImGui::EndDisabled();
        float buttons_mid = ImGui::GetItemRectMax().y - S(54) * 0.5f;  // for what is beside them
        if (!ready) {
            ImGui::SameLine(0, S(22));
            if (Button(T("Set up game files"), {0, S(54)}, Kind::Secondary)) page_ = 5;
        } else if (files_.Releases().size() > 1) {
            ReleaseChoice({ImGui::GetItemRectMax().x + S(22), buttons_mid});
        }
        // beside the buttons, mid way down them (a line of its own: SameLine would put it at their top)
        ImVec2 end = ImGui::GetItemRectMax();
        ImGui::SetCursorScreenPos({end.x + S(20), buttons_mid - g_semi->FontSize * 0.5f});
        if (!UsedGpu().d3d12) Status(kIconWarn, kRed, T(CV_WIN_LINUX("No Direct3D 12", "No Vulkan")));
        else if (ready) Status(kIconCheck, kGreen, T("Ready to play"));
        else if (!files_.exe) Status(kIconWarn, kRed, T(CV_WIN_LINUX("Castlevania.exe is missing", "Castlevania is missing")));
        else Status(kIconWarn, kAmber, T("Game files needed"));
        ImGui::SetCursorScreenPos(a + ImVec2(0, h + S(20)));
        if (!UsedGpu().d3d12) {
            BeginCard("gpu");
            GpuStatus();
            EndCard();
        }

        // the HD texture pack: whether there is one, and its switch, a click before Play (its size is on Graphics); one
        // row, the button's height
        BeginCard("pack", S(12));
        float row = S(38), y = ImGui::GetCursorPosY(), mid = y + (row - g_semi->FontSize) * 0.5f;
        ImGui::SetCursorPosY(mid);
        Text(g_semi, kText, T("HD texture pack"));
        ImGui::SameLine(0, S(26));
        ImGui::SetCursorPosY(mid);
        std::string pictures = words::Pictures(files_.pictures);
        if (!files_.pictures) Status(kIconWarn, kMuted, T("None in the textures folder (optional)"));
        else if (settings_.texture_pack) Status(kIconCheck, kGreen, F("{} found, in use", {pictures}));
        else Status(kIconCheck, kMuted, F("{} found, off: the game's own textures", {pictures}));
        ImGui::SameLine();
        if (files_.pictures) {
            ImGui::SetCursorPos({ImGui::GetContentRegionMax().x - S(46), y + (row - S(22)) * 0.5f});
            Toggle("homepack", settings_.texture_pack);
        } else {
            const char* open = T("Open textures folder");
            float bw = g_italic->CalcTextSizeA(g_italic->FontSize, FLT_MAX, 0, open).x + S(40) + row * 0.6f;
            ImGui::SetCursorPos({ImGui::GetContentRegionMax().x - bw, y});
            if (Button(open, {bw, row}, Kind::Secondary)) OpenTextures();
        }
        ImGui::SetCursorPosY(y + row);
        ImGui::Dummy({0, 0});
        EndCard();

        // what is set, at a glance; each opens its page
        Label(T("CURRENT SETUP"));
        ImGui::Dummy({0, S(2)});
        Res r{settings_.width, settings_.height};
        std::string res = r.w ? std::to_string(r.w) + " x " + std::to_string(r.h) : T("Desktop");
        std::string mode = settings_.display_mode == "fullscreen" ? T("Fullscreen")
                           : settings_.display_mode == "windowed" ? T("Windowed")
                                                                  : T("Borderless");
        struct Tile {
            const char* label;
            std::string value;
            int page;
        } tiles[] = {{T("Display"), mode + "  |  " + res, 1},
                     {T("Widescreen"), settings_.widescreen ? T("On (16:9, Hor+)") : T("Off (4:3)"), 2},
                     {T("Render scale"), std::to_string(settings_.render_percent) + "%", 2},
                     {T("Menu key"), KeyName(settings_.menu_key) + "  |  L3 + R3", 3}};
        float gap = S(12), tw = (ImGui::GetContentRegionAvail().x - gap) / 2, th = S(68);
        for (int i = 0; i < 4; i++) {
            if (i % 2) ImGui::SameLine(0, gap);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("tile", {tw, th})) page_ = tiles[i].page;
            float hv = Ease(ImGui::GetItemID(), ImGui::IsItemHovered(), 14);
            ImGui::PopID();
            Box(dl, p, p + ImVec2(tw, th), Mix(kCard, kCardHi, hv), kCardLo, Mix(kLine, kLineHi, hv));
            dl->AddText(g_small, g_small->FontSize, p + ImVec2(S(18), S(13)), kMuted, tiles[i].label);
            dl->AddText(g_semi, g_semi->FontSize, p + ImVec2(S(18), S(34)), kText, tiles[i].value.c_str());
            Cursor(dl, {p.x + tw - S(38) + S(6) * hv, p.y + th * 0.5f}, S(18), 0.3f + 0.7f * hv);
        }
        ImGui::Dummy({0, S(8)});
        std::string tip = F("In game, press {} or click both sticks (L3 + R3) for these settings.", {KeyName(settings_.menu_key)});
        Wrapped(g_small, kMuted, tip.c_str());
        return keep;
    }

    // the card the game will draw with: the one chosen, when it is here with Direct3D 12, else the automatic one
    const Gpu& UsedGpu() const {
        if (!settings_.gpu.empty())
            for (const Gpu& g : gpus_)
                if (g.name == settings_.gpu && g.d3d12) return g;
        return auto_gpu_;
    }

    // the graphics the game will use, and what to do when they cannot run it
    void GpuStatus() {
        const Gpu& gpu = UsedGpu();
        if (!settings_.gpu.empty() && &gpu == &auto_gpu_)
            Status(kIconWarn, kAmber, F(CV_WIN_LINUX("{}  -  not here with Direct3D 12, the game takes the fastest",
                                                     "{}  -  not here with Vulkan, the game takes the fastest"),
                                        {settings_.gpu}));
        std::string name = gpu.name.empty() ? T("This PC's graphics") : gpu.name;
        if (gpu.d3d12 && gpu.software) {
            Status(kIconWarn, kAmber, F(CV_WIN_LINUX("{}  -  Direct3D 12 on the processor only",
                                                     "{}  -  Vulkan on the processor only"), {name}));
            Wrapped(g_small, kMuted,
                    T(CV_WIN_LINUX("No graphics driver is installed, so Windows draws on the processor, far too slowly "
                                   "for the game. Install the driver from NVIDIA, AMD or Intel.",
                                   "No Vulkan driver for the graphics is installed, so the processor draws, far too "
                                   "slowly for the game. Install your distribution's Mesa Vulkan drivers, or NVIDIA's "
                                   "driver.")));
        } else if (gpu.d3d12) {
            Status(kIconCheck, kGreen, F(CV_WIN_LINUX("{}  -  Direct3D 12, ready", "{}  -  Vulkan, ready"), {name}));
        } else {
            Status(kIconWarn, kRed, F(CV_WIN_LINUX("{}  -  no Direct3D 12", "{}  -  no Vulkan"), {name}));
            Wrapped(g_small, kMuted,
                    T(CV_WIN_LINUX("The game draws with Direct3D 12, which every Windows 10 and 11 has, but only "
                                   "graphics with a driver for it can run. Install the newest driver from NVIDIA, AMD "
                                   "or Intel. Graphics older than NVIDIA's GeForce 600 series, AMD's Radeon HD 7700 or "
                                   "Intel's HD 4200 (2012 and 2013) have none, and the game cannot start on them.",
                                   "The game draws with Direct3D 12 on Vulkan (vkd3d), so the graphics need a Vulkan "
                                   "driver: your distribution's Mesa drivers for AMD and Intel, or NVIDIA's own. "
                                   "Graphics older than NVIDIA's GeForce 600 series, AMD's Radeon HD 7700 or Intel's "
                                   "HD 4200 (2012 and 2013) have none, and the game cannot start on them.")));
        }
    }

    void DisplayPage() {
        PageTitle(T("Display"), T("How the game window fills your screen."));
        BeginCard("display");
        BeginSetting(T("Display mode"), T("Borderless fills the monitor without changing its mode and switches away instantly."), true);
        Segmented<std::string>("mode", settings_.display_mode, {"fullscreen", "borderless", "windowed"},
                               {T("Fullscreen"), T("Borderless"), T("Windowed")});
        EndSetting();
        BeginSetting(T("Resolution"), T("The output size. Fullscreen switches the monitor to it; borderless always fills the monitor."));
        {
            size_t cur = 0;
            for (size_t i = 0; i < modes_.size(); i++)
                if (modes_[i].w == settings_.width && modes_[i].h == settings_.height) cur = i;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##res", ResName(modes_[cur]).c_str())) {
                for (size_t i = 0; i < modes_.size(); i++)
                    if (ImGui::Selectable(ResName(modes_[i]).c_str(), i == cur)) {
                        settings_.width = modes_[i].w;
                        settings_.height = modes_[i].h;
                    }
                ImGui::EndCombo();
            }
        }
        EndSetting();
        BeginSetting("V-Sync", T("Off can tear and lowers input latency a little. The game runs at 60 FPS either way."));
        ToggleRight("vsync", settings_.vsync);
        EndSetting();
        EndCard();
    }

    void GraphicsPage() {
        PageTitle(T("Graphics"), T("Picture quality and shape."));
        BeginCard("gpu");
        BeginSetting(T("Graphics card"), T("The one the game draws with, from its next start. Automatic takes the fastest: on "
                                           "a laptop the dedicated GPU, not the one in the processor."), true);
        {
            std::vector<std::string> values{""};
            std::vector<std::string> names{T("Automatic")};  // which one, the line below says
            for (const Gpu& g : gpus_)
                if (g.d3d12 && std::find(values.begin(), values.end(), g.name) == values.end()) {
                    values.push_back(g.name);
                    names.push_back(g.name);
                }
            if (std::find(values.begin(), values.end(), settings_.gpu) == values.end()) {  // chosen, gone since
                values.push_back(settings_.gpu);
                names.push_back(F("{}  (not found)", {settings_.gpu}));
            }
            Combo<std::string>("##gpu", settings_.gpu, values, names);
        }
        EndSetting();
        GpuStatus();
        EndCard();
        BeginCard("presets");
        BeginSetting(T("Quality preset"), T("Sets render scale and texture filtering together."), true);
        int preset = settings_.render_percent == 75 && settings_.aniso == 0      ? 0
                     : settings_.render_percent == 100 && settings_.aniso == 0   ? 1
                     : settings_.render_percent == 150 && settings_.aniso == 8   ? 2
                     : settings_.render_percent == 200 && settings_.aniso == 16  ? 3
                                                                                 : -1;
        if (Segmented<int>("preset", preset, {0, 1, 2, 3}, {T("Low"), T("Original"), T("High"), T("Ultra")})) {
            static const uint32_t kScale[] = {75, 100, 150, 200}, kAniso[] = {0, 0, 8, 16};
            settings_.render_percent = kScale[preset];
            settings_.aniso = kAniso[preset];
        }
        EndSetting();
        EndCard();
        BeginCard("graphics");
        BeginSetting(T("Widescreen"), T("16:9 with more of the world at the sides, nothing stretched. The HUD, menus and "
                                        "movies stay 4:3 in the middle. Needs a 16:9 or wider resolution."), true);
        ToggleRight("wide", settings_.widescreen);
        EndSetting();
        BeginSetting(T("Render scale"), T("Of the output height. Above 100% supersamples for a sharper picture; below is "
                                          "faster on weak GPUs."));
        Segmented<uint32_t>("scale", settings_.render_percent, {50, 75, 100, 150, 200}, {"50%", "75%", "100%", "150%", "200%"});
        EndSetting();
        BeginSetting(T("Anisotropic filtering"), T("Sharper textures on floors and walls seen at an angle."));
        Segmented<uint32_t>("aniso", settings_.aniso, {0, 2, 4, 8, 16}, {T("Off"), "2x", "4x", "8x", "16x"});
        EndSetting();
        BeginSetting(T("HUD at the screen edges"), T("Widescreen: the health bar and the rest of the HUD in the screen's "
                                                    "corners rather than at the corners of the 4:3 middle."));
        ToggleRight("hudedges", settings_.hud_edges);
        EndSetting();
        EndCard();
        BeginCard("view");
        BeginSetting(T("Field of view"), T("How much of the world the camera takes in: 100% is the game's own, which each "
                                           "room's camera sets; more is wider. Story scenes keep the game's."), true);
        {
            std::string name = std::to_string(settings_.fov) + "%";
            if (settings_.fov == 100) name += "  -  " + std::string(T("the game's"));
            Slider("fov", settings_.fov, 100, 200, name);
        }
        EndSetting();
        BeginSetting(T("Far view"), T("The fog and the draw distance both 4x as far out. Off puts both back to the "
                                      "game's."));
        {
            bool was = settings_.draw_distance == 400 && settings_.fog_distance == 400, on = was;
            ToggleRight("farview", on);
            if (on != was) settings_.draw_distance = settings_.fog_distance = on ? 400 : 100;
        }
        EndSetting();
        BeginSetting(T("Draw distance"), T("How far the world is drawn. Farther shows more once there is less fog, and may "
                                           "show where a room's scenery ends."));
        Segmented<uint32_t>("far", settings_.draw_distance, {100, 150, 200, 400}, {T("Original"), "1.5x", "2x", "4x"});
        EndSetting();
        BeginSetting(T("Fog"), T("The fog and darkness that hide the distance in most rooms: as far out as the game puts "
                                 "it, farther, or none."));
        Segmented<uint32_t>("fog", settings_.fog_distance, {100, 150, 200, 400, 0},
                            {T("Original"), "1.5x", "2x", "4x", T("None")});
        EndSetting();
        EndCard();
        BeginCard("texpack");
        BeginSetting(T("HD texture pack"), T("Texture packs made for PCSX2 replace the game's textures: put a pack's "
                                             "replacement pictures in the textures folder."), true);
        ToggleRight("texpack", settings_.texture_pack);
        EndSetting();
        BeginSetting(T("Texture size"), T("The largest side a picture is loaded at. Larger is sharper and needs more video "
                                          "memory and loading time."));
        Segmented<uint32_t>("texsize", settings_.texture_pack_size, {1024, 2048, 0}, {"1024", "2048", T("Full")});
        EndSetting();
        ImGui::Dummy({0, S(2)});
        if (files_.pictures) Status(kIconCheck, kGreen, F("{} in the textures folder", {words::Pictures(files_.pictures)}));
        else Status(kIconWarn, kMuted, T("No pack in the textures folder"));
        ImGui::Dummy({0, S(4)});
        if (Button(T("Open textures folder"), {0, 0}, Kind::Secondary)) OpenTextures();
        ImGui::SameLine();
        if (Button(T("Check again"), {0, 0}, Kind::Secondary)) rescan_ = true;
        EndCard();
    }

    // made where the game looks for it when there is none
    void OpenTextures() {
        fs::path t = files_.textures.empty() ? ExeDir() / "textures" : files_.textures;
        std::error_code ec;
        fs::create_directories(t, ec);
        Open(t);
        rescan_ = true;
    }

    void GamePage() {
        PageTitle(T("Game"), T("In-game overlay and behaviour."));
        // one card: three would not fit the window
        BeginCard("game");
        BeginSetting(T("Game language"), T("The European release's text, from its next start. The USA release has English only."), true);
        {
            std::vector<uint32_t> values;
            std::vector<std::string> names;
            for (const GameLanguage& g : kGameLanguages) {
                values.push_back(g.console);
                names.push_back(g.name);
            }
            Combo<uint32_t>("##gamelanguage", settings_.game_language, values, names);
        }
        EndSetting();
        BeginSetting(T("FPS counter"), T("Shows the frame rate in the corner."));
        ToggleRight("fps", settings_.fps_counter);
        EndSetting();
        BeginSetting(T("Pause on focus loss"), T("Stops the game while another window is in front."));
        ToggleRight("pause", settings_.pause_on_focus_loss);
        EndSetting();
        BeginSetting(T("Lament of Innocence save"), T("As the PS2 game with a Lament of Innocence save on the memory "
                                                      "card: a Moai at the far end of the room behind the first "
                                                      "courtyard, beside the potion, for the item list."));
        ToggleRight("lament", settings_.lament_save);
        EndSetting();
        BeginSetting(T("Settings menu key"), T("Opens the in-game settings. Pick another if F10 does nothing on your keyboard "
                                               "(laptops often need Fn for it). L3 + R3 on a controller always works."));
        static const std::vector<uint32_t> keys = {VK_F10, VK_F1, VK_F2, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9,
                                                   VK_F11, VK_F12, VK_INSERT, VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
                                                   VK_PAUSE, VK_SCROLL, VK_OEM_3};
        // a key Windows does not name gets one of the language's
        std::vector<std::string> names;
        for (uint32_t k : keys) names.push_back(KeyName(k));
        Combo<uint32_t>("##key", settings_.menu_key, keys, names);
        EndSetting();
        BeginSetting(T("Keep launcher open"), T("Leave this window open while the game runs."));
        ToggleRight("keep", settings_.keep_open);
        EndSetting();
        EndCard();
    }

    void ControlsPage() {
        PageTitle(T("Controls"), T("Xbox, PlayStation and Switch controllers play as player 1, with rumble. The keyboard "
                                   "plays alongside."));
        BeginCard("layout");
        BeginSetting(T("Button layout"), T("PS2 shows the PS2's button icons and gives a controller the PS2's shoulders: L1 "
                                           "resets the camera, R1 guards, L2 and R2 lock on. The keyboard keeps its keys."), true);
        Segmented<uint32_t>("layout", settings_.button_layout, {0, 1}, {"Xbox", "PS2"});
        EndSetting();
        if (settings_.button_layout == 1) {
            ImGui::Dummy({0, S(2)});
            // the icons are the PS2 sheet of an HD pack made from the PS2 game (texpack.cpp)
            if (!settings_.texture_pack) Status(kIconWarn, kMuted, T("The PS2 icons need the HD texture pack on (Graphics)"));
            else if (files_.ps2_buttons) Status(kIconCheck, kGreen, T("PS2 icons found in the texture pack"));
            else Status(kIconWarn, kMuted, T("No PS2 icons in the textures folder: the Xbox ones stay, the shoulders change"));
        }
        EndCard();
        BeginCard("sticks");
        BeginSetting(T("Stick response"), T("PS2: Hector walks any way the stick points, from a slow walk up, as on the PS2. "
                                            "Xbox: the Xbox release's, where a stick near up, down, left or right walks "
                                            "straight along it."), true);
        Segmented<bool>("stick", settings_.ps2_stick, {true, false}, {"PS2", "Xbox"});
        EndSetting();
        BeginSetting(T("Invert camera left / right"), T("The right stick and the arrow keys turn the camera the other way."));
        ToggleRight("invx", settings_.invert_camera_x);
        EndSetting();
        BeginSetting(T("Invert camera up / down"), T("The right stick and the arrow keys move the camera the other way up "
                                                     "and down."));
        ToggleRight("invy", settings_.invert_camera_y);
        EndSetting();
        EndCard();
        BeginCard("keys");
        // the keys by their names on a keyboard, the controller's buttons (B, Y, Start, Back) by the Xbox's
        std::string menu = KeyName(settings_.menu_key);
        const char* kKeys[][2] = {
            {"W A S D", T("Move")},       {T("Arrows"), T("Camera")}, {T("K / Space"), T("Jump (A)")}, {"J", T("Attack (X)")},
            {"L", "B"},                   {"I", "Y"},                 {"Q / E", T("Triggers")},        {"Z / C", T("White / Black")},
            {T("Enter"), "Start"},        {T("Backspace"), "Back"},   {"Num 8 2 4 6", T("D-pad")},     {menu.c_str(), T("Settings menu")},
        };
        if (ImGui::BeginTable("keys", 4, ImGuiTableFlags_SizingStretchProp)) {
            for (auto& k : kKeys) {
                ImGui::TableNextColumn();
                Keycap(k[0]);
                ImGui::TableNextColumn();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(3));
                ImGui::TextUnformatted(k[1]);
            }
            ImGui::EndTable();
        }
        EndCard();
    }

    void FilesPage() {
        PageTitle(T("Game files"), T("Your own copy of the game: the USA (NTSC-U) or European (PAL) release, as a disc image or extracted."));
        BeginCard("status");
        if (files_.assets.empty()) {
            Status(kIconWarn, kRed, T("No assets folder found"));
        } else {
            if (files_.xbe.release != Release::Missing) CopyStatus(kIconCheck, kXbe, files_.xbe);
            else if (files_.isos.empty()) Status(kIconWarn, kMuted, F("{}  -  not extracted", {kXbe}));  // none beside an image
            for (const Files::Image& iso : files_.isos) {
                std::string name = Utf8(iso.path.filename().wstring());
                if (iso.copy.release == Release::Missing) Status(kIconWarn, kRed, F("{}  -  not an Xbox disc image", {name}));
                else CopyStatus(kIconDisc, name, iso.copy);
            }
        }
        if (!files_.exe)
            Status(kIconWarn, kRed, T(CV_WIN_LINUX("Castlevania.exe is not beside the launcher",
                                                   "Castlevania is not beside the launcher")));
        if (!files_.assets.empty() && files_.xbe.release == Release::Missing && files_.isos.empty())
            Wrapped(g_small, kMuted, T("Add your disc image (.iso), or extract the disc into the assets folder: default.xbe "
                                       "with its data, movie and sound folders."));
        if (!files_.assets.empty()) Wrapped(g_small, kMuted, Utf8(files_.assets.wstring()).c_str());
        ImGui::Dummy({0, S(6)});
        if (import_.running) {
            ImGui::ProgressBar(import_.progress, {-FLT_MIN, S(36)}, T("Copying disc image..."));
        } else {
            if (Button(T("Add disc image"), {0, 0}, Kind::Primary)) AddImage();
            ImGui::SameLine();
            if (Button(T("Open folder"), {0, 0}, Kind::Secondary)) {
                fs::path a = files_.assets.empty() ? ExeDir() / "assets" : files_.assets;
                std::error_code ec;
                fs::create_directories(a, ec);
                Open(a);
                rescan_ = true;
            }
            ImGui::SameLine();
            if (Button(T("Check again"), {0, 0}, Kind::Secondary)) rescan_ = true;
        }
        EndCard();
        BeginCard("folders");
        Text(g_semi, kText, T("Saves and logs"));
        Wrapped(g_small, kMuted, T(CV_WIN_LINUX("Saves are kept beside the game in hdd\\E\\UDATA\\4b4e002d.",
                                                "Saves are kept beside the game in hdd/E/UDATA/4b4e002d.")));
        ImGui::Dummy({0, S(4)});
        if (Button(T("Open saves"), {0, 0}, Kind::Secondary)) {
            fs::path s = ExeDir() / kSaves;
            std::error_code ec;
            Open(fs::exists(s, ec) ? s : ExeDir());
        }
        ImGui::SameLine();
        if (Button(T("Open logs"), {0, 0}, Kind::Secondary)) {
            std::error_code ec;
            Open(fs::exists(ExeDir() / "logs", ec) ? ExeDir() / "logs" : ExeDir());
        }
        EndCard();
    }

    // a copy of the game: which release it is, and that it plays
    void CopyStatus(uint32_t icon, const std::string& name, const Copy& copy) {
        if (copy.release == Release::Other)
            Status(kIconWarn, kRed, F("{}  -  another release, only the USA and European ones work", {name}));
        else if (copy.europe())
            Status(icon, kGreen, copy.release == Release::Match ? F("{}  -  European release, ready", {name})
                                                                : F("{}  -  European release (patched header), ready", {name}));
        else
            Status(icon, kGreen, copy.release == Release::Match ? F("{}  -  USA release, ready", {name})
                                                                : F("{}  -  USA release (patched header), ready", {name}));
    }

    bool Play() {
        saved_ = Snapshot();
        if (!settings_.Save()) {
            Note(T("Could not write the settings file"));
            return true;
        }
        fs::path exe = files_.Player(settings_.release);
#ifndef _WIN32
        std::string path = exe.string(), dir = ExeDir().string();
        char* argv[] = {path.data(), nullptr};
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addchdir_np(&actions, dir.c_str());
        pid_t pid = 0;
        int e = posix_spawn(&pid, path.c_str(), &actions, nullptr, argv, environ);
        posix_spawn_file_actions_destroy(&actions);
        if (e) {
            Note(F("Could not start the game (error {})", {std::to_string(e)}));
            return true;
        }
        std::thread([pid] { waitpid(pid, nullptr, 0); }).detach();  // reaped when it ends
#else
        std::wstring cmd = L"\"" + exe.wstring() + L"\"";
        STARTUPINFOW si{sizeof(si)};
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, ExeDir().c_str(), &si, &pi)) {
            Note(F("Could not start the game (error {})", {std::to_string(GetLastError())}));
            return true;
        }
        // the game may bring its window to the front
        AllowSetForegroundWindow(pi.dwProcessId);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
#endif
        Note(T("Game started"));
        return settings_.keep_open;
    }

#ifndef _WIN32
    // Linux: SDL's file dialog (the desktop's portal, else zenity) answers later, from another thread perhaps;
    // the frame after takes the file (Picked)
    void AddImage() {
        static std::string iso, all;  // the dialog keeps the filters' names while it is up
        iso = T("Xbox disc image (*.iso)");
        all = T("All files");
        static SDL_DialogFileFilter filters[2];
        filters[0] = {iso.c_str(), "iso"};
        filters[1] = {all.c_str(), "*"};
        SDL_ShowOpenFileDialog(
            [](void* self, const char* const* files, int) {
                auto* l = static_cast<Launcher*>(self);
                std::lock_guard lock(l->picked_lock_);
                if (files && files[0]) l->picked_ = files[0];
            },
            this, g_window, filters, 2, nullptr, false);
    }

    void Picked() {
        fs::path from;
        {
            std::lock_guard lock(picked_lock_);
            if (picked_.empty()) return;
            from = std::exchange(picked_, {});
        }
        ImportFrom(from);
    }
#else
    void AddImage() {
        wchar_t file[MAX_PATH] = {};
        // pairs of a name and its pattern, each ended by a NUL, the list by another (c_str's)
        std::wstring filter = Wide(T("Xbox disc image (*.iso)")) + L'\0' + L"*.iso" + L'\0' + Wide(T("All files")) + L'\0' +
                              L"*.*" + L'\0';
        std::wstring title = Wide(T("Your Castlevania: Curse of Darkness disc image"));
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = g_hwnd;
        ofn.lpstrFilter = filter.c_str();
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&ofn)) return;
        ImportFrom(file);
    }
#endif

    void ImportFrom(const fs::path& from) {
        fs::path assets = files_.assets.empty() ? ExeDir() / "assets" : files_.assets;
        fs::path to = assets / from.filename();
        std::error_code ec;
        if (fs::equivalent(from.parent_path(), assets, ec)) {
            Note(T("That image is already in the assets folder"));
            rescan_ = true;
            return;
        }
        import_.Start(from, to);
        rescan_ = true;
    }

    // the player's own pictures: the pack's logo, the crest of the icon
    void LoadArt() {
        if (!crest_.view) crest_ = LoadCrest();
        if (files_.logo == logo_file_) return;
        logo_.Release();
        logo_file_ = files_.logo;
        if (!logo_file_.empty()) logo_ = LoadLogo(logo_file_, uint32_t(S(1100)));
    }

    Settings settings_;
    Files files_;
    std::vector<Gpu> gpus_;
    Gpu auto_gpu_;
    Picture logo_, crest_;
    fs::path logo_file_;
    Import import_;
#ifndef _WIN32
    std::mutex picked_lock_;
    fs::path picked_;  // the dialog's file, for the next frame (Picked)
#endif
    std::vector<Res> modes_;
    std::string status_, saved_, shown_language_;
    double status_time_ = -100;
    bool rescan_ = false;
    int page_ = 0;
};

}  // namespace

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    if (std::wcsstr(GetCommandLineW(), L"--check-fonts")) return CheckFonts();
    if (std::wcsstr(GetCommandLineW(), L"--list-gpus")) return ListGpus();
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);  // WIC, for the pack's logo
    float scale = float(GetDpiForSystem()) / 96.0f;
    {
        Settings settings;  // the language, for the window's title and an error before the launcher is up
        settings.Load();
        words::Set(settings.language);
    }

    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"CastlevaniaLauncher";
    RegisterClassExW(&wc);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r{0, 0, LONG(1060 * scale), LONG(690 * scale)};
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, GetDpiForSystem());
    int w = r.right - r.left, h = r.bottom - r.top;
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, Wide(T("Castlevania: Curse of Darkness - Launcher")).c_str(), style,
                             (GetSystemMetrics(SM_CXSCREEN) - w) / 2, (GetSystemMetrics(SM_CYSCREEN) - h) / 2, w, h,
                             nullptr, nullptr, inst, nullptr);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    // Windows 11: the title bar in the menus' navy, its words in the logo's gold, its edge light blue
    COLORREF caption = RGB(1, 6, 28), words = RGB(236, 206, 140), edge = RGB(56, 130, 190);
    DwmSetWindowAttribute(g_hwnd, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
    DwmSetWindowAttribute(g_hwnd, 36 /* DWMWA_TEXT_COLOR */, &words, sizeof(words));
    DwmSetWindowAttribute(g_hwnd, 34 /* DWMWA_BORDER_COLOR */, &edge, sizeof(edge));
    if (!CreateDevice(g_hwnd)) {
        MessageBoxW(nullptr, Wide(T("Direct3D 11 is not available.")).c_str(), L"Castlevania Launcher", MB_ICONERROR);
        return 1;
    }
    ShowWindow(g_hwnd, show);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    g_scale = scale;
    Style();
    Fonts();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);

    Launcher launcher;
    // --page home|display|graphics|game|controls|files opens on that page
    static const wchar_t* kPageNames[] = {L"home", L"display", L"graphics", L"game", L"controls", L"files"};
    for (int i = 0; i < 6; i++)
        if (std::wcsstr(GetCommandLineW(), (std::wstring(L"--page ") + kPageNames[i]).c_str())) launcher.ShowPage(i);
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;
        if (IsIconic(g_hwnd)) {
            Sleep(50);
            continue;
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (!launcher.Frame()) running = false;
        ImGui::Render();
        ImVec4 bg = V(kBg);
        const float clear[4] = {bg.x, bg.y, bg.z, 1};
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_rtv) g_rtv->Release();
    g_swap->Release();
    g_ctx->Release();
    g_dev->Release();
    DestroyWindow(g_hwnd);
    return 0;
}
#else
// Linux: SDL's window at the display's scale, drawn with OpenGL 3; ImGui's input from the SDK's SDL layer
int main(int argc, char** argv) {
    for (int a = 1; a < argc; a++)
        if (!std::strcmp(argv[a], "--check-fonts")) return CheckFonts();
        else if (!std::strcmp(argv[a], "--list-gpus")) return ListGpus();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "SDL failed to start: %s\n", SDL_GetError());
        return 1;
    }
    {
        Settings settings;  // the language, for the window's title and an error before the launcher is up
        settings.Load();
        words::Set(settings.language);
    }
    float scale = std::max(1.0f, SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay()));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    g_window = SDL_CreateWindow(T("Castlevania: Curse of Darkness - Launcher"), int(1060 * scale), int(690 * scale),
                                SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    SDL_GLContext gl = g_window ? SDL_GL_CreateContext(g_window) : nullptr;
    if (!gl) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Castlevania Launcher", T("OpenGL is not available."), nullptr);
        return 1;
    }
    SDL_GL_MakeCurrent(g_window, gl);
    SDL_GL_SetSwapInterval(1);
    SDL_ShowWindow(g_window);
    linuxapp::g_window = g_window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    // the layout's sizes in the window's pixels: the display's scale, and its pixel density on a HiDPI screen
    g_scale = scale * SDL_GetWindowPixelDensity(g_window);
    Style();
    Fonts();
    ImGui_ImplOpenGL3_Init("#version 130");
    linuxapp::ImGuiInit(g_window);
    // ImGui's controller navigation: the first controller plugged in
    static SDL_Gamepad* pad = nullptr;
    linuxapp::g_nav_pad = [] { return pad; };

    Launcher launcher;
    // --page home|display|graphics|game|controls|files opens on that page
    static const char* kPageNames[] = {"home", "display", "graphics", "game", "controls", "files"};
    for (int a = 1; a + 1 < argc; a++)
        for (int i = 0; i < 6; i++)
            if (!std::strcmp(argv[a], "--page") && !std::strcmp(argv[a + 1], kPageNames[i])) launcher.ShowPage(i);
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            linuxapp::ImGuiEvent(e);
            if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
            if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED) g_activated = true;
            if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad) pad = SDL_OpenGamepad(e.gdevice.which);
            if (e.type == SDL_EVENT_GAMEPAD_REMOVED && pad && SDL_GetGamepadID(pad) == e.gdevice.which) {
                SDL_CloseGamepad(pad);
                pad = nullptr;
            }
        }
        if (!running) break;
        if (SDL_GetWindowFlags(g_window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(50);
            continue;
        }
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(g_window, &w, &h);
        linuxapp::g_pixel_w = uint32_t(w);
        linuxapp::g_pixel_h = uint32_t(h);
        ImGui_ImplOpenGL3_NewFrame();
        linuxapp::ImGuiNewFrame();
        ImGui::NewFrame();
        if (!launcher.Frame()) running = false;
        ImGui::Render();
        ImVec4 bg = V(kBg);
        glViewport(0, 0, w, h);
        glClearColor(bg.x, bg.y, bg.z, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(g_window);
        linuxapp::ApplyCursor();
        // text typed into an ImGui field
        bool text = io.WantTextInput;
        if (text != SDL_TextInputActive(g_window)) text ? SDL_StartTextInput(g_window) : SDL_StopTextInput(g_window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
    return 0;
}
#endif
