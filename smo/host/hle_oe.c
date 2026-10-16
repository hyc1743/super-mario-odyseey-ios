// nn::oe：运行环境（运行模式、语言、性能模式、前后台、系统通知队列）。
// 派生自参考项目 d2r-mobile 的 dm/host/hle_oe.c（MIT），只保留环境查询与通知队列本身，
// 去掉其与自家 UI/商店等耦合的部分。
//
// 真机上这些走 oe 服务；宿主按当前「设备形态」直接给值。SMO 会在启动时读运行模式与语言，
// 之后按通知（运行模式变化 / 前后台）调整行为，所以这三样都要有合理的初值与推送。
#include "dm.h"
#include "hle.h"

enum { OPMODE_HANDHELD = 0, OPMODE_DOCKED = 1 };
enum { PERF_NORMAL = 0, PERF_BOOST = 1 };
enum { FOCUS_IN_FOREGROUND = 1, FOCUS_OUT = 2, FOCUS_BACKGROUND = 3 };

// Horizon 的 Language 代码（nn::oe::Language）：0=ja 1=en-US 2=fr 3=de 4=it 5=es 6=zh-CN 7=ko …
// SMO 的美版默认 en-US；中文用 6（简中）。
static uint64_t g_lang = 1;
static int g_opmode = OPMODE_HANDHELD;
static int g_focus = FOCUS_IN_FOREGROUND;
static int g_inactivity_extended;

// 系统通知队列（TryPopNotificationMessage 从这里取；前端在状态变化时入队）
#define NOTI_MAX 64
static uint32_t g_noti[NOTI_MAX];
static int g_noti_head, g_noti_tail;

void dm_oe_push_notification(unsigned msg) {
    int next = (g_noti_tail + 1) % NOTI_MAX;
    if (next == g_noti_head) { LOG("通知队列满，丢弃 %u", msg); return; }
    g_noti[g_noti_tail] = msg;
    g_noti_tail = next;
}
// 通知号取 Horizon 的 KnownNotificationMessage：15 = 焦点变化（前台/失焦/后台），
// 30 / 31 = 运行模式变化 / 性能模式变化。
void dm_oe_set_focus(int st) {
    if (st == g_focus) return;
    g_focus = st;
    dm_oe_push_notification(15);
}
void dm_oe_set_opmode(int m) {
    if (m == g_opmode) return;
    g_opmode = m;
    dm_oe_push_notification(30);        // OperationModeChanged
    dm_oe_push_notification(31);        // PerformanceModeChanged
}
int dm_oe_opmode(void) { return g_opmode; }
int dm_oe_focus(void) { return g_focus; }
void dm_oe_set_language(uint64_t lang) { g_lang = lang; }

// ---------------------------------------------------------------- 接管
static int h_GetOpMode(void) { return g_opmode; }
static int h_GetPerfMode(void) { return g_opmode == OPMODE_DOCKED ? PERF_BOOST : PERF_NORMAL; }
// LanguageCode is an 8-byte value returned in x0 (SDK Make at +0x15b740).
// These helpers must not be generic zero-returning service stubs: the game uses
// them to select LocalizedData/<language> before loading layout/font resources.
static uint64_t h_MakeLanguageCode(int language) {
    static const char *const codes[] = {
        "ja", "en-US", "fr", "de", "it", "es", "zh-CN", "ko", "nl", "pt", "ru",
        "zh-TW", "en-GB", "fr-CA", "es-419", "zh-Hans", "zh-Hant", "pt-BR"
    };
    uint64_t value = 0;
    if (language >= 0 && (unsigned)language < SMO_ARRAY_LEN(codes))
        memcpy(&value, codes[language], strlen(codes[language]));
    return value;
}
// SDK GetDesiredLanguage returns LanguageCode by value, not the Language enum.
static uint64_t h_GetLang(void) { return h_MakeLanguageCode((int)g_lang); }
static int h_LanguageCodeEqual(const char *a, const char *b) { return !strncmp(a, b, 8); }
static int h_GetFocus(void) { return g_focus; }
static void h_SetFocusHandling(int mode) { (void)mode; }
static int h_IsInactivityExt(void) { return g_inactivity_extended; }
static void h_SetInactivityExt(int on) { g_inactivity_extended = on; }
static uint32_t h_PopNoti(uint32_t *out) {
    if (g_noti_head == g_noti_tail) return 0;
    if (out) *out = g_noti[g_noti_head];
    g_noti_head = (g_noti_head + 1) % NOTI_MAX;
    return 1;
}
static void h_SetPerfConfig(int mode, int cfg) { (void)mode; (void)cfg; }   // 宿主不调频
static int h_GetPerfConfig(int mode) { (void)mode; return 0; }
static uint64_t h_GetDisplayVersion(void *out) {                            // 显示版本（宿主自报）
    if (out) memset(out, 0, 0x100);
    return 0;                                                               // 返回 0 = 用宿主给的版本串
}
static void h_GetPseudoDeviceId(void *uuid) { if (uuid) memset(uuid, 0x5A, 16); }

static const hle_impl_ent g_oe_impl[] = {
    {"_ZN2nn8settings12LanguageCode4MakeENS0_8LanguageE", (void *)h_MakeLanguageCode, "settings；语言代码值"},
    {"_ZN2nn8settingseqERKNS0_12LanguageCodeES3_", (void *)h_LanguageCodeEqual, "settings；比较语言代码"},
    {"_ZN2nn2oe10InitializeEv", (void *)h_GetOpMode, "oe；初始化，宿主无事可做"},
    {"_ZN2nn2oe16GetOperationModeEv", (void *)h_GetOpMode, "oe；运行模式（掌机/主机）"},
    {"_ZN2nn2oe18GetPerformanceModeEv", (void *)h_GetPerfMode, "oe；性能模式与运行模式一致"},
    {"_ZN2nn2oe18GetDesiredLanguageEv", (void *)h_GetLang, "oe；系统语言"},
    {"_ZN2nn2oe20GetCurrentFocusStateEv", (void *)h_GetFocus, "oe；前后台状态"},
    {"_ZN2nn2oe20SetFocusHandlingModeENS0_17FocusHandlingModeE", (void *)h_SetFocusHandling, "oe；前后台切换策略（宿主只记不做）"},
    {"_ZN2nn2oe25TryPopNotificationMessageEPj", (void *)h_PopNoti, "oe；系统通知队列（前端状态变化入队）"},
    {"_ZN2nn2oe27GetPerformanceConfigurationENS0_15PerformanceModeE", (void *)h_GetPerfConfig, "oe；性能档查询（宿主不调频）"},
    {"_ZN2nn2oe27SetPerformanceConfigurationENS0_15PerformanceModeEi", (void *)h_SetPerfConfig, "oe；性能档设置（宿主不调频）"},
    {"_ZN2nn2oe37IsUserInactivityDetectionTimeExtendedEv", (void *)h_IsInactivityExt, "oe；无操作检测是否延长"},
    {"_ZN2nn2oe38SetUserInactivityDetectionTimeExtendedEb", (void *)h_SetInactivityExt, "oe；设置无操作检测延长"},
    {"_ZN2nn2oe17GetDisplayVersionEPNS0_14DisplayVersionE", (void *)h_GetDisplayVersion, "oe；显示版本"},
    {"_ZN2nn2oe17GetPseudoDeviceIdEPNS_4util4UuidE", (void *)h_GetPseudoDeviceId, "oe；伪设备号（固定值）"},
    {"_ZN2nn2oe15EnableRecordingEv", (void *)h_SetFocusHandling, "oe；录像开关（宿主无系统录像）"},
    {"_ZN2nn2oe16DisableRecordingEv", (void *)h_SetFocusHandling, "oe；同上"},
    {"_ZN2nn2oe16EnableScreenShotEv", (void *)h_SetFocusHandling, "oe；截图开关（宿主无系统截图）"},
    {"_ZN2nn2oe17DisableScreenShotEv", (void *)h_SetFocusHandling, "oe；同上"},
    {"_ZN2nn2oe28SetResumeNotificationEnabledEb", (void *)h_SetFocusHandling, "oe；恢复通知开关（宿主以焦点通知 15 表达）"},
    {"_ZN2nn2oe42SetOperationModeChangedNotificationEnabledEb", (void *)h_SetFocusHandling, "oe；运行模式变化通知开关"},
    {"_ZN2nn2oe44SetPerformanceModeChangedNotificationEnabledEb", (void *)h_SetFocusHandling, "oe；性能模式变化通知开关"},
    {"_ZN2nn2oe24GetExpectedVolumeBalanceEPfS1_", (void *)h_SetFocusHandling, "oe；音量平衡（宿主不实现）"},
    {"_ZN2nn2oe24SetExpectedVolumeBalanceEff", (void *)h_SetFocusHandling, "oe；同上"},
    {"_ZN2nn2oe23EnableGamePlayRecordingEPvm", (void *)h_SetFocusHandling, "oe；录像（宿主无系统录像）"},
    {"_ZN2nn2oe17FinishStartupLogoEv", (void *)h_SetFocusHandling, "oe；结束启动 logo（宿主无此阶段）"},
    {"_ZN2nn2oe29SetScreenShotImageOrientationENS_5album16ImageOrientationE", (void *)h_SetFocusHandling, "oe；截图方向（宿主无系统截图）"},
};

static void __attribute__((constructor)) oe_reg(void) { hle_impl_register(g_oe_impl, SMO_ARRAY_LEN(g_oe_impl)); }
