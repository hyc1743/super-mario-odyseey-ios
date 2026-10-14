// 宿主侧输入状态：前台（触屏层 / 手柄 / 按键脚本）写，hid 服务读。
// 与参考项目 dm/host/dm_input.h 同义，字段按本项目重做的触屏方案裁剪。
#pragma once
#include <stdint.h>

// Horizon 的 NpadButton 位序（公开 ABI，按位用）
enum {
    DM_BTN_A = 1ull << 0, DM_BTN_B = 1ull << 1, DM_BTN_X = 1ull << 2, DM_BTN_Y = 1ull << 3,
    DM_BTN_LSTICK = 1ull << 4, DM_BTN_RSTICK = 1ull << 5, DM_BTN_L = 1ull << 6, DM_BTN_R = 1ull << 7,
    DM_BTN_ZL = 1ull << 8, DM_BTN_ZR = 1ull << 9, DM_BTN_PLUS = 1ull << 10, DM_BTN_MINUS = 1ull << 11,
    DM_BTN_LEFT = 1ull << 12, DM_BTN_UP = 1ull << 13, DM_BTN_RIGHT = 1ull << 14, DM_BTN_DOWN = 1ull << 15,
};

void dm_input_set_buttons(uint64_t buttons);
uint64_t dm_input_buttons(void);
void dm_input_set_sticks(int32_t lx, int32_t ly, int32_t rx, int32_t ry);
uint32_t dm_input_style(void);
void dm_input_set_style(uint32_t style);
void dm_input_set_touch(int count, const uint8_t *touches);   // touches = count × 0x28
