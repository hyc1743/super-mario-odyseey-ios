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
    CHK(olang() == 1, "默认语言 = en-US(1)");
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
