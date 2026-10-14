// 运行环境：前端在状态变化时推送（oe 服务读）
#pragma once
void dm_oe_push_notification(unsigned msg);
void dm_oe_set_focus(int st);        // 1 前台 / 2 失焦 / 3 后台（变化即入队通知 15）
void dm_oe_set_opmode(int m);        // 0 掌机 / 1 主机（变化即入队通知 30/31）
int dm_oe_opmode(void);
int dm_oe_focus(void);
void dm_oe_set_language(uint64_t lang);
