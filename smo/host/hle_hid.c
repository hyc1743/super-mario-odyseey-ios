// nn::hid：手柄与触屏。派生自参考项目 d2r-mobile 的 dm/host/hle_hid.c（MIT）。
//
// 与参考项目的差别：他们的 fill() 与自家触屏/界面/功能层深度耦合（独占按键裁决、界面鼠标消息…），
// 本项目只保留**核心**：宿主侧一份输入状态 → 填进 Horizon 的 NpadState。
// 前台（触屏层、手柄、按键脚本）通过 dm_input_* 写这份状态。
//
// 布局按公开 Horizon ABI（两者相同，_Static_assert 钉住）：
//   NpadState  = {s64 sampling; u64 buttons; s32 lx, ly, rx, ry; u32 attributes; u32 pad}  —— 0x28
//   TouchScreenState<16> = {s64 sampling; s32 count; s32 pad; u8 touches[16][0x28]}       —— 0x290
#include "dm.h"
#include "hle.h"
#include "dm_input.h"
#include <pthread.h>

typedef struct {
    int64_t sampling;
    uint64_t buttons;
    int32_t lx, ly, rx, ry;
    uint32_t attributes;
    uint32_t _pad;
} NpadState;
_Static_assert(sizeof(NpadState) == 0x28, "NpadState 布局必须与 Horizon ABI 一致");

typedef struct { int64_t sampling; int32_t count, _r; uint8_t touches[16][0x28]; } TouchState16;
_Static_assert(sizeof(TouchState16) == 0x290, "TouchScreenState<16> 布局");

enum { NPAD_ID_HANDHELD = 0x20 };
enum { STYLE_FULLKEY = 1u << 0, STYLE_HANDHELD = 1u << 1 };
enum { ATTR_CONNECTED = 1u << 0 };

// ---------------------------------------------------------------- 宿主侧输入状态（前台写）
static pthread_mutex_t g_in_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t g_buttons;
static int32_t g_lx, g_ly, g_rx, g_ry;
static uint32_t g_style = STYLE_HANDHELD | STYLE_FULLKEY;   // 默认两种样式都算接着
static int g_touch_count;
static uint8_t g_touch[16][0x28];

void dm_input_set_buttons(uint64_t buttons) {
    pthread_mutex_lock(&g_in_lock);
    g_buttons = buttons;
    pthread_mutex_unlock(&g_in_lock);
}
uint64_t dm_input_buttons(void) {
    pthread_mutex_lock(&g_in_lock);
    uint64_t b = g_buttons;
    pthread_mutex_unlock(&g_in_lock);
    return b;
}
void dm_input_set_sticks(int32_t lx, int32_t ly, int32_t rx, int32_t ry) {
    pthread_mutex_lock(&g_in_lock);
    g_lx = lx; g_ly = ly; g_rx = rx; g_ry = ry;
    pthread_mutex_unlock(&g_in_lock);
}
uint32_t dm_input_style(void) { return g_style; }
void dm_input_set_style(uint32_t style) { g_style = style; }
void dm_input_set_touch(int count, const uint8_t *touches) {           // touches = count × 0x28
    pthread_mutex_lock(&g_in_lock);
    g_touch_count = count > 16 ? 16 : (count < 0 ? 0 : count);
    if (touches && g_touch_count) memcpy(g_touch, touches, (size_t)g_touch_count * 0x28);
    pthread_mutex_unlock(&g_in_lock);
}

// ---------------------------------------------------------------- NpadState 填充
static int64_t g_sampling;

static void fill(NpadState *s, uint32_t id, uint32_t style) {
    (void)id;
    pthread_mutex_lock(&g_in_lock);
    uint64_t buttons = g_buttons;
    int32_t lx = g_lx, ly = g_ly, rx = g_rx, ry = g_ry;
    uint32_t st = g_style;
    pthread_mutex_unlock(&g_in_lock);
    memset(s, 0, sizeof *s);
    s->sampling = ++g_sampling;
    if (!(st & style)) return;                                          // 这个样式没接着：全 0（attributes 也不置位）
    s->buttons = buttons;
    s->lx = lx; s->ly = ly; s->rx = rx; s->ry = ry;
    s->attributes = ATTR_CONNECTED;
}

// ---------------------------------------------------------------- 接管
static void h_Init(void) { }
static void h_InitNpad(void) { }
static void h_FullKey(NpadState *s, const uint32_t *id) { fill(s, id ? *id : 0, STYLE_FULLKEY); }
static void h_Handheld(NpadState *s, const uint32_t *id) { fill(s, id ? *id : NPAD_ID_HANDHELD, STYLE_HANDHELD); }
static void h_JoyDual(NpadState *s, const uint32_t *id) { fill(s, id ? *id : 0, STYLE_FULLKEY); }      // 双 Joy-Con 按全键处理
static void h_JoyLeft(NpadState *s, const uint32_t *id) { fill(s, id ? *id : 0, 0); }                 // 半边手柄：SMO 单机用不到
static void h_JoyRight(NpadState *s, const uint32_t *id) { fill(s, id ? *id : 0, 0); }
static uint32_t h_GetStyleSet(const uint32_t *id) {
    (void)id;
    return g_style;
}
static void h_Disconnect(const uint32_t *id) { (void)id; }
static void h_SetJoyHold(int t) { (void)t; }
static void h_SwapAssign(const uint32_t *a, const uint32_t *b) { (void)a; (void)b; }
static void h_GetJoyAssign(uint32_t *out, const uint32_t *id) { (void)id; if (out) *out = 0; }
static void h_GetControllerColor(void *out, void *out2, const uint32_t *id) { (void)id; if (out) memset(out, 0, 12); if (out2) memset(out2, 0, 12); }
static void h_GetSixAxis(uint32_t *out, const void *h) { (void)h; if (out) *out = 0; }                 // 六轴：报「没接着」
static void h_GetDebugPad(void *s) { if (s) memset(s, 0, 0x28); }
static void h_Vibration(const void *h, const void *v) { (void)h; (void)v; }                            // 振动：宿主先收下不用

static int h_GetTouch(TouchState16 *out, int n) {
    if (n < 1 || !out) return 0;
    pthread_mutex_lock(&g_in_lock);
    memset(out, 0, sizeof *out);
    out->sampling = ++g_sampling;
    out->count = g_touch_count;
    if (g_touch_count) memcpy(out->touches, g_touch, (size_t)g_touch_count * 0x28);
    pthread_mutex_unlock(&g_in_lock);
    return 1;
}
static void h_ActivateTouch(void) { }
static void h_InitTouchScreen(void) { }

// 名字取自 smo/data/hle_imports.tsv（hle.c 装载时校验每条真实现都在接管清单里）
static const hle_impl_ent g_hid_impl[] = {
    {"_ZN2nn3hid10InitializeEv", (void *)h_Init, "hid；初始化"},
    {"_ZN2nn3hid14InitializeNpadEv", (void *)h_InitNpad, "hid；初始化手柄"},
    {"_ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj", (void *)h_FullKey, "hid；全键手柄状态 ← 宿主输入状态"},
    {"_ZN2nn3hid12GetNpadStateEPNS0_17NpadHandheldStateERKj", (void *)h_Handheld, "hid；掌机手柄状态 ← 宿主输入状态"},
    {"_ZN2nn3hid12GetNpadStateEPNS0_16NpadJoyDualStateERKj", (void *)h_JoyDual, "hid；双 Joy-Con 状态"},
    {"_ZN2nn3hid12GetNpadStateEPNS0_16NpadJoyLeftStateERKj", (void *)h_JoyLeft, "hid；左 Joy-Con（单机用不到）"},
    {"_ZN2nn3hid12GetNpadStateEPNS0_17NpadJoyRightStateERKj", (void *)h_JoyRight, "hid；右 Joy-Con（单机用不到）"},
    {"_ZN2nn3hid13GetNpadStatesEPNS0_16NpadFullKeyStateEiRKj", (void *)h_FullKey, "hid；全键手柄状态（多个）"},
    {"_ZN2nn3hid13GetNpadStatesEPNS0_17NpadHandheldStateEiRKj", (void *)h_Handheld, "hid；掌机手柄状态（多个）"},
    {"_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyDualStateEiRKj", (void *)h_JoyDual, "hid；双 Joy-Con（多个）"},
    {"_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyLeftStateEiRKj", (void *)h_JoyLeft, "hid；左 Joy-Con（多个）"},
    {"_ZN2nn3hid13GetNpadStatesEPNS0_17NpadJoyRightStateEiRKj", (void *)h_JoyRight, "hid；右 Joy-Con（多个）"},
    {"_ZN2nn3hid15GetNpadStyleSetERKj", (void *)h_GetStyleSet, "hid；手柄样式集合"},
    {"_ZN2nn3hid14DisconnectNpadERKj", (void *)h_Disconnect, "hid；断开手柄（宿主无实际设备）"},
    {"_ZN2nn3hid18SetNpadJoyHoldTypeENS0_15NpadJoyHoldTypeE", (void *)h_SetJoyHold, "hid；Joy-Con 握持方式"},
    {"_ZN2nn3hid18SwapNpadAssignmentERKjS2_", (void *)h_SwapAssign, "hid；交换手柄分配"},
    {"_ZN2nn3hid20GetNpadJoyAssignmentERKj", (void *)h_GetJoyAssign, "hid；手柄分配查询"},
    {"_ZN2nn3hid22GetNpadControllerColorEPNS0_19NpadControllerColorES2_RKj", (void *)h_GetControllerColor, "hid；手柄颜色"},
    {"_ZN2nn3hid16GetDebugPadStateEPNS0_13DebugPadStateE", (void *)h_GetDebugPad, "hid；调试手柄（全 0）"},
    {"_ZN2nn3hid18InitializeDebugPadEv", (void *)h_Init, "hid；调试手柄初始化"},
    {"_ZN2nn3hid18SendVibrationValueERKNS0_21VibrationDeviceHandleERKNS0_14VibrationValueE", (void *)h_Vibration, "hid；振动（宿主先收下不用）"},
    {"_ZN2nn3hid18StartSixAxisSensorERKNS0_19SixAxisSensorHandleE", (void *)h_GetSixAxis, "hid；六轴（报没接着）"},
    {"_ZN2nn3hid21InitializeTouchScreenEv", (void *)h_InitTouchScreen, "hid；触屏初始化"},
    {"_ZN2nn3hid20GetTouchScreenStatesILm16EEEiPNS0_16TouchScreenStateIXT_EEEi", (void *)h_GetTouch, "hid；触屏状态 ← 前台触点"},
    {"_ZN2nn3hid19GetTouchScreenStateILm1EEEvPNS0_16TouchScreenStateIXT_EEE", (void *)h_GetTouch, "hid；触屏状态（1 点）"},
    {"_ZN2nn3hid24ActivateTouchScreenInputEv", (void *)h_ActivateTouch, "hid；启用触屏输入"},
    {"_ZN2nn3hid19SetTouchScreenDefaultConfigurationEPKNS0_26TouchScreenConfigurationE", (void *)h_ActivateTouch, "hid；触屏配置（宿主忽略）"},
};

static void __attribute__((constructor)) hid_reg(void) { hle_impl_register(g_hid_impl, SMO_ARRAY_LEN(g_hid_impl)); }
