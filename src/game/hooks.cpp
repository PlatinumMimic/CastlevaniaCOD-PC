// Hooks for default.xbe. A strong definition replaces the recompiled function at that address, for example:
//
// #include "xdk/hle.h"
//
// X86_EXTERN_FUNC(__imp__sub_00012345);
// X86_FUNC(sub_00012345) {
//     __imp__sub_00012345(ctx, base);  // the original
// }
#include <cstring>

#include "app/app.h"
#include "cpu/x86_ops.h"

extern bool g_lament_save;  // menu.cpp

namespace {

constexpr float kWiden = (4.0f / 3.0f) / (16.0f / 9.0f);

void Scale(uint8_t* base, uint32_t va, float k) {
    float v;
    std::memcpy(&v, base + va, 4);
    v *= k;
    std::memcpy(base + va, &v, 4);
}

}  // namespace

// The camera's matrices, made again when the camera changed (a PS2 style libvu0 camera: sceVu0ViewScreenMatrix and
// clip matrices). Widescreen draws more at the sides (widescreen.cpp), so the clip data objects are culled against
// is made wider by the same 4:3 to 16:9; the matrices the scene is drawn with are left alone.
X86_EXTERN_FUNC(__imp__sub_00012CD0);
X86_FUNC(sub_00012CD0) {
    __imp__sub_00012CD0(ctx, base);
    if (!app::Widescreen()) return;
    Scale(base, 0x012FC500, kWiden);  // clip screen matrix, x scale
    Scale(base, 0x012FC5C0, kWiden);  // view clip matrix, x scale
    Scale(base, 0x012FC640, kWiden);  // its x scale as a vector
    Scale(base, 0x012FC660, kWiden);  // the projection's x scale as a vector
}

// The Lament of Innocence bonus. The PS2 game looked on the memory card for a Lament of Innocence save (BASLUS-20733,
// its name still in the image, sub_0033D590) and, finding one, set flag 0x4002, which a game copies into its own flag
// 0x30008006 (sub_00321860). This is that flag's one reader: a room event (sub_001425E0) that puts a Moai at the far
// end of 5.0, left of the potion there. The Xbox has no PS2 memory card, so the setting answers for it.
X86_EXTERN_FUNC(__imp__sub_0033E1D0);
X86_FUNC(sub_0033E1D0) {
    __imp__sub_0033E1D0(ctx, base);
    if (g_lament_save) ctx.eax = 1;
}
