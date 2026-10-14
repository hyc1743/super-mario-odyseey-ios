// 把图形层接到 HLE 接线表上：main 只 import 一个图形入口 nvnBootstrapLoader，
// 宿主接管它就等于接管整个 NVN 接口（469 个函数，机制见 STATUS.md 2.46/2.51）。
//
// 为什么必须抢在 sdk 前面：**sdk 自己导出 nvnBootstrapLoader**（0x2f05ec，184 字节），
// 它的实现要去和真 NVN 驱动（nvdrv 的 ioctl）说话 —— 在 iOS 上那条路不通。
// 装载器的解析顺序是「① 强制接管表 → ② 本模块 → ③ 别模块导出 → ④ 桩」，
// 所以把名字放进接管清单（gen_hle_force.py 的 EXPLICIT）就能先接管。
#include "hle.h"
#include "../gfx/nvn/nvn_host.h"

static const hle_impl_ent g_nvn_impl[] = {
    { "nvnBootstrapLoader", (void *)(uintptr_t)nvn_BootstrapLoader,
      "唯一的图形 import：按名字返回实现（469 项表）" },
};

static void __attribute__((constructor)) nvn_reg(void) {
    hle_impl_register(g_nvn_impl, sizeof g_nvn_impl / sizeof g_nvn_impl[0]);
}
