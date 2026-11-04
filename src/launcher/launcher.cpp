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
#include <bcrypt.h>
#include <commdlg.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dwmapi.h>
#include <dxgi1_6.h>
#include <shellapi.h>
#include <urlmon.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cwchar>
#include <string>
#include <thread>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <toml++/toml.hpp>

#include "kernel/disc.h"
#include "words.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
extern const char kImageSha1[];  // the xbe the game exe was recompiled from, written by CMake
extern const unsigned char kImageCertificate[92];  // its certificate's title name, then media, region and ratings

namespace {

namespace fs = std::filesystem;
using words::F;
using words::T;

constexpr wchar_t kGameExe[] = L"Castlevania.exe";
constexpr char kSettings[] = "castlevania.toml";
constexpr char kXbe[] = "default.xbe";
constexpr wchar_t kSaves[] = L"hdd\\E\\UDATA\\4b4e002d";

// D3D11 and the window

ID3D11Device* g_dev = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;
IDXGISwapChain* g_swap = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
HWND g_hwnd = nullptr;
bool g_activated = false;  // the window came back to the front: the files are looked at again (a pack added meanwhile)
// the launcher's layout at 100%: everything it shows fits this, and the window scales it to whatever size it has
constexpr float kBaseW = 1060, kBaseH = 690;
constexpr float kMinZoom = 0.6f;  // the smallest the window shrinks to, a part of that
bool g_sizing = false;            // the window's edge is being dragged: frames come from a timer (WM_TIMER)
bool g_full = false;              // full screen (F11, Alt+Enter): the window borderless over its monitor
WINDOWPLACEMENT g_placement{sizeof(WINDOWPLACEMENT)};  // where it was before full screen
void (*g_paint)() = nullptr;      // one frame of the launcher, for the timer while sizing
bool g_toggle_full = false;       // the button asked for full screen: between frames, not in the middle of one

float WindowDpi(HWND hwnd) { return float(GetDpiForWindow(hwnd)) / 96.0f; }

// full screen: borderless over the whole monitor the window is on; again, back as it was
void ToggleFullScreen() {
    LONG style = GetWindowLongW(g_hwnd, GWL_STYLE);
    if (!g_full) {
        MONITORINFO mi{sizeof(mi)};
        if (!GetWindowPlacement(g_hwnd, &g_placement) ||
            !GetMonitorInfoW(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi))
            return;
        SetWindowLongW(g_hwnd, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        const RECT& m = mi.rcMonitor;
        SetWindowPos(g_hwnd, HWND_TOP, m.left, m.top, m.right - m.left, m.bottom - m.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_full = true;
    } else {
        SetWindowLongW(g_hwnd, GWL_STYLE, (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(g_hwnd, &g_placement);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_full = false;
    }
}

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
            if (g_dev && wp != SIZE_MINIMIZED && LOWORD(lp) && HIWORD(lp)) {
                g_ctx->OMSetRenderTargets(0, nullptr, nullptr);  // nothing may hold the buffers as they are resized
                if (g_rtv) g_rtv->Release(), g_rtv = nullptr;
                g_swap->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
                MakeTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_KEYMENU) return 0;  // Alt alone
            break;
        case WM_GETMINMAXINFO: {
            // no smaller than the layout at kMinZoom, its frame added
            float dpi = WindowDpi(hwnd);
            RECT r{0, 0, LONG(kBaseW * kMinZoom * dpi), LONG(kBaseH * kMinZoom * dpi)};
            AdjustWindowRectExForDpi(&r, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForWindow(hwnd));
            auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
            mm->ptMinTrackSize = {r.right - r.left, r.bottom - r.top};
            return 0;
        }
        case WM_DPICHANGED: {
            // to another monitor: the size Windows suggests there
            const RECT* r = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_ENTERSIZEMOVE:
            // Windows keeps the message loop while an edge is dragged: a timer draws the frames meanwhile
            g_sizing = true;
            SetTimer(hwnd, 1, 16, nullptr);
            return 0;
        case WM_EXITSIZEMOVE:
            g_sizing = false;
            KillTimer(hwnd, 1);
            return 0;
        case WM_TIMER:
            if (wp == 1 && g_paint) g_paint();
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            // F11, or Alt+Enter as games have it: full screen and back
            if ((wp == VK_F11 && msg == WM_KEYDOWN) || (wp == VK_RETURN && msg == WM_SYSKEYDOWN && (lp & (1 << 29)))) {
                if (!(lp & (1 << 30))) ToggleFullScreen();  // not again while held
                return 0;
            }
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

// files

fs::path ExeDir() {
    wchar_t p[MAX_PATH];
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    return fs::path(p).parent_path();
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
    if (BCryptHash(BCRYPT_SHA1_ALG_HANDLE, nullptr, 0, const_cast<PUCHAR>(data.data()), ULONG(data.size()), digest, 20) != 0)
        return {};
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

// the primary display, where the game opens: in Windows HDR mode or not, and its reported peak in nits; asked again
// every two seconds, so switching HDR in Windows shows
struct DisplayHdr {
    bool hdr = false;
    uint32_t peak = 0;
};
DisplayHdr PrimaryHdr() {
    static DisplayHdr last;
    static ULONGLONG asked = 0;
    ULONGLONG now = GetTickCount64();
    if (asked && now - asked < 2000) return last;
    asked = now;
    last = {};
    HMONITOR mon = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return last;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> a;
    for (UINT i = 0; factory->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; i++) {
        Microsoft::WRL::ComPtr<IDXGIOutput> o;
        for (UINT j = 0; a->EnumOutputs(j, &o) != DXGI_ERROR_NOT_FOUND; j++) {
            Microsoft::WRL::ComPtr<IDXGIOutput6> o6;
            DXGI_OUTPUT_DESC1 d{};
            if (FAILED(o.As(&o6)) || FAILED(o6->GetDesc1(&d)) || d.Monitor != mon) continue;
            last.hdr = d.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
            last.peak = uint32_t(d.MaxLuminance + 0.5f);
        }
    }
    return last;
}

// Whether a graphics card traces rays as the game's renderer does (runtime/gpu/raytrace.cpp): DXR 1.1 and shader
// model 6.5. Asked once a card, by its name (empty: the fastest). CV_NO_RAYS=1 pretends none does
bool TracesRays(const std::string& name) {
    static std::vector<std::pair<std::string, bool>> known;
    for (auto& k : known)
        if (k.first == name) return k.second;
    bool yes = false;
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    Microsoft::WRL::ComPtr<IDXGIFactory6> f6;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> a;
    if (!GetEnvironmentVariableW(L"CV_NO_RAYS", nullptr, 0) && SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        factory.As(&f6);
        for (UINT i = 0;; i++) {
            HRESULT hr = f6 ? f6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&a))
                            : factory->EnumAdapters1(i, &a);
            if (FAILED(hr)) break;
            Gpu g = Describe(a.Get(), true);
            if (g.software || (!name.empty() && g.name != name)) continue;
            Microsoft::WRL::ComPtr<ID3D12Device> dev;
            if (SUCCEEDED(D3D12CreateDevice(a.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) {
                D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5{};
                D3D12_FEATURE_DATA_SHADER_MODEL sm{D3D_SHADER_MODEL_6_5};
                yes = SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &o5, sizeof(o5))) &&
                      o5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1 &&
                      SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &sm, sizeof(sm))) &&
                      sm.HighestShaderModel >= D3D_SHADER_MODEL_6_5;
                break;
            }
            if (!name.empty()) break;
        }
    }
    known.push_back({name, yes});
    return yes;
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

// the game's check: the xbe the exe was made from, or that with the certificate's title, media and region patched
enum class Release { Missing, Match, Patched, Other };

Release Check(const std::vector<uint8_t>& xbe) {
    if (Sha1(xbe) == kImageSha1) return Release::Match;
    uint32_t base = 0, cert = 0;
    if (xbe.size() >= 0x11C) {
        memcpy(&base, &xbe[0x104], 4);
        memcpy(&cert, &xbe[0x118], 4);
    }
    if (cert < base || uint64_t(cert - base) + 0xA8 > xbe.size()) return Release::Other;
    std::vector<uint8_t> copy = xbe;
    memcpy(&copy[cert - base + 0x0C], kImageCertificate, 80);
    memcpy(&copy[cert - base + 0x9C], kImageCertificate + 80, 12);
    return Sha1(copy) == kImageSha1 ? Release::Patched : Release::Other;
}

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
    Release xbe = Release::Missing;
    struct Image {
        fs::path path;
        Release release = Release::Missing;  // Missing: no Xbox disc, or no default.xbe on it
    };
    std::vector<Image> isos;
    bool exe = false;
    fs::path textures;
    size_t pictures = 0;      // .png and .dds in textures and the folders inside it
    bool ps2_buttons = false;  // the PS2's button icons among them (texpack.cpp, the PS2 button layout)
    bool movies = false;       // a movie pack beside the game: movies\movies.toml (movies.cpp)
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
        {
            std::error_code ec;
            movies = fs::is_regular_file(ExeDir() / "movies" / "movies.toml", ec);
        }
        assets = FindAssets();
        xbe = Release::Missing;
        isos.clear();
        exe = fs::exists(ExeDir() / kGameExe);
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
            if (disc.Open(e.path(), error) && disc.Load(kXbe, data)) image.release = Check(data);
            isos.push_back(image);
        }
        std::sort(isos.begin(), isos.end(), [](const Image& a, const Image& b) { return a.path < b.path; });
    }
    static bool Good(Release r) { return r == Release::Match || r == Release::Patched; }
    bool Playable() const {
        return exe && (Good(xbe) || std::any_of(isos.begin(), isos.end(), [](const Image& i) { return Good(i.release); }));
    }
};

// a disc image into assets: a hard link on the same drive, a copy with progress otherwise
struct Import {
    std::atomic<bool> running{false};
    std::atomic<float> progress{0};
    std::string result;
    std::thread worker;

    static DWORD CALLBACK Progress(LARGE_INTEGER total, LARGE_INTEGER done, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD,
                                  HANDLE, HANDLE, LPVOID self) {
        static_cast<Import*>(self)->progress = total.QuadPart ? float(double(done.QuadPart) / double(total.QuadPart)) : 0;
        return PROGRESS_CONTINUE;
    }

    void Start(fs::path from, fs::path to) {
        if (running) return;
        if (worker.joinable()) worker.join();
        running = true;
        progress = 0;
        result.clear();
        worker = std::thread([this, from, to] {
            std::error_code ec;
            fs::create_directories(to.parent_path(), ec);
            if (CreateHardLinkW(to.c_str(), from.c_str(), nullptr)) {
                result = F("Linked {} into assets.", {Utf8(to.filename().wstring())});
            } else if (CopyFileExW(from.c_str(), to.c_str(), Progress, this, nullptr, COPY_FILE_FAIL_IF_EXISTS)) {
                result = F("Copied {} into assets.", {Utf8(to.filename().wstring())});
            } else {
                result = F("Could not copy the disc image (error {}).", {std::to_string(GetLastError())});
            }
            running = false;
        });
    }
    ~Import() {
        if (worker.joinable()) worker.join();
    }
};

// Frame generation's libraries (the SDK's runtime/gpu/framegen.h), which this game's download does not carry: the
// player's own copy, downloaded from the vendor's official GitHub release into the game's folder with their licences.
// AMD FSR 3: its signed DLLs beside the game, the loader, frame generation and the upscaler (the game's
// anti-aliasing). NVIDIA DLSS: Streamline's release, its DLSS-G and DLSS files in streamline\ (frame generation, and
// the game's anti-aliasing and upscaling).
constexpr const wchar_t* kFsrRaw = L"https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/v2.3.0/Kits/FidelityFX/";
constexpr const char* kFsrLicence = "https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/v2.3.0/Kits/FidelityFX/docs/license.md";
constexpr const wchar_t* kDlssZip = L"https://github.com/NVIDIA-RTX/Streamline/releases/download/v2.14.1/streamline-sdk-v2.14.1.zip";
constexpr const char* kDlssLicence = "https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.1/license.txt";
const wchar_t* const kFsrFiles[] = {L"amd_fidelityfx_loader_dx12.dll", L"amd_fidelityfx_framegeneration_dx12.dll",
                                    L"amd_fidelityfx_upscaler_dx12.dll"};
const wchar_t* const kDlssFiles[] = {L"sl.interposer.dll", L"sl.common.dll", L"sl.dlss_g.dll", L"sl.reflex.dll",
                                     L"sl.pcl.dll",       L"nvngx_dlssg.dll", L"sl.dlss.dll",   L"nvngx_dlss.dll"};

// all of a kind's files there, beside the game (FSR) or in its streamline folder (DLSS)
bool HaveFrameGen(const fs::path& game, uint32_t kind) {
    std::error_code ec;
    if (kind == 1) {
        for (const wchar_t* f : kFsrFiles)
            if (!fs::is_regular_file(game / f, ec)) return false;
        return true;
    }
    for (const wchar_t* f : kDlssFiles)
        if (!fs::is_regular_file(game / L"streamline" / f, ec)) return false;
    return true;
}

// a GeForce RTX 40 or 50: DLSS frame generation's cards
bool DlssCard(const std::string& name) {
    size_t at = name.find("RTX ");
    return at != std::string::npos && at + 5 < name.size() && (name[at + 4] == '4' || name[at + 4] == '5') &&
           std::isdigit(static_cast<unsigned char>(name[at + 5]));
}

// any GeForce RTX (or NVIDIA RTX workstation card): DLSS anti-aliasing's
bool RtxCard(const std::string& name) {
    return name.find("NVIDIA") != std::string::npos && name.find("RTX") != std::string::npos;
}

struct FrameGenDownload {
    std::atomic<bool> running{false};
    std::atomic<float> progress{0};
    std::atomic<uint32_t> kind{0};
    std::string result;
    std::thread worker;

    // URLDownloadToFile's progress
    struct Progress : IBindStatusCallback {
        std::atomic<float>* out;
        float from, span;
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** p) override {
            if (id == IID_IUnknown || id == IID_IBindStatusCallback) {
                *p = this;
                return S_OK;
            }
            *p = nullptr;
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
        ULONG STDMETHODCALLTYPE Release() override { return 1; }
        HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD, IBinding*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE GetPriority(LONG*) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE OnLowResource(DWORD) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnProgress(ULONG done, ULONG total, ULONG, LPCWSTR) override {
            if (total) *out = from + span * float(double(done) / double(total));
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT, LPCWSTR) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD*, BINDINFO*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID, IUnknown*) override { return S_OK; }
    };

    // url to file through a .part beside it, so a file is whole or not there
    bool Fetch(const std::wstring& url, const fs::path& to, float from, float span, HRESULT& hr) {
        fs::path part = to;
        part += L".part";
        Progress p{};
        p.out = &progress;
        p.from = from;
        p.span = span;
        hr = URLDownloadToFileW(nullptr, url.c_str(), part.c_str(), 0, &p);
        std::error_code ec;
        if (FAILED(hr) || fs::file_size(part, ec) == 0) {
            fs::remove(part, ec);
            if (SUCCEEDED(hr)) hr = E_FAIL;
            return false;
        }
        fs::rename(part, to, ec);
        if (ec) hr = HRESULT_FROM_WIN32(ec.value());
        return !ec;
    }

    // Windows' own tar takes the named files out of a zip
    static bool Extract(const fs::path& zip, const fs::path& into, const std::vector<std::wstring>& names) {
        wchar_t sys[MAX_PATH];
        GetSystemDirectoryW(sys, MAX_PATH);
        std::wstring cmd = L"\"" + std::wstring(sys) + L"\\tar.exe\" -xf \"" + zip.wstring() + L"\" -C \"" + into.wstring() + L"\"";
        for (const std::wstring& n : names) cmd += L" \"" + n + L"\"";
        STARTUPINFOW si{sizeof(si)};
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
            return false;
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return code == 0;
    }

    void Start(uint32_t k, fs::path game) {
        if (running) return;
        if (worker.joinable()) worker.join();
        running = true;
        progress = 0;
        kind = k;
        result.clear();
        worker = std::thread([this, k, game] {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            HRESULT hr = S_OK;
            std::error_code ec;
            bool ok = true;
            fs::create_directories(game / L"licences", ec);
            if (k == 1) {
                ok = Fetch(std::wstring(kFsrRaw) + L"docs/license.md", game / L"licences" / L"AMD-FidelityFX-licence.md", 0, 0.02f, hr);
                constexpr int n = int(std::size(kFsrFiles));
                for (int i = 0; ok && i < n; i++)
                    ok = Fetch(std::wstring(kFsrRaw) + L"signedbin/" + kFsrFiles[i], game / kFsrFiles[i],
                               0.02f + 0.98f * float(i) / n, 0.98f / n, hr);
            } else {
                fs::path tmp = game / L"streamline-download";
                fs::create_directories(tmp, ec);
                fs::path zip = tmp / L"streamline.zip";
                ok = Fetch(kDlssZip, zip, 0, 0.95f, hr);
                std::vector<std::wstring> names{L"license.txt", L"bin/x64/nvngx_dlss.license.txt", L"bin/x64/reflex.license.txt"};
                for (const wchar_t* f : kDlssFiles) names.push_back(std::wstring(L"bin/x64/") + f);
                if (ok && !Extract(zip, tmp, names)) {
                    ok = false;
                    hr = E_FAIL;
                }
                if (ok) {
                    fs::create_directories(game / L"streamline", ec);
                    for (const wchar_t* f : kDlssFiles) {
                        fs::copy_file(tmp / L"bin" / L"x64" / f, game / L"streamline" / f, fs::copy_options::overwrite_existing, ec);
                        if (ec) ok = false;
                    }
                    fs::copy_file(tmp / L"license.txt", game / L"licences" / L"NVIDIA-Streamline-licence.txt",
                                  fs::copy_options::overwrite_existing, ec);
                    fs::copy_file(tmp / L"bin" / L"x64" / L"nvngx_dlss.license.txt",
                                  game / L"licences" / L"NVIDIA-DLSS-licence.txt", fs::copy_options::overwrite_existing, ec);
                    fs::copy_file(tmp / L"bin" / L"x64" / L"reflex.license.txt",
                                  game / L"licences" / L"NVIDIA-Reflex-licence.txt", fs::copy_options::overwrite_existing, ec);
                    if (!ok) hr = E_FAIL;
                }
                fs::remove_all(tmp, ec);
            }
            progress = 1;
            result = ok ? T("Downloaded. The game uses them from its next start.")
                        : F("The download failed (error {}): check the connection and try again.",
                            {std::to_string(uint32_t(hr))});
            CoUninitialize();
            running = false;
        });
    }
    ~FrameGenDownload() {
        if (worker.joinable()) worker.join();
    }
};

// settings: the keys the game reads, the rest of the file kept

struct Settings {
    std::string display_mode = "borderless";
    uint32_t width = 0, height = 0;
    uint32_t render_percent = 100;
    uint32_t aniso = 0;
    bool vsync = true;
    bool hdr = true;                    // HDR output on a display in Windows HDR mode (the SDK's)
    uint32_t hdr_peak_nits = 0;         // 0 the display's own peak
    uint32_t hdr_scene_nits = 200;
    uint32_t frame_generation = 0;      // 0 off, 1 AMD FSR 3, 2 NVIDIA DLSS (the SDK's, from the next start)
    bool widescreen = true;
    uint32_t widescreen_aspect = 169;  // 169 (16:9) or 1610 (16:10)
    bool fps_counter = false;
    bool pause_on_focus_loss = true;
    uint32_t menu_key = VK_F10;
    bool keep_open = false;        // the launcher's own
    std::string language = "en";   // the launcher's own too, a code of words.h
    bool texture_pack = true;
    uint32_t texture_pack_size = 2048;  // 0 full size
    bool texture_pack_materials = true;  // the pack's normal and ORM maps (texpack.cpp)
    uint32_t button_layout = 0;         // 0 Xbox, 1 PS2
    bool lament_save = false;           // the Moai of a Lament of Innocence save (hooks.cpp)
    bool ps2_stick = true;              // the sticks as the PS2 read them (hooks.cpp)
    bool invert_camera_x = false, invert_camera_y = false;  // the camera stick the other way (keyboard.cpp)
    uint32_t fov = 100;                 // percent of the game's field of view in play (game/view.cpp)
    uint32_t draw_distance = 100;       // percent of the game's far plane (game/view.cpp)
    uint32_t fog_distance = 100;        // percent of the game's fog distance, 0 none (game/view.cpp)
    bool hud_edges = true;              // widescreen: the HUD at the screen's corners (widescreen.cpp)
    bool menu_spread = true;            // widescreen: the pause menu's columns toward the sides (widescreen.cpp)
    bool hd_movies = true;              // a movie pack's HD movies (movies.cpp)
    uint32_t movie_view = 1;            // 0 the game's 4:3 frame, 1 fit, 2 fill (movies.cpp)
    uint32_t antialiasing = 1;          // 0 off, 1 AMD FSR, 2 NVIDIA DLSS (menu.cpp, the SDK's renderer)
    uint32_t upscaling = 0;             // 0 native, 1 quality, 2 balanced, 3 performance
    uint32_t sharpening = 30;           // percent, AMD FSR's
    uint32_t ambient_occlusion = 1;     // 0 off, 1 low, 2 high
    uint32_t ray_tracing = 0;           // 0 off, 1 ambient occlusion, 5 lighting, 7 lighting and reflections (menu.cpp)
    uint32_t surface_depth = 2;         // 0 off, 1 low, 2 high: a pack's material maps seen into (parallax)
    std::string gpu;                    // the graphics card by its name, empty the fastest (the SDK's)
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
        hdr = rest["hdr"].value_or(hdr);
        hdr_peak_nits = rest["hdr_peak_nits"].value_or(hdr_peak_nits);
        hdr_scene_nits = rest["hdr_scene_nits"].value_or(hdr_scene_nits);
        frame_generation = std::min(rest["frame_generation"].value_or(frame_generation), 2u);
        widescreen = rest["widescreen"].value_or(widescreen);
        widescreen_aspect = rest["widescreen_aspect"].value_or(widescreen_aspect) == 1610 ? 1610u : 169u;
        fps_counter = rest["fps_counter"].value_or(fps_counter);
        pause_on_focus_loss = rest["pause_on_focus_loss"].value_or(pause_on_focus_loss);
        menu_key = rest["menu_key"].value_or(menu_key);
        keep_open = rest["launcher_keep_open"].value_or(keep_open);
        language = rest["launcher_language"].value_or(language);
        texture_pack = rest["texture_pack"].value_or(texture_pack);
        texture_pack_size = rest["texture_pack_size"].value_or(texture_pack_size);
        texture_pack_materials = rest["texture_pack_materials"].value_or(texture_pack_materials);
        button_layout = rest["button_layout"].value_or(button_layout);
        lament_save = rest["lament_save"].value_or(lament_save);
        ps2_stick = rest["ps2_stick"].value_or(ps2_stick);
        invert_camera_x = rest["invert_camera_x"].value_or(invert_camera_x);
        invert_camera_y = rest["invert_camera_y"].value_or(invert_camera_y);
        fov = rest["fov"].value_or(fov);
        draw_distance = rest["draw_distance"].value_or(draw_distance);
        fog_distance = rest["fog_distance"].value_or(fog_distance);
        hud_edges = rest["hud_edges"].value_or(hud_edges);
        menu_spread = rest["menu_spread"].value_or(menu_spread);
        hd_movies = rest["hd_movies"].value_or(hd_movies);
        movie_view = std::min(rest["movie_view"].value_or(movie_view), 2u);
        antialiasing = std::min(rest["antialiasing"].value_or(antialiasing), 2u);
        upscaling = std::min(rest["upscaling"].value_or(upscaling), 3u);
        sharpening = std::min(rest["sharpening"].value_or(sharpening), 100u);
        ambient_occlusion = std::min(rest["ambient_occlusion"].value_or(ambient_occlusion), 2u);
        // bits (1 ambient occlusion, 2 reflections, 4 lighting) as the choices offer them
        uint32_t rt = rest["ray_tracing"].value_or(ray_tracing);
        ray_tracing = (rt & 2) ? 7u : (rt & 4) ? 5u : (rt & 1);
        surface_depth = std::min(rest["surface_depth"].value_or(surface_depth), 2u);
        gpu = rest["gpu"].value_or(gpu);
    }

    bool Save() {
        rest.insert_or_assign("display_mode", display_mode);
        rest.insert_or_assign("width", int64_t(width));
        rest.insert_or_assign("height", int64_t(height));
        rest.insert_or_assign("render_percent", int64_t(render_percent));
        rest.insert_or_assign("anisotropic_filtering", int64_t(aniso));
        rest.insert_or_assign("vsync", vsync);
        rest.insert_or_assign("hdr", hdr);
        rest.insert_or_assign("hdr_peak_nits", int64_t(hdr_peak_nits));
        rest.insert_or_assign("hdr_scene_nits", int64_t(hdr_scene_nits));
        rest.insert_or_assign("frame_generation", int64_t(frame_generation));
        rest.insert_or_assign("widescreen", widescreen);
        rest.insert_or_assign("widescreen_aspect", int64_t(widescreen_aspect));
        rest.insert_or_assign("fps_counter", fps_counter);
        rest.insert_or_assign("pause_on_focus_loss", pause_on_focus_loss);
        rest.insert_or_assign("menu_key", int64_t(menu_key));
        rest.insert_or_assign("launcher_keep_open", keep_open);
        rest.insert_or_assign("launcher_language", language);
        rest.insert_or_assign("texture_pack", texture_pack);
        rest.insert_or_assign("texture_pack_size", int64_t(texture_pack_size));
        rest.insert_or_assign("texture_pack_materials", texture_pack_materials);
        rest.insert_or_assign("button_layout", int64_t(button_layout));
        rest.insert_or_assign("lament_save", lament_save);
        rest.insert_or_assign("ps2_stick", ps2_stick);
        rest.insert_or_assign("invert_camera_x", invert_camera_x);
        rest.insert_or_assign("invert_camera_y", invert_camera_y);
        rest.insert_or_assign("fov", int64_t(fov));
        rest.insert_or_assign("draw_distance", int64_t(draw_distance));
        rest.insert_or_assign("fog_distance", int64_t(fog_distance));
        rest.insert_or_assign("hud_edges", hud_edges);
        rest.insert_or_assign("menu_spread", menu_spread);
        rest.insert_or_assign("hd_movies", hd_movies);
        rest.insert_or_assign("movie_view", int64_t(movie_view));
        rest.insert_or_assign("antialiasing", int64_t(antialiasing));
        rest.insert_or_assign("upscaling", int64_t(upscaling));
        rest.insert_or_assign("sharpening", int64_t(sharpening));
        rest.insert_or_assign("ambient_occlusion", int64_t(ambient_occlusion));
        rest.insert_or_assign("ray_tracing", int64_t(ray_tracing));
        rest.insert_or_assign("surface_depth", int64_t(surface_depth));
        rest.insert_or_assign("gpu", gpu);
        std::ofstream f(File());
        f << rest << "\n";
        return bool(f);
    }

    void Defaults() {
        Settings d;
        d.rest = std::move(rest);
        d.keep_open = keep_open;
        d.language = language;
        *this = std::move(d);
    }
};

// UI pieces

struct Res {
    uint32_t w, h;
};

// the monitor's modes, and 1920x1200 for a 16:10 window (or a 16:10 screen streamed to) where it has no such mode
std::vector<Res> Resolutions() {
    std::vector<Res> list{{0, 0}};
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    auto add = [&](uint32_t w, uint32_t h) {
        bool seen = false;
        for (const Res& r : list) seen |= r.w == w && r.h == h;
        if (!seen) list.push_back({w, h});
    };
    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++)
        if (dm.dmPelsWidth >= 640 && dm.dmPelsHeight >= 480) add(dm.dmPelsWidth, dm.dmPelsHeight);
    add(1920, 1200);
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

// Segoe Fluent / MDL2 icons, for status lines and the language
enum : uint32_t {
    kIconCheck = 0xE73E, kIconWindow = 0xE73F, kIconFullScreen = 0xE740, kIconGlobe = 0xE774, kIconWarn = 0xE7BA,
    kIconDisc = 0xE958,
};
const ImWchar kIconRanges[] = {0xE73E, 0xE740, 0xE774, 0xE774, 0xE7BA, 0xE7BA, 0xE958, 0xE958, 0};
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

// controls that do nothing as the other settings stand (upscaling with anti-aliasing off): shown under a veil of the
// card's navy and not taken, a note under them saying what they need
bool g_off = false;
void BeginOff(bool off) {
    ImGui::BeginDisabled(off);
    g_off = off;
}
void EndOff() {
    ImGui::EndDisabled();
    g_off = false;
}
void Veil(ImDrawList* dl, ImVec2 a, ImVec2 b) {
    if (g_off) dl->AddRectFilled(a - ImVec2(S(1), S(1)), b + ImVec2(S(1), S(1)), Rgb(2, 9, 38, 175));
}

// an on/off switch; true when flipped
bool Toggle(const char* id, bool& v) {
    ImVec2 size{S(46), S(22)};
    ImVec2 a = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(id, size);
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
    Veil(dl, a, a + size);
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
    Veil(dl, a, a + ImVec2(avail, h));
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
    Veil(dl, a, a + ImVec2(avail, h));
    ImGui::PopID();
    return changed;
}

template <typename T>
void Combo(const char* id, T& value, const std::vector<T>& values, const std::vector<std::string>& names) {
    size_t cur = 0;
    for (size_t i = 0; i < values.size(); i++)
        if (values[i] == value) cur = i;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo(id, names[cur].c_str())) {
        for (size_t i = 0; i < values.size(); i++) {
            if (ImGui::Selectable(names[i].c_str(), i == cur)) value = values[i];
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

// a setting with many choices: title and description across the card, its control under them across it too
void BeginWideSetting(const char* title, const char* desc, bool first = false) {
    if (!first) {
        Rule(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), ImGui::GetContentRegionAvail().x, kLine);
        ImGui::Dummy({0, S(7)});
    }
    Text(g_semi, kText, title);
    if (desc) Wrapped(g_small, kMuted, desc);
    ImGui::Dummy({0, S(4)});
    ImGui::BeginGroup();
}

// a switch at the right end of the control column
void ToggleRight(const char* id, bool& v) {
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - S(46));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(5));
    Toggle(id, v);
}

// a line under a setting's control, in its column: what the choice does, or what it needs (amber)
void ControlNote(ImU32 col, const std::string& s) {
    ImGui::Dummy({0, S(1)});
    Wrapped(g_small, col, s.c_str());
}

// the name of the cards under it, in the band's teal
void Section(const char* s) {
    ImGui::Dummy({0, S(4)});
    Text(g_small, kTealHi, s);
    ImGui::Dummy({0, S(1)});
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
    ImGui::PushTextWrapPos(0.0f);  // a long line (some languages') wraps under itself, not out of its card
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
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

float g_font_scale = 0;  // the scale the fonts were last made at

// the scale the window gives: the layout at 100% fitted into the window's size, at its monitor's DPI
float FitScale() {
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    float dpi = WindowDpi(g_hwnd);
    float zoom = std::min(float(rc.right - rc.left) / (kBaseW * dpi), float(rc.bottom - rc.top) / (kBaseH * dpi));
    return dpi * std::clamp(zoom, kMinZoom, 4.0f);
}

// before a frame: the style and the fonts at the scale the window's size gives. While an edge is being dragged the
// fonts are only stretched to it; they are made anew at that size once the window stops (sharp at every size)
void Rescale() {
    float want = FitScale();
    bool fonts = std::fabs(want - g_font_scale) > 0.001f && !g_sizing;
    if (std::fabs(want - g_scale) < 0.001f && !fonts) return;
    g_scale = want;
    ImGui::GetStyle() = ImGuiStyle();
    Style();
    ImGuiIO& io = ImGui::GetIO();
    if (fonts) {
        io.Fonts->Clear();
        Fonts();
        ImGui_ImplDX11_InvalidateDeviceObjects();  // the font texture made again at the next frame
        g_font_scale = want;
    }
    io.FontGlobalScale = g_scale / g_font_scale;
}

// a picture the launcher shows: a texture of its own
struct Picture {
    ID3D11ShaderResourceView* view = nullptr;
    float w = 0, h = 0;
    void Release() {
        if (view) view->Release();
        *this = {};
    }
};

// px: BGRA with straight alpha, as ImGui blends, the top row first
Picture Upload(const std::vector<uint8_t>& px, uint32_t w, uint32_t h) {
    Picture p;
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
}

// the HD texture pack's title logo: cut to its letters (its alpha the PS2's, 128 opaque) and shrunk to at most max_w
// wide, premultiplied while it is scaled so that its edges keep their colour
Picture LoadLogo(const fs::path& file, uint32_t max_w) {
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
    ComPtr<IWICBitmap> cut;
    ComPtr<IWICBitmapScaler> sc;
    std::vector<uint8_t> out(size_t(tw) * th * 4);
    if (FAILED(wic->CreateBitmapFromMemory(cw, ch, GUID_WICPixelFormat32bppPBGRA, w * 4, UINT(px.size() - at), &px[at], &cut)) ||
        FAILED(wic->CreateBitmapScaler(&sc)) || FAILED(sc->Initialize(cut.Get(), tw, th, WICBitmapInterpolationModeFant)) ||
        FAILED(sc->CopyPixels(nullptr, tw * 4, UINT(out.size()), out.data())))
        return {};
    for (size_t i = 0; i < out.size(); i += 4)
        if (out[i + 3])
            for (int k = 0; k < 3; k++) out[i + k] = uint8_t(std::min(255u, out[i + k] * 255u / out[i + 3]));
    return Upload(out, tw, th);
}

// the crest the build put in the launcher's icon (resource 1), at its largest; none in a release
Picture LoadCrest() {
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
}

// the pages, in the sidebar's order
enum Page : int { kHome, kDisplay, kGraphics, kView, kTextures, kGame, kControls, kFiles, kPageCount };

// the Graphics page's quality presets: anti-aliasing (on keeps the kind chosen), upscaling and ambient occlusion
// together. Original is the Xbox's picture; anti-aliasing off, the others do nothing, so off is Original whatever
// they are
struct Preset {
    uint32_t aa, upscaling, ao;
};
constexpr Preset kPresets[] = {{0, 0, 0}, {1, 3, 0}, {1, 2, 1}, {1, 0, 1}, {1, 0, 2}};
enum : int { kOriginal, kLow, kMedium, kHigh, kUltra, kCustom = -1 };

int PresetOf(const Settings& s) {
    if (!s.antialiasing) return kOriginal;
    for (int i = kLow; i <= kUltra; i++)
        if (s.upscaling == kPresets[i].upscaling && s.ambient_occlusion == kPresets[i].ao) return i;
    return kCustom;
}

void SetPreset(Settings& s, int p) {
    s.antialiasing = kPresets[p].aa ? std::max(s.antialiasing, 1u) : 0;
    s.upscaling = kPresets[p].upscaling;
    s.ambient_occlusion = kPresets[p].ao;
}

const char* PresetName(int p) {
    switch (p) {
        case kOriginal: return T("Original");
        case kLow: return T("Low");
        case kMedium: return T("Medium");
        case kHigh: return T("High");
        case kUltra: return T("Ultra");
        default: return T("Custom");
    }
}

const char* AntiAliasName(uint32_t aa) { return aa == 2 ? "NVIDIA DLSS" : aa == 1 ? "AMD FSR" : T("Off"); }

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
            case kHome: keep = HomePage(); break;
            case kDisplay: DisplayPage(); break;
            case kGraphics: GraphicsPage(); break;
            case kView: ViewPage(); break;
            case kTextures: TexturesPage(); break;
            case kGame: GamePage(); break;
            case kControls: ControlsPage(); break;
            default: FilesPage(); break;
        }
        ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::End();
        AutoSave();
        return keep;
    }

private:
    // every setting the launcher shows, to see when one changed
    std::string Snapshot() const {
        const Settings& s = settings_;
        return s.display_mode + "|" + std::to_string(s.width) + "x" + std::to_string(s.height) + "|" +
               std::to_string(s.render_percent) + "|" + std::to_string(s.aniso) + "|" + std::to_string(s.vsync) +
               std::to_string(s.widescreen) + std::to_string(s.widescreen_aspect) + std::to_string(s.fps_counter) + std::to_string(s.pause_on_focus_loss) +
               std::to_string(s.keep_open) + "|" + std::to_string(s.menu_key) + "|" + std::to_string(s.texture_pack) +
               std::to_string(s.texture_pack_size) + std::to_string(s.texture_pack_materials) + "|" +
               std::to_string(s.button_layout) +
               std::to_string(s.lament_save) + "|" + s.gpu + "|" + s.language + "|" + std::to_string(s.ps2_stick) +
               std::to_string(s.invert_camera_x) + std::to_string(s.invert_camera_y) + std::to_string(s.hud_edges) + "|" +
               std::to_string(s.fov) + "|" + std::to_string(s.draw_distance) + "|" + std::to_string(s.fog_distance) + "|" +
               std::to_string(s.hdr) + std::to_string(s.hdr_peak_nits) + "|" + std::to_string(s.hdr_scene_nits) + "|" +
               std::to_string(s.frame_generation) + "|" + std::to_string(s.menu_spread) + std::to_string(s.antialiasing) +
               std::to_string(s.upscaling) + std::to_string(s.ambient_occlusion) + std::to_string(s.surface_depth) + std::to_string(s.ray_tracing) + "|" +
               std::to_string(s.sharpening) + "|" + std::to_string(s.hd_movies) + std::to_string(s.movie_view);
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
        SetWindowTextW(g_hwnd, Wide(T("Castlevania: Curse of Darkness - Launcher")).c_str());
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

        static const words::Key kPages[kPageCount] = {"Home", "Display", "Graphics", "View", "Textures",
                                                      "Game", "Controls", "Game files"};
        for (int i = 0; i < kPageCount; i++) {
            if (i == kDisplay) {
                ImGui::Dummy({0, S(4)});
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + S(30));
                Label(T("SETTINGS"));
            }
            if (i == kFiles) ImGui::Dummy({0, S(6)});
            if (NavItem(T(kPages[i]), page_ == i)) page_ = i;
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
        float side = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - side - S(6));
        if (ImGui::BeginCombo("##language", preview.c_str())) {
            for (int i : words::kMenu) {
                if (ImGui::Selectable(words::kLanguages[i].name, i == cur)) settings_.language = words::kLanguages[i].code;
                if (i == cur) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Language"));
        // beside it, full screen and back (also F11 or Alt+Enter)
        ImGui::SameLine(0, S(6));
        ImGui::PushID("fullscreen");
        if (Button(Icon(g_full ? kIconWindow : kIconFullScreen).c_str(), {side, side}, Kind::Ghost, g_body))
            g_toggle_full = true;
        ImGui::PopID();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", g_full ? T("Leave full screen (F11)") : T("Full screen (F11)"));
        ImGui::PopFont();
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
        ImGui::EndDisabled();
        if (!ready) {
            ImGui::SameLine(0, S(22));
            if (Button(T("Set up game files"), {0, S(54)}, Kind::Secondary)) page_ = kFiles;
        }
        // beside the buttons, mid way down them (a line of its own: SameLine would put it at their top)
        ImVec2 end = ImGui::GetItemRectMax();
        ImGui::SetCursorScreenPos({end.x + S(20), end.y - S(54) * 0.5f - g_semi->FontSize * 0.5f});
        if (!UsedGpu().d3d12) Status(kIconWarn, kRed, T("No Direct3D 12"));
        else if (ready) Status(kIconCheck, kGreen, T("Ready to play"));
        else if (!files_.exe) Status(kIconWarn, kRed, T("Castlevania.exe is missing"));
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
        std::string quality = PresetName(PresetOf(settings_));
        if (settings_.antialiasing) quality += "  |  " + std::string(AntiAliasName(settings_.antialiasing));
        struct Tile {
            const char* label;
            std::string value;
            int page;
        } tiles[] = {{T("Display"), mode + "  |  " + res, kDisplay},
                     {T("Graphics quality"), quality, kGraphics},
                     {T("Widescreen"),
                      settings_.widescreen ? F("On ({}, Hor+)", {settings_.widescreen_aspect == 1610 ? "16:10" : "16:9"})
                                           : T("Off (4:3)"),
                      kView},
                     {T("Menu key"), KeyName(settings_.menu_key) + "  |  L3 + R3", kGame}};
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
        // anti-aliasing is on by default but its vendor's files are not in the download: without them it, upscaling
        // and ambient occlusion do nothing, so say so where everyone looks
        if (settings_.antialiasing && !HaveFrameGen(ExeDir(), AntiAliasVendor())) {
            Status(kIconWarn, kAmber, F("Anti-aliasing ({}) needs its files: download them on the Graphics page.",
                                        {AntiAliasName(AntiAliasVendor())}));
            ImGui::Dummy({0, S(4)});
        }
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
            Status(kIconWarn, kAmber, F("{}  -  not here with Direct3D 12, the game takes the fastest", {settings_.gpu}));
        std::string name = gpu.name.empty() ? T("This PC's graphics") : gpu.name;
        if (gpu.d3d12 && gpu.software) {
            Status(kIconWarn, kAmber, F("{}  -  Direct3D 12 on the processor only", {name}));
            Wrapped(g_small, kMuted, T("No graphics driver is installed, so Windows draws on the processor, far too slowly "
                                       "for the game. Install the driver from NVIDIA, AMD or Intel."));
        } else if (gpu.d3d12) {
            Status(kIconCheck, kGreen, F("{}  -  Direct3D 12, ready", {name}));
        } else {
            Status(kIconWarn, kRed, F("{}  -  no Direct3D 12", {name}));
            Wrapped(g_small, kMuted, T("The game draws with Direct3D 12, which every Windows 10 and 11 has, but only "
                                       "graphics with a driver for it can run. Install the newest driver from NVIDIA, AMD "
                                       "or Intel. Graphics older than NVIDIA's GeForce 600 series, AMD's Radeon HD 7700 or "
                                       "Intel's HD 4200 (2012 and 2013) have none, and the game cannot start on them."));
        }
    }

    // a vendor's files (1 AMD FSR, 2 NVIDIA DLSS), for anti-aliasing and frame generation: here, or a download from
    // the vendor's own release, with its licence a click away
    void VendorFiles(uint32_t k) {
        bool fsr = k == 1;
        if (fg_download_.running && fg_download_.kind == k) {
            ImGui::ProgressBar(fg_download_.progress, {-FLT_MIN, S(30)},
                               fsr ? T("Downloading from AMD...") : T("Downloading from NVIDIA..."));
            return;
        }
        if (HaveFrameGen(ExeDir(), k)) {
            Status(kIconCheck, kGreen, fsr ? T("AMD FSR's files are here") : T("NVIDIA DLSS's files are here"));
        } else {
            Status(kIconWarn, kAmber, fsr ? T("AMD FSR's files are not here yet") : T("NVIDIA DLSS's files are not here yet"));
            Wrapped(g_small, kMuted, fsr ? T("From AMD's own release on GitHub. The files are AMD's, under AMD's licence, "
                                             "which is saved with them; they are not part of this game's download.")
                                         : T("From NVIDIA's own Streamline release on GitHub (about 280 MB, of which the game "
                                             "keeps 70). The files are NVIDIA's, under NVIDIA's licences, which are saved "
                                             "with them; they are not part of this game's download."));
            ImGui::Dummy({0, S(4)});
            ImGui::BeginDisabled(fg_download_.running);
            if (Button(fsr ? T("Download from AMD") : T("Download from NVIDIA"), {0, 0}, Kind::Primary))
                fg_download_.Start(k, ExeDir());
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (Button(T("Read the licence"), {0, 0}, Kind::Secondary))
                ShellExecuteW(nullptr, L"open", Wide(fsr ? kFsrLicence : kDlssLicence).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        if (!fg_download_.running && !fg_download_.result.empty() && fg_download_.kind == k) {
            Note(fg_download_.result);
            fg_download_.result.clear();
        }
    }

    // the vendor anti-aliasing takes its files from: DLSS chosen on a card without it runs AMD FSR
    uint32_t AntiAliasVendor() const {
        uint32_t aa = settings_.antialiasing;
        return aa == 2 && !RtxCard(UsedGpu().name) ? 1 : aa;
    }

    // frame generation's warnings, and its files when anti-aliasing's card above does not show them already
    void FrameGenStatus() {
        uint32_t k = settings_.frame_generation;
        if (!k) return;
        if (k == 2 && !DlssCard(UsedGpu().name)) {
            Status(kIconWarn, kAmber, T("Needs a GeForce RTX 40 or 50: this graphics card has no DLSS frame generation. "
                                        "AMD FSR 3 works on it."));
        }
        if (k == 2 && settings_.hdr) {
            Wrapped(g_small, kMuted, T("With HDR on, the game sends HDR10 for DLSS frame generation, the HDR format it "
                                       "takes."));
        }
        if (k != AntiAliasVendor()) VendorFiles(k);
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
        BeginCard("hdr");
        BeginSetting("HDR", T("On a display with HDR on in Windows: fire, magic, light and the other brightest parts of "
                              "the picture shine brighter than the rest. Off sends the usual picture."), true);
        ToggleRight("hdr", settings_.hdr);
        EndSetting();
        DisplayHdr shown = PrimaryHdr();
        if (shown.hdr) {
            Status(kIconCheck, kGreen, F("This display: HDR on in Windows, peak {} nits", {std::to_string(shown.peak)}));
        } else {
            Status(kIconWarn, kAmber, T("This display: HDR off in Windows"));
            Wrapped(g_small, kMuted, T("For HDR in the game, turn on Use HDR in Windows' display settings."));
        }
        if (settings_.hdr) {
            BeginSetting(T("Peak brightness"), T("The brightest highlights. The display's own peak suits most; lower it if "
                                                 "highlights glare."));
            {
                uint32_t own = shown.peak >= 400 ? shown.peak : 1000;
                uint32_t v = settings_.hdr_peak_nits ? settings_.hdr_peak_nits : own;
                std::string name = F("{} nits", {std::to_string(v)});
                if (!settings_.hdr_peak_nits) name += "  -  " + std::string(T("the display's"));
                if (Slider("peak", v, 400, 4000, name)) settings_.hdr_peak_nits = (v + 5) / 10 * 10;
            }
            EndSetting();
            BeginSetting(T("Scene brightness"), T("How bright the picture is apart from its highlights. 200 nits suits most "
                                                  "rooms; more for a bright room, less for a dark one."));
            Slider("scene", settings_.hdr_scene_nits, 80, 500, F("{} nits", {std::to_string(settings_.hdr_scene_nits)}));
            EndSetting();
        }
        EndCard();
    }

    void GraphicsPage() {
        PageTitle(T("Graphics"), T("Picture quality and speed. A running game takes the changes at once, unless a setting "
                                   "says otherwise."));
        uint32_t& aa = settings_.antialiasing;

        Section(T("QUALITY"));
        BeginCard("presets");
        BeginWideSetting(T("Quality preset"), T("Sets anti-aliasing, upscaling and ambient occlusion together. Pick the "
                                                "closest, then change any of them below."), true);
        {
            int preset = PresetOf(settings_);
            if (Segmented<int>("preset", preset, {kOriginal, kLow, kMedium, kHigh, kUltra},
                               {PresetName(kOriginal), PresetName(kLow), PresetName(kMedium), PresetName(kHigh),
                                PresetName(kUltra)}))
                SetPreset(settings_, preset);
            const char* note = preset == kOriginal ? T("The game as the Xbox showed it: no smoothing, no added shadow.")
                               : preset == kLow    ? T("For handhelds and older graphics: the 3D at half resolution, "
                                                       "smoothed and rebuilt to full size.")
                               : preset == kMedium ? T("For handhelds and laptops: balanced upscaling and soft shadow. It "
                                                       "held 60 FPS on a Legion Go 2 in testing.")
                               : preset == kHigh   ? T("Full resolution, smooth edges and soft shadow. The default.")
                               : preset == kUltra  ? T("As High, with deeper shadow in corners and crevices.")
                                                   : T("Your own mix of the settings below.");
            ControlNote(kMuted, note);
        }
        EndSetting();
        EndCard();

        Section(T("ANTI-ALIASING AND UPSCALING"));
        BeginCard("aa");
        BeginSetting(T("Anti-aliasing"), T("Smooths the jagged, shimmering edges of the 3D world. AMD FSR works on any "
                                           "graphics card; NVIDIA DLSS needs a GeForce RTX. The HUD and menus stay sharp."),
                     true);
        Segmented<uint32_t>("aa", aa, {0, 1, 2}, {T("Off"), "AMD FSR", "NVIDIA DLSS"});
        if (aa == 2 && AntiAliasVendor() != 2) ControlNote(kAmber, T("This graphics card has no DLSS: the game uses AMD FSR."));
        else if (aa == 2) ControlNote(kMuted, T("From the game's next start."));
        EndSetting();
        if (aa) VendorFiles(AntiAliasVendor());
        if (aa) ImGui::Dummy({0, S(6)});
        BeginOff(!aa);
        BeginWideSetting(T("Upscaling"), T("Draws the 3D world at a lower resolution and rebuilds it to full size, for a "
                                           "higher frame rate on slower graphics or a handheld. The HUD and menus stay at "
                                           "full resolution."));
        Segmented<uint32_t>("up", settings_.upscaling, {0, 1, 2, 3},
                            {T("Native"), T("Quality"), T("Balanced"), T("Performance")});
        EndOff();
        if (!aa) {
            ControlNote(kAmber, T("Needs anti-aliasing on."));
        } else {
            static const words::Key kUpNotes[] = {
                "The 3D at the screen's full resolution.",
                "The 3D at two thirds of the resolution, its textures as sharp as at full size.",
                "The 3D at 59% of the resolution: a good trade on a handheld.",
                "The 3D at half the resolution: the fastest, a little softer.",
            };
            ControlNote(kMuted, T(kUpNotes[std::min(settings_.upscaling, 3u)]));
        }
        EndSetting();
        BeginOff(AntiAliasVendor() != 1);
        BeginSetting(T("Sharpening"), T("Brings back fine texture detail after AMD FSR smooths the picture; too much "
                                        "outlines the edges. 30% suits most."));
        Slider("sharp", settings_.sharpening, 0, 100,
               settings_.sharpening ? std::to_string(settings_.sharpening) + "%" : std::string(T("Off")));
        EndOff();
        if (!aa) ControlNote(kAmber, T("Needs anti-aliasing on."));
        else if (AntiAliasVendor() == 2) ControlNote(kMuted, T("NVIDIA DLSS has no sharpening of its own."));
        EndSetting();
        EndCard();

        Section(T("LIGHTING AND DETAIL"));
        BeginCard("detail");
        BeginOff(!aa);
        BeginSetting(T("Ambient occlusion"), T("Soft shadow where surfaces meet: in corners, between stones, under ledges "
                                               "and where feet touch the floor. Low is lighter on the graphics card."),
                     true);
        Segmented<uint32_t>("ao", settings_.ambient_occlusion, {0, 1, 2}, {T("Off"), T("Low"), T("High")});
        EndOff();
        if (!aa) ControlNote(kAmber, T("Needs anti-aliasing on."));
        EndSetting();
        {
            bool rays = TracesRays(settings_.gpu);
            BeginOff(!aa || !rays);
            BeginSetting(T("Ray tracing"), T("Rays through the room itself. Ambient occlusion: found from the room "
                                             "rather than from what the screen shows, so it holds at the screen's "
                                             "edges and behind things. Lighting adds the light the room's surfaces "
                                             "throw onto each other: water's on the stone beside it, a torchlit wall's "
                                             "on the floor. Reflections: iron and dressed stone mirror the room too, "
                                             "with a texture pack's material maps. Needs a graphics card with DirectX "
                                             "ray tracing."),
                         true);
            Segmented<uint32_t>("rt", settings_.ray_tracing, {0, 1, 5, 7},
                                {T("Off"), T("Ambient occlusion"), T("Lighting"), T("Lighting and reflections")});
            EndOff();
            if (!rays) ControlNote(kAmber, T("This graphics card has no DirectX ray tracing."));
            else if (!aa) ControlNote(kAmber, T("Needs anti-aliasing on."));
            else if ((settings_.ray_tracing & 1) && !settings_.ambient_occlusion)
                ControlNote(kAmber, T("Ambient occlusion is off: only the lighting is traced."));
            EndSetting();
        }
        BeginSetting(T("Anisotropic filtering"), T("Keeps floors and walls sharp where they are seen at an angle. It costs "
                                                   "little."));
        Segmented<uint32_t>("aniso", settings_.aniso, {0, 2, 4, 8, 16}, {T("Off"), "2x", "4x", "8x", "16x"});
        EndSetting();
        BeginSetting(T("Render scale"), T("The whole picture drawn at more or less than the screen's resolution. Above "
                                          "100% is sharper and far slower; for speed, upscaling is the better way."));
        Segmented<uint32_t>("scale", settings_.render_percent, {50, 75, 100, 150, 200}, {"50%", "75%", "100%", "150%", "200%"});
        EndSetting();
        EndCard();

        Section(T("FRAME GENERATION"));
        BeginCard("framegen");
        BeginSetting(T("Frame generation"), T("A frame made between each two the game draws, for twice the frame rate on a "
                                              "screen that shows it (120 Hz or more). AMD FSR 3 works on any graphics card; "
                                              "NVIDIA DLSS needs a GeForce RTX 40 or 50. From the next start."), true);
        Segmented<uint32_t>("fg", settings_.frame_generation, {0, 1, 2}, {T("Off"), "AMD FSR 3", "NVIDIA DLSS"});
        EndSetting();
        FrameGenStatus();
        EndCard();

        Section(T("GRAPHICS CARD"));
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
    }

    void ViewPage() {
        PageTitle(T("View"), T("The picture's shape, where the HUD sits, and how far and wide you see."));
        Section(T("WIDESCREEN"));
        BeginCard("wide");
        BeginSetting(T("Widescreen"), T("More of the world at the sides, nothing stretched: 16:9 for most screens, 16:10 "
                                        "for 16:10 ones such as 1920x1200. Off shows the game in 4:3, as on an old TV."),
                     true);
        {
            uint32_t wide = settings_.widescreen ? settings_.widescreen_aspect : 0;
            if (Segmented<uint32_t>("wide", wide, {0, 169, 1610}, {T("Off"), "16:9", "16:10"})) {
                settings_.widescreen = wide != 0;
                if (wide) settings_.widescreen_aspect = wide;
            }
        }
        EndSetting();
        bool narrow = !settings_.widescreen;
        BeginOff(narrow);
        BeginSetting(T("HUD at the screen edges"), T("The health bar and the rest of the HUD in the screen's corners, "
                                                    "rather than around the 4:3 middle."));
        ToggleRight("hudedges", settings_.hud_edges);
        EndOff();
        if (narrow) ControlNote(kAmber, T("Needs widescreen on."));
        EndSetting();
        BeginOff(narrow);
        BeginSetting(T("Pause menu across the screen"), T("The pause menu's columns spread toward the screen's sides, its "
                                                         "page buttons in the middle, rather than all of it in the 4:3 "
                                                         "middle."));
        ToggleRight("menuspread", settings_.menu_spread);
        EndOff();
        if (narrow) ControlNote(kAmber, T("Needs widescreen on."));
        EndSetting();
        EndCard();

        Section(T("MOVIES"));
        BeginCard("movies");
        BeginSetting(T("HD movies"), T("The story's movies from an HD movie pack in the movies folder. Off plays the "
                                       "game's own."),
                     true);
        ToggleRight("hdmovies", settings_.hd_movies);
        ControlNote(files_.movies ? kMuted : kAmber,
                    files_.movies ? T("Movie pack found in the movies folder.") : T("No movie pack in the movies folder."));
        EndSetting();
        BeginSetting(T("Movie size"), T("Fit: the picture as large as the screen shows it whole, without the black bars. "
                                        "Fill: the whole screen, a little of the picture's top and bottom cut off. "
                                        "Original: the game's own 4:3 frame."));
        Segmented<uint32_t>("movieview", settings_.movie_view, {1, 2, 0}, {T("Fit"), T("Fill"), T("Original")});
        EndSetting();
        EndCard();

        Section(T("CAMERA AND DISTANCE"));
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
    }

    void TexturesPage() {
        PageTitle(T("Textures"), T("An HD texture pack replaces the game's textures with sharper ones. Packs made for "
                                   "PCSX2 work: put the pack's pictures in the textures folder."));
        BeginCard("texpack");
        BeginSetting(T("HD texture pack"), T("Uses the pack in the textures folder. Off shows the game's own textures."),
                     true);
        ToggleRight("texpack", settings_.texture_pack);
        ControlNote(kMuted, T("From the game's next start."));
        EndSetting();
        ImGui::Dummy({0, S(2)});
        if (files_.pictures) Status(kIconCheck, kGreen, F("{} in the textures folder", {words::Pictures(files_.pictures)}));
        else Status(kIconWarn, kMuted, T("No pack in the textures folder"));
        ImGui::Dummy({0, S(4)});
        if (Button(T("Open textures folder"), {0, 0}, Kind::Secondary)) OpenTextures();
        ImGui::SameLine();
        if (Button(T("Check again"), {0, 0}, Kind::Secondary)) rescan_ = true;
        EndCard();

        Section(T("PACK OPTIONS"));
        BeginCard("texdetail");
        bool pack = settings_.texture_pack, maps = pack && settings_.texture_pack_materials;
        BeginOff(!pack);
        BeginSetting(T("Texture size"), T("The largest side a picture is loaded at. Larger is sharper and needs more video "
                                          "memory and loading time."), true);
        Segmented<uint32_t>("texsize", settings_.texture_pack_size, {1024, 2048, 0}, {"1024", "2048", T("Full")});
        EndOff();
        ControlNote(pack ? kMuted : kAmber, pack ? T("From the game's next start.") : T("Needs the HD texture pack on."));
        EndSetting();
        BeginOff(!pack);
        BeginSetting(T("Material maps"), T("A pack's normal and ORM maps (its materials folder) shade its pictures: relief "
                                           "that catches the light, darker crevices, shine on metal."));
        ToggleRight("texmat", settings_.texture_pack_materials);
        EndOff();
        if (!pack) ControlNote(kAmber, T("Needs the HD texture pack on."));
        EndSetting();
        BeginOff(!maps);
        BeginSetting(T("Surface depth"), T("With material maps, bricks, mortar, cracks and carvings get real depth that "
                                           "shifts as the camera moves (parallax), not only shading. Low is shallower and "
                                           "lighter on the graphics card."));
        Segmented<uint32_t>("depth", settings_.surface_depth, {0, 1, 2}, {T("Off"), T("Low"), T("High")});
        EndOff();
        if (!maps) ControlNote(kAmber, pack ? T("Needs material maps on.") : T("Needs the HD texture pack on."));
        EndSetting();
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
        BeginCard("game");
        BeginSetting(T("FPS counter"), T("Shows the frame rate in the corner."), true);
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
        EndCard();
        BeginCard("launcher");
        BeginSetting(T("Keep launcher open"), T("Leave this window open while the game runs."), true);
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
        PageTitle(T("Game files"), T("Your own copy of the game: the USA (NTSC-U) release, as a disc image or extracted."));
        BeginCard("status");
        if (files_.assets.empty()) {
            Status(kIconWarn, kRed, T("No assets folder found"));
        } else {
            switch (files_.xbe) {
                case Release::Match: Status(kIconCheck, kGreen, F("{}  -  USA release, ready", {kXbe})); break;
                case Release::Patched: Status(kIconCheck, kGreen, F("{}  -  USA release (patched header), ready", {kXbe})); break;
                case Release::Other: Status(kIconWarn, kRed, F("{}  -  another release, only the USA one works", {kXbe})); break;
                case Release::Missing:  // no matter beside a disc image
                    if (files_.isos.empty()) Status(kIconWarn, kMuted, F("{}  -  not extracted", {kXbe}));
                    break;
            }
            for (const Files::Image& iso : files_.isos) {
                std::string name = Utf8(iso.path.filename().wstring());
                switch (iso.release) {
                    case Release::Match: Status(kIconDisc, kGreen, F("{}  -  USA release, ready", {name})); break;
                    case Release::Patched: Status(kIconDisc, kGreen, F("{}  -  USA release (patched header), ready", {name})); break;
                    case Release::Other: Status(kIconWarn, kRed, F("{}  -  another release, only the USA one works", {name})); break;
                    case Release::Missing: Status(kIconWarn, kRed, F("{}  -  not an Xbox disc image", {name})); break;
                }
            }
        }
        if (!files_.exe) Status(kIconWarn, kRed, T("Castlevania.exe is not beside the launcher"));
        if (!files_.assets.empty() && files_.xbe == Release::Missing && files_.isos.empty())
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
        Wrapped(g_small, kMuted, T("Saves are kept beside the game in hdd\\E\\UDATA\\4b4e002d."));
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

    bool Play() {
        saved_ = Snapshot();
        if (!settings_.Save()) {
            Note(T("Could not write the settings file"));
            return true;
        }
        fs::path exe = ExeDir() / kGameExe;
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
        Note(T("Game started"));
        return settings_.keep_open;
    }

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
        fs::path from = file;
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
    std::vector<Res> modes_;
    FrameGenDownload fg_download_;
    std::string status_, saved_, shown_language_;
    double status_time_ = -100;
    bool rescan_ = false;
    int page_ = 0;
};

Launcher* g_launcher = nullptr;
bool g_running = true;

// one frame: the scale the window's size gives, the launcher, shown
void PaintFrame() {
    if (IsIconic(g_hwnd) || !g_launcher) return;
    if (g_toggle_full) {
        g_toggle_full = false;
        ToggleFullScreen();
    }
    Rescale();
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (!g_launcher->Frame()) g_running = false;
    ImGui::Render();
    ImVec4 bg = V(kBg);
    const float clear[4] = {bg.x, bg.y, bg.z, 1};
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swap->Present(1, 0);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);  // WIC, for the pack's logo
    // --download-framegen 1 (AMD FSR 3) or 2 (NVIDIA DLSS): the Graphics page's download, without the window; exit
    // code 0 once the files are there
    if (const wchar_t* at = std::wcsstr(GetCommandLineW(), L"--download-framegen ")) {
        uint32_t kind = uint32_t(std::wcstoul(at + 20, nullptr, 10));
        if (kind != 1 && kind != 2) return 2;
        FrameGenDownload d;
        d.Start(kind, ExeDir());
        if (d.worker.joinable()) d.worker.join();
        return HaveFrameGen(ExeDir(), kind) ? 0 : 1;
    }
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
    // a window of its own size that can be resized, maximized or made full screen (F11): the layout scales to it.
    // At 100%, unless the screen's work area is smaller: then as large as fits nine tenths of it
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT work{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    float zoom = std::min({1.0f, 0.9f * float(work.right - work.left) / (kBaseW * scale),
                           0.9f * float(work.bottom - work.top) / (kBaseH * scale)});
    zoom = std::max(zoom, kMinZoom);
    RECT r{0, 0, LONG(kBaseW * scale * zoom), LONG(kBaseH * scale * zoom)};
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, GetDpiForSystem());
    int w = r.right - r.left, h = r.bottom - r.top;
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, Wide(T("Castlevania: Curse of Darkness - Launcher")).c_str(), style,
                             work.left + (work.right - work.left - w) / 2, work.top + (work.bottom - work.top - h) / 2,
                             w, h, nullptr, nullptr, inst, nullptr);
    scale *= zoom;
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
    g_font_scale = scale;
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);

    Launcher launcher;
    // --page home|display|graphics|view|textures|game|controls|files opens on that page
    static const wchar_t* kPageNames[kPageCount] = {L"home", L"display", L"graphics", L"view",
                                                    L"textures", L"game", L"controls", L"files"};
    for (int i = 0; i < kPageCount; i++)
        if (std::wcsstr(GetCommandLineW(), (std::wstring(L"--page ") + kPageNames[i]).c_str())) launcher.ShowPage(i);
    g_launcher = &launcher;
    g_paint = PaintFrame;
    while (g_running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        if (!g_running) break;
        if (IsIconic(g_hwnd)) {
            Sleep(50);
            continue;
        }
        PaintFrame();
    }
    g_paint = nullptr;
    g_launcher = nullptr;

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
