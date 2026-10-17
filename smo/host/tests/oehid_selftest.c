// oe 与 hid 服务单测：不需要游戏文件，也不需要 Apple 平台。
// 检查的是启动路径上真正会读的东西：
//   hid：NpadState 布局（0x28）、按钮/摇杆/连接位、采样计数、样式过滤、触屏状态布局（0x290）
//   oe ：运行模式 / 语言 / 前后台 / 性能模式，以及通知队列（状态变化入队、按序弹出）
#include "../dm.h"
#include "../hle.h"
#include "../dm_input.h"
#include "../dm_oe.h"
#include <sched.h>

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

// hle.c / 平台层的桩
void unimpl_stubs(void) { }
int svc_dispatch(unsigned n, uint64_t *r) { (void)n; (void)r; return -1; }
int svc_thread(int n, uint64_t *r) { (void)n; (void)r; return 0; }

typedef struct { int64_t sampling; uint64_t buttons; int32_t lx, ly, rx, ry; uint32_t attributes; uint32_t _pad; } NpadState;
typedef struct { int64_t sampling; int32_t count, _r; uint8_t touches[16][0x28]; } TouchState16;

int main(void) {
    (void)dm_hle_force_lookup("_ZN2nn3hid10InitializeEv");        // 触发生成表的装载与校验
    printf("[smo] 接管清单 %u 条，真实现 %u 条\n", g_hle_force_n, hle_impl_count());

    printf("== hid：NpadState ==\n");
    NpadState s;
    uint32_t id = 0;
    void (*get)(NpadState *, const uint32_t *) = (void *)dm_hle_force_lookup("_ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj");
    CHK(get != NULL, "取到 GetNpadState(NpadFullKeyState) 的真实现");
    dm_input_set_buttons(DM_BTN_A | DM_BTN_PLUS);
    dm_input_set_sticks(100, -200, 300, -400);
    get(&s, &id);
    CHK(s.buttons == (DM_BTN_A | DM_BTN_PLUS), "buttons 原样传下去（偏移 8）");
    CHK(s.lx == 100 && s.ly == -200 && s.rx == 300 && s.ry == -400, "摇杆四个轴（偏移 16/20/24/28）");
    CHK(s.attributes == 1, "attributes = 连接位（偏移 32）");
    CHK(s.sampling > 0, "sampling 递增（偏移 0）");
    int64_t s1 = s.sampling;
    get(&s, &id);
    CHK(s.sampling == s1 + 1, "第二次采样号 +1");

    printf("== hid：样式过滤 ==\n");
    dm_input_set_style(1u << 1);                                  // 只接掌机样式
    get(&s, &id);
    CHK(s.buttons == 0 && s.attributes == 0, "没接这个样式时全 0（含 attributes）");
    dm_input_set_style(3);
    get(&s, &id);
    CHK(s.buttons == (DM_BTN_A | DM_BTN_PLUS), "接上后恢复");

    printf("== hid：触屏 ==\n");
    uint8_t t[0x28] = {0};
    t[0] = 7;                                                     // finger id
    t[4] = 1; t[8] = 2; t[12] = 3; t[16] = 4;                      // x, y, diameter, rotation（布局按 Horizon）
    dm_input_set_touch(1, t);
    TouchState16 ts;
    int (*gett)(TouchState16 *, int) = (void *)dm_hle_force_lookup("_ZN2nn3hid20GetTouchScreenStatesILm16EEEiPNS0_16TouchScreenStateIXT_EEEi");
    CHK(gett(&ts, 16) == 1, "GetTouchScreenStates 返回 1");
    CHK(ts.count == 1, "触点数 = 1（偏移 8）");
    CHK(ts.touches[0][0] == 7 && ts.touches[0][4] == 1 && ts.touches[0][16] == 4, "触点字段落在 touches[0] 内");
    dm_input_set_touch(0, NULL);
    gett(&ts, 16);
    CHK(ts.count == 0, "抬起后点数 = 0");

    printf("== hid: ABI and output boundaries ==\n");
    union { uint64_t align; uint8_t bytes[0x300]; } small;
    memset(small.bytes, 0xA5, sizeof small.bytes);
    void (*getone)(void *) = (void *)dm_hle_force_lookup("_ZN2nn3hid19GetTouchScreenStateILm1EEEvPNS0_16TouchScreenStateIXT_EEE");
    dm_input_set_touch(1, t);
    getone(small.bytes);
    int guard_ok = 1;
    for (size_t i = 0x38; i < sizeof small.bytes; i++) if (small.bytes[i] != 0xA5) guard_ok = 0;
    CHK(guard_ok, "TouchScreenState<1> writes exactly 0x38 bytes; caller objects survive");
    CHK(((int32_t *)small.bytes)[2] == 1 && small.bytes[0x10] == 7, "single touch contains first published point");
    int (*getmany)(NpadState *, int, const uint32_t *) = (void *)dm_hle_force_lookup("_ZN2nn3hid13GetNpadStatesEPNS0_16NpadFullKeyStateEiRKj");
    NpadState states[2];
    memset(states, 0xA5, sizeof states);
    CHK(getmany(states, 0, &id) == 0 && ((uint8_t *)states)[0] == 0xA5, "zero sample capacity writes nothing");
    CHK(getmany(states, 2, &id) == 1 && states[0].attributes == 1, "Npad array API receives capacity and id in their correct argument slots");
    CHK(((uint8_t *)&states[1])[0] == 0xA5, "returns one available sample without overwriting the next slot");
    dm_input_set_state(DM_BTN_A|DM_BTN_ZL,32767,-32767,-16384,16384);
    getmany(states,1,&id);
    CHK(states[0].buttons==(DM_BTN_A|DM_BTN_ZL) && states[0].lx==32767 && states[0].ly==-32767 && states[0].rx==-16384 && states[0].ry==16384,
        "combined frontend state reaches the guest Npad snapshot");
    dm_input_set_state(0,0,0,0,0);
    getmany(states,1,&id);
    CHK(!states[0].buttons && !states[0].lx && !states[0].ly && !states[0].rx && !states[0].ry,
        "frontend cancellation clears every guest input field");
    uint32_t (*assignment)(const uint32_t *) = (void *)dm_hle_force_lookup("_ZN2nn3hid20GetNpadJoyAssignmentERKj");
    uint32_t const_id = 0x20;
    CHK(assignment(&const_id) == 0 && const_id == 0x20, "assignment query returns an enum and preserves const id");
    void (*startaxis)(const void *) = (void *)dm_hle_force_lookup("_ZN2nn3hid18StartSixAxisSensorERKNS0_19SixAxisSensorHandleE");
    startaxis(&const_id);
    CHK(const_id == 0x20, "StartSixAxisSensor preserves its input handle");

    printf("== oe：环境与通知 ==\n");
    int (*opo)(void) = (void *)dm_hle_force_lookup("_ZN2nn2oe16GetOperationModeEv");
    int (*operf)(void) = (void *)dm_hle_force_lookup("_ZN2nn2oe18GetPerformanceModeEv");
    int (*ofoc)(void) = (void *)dm_hle_force_lookup("_ZN2nn2oe20GetCurrentFocusStateEv");
    uint64_t (*olang)(void) = (void *)dm_hle_force_lookup("_ZN2nn2oe18GetDesiredLanguageEv");
    unsigned (*opop)(uint32_t *) = (void *)dm_hle_force_lookup("_ZN2nn2oe25TryPopNotificationMessageEPj");
    CHK(opo && operf && ofoc && olang && opop, "oe 的五个关键查询都取到了");
    CHK(opo() == 0, "默认运行模式 = 掌机(0)");
    CHK(operf() == 0, "掌机下性能模式 = 正常(0)");
    CHK(ofoc() == 1, "默认前台(1)");
    uint64_t (*make_lang)(int) = (void *)dm_hle_force_lookup("_ZN2nn8settings12LanguageCode4MakeENS0_8LanguageE");
    int (*equal_lang)(const char *, const char *) = (void *)dm_hle_force_lookup("_ZN2nn8settingseqERKNS0_12LanguageCodeES3_");
    uint64_t english = make_lang(1), japanese = make_lang(0);
    CHK(memcmp(&english, "en-US\0\0\0", 8) == 0, "LanguageCode::Make returns the 8-byte code for localized resource lookup");
    CHK(olang() == english, "GetDesiredLanguage returns en-US LanguageCode by value, not enum 1");
    CHK(equal_lang((const char *)&english, "en-US") && !equal_lang((const char *)&english, (const char *)&japanese),
        "language code equality distinguishes language folders");
    uint32_t m = 0;
    CHK(opop(&m) == 0, "初始通知队列为空");
    dm_oe_set_opmode(1);
    CHK(opo() == 1 && operf() == 1, "切主机后运行模式 = 1、性能模式 = 1");
    CHK(opop(&m) == 1 && m == 30, "入队通知 30（运行模式变化）并能弹出");
    CHK(opop(&m) == 1 && m == 31, "接着是通知 31（性能模式变化）");
    dm_oe_set_focus(3);
    CHK(opop(&m) == 1 && m == 15, "切后台入队通知 15（焦点变化）");
    CHK(opop(&m) == 0, "取完后队列再次为空");

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
