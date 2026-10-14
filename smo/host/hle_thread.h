// 线程族 svc 的对外接口（hle_thread.c）
#pragma once
#include "dm.h"

int svc_thread(int num, uint64_t *regs);      // 返回 1 = 处理了，0 = 不是这一族
uint32_t svc_thread_main_init(void);          // 主线程登记，返回其句柄（交给 nn::os 当主线程句柄）
void dm_thread_returned(void);
unsigned dm_thread_waiting(int kind);       // 当前在等的线程数（kind 0=全部）                // 客体线程入口返回（违规）
void dm_guest_clock_set(uint64_t ns);         // hle_os.c 维护客体时钟（超时口径用）
uint64_t dm_guest_clock(void);
// 服务层的 SystemEvent：在客体线程上绑定 SDK 的系统事件，之后宿主任何线程都能触发
void hle_bind_system_event(void *ev);
void hle_signal_system_event(void *ev);
