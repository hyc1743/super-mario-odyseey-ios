// 【自动生成，不要手改】由 smo/tools/gen_nvn.py 从 smo/gfx/nvn/names.txt 生成。
//
// 游戏通过 nvnBootstrapLoader(名字) 按名取接口；这张表就是「名字 → 宿主实现」。
// 未实现的名字统一指向 nvn_unimplemented（首次调用打印一行、返回 0），
// 真机日志里出现哪个名字就是下一批要补的。
#include "nvn_host.h"


uint64_t nvn_BufferBuilderGetMemoryOffset(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferBuilderGetMemoryPool(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferBuilderGetSize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferBuilderSetDefaults(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferBuilderSetDevice(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferBuilderSetStorage(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferFlushMappedRange(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferGetAddress(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferGetDebugID(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferGetMemoryOffset(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferGetMemoryPool(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferGetSize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferInvalidateMappedRange(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferMap(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_BufferSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceApplyDeferredFinalizes(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceBuilderSetDefaults(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceBuilderSetFlags(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceFinalizeCommandHandle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetCurrentTimestampInNanoseconds(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetDepthMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetImageHandle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetInteger(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetProcAddress(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetSeparateSamplerHandle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetSeparateTextureHandle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetTexelFetchHandle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetTextureHandle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetTimestampInNanoseconds(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceGetWindowOriginMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceInstallDebugCallback(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceRegisterFastClearColor(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceRegisterFastClearColori(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceRegisterFastClearColorui(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceRegisterFastClearDepth(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceSetDepthMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceSetIntermediateShaderCache(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceSetWindowOriginMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_DeviceWalkDebugDatabase(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolBuilderGetFlags(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolBuilderGetMemory(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolBuilderGetSize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolBuilderSetDefaults(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolBuilderSetDevice(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolBuilderSetFlags(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolBuilderSetStorage(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolFlushMappedRange(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolGetBufferAddress(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolGetFlags(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolGetSize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolInitializeEP13NVNmemoryPoolPK20(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolInvalidateMappedRange(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolMap(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolMapVirtual(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_MemoryPoolSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_ProgramFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_ProgramInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_ProgramSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_ProgramSetShaders(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_ProgramSetSubroutineLinkage(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueAcquireTexture(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueBuilderGetQueueMemorySize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueBuilderSetCommandFlushThreshold(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueBuilderSetCommandMemorySize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueBuilderSetComputeMemorySize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueBuilderSetControlMemorySize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueBuilderSetDefaults(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueBuilderSetDevice(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueBuilderSetFlags(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueBuilderSetQueueMemory(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueFenceSync(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueFinish(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueFlush(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueGetError(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueGetTotalCommandMemoryUsed(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueGetTotalComputeMemoryUsed(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueGetTotalControlMemoryUsed(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueuePresentTexture(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueResetMemoryUsageCounts(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueSubmitCommands(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_QueueWaitSync(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderGetBorderColor(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderGetBorderColori(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderGetBorderColorui(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderGetCompare(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderGetLodBias(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderGetLodClamp(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderGetMaxAnisotropy(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderGetMinMagFilter(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderGetReductionFilter(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderGetWrapMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetBorderColor(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetBorderColori(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetBorderColorui(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetCompare(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetDefaults(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetDevice(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetLodBias(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetLodClamp(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetMaxAnisotropy(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetMinMagFilter(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetReductionFilter(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerBuilderSetWrapMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerCompare(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetBorderColor(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetBorderColori(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetBorderColorui(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetCompare(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetDebugID(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetLodBias(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetLodClamp(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetMaxAnisotropy(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetMinMagFilter(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetReductionFilter(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerGetWrapMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerPoolFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerPoolGetMemoryOffset(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerPoolGetMemoryPool(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerPoolGetSize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerPoolInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerPoolRegisterSampler(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerPoolRegisterSamplerBuilder(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerPoolSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SamplerSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SyncFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SyncInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SyncSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_SyncWait(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetDepth(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetDepthStencilMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetFlags(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetFormat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetGLTextureName(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetHeight(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetLevels(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetMemoryOffset(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetMemoryPool(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetPackagedTextureData(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetSamples(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetSparseTileLayout(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetStorageAlignment(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetStorageClass(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetStorageSize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetStride(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetSwizzle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetTarget(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetWidth(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderGetZCullStorageSize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetDefaults(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetDepth(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetDepthStencilMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetDevice(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetFlags(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetFormat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetGLTextureName(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetHeight(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetLevels(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetPackagedTextureData(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetPackagedTextureLayout(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetSamples(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetSize1D(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetSize2D(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetSize3D(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetStorage(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetStride(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetSwizzle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetTarget(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureBuilderSetWidth(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureCompare(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureFlushTexels(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetDebugID(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetDepth(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetDepthStencilMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetFlags(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetFormat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetHeight(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetLevels(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetMemoryOffset(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetMemoryPool(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetSamples(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetSparseTileLayout(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetStorageClass(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetStorageSize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetStride(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetSwizzle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetTarget(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetTextureAddress(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetViewOffset(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetWidth(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureGetZCullStorageSize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureInvalidateTexels(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TexturePoolFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TexturePoolGetMemoryOffset(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TexturePoolGetMemoryPool(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TexturePoolGetSize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TexturePoolInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TexturePoolRegisterImage(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TexturePoolRegisterTexture(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TexturePoolSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureReadTexels(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureReadTexelsStrided(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewCompare(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewGetDepthStencilMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewGetFormat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewGetLayers(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewGetLevels(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewGetSwizzle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewGetTarget(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewSetDefaults(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewSetDepthStencilMode(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewSetFormat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewSetLayers(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewSetLevels(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewSetSwizzle(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureViewSetTarget(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureWriteTexels(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_TextureWriteTexelsStrided(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowAcquireTexture(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowBuilderGetNativeWindow(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowBuilderGetPresentInterval(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowBuilderSetDefaults(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowBuilderSetDevice(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowBuilderSetNativeWindow(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowBuilderSetPresentInterval(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowBuilderSetTextures(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowFinalize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowGetCrop(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowGetNativeWindow(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowGetPresentInterval(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowInitialize(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowSetCrop(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowSetDebugLabel(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
uint64_t nvn_WindowSetPresentInterval(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);

// 未实现：打印一次名字（含调用次数），返回 0
uint64_t nvn_unimplemented(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);

// 记录桩（生成）：把这次调用连同参数记下来，首次调用打印一行。
// 它们要按 NVN 的真实结构布局逐批换成真实现（见 gfx/README.md 的 TODO）。
const char *const g_nvn_record_names[] = {
    "nvnBufferBuilderGetMemoryOffset",
    "nvnBufferBuilderGetMemoryPool",
    "nvnBufferBuilderGetSize",
    "nvnBufferBuilderSetDefaults",
    "nvnBufferBuilderSetDevice",
    "nvnBufferBuilderSetStorage",
    "nvnBufferFinalize",
    "nvnBufferFlushMappedRange",
    "nvnBufferGetAddress",
    "nvnBufferGetDebugID",
    "nvnBufferGetMemoryOffset",
    "nvnBufferGetMemoryPool",
    "nvnBufferGetSize",
    "nvnBufferInitialize",
    "nvnBufferInvalidateMappedRange",
    "nvnBufferMap",
    "nvnBufferSetDebugLabel",
    "nvnDeviceApplyDeferredFinalizes",
    "nvnDeviceBuilderSetDefaults",
    "nvnDeviceBuilderSetFlags",
    "nvnDeviceFinalize",
    "nvnDeviceFinalizeCommandHandle",
    "nvnDeviceGetCurrentTimestampInNanoseconds",
    "nvnDeviceGetDepthMode",
    "nvnDeviceGetImageHandle",
    "nvnDeviceGetInteger",
    "nvnDeviceGetProcAddress",
    "nvnDeviceGetSeparateSamplerHandle",
    "nvnDeviceGetSeparateTextureHandle",
    "nvnDeviceGetTexelFetchHandle",
    "nvnDeviceGetTextureHandle",
    "nvnDeviceGetTimestampInNanoseconds",
    "nvnDeviceGetWindowOriginMode",
    "nvnDeviceInitialize",
    "nvnDeviceInstallDebugCallback",
    "nvnDeviceRegisterFastClearColor",
    "nvnDeviceRegisterFastClearColori",
    "nvnDeviceRegisterFastClearColorui",
    "nvnDeviceRegisterFastClearDepth",
    "nvnDeviceSetDebugLabel",
    "nvnDeviceSetDepthMode",
    "nvnDeviceSetIntermediateShaderCache",
    "nvnDeviceSetWindowOriginMode",
    "nvnDeviceWalkDebugDatabase",
    "nvnMemoryPoolBuilderGetFlags",
    "nvnMemoryPoolBuilderGetMemory",
    "nvnMemoryPoolBuilderGetSize",
    "nvnMemoryPoolBuilderSetDefaults",
    "nvnMemoryPoolBuilderSetDevice",
    "nvnMemoryPoolBuilderSetFlags",
    "nvnMemoryPoolBuilderSetStorage",
    "nvnMemoryPoolFinalize",
    "nvnMemoryPoolFlushMappedRange",
    "nvnMemoryPoolGetBufferAddress",
    "nvnMemoryPoolGetFlags",
    "nvnMemoryPoolGetSize",
    "nvnMemoryPoolInitialize",
    "nvnMemoryPoolInitializeEP13NVNmemoryPoolPK20",
    "nvnMemoryPoolInvalidateMappedRange",
    "nvnMemoryPoolMap",
    "nvnMemoryPoolMapVirtual",
    "nvnMemoryPoolSetDebugLabel",
    "nvnProgramFinalize",
    "nvnProgramInitialize",
    "nvnProgramSetDebugLabel",
    "nvnProgramSetShaders",
    "nvnProgramSetSubroutineLinkage",
    "nvnQueueAcquireTexture",
    "nvnQueueBuilderGetQueueMemorySize",
    "nvnQueueBuilderSetCommandFlushThreshold",
    "nvnQueueBuilderSetCommandMemorySize",
    "nvnQueueBuilderSetComputeMemorySize",
    "nvnQueueBuilderSetControlMemorySize",
    "nvnQueueBuilderSetDefaults",
    "nvnQueueBuilderSetDevice",
    "nvnQueueBuilderSetFlags",
    "nvnQueueBuilderSetQueueMemory",
    "nvnQueueFenceSync",
    "nvnQueueFinalize",
    "nvnQueueFinish",
    "nvnQueueFlush",
    "nvnQueueGetError",
    "nvnQueueGetTotalCommandMemoryUsed",
    "nvnQueueGetTotalComputeMemoryUsed",
    "nvnQueueGetTotalControlMemoryUsed",
    "nvnQueueInitialize",
    "nvnQueuePresentTexture",
    "nvnQueueResetMemoryUsageCounts",
    "nvnQueueSetDebugLabel",
    "nvnQueueSubmitCommands",
    "nvnQueueWaitSync",
    "nvnSamplerBuilderGetBorderColor",
    "nvnSamplerBuilderGetBorderColori",
    "nvnSamplerBuilderGetBorderColorui",
    "nvnSamplerBuilderGetCompare",
    "nvnSamplerBuilderGetLodBias",
    "nvnSamplerBuilderGetLodClamp",
    "nvnSamplerBuilderGetMaxAnisotropy",
    "nvnSamplerBuilderGetMinMagFilter",
    "nvnSamplerBuilderGetReductionFilter",
    "nvnSamplerBuilderGetWrapMode",
    "nvnSamplerBuilderSetBorderColor",
    "nvnSamplerBuilderSetBorderColori",
    "nvnSamplerBuilderSetBorderColorui",
    "nvnSamplerBuilderSetCompare",
    "nvnSamplerBuilderSetDefaults",
    "nvnSamplerBuilderSetDevice",
    "nvnSamplerBuilderSetLodBias",
    "nvnSamplerBuilderSetLodClamp",
    "nvnSamplerBuilderSetMaxAnisotropy",
    "nvnSamplerBuilderSetMinMagFilter",
    "nvnSamplerBuilderSetReductionFilter",
    "nvnSamplerBuilderSetWrapMode",
    "nvnSamplerCompare",
    "nvnSamplerFinalize",
    "nvnSamplerGetBorderColor",
    "nvnSamplerGetBorderColori",
    "nvnSamplerGetBorderColorui",
    "nvnSamplerGetCompare",
    "nvnSamplerGetDebugID",
    "nvnSamplerGetLodBias",
    "nvnSamplerGetLodClamp",
    "nvnSamplerGetMaxAnisotropy",
    "nvnSamplerGetMinMagFilter",
    "nvnSamplerGetReductionFilter",
    "nvnSamplerGetWrapMode",
    "nvnSamplerInitialize",
    "nvnSamplerPoolFinalize",
    "nvnSamplerPoolGetMemoryOffset",
    "nvnSamplerPoolGetMemoryPool",
    "nvnSamplerPoolGetSize",
    "nvnSamplerPoolInitialize",
    "nvnSamplerPoolRegisterSampler",
    "nvnSamplerPoolRegisterSamplerBuilder",
    "nvnSamplerPoolSetDebugLabel",
    "nvnSamplerSetDebugLabel",
    "nvnSyncFinalize",
    "nvnSyncInitialize",
    "nvnSyncSetDebugLabel",
    "nvnSyncWait",
    "nvnTextureBuilderGetDepth",
    "nvnTextureBuilderGetDepthStencilMode",
    "nvnTextureBuilderGetFlags",
    "nvnTextureBuilderGetFormat",
    "nvnTextureBuilderGetGLTextureName",
    "nvnTextureBuilderGetHeight",
    "nvnTextureBuilderGetLevels",
    "nvnTextureBuilderGetMemoryOffset",
    "nvnTextureBuilderGetMemoryPool",
    "nvnTextureBuilderGetPackagedTextureData",
    "nvnTextureBuilderGetSamples",
    "nvnTextureBuilderGetSparseTileLayout",
    "nvnTextureBuilderGetStorageAlignment",
    "nvnTextureBuilderGetStorageClass",
    "nvnTextureBuilderGetStorageSize",
    "nvnTextureBuilderGetStride",
    "nvnTextureBuilderGetSwizzle",
    "nvnTextureBuilderGetTarget",
    "nvnTextureBuilderGetWidth",
    "nvnTextureBuilderGetZCullStorageSize",
    "nvnTextureBuilderSetDefaults",
    "nvnTextureBuilderSetDepth",
    "nvnTextureBuilderSetDepthStencilMode",
    "nvnTextureBuilderSetDevice",
    "nvnTextureBuilderSetFlags",
    "nvnTextureBuilderSetFormat",
    "nvnTextureBuilderSetGLTextureName",
    "nvnTextureBuilderSetHeight",
    "nvnTextureBuilderSetLevels",
    "nvnTextureBuilderSetPackagedTextureData",
    "nvnTextureBuilderSetPackagedTextureLayout",
    "nvnTextureBuilderSetSamples",
    "nvnTextureBuilderSetSize1D",
    "nvnTextureBuilderSetSize2D",
    "nvnTextureBuilderSetSize3D",
    "nvnTextureBuilderSetStorage",
    "nvnTextureBuilderSetStride",
    "nvnTextureBuilderSetSwizzle",
    "nvnTextureBuilderSetTarget",
    "nvnTextureBuilderSetWidth",
    "nvnTextureCompare",
    "nvnTextureFinalize",
    "nvnTextureFlushTexels",
    "nvnTextureGetDebugID",
    "nvnTextureGetDepth",
    "nvnTextureGetDepthStencilMode",
    "nvnTextureGetFlags",
    "nvnTextureGetFormat",
    "nvnTextureGetHeight",
    "nvnTextureGetLevels",
    "nvnTextureGetMemoryOffset",
    "nvnTextureGetMemoryPool",
    "nvnTextureGetSamples",
    "nvnTextureGetSparseTileLayout",
    "nvnTextureGetStorageClass",
    "nvnTextureGetStorageSize",
    "nvnTextureGetStride",
    "nvnTextureGetSwizzle",
    "nvnTextureGetTarget",
    "nvnTextureGetTextureAddress",
    "nvnTextureGetViewOffset",
    "nvnTextureGetWidth",
    "nvnTextureGetZCullStorageSize",
    "nvnTextureInitialize",
    "nvnTextureInvalidateTexels",
    "nvnTexturePoolFinalize",
    "nvnTexturePoolGetMemoryOffset",
    "nvnTexturePoolGetMemoryPool",
    "nvnTexturePoolGetSize",
    "nvnTexturePoolInitialize",
    "nvnTexturePoolRegisterImage",
    "nvnTexturePoolRegisterTexture",
    "nvnTexturePoolSetDebugLabel",
    "nvnTextureReadTexels",
    "nvnTextureReadTexelsStrided",
    "nvnTextureSetDebugLabel",
    "nvnTextureViewCompare",
    "nvnTextureViewGetDepthStencilMode",
    "nvnTextureViewGetFormat",
    "nvnTextureViewGetLayers",
    "nvnTextureViewGetLevels",
    "nvnTextureViewGetSwizzle",
    "nvnTextureViewGetTarget",
    "nvnTextureViewSetDefaults",
    "nvnTextureViewSetDepthStencilMode",
    "nvnTextureViewSetFormat",
    "nvnTextureViewSetLayers",
    "nvnTextureViewSetLevels",
    "nvnTextureViewSetSwizzle",
    "nvnTextureViewSetTarget",
    "nvnTextureWriteTexels",
    "nvnTextureWriteTexelsStrided",
    "nvnWindowAcquireTexture",
    "nvnWindowBuilderGetNativeWindow",
    "nvnWindowBuilderGetPresentInterval",
    "nvnWindowBuilderSetDefaults",
    "nvnWindowBuilderSetDevice",
    "nvnWindowBuilderSetNativeWindow",
    "nvnWindowBuilderSetPresentInterval",
    "nvnWindowBuilderSetTextures",
    "nvnWindowFinalize",
    "nvnWindowGetCrop",
    "nvnWindowGetNativeWindow",
    "nvnWindowGetPresentInterval",
    "nvnWindowInitialize",
    "nvnWindowSetCrop",
    "nvnWindowSetDebugLabel",
    "nvnWindowSetPresentInterval",
};
const unsigned g_nvn_record_n = 248;

const nvn_ent g_nvn_table[] = {
    {"nvnBlendStateGetAdvancedMode", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateGetAdvancedNormalizedDst", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateGetAdvancedOverlap", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateGetAdvancedPremultipliedSrc", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateGetBlendEquation", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateGetBlendFunc", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateGetBlendTarget", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateSetAdvancedMode", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateSetAdvancedNormalizedDst", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateSetAdvancedOverlap", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateSetAdvancedPremultipliedSrc", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateSetBlendEquation", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateSetBlendFunc", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateSetBlendTarget", (nvn_fn)nvn_unimplemented},
    {"nvnBlendStateSetDefaults", (nvn_fn)nvn_unimplemented},
    {"nvnBootstrapLoader", (nvn_fn)nvn_unimplemented},
    {"nvnBufferBuilderGetMemoryOffset", (nvn_fn)nvn_BufferBuilderGetMemoryOffset},
    {"nvnBufferBuilderGetMemoryPool", (nvn_fn)nvn_BufferBuilderGetMemoryPool},
    {"nvnBufferBuilderGetSize", (nvn_fn)nvn_BufferBuilderGetSize},
    {"nvnBufferBuilderSetDefaults", (nvn_fn)nvn_BufferBuilderSetDefaults},
    {"nvnBufferBuilderSetDevice", (nvn_fn)nvn_BufferBuilderSetDevice},
    {"nvnBufferBuilderSetStorage", (nvn_fn)nvn_BufferBuilderSetStorage},
    {"nvnBufferFinalize", (nvn_fn)nvn_BufferFinalize},
    {"nvnBufferFlushMappedRange", (nvn_fn)nvn_BufferFlushMappedRange},
    {"nvnBufferGetAddress", (nvn_fn)nvn_BufferGetAddress},
    {"nvnBufferGetDebugID", (nvn_fn)nvn_BufferGetDebugID},
    {"nvnBufferGetMemoryOffset", (nvn_fn)nvn_BufferGetMemoryOffset},
    {"nvnBufferGetMemoryPool", (nvn_fn)nvn_BufferGetMemoryPool},
    {"nvnBufferGetSize", (nvn_fn)nvn_BufferGetSize},
    {"nvnBufferInitialize", (nvn_fn)nvn_BufferInitialize},
    {"nvnBufferInvalidateMappedRange", (nvn_fn)nvn_BufferInvalidateMappedRange},
    {"nvnBufferMap", (nvn_fn)nvn_BufferMap},
    {"nvnBufferSetDebugLabel", (nvn_fn)nvn_BufferSetDebugLabel},
    {"nvnChannelMaskStateGetChannelMask", (nvn_fn)nvn_unimplemented},
    {"nvnChannelMaskStateSetChannelMask", (nvn_fn)nvn_unimplemented},
    {"nvnChannelMaskStateSetDefaults", (nvn_fn)nvn_unimplemented},
    {"nvnColorStateGetAlphaTest", (nvn_fn)nvn_unimplemented},
    {"nvnColorStateGetBlendEnable", (nvn_fn)nvn_unimplemented},
    {"nvnColorStateGetLogicOp", (nvn_fn)nvn_unimplemented},
    {"nvnColorStateSetAlphaTest", (nvn_fn)nvn_unimplemented},
    {"nvnColorStateSetBlendEnable", (nvn_fn)nvn_unimplemented},
    {"nvnColorStateSetDefaults", (nvn_fn)nvn_unimplemented},
    {"nvnColorStateSetLogicOp", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferAddCommandMemory", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferAddControlMemory", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBarrier", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBarrierEPNS_11DrawContextENS", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBarrier_ShaderEPNS_11DrawCon", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBeginRecording", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBeginTransformFeedback", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindBlendState", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindChannelMaskState", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindColorState", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindCoverageModulationTable", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindDepthStencilState", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindImage", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindImageEPNS_11DrawContextE", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindImages", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindMultisampleState", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindPolygonState", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindProgram", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindSeparateSampler", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindSeparateSamplers", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindSeparateTexture", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindSeparateTextures", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindStorageBuffer", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindStorageBuffers", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindTexture", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindTextureEPNS_11DrawContex", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindTextures", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindTransformFeedbackBuffer", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindTransformFeedbackBuffers", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindUniformBuffer", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindUniformBuffers", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindVertexAttribState", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindVertexBuffer", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindVertexBuffers", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferBindVertexStreamState", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferCallCommands", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferClearBuffer", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferClearColor", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferClearColori", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferClearColorui", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferClearDepthStencil", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferClearTexture", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferClearTexturei", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferClearTextureui", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferCopyBufferToBuffer", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferCopyBufferToTexture", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferCopyCommands", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferCopyTextureToBuffer", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferCopyTextureToTexture", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDiscardColor", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDiscardDepthStencil", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDispatchCompute", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDispatchComputeIndirect", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDownsample", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDrawArrays", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDrawArraysIndirect", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDrawArraysInstanced", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDrawElements", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDrawElementsBaseVertex", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDrawElementsIndirect", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDrawElementsInstanced", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDrawTexture", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferDrawTransformFeedback", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferEndRecording", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferEndTransformFeedback", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferFenceSync", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferFinalize", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferGetCommandMemoryFree", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferGetCommandMemorySize", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferGetCommandMemoryUsed", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferGetControlMemoryFree", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferGetControlMemorySize", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferGetControlMemoryUsed", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferGetCopyImageStride", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferGetCopyRowStride", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferGetMemoryCallback", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferGetMemoryCallbackData", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferInitialize", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferInsertDebugMarker", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferIsRecording", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferMultiDrawArraysIndirectCount", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferMultiDrawElementsIndirectCou", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferPauseTransformFeedback", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferPopDebugGroup", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferPopDebugGroupId", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferPushDebugGroup", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferPushDebugGroupDynamic", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferPushDebugGroupStatic", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferReportCounter", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferReportValue", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferResetCounter", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferResolveDepthBuffer", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferRestoreZCullData", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferResumeTransformFeedback", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSaveZCullData", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetAlphaRef", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetBlendColor", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetConservativeRasterDilate", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetConservativeRasterEnable", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetCopyImageStride", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetCopyRowStride", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetDebugLabel", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetDepthBounds", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetDepthClamp", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetDepthRange", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetDepthRanges", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetInnerTessellationLevels", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetLineWidth", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetMemoryCallback", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetMemoryCallbackData", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetOuterTessellationLevels", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetPatchSize", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetPointSize", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetPolygonOffsetClamp", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetPrimitiveRestart", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetProgramSubroutines", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetRasterizerDiscard", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetRenderEnable", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetRenderEnableConditional", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetRenderTargets", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetSampleMask", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetSamplerPool", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetScissor", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetScissors", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetShaderScratchMemory", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetStencilMask", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetStencilRef", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetStencilValueMask", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetSubpixelPrecisionBias", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetTexturePool", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetTiledCacheAction", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetTiledCacheTileSize", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetViewport", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetViewportSwizzles", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferSetViewports", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferTiledDownsample", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferUpdateUniformBuffer", (nvn_fn)nvn_unimplemented},
    {"nvnCommandBufferWaitSync", (nvn_fn)nvn_unimplemented},
    {"nvnDebugCallbackE22NVNdebugCallbackSource20N", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateGetDepthFunc", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateGetDepthTestEnable", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateGetDepthWriteEnable", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateGetStencilFunc", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateGetStencilOp", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateGetStencilTestEnable", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateSetDefaults", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateSetDepthFunc", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateSetDepthTestEnable", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateSetDepthWriteEnable", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateSetStencilFunc", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateSetStencilOp", (nvn_fn)nvn_unimplemented},
    {"nvnDepthStencilStateSetStencilTestEnable", (nvn_fn)nvn_unimplemented},
    {"nvnDeviceApplyDeferredFinalizes", (nvn_fn)nvn_DeviceApplyDeferredFinalizes},
    {"nvnDeviceBuilderSetDefaults", (nvn_fn)nvn_DeviceBuilderSetDefaults},
    {"nvnDeviceBuilderSetFlags", (nvn_fn)nvn_DeviceBuilderSetFlags},
    {"nvnDeviceFinalize", (nvn_fn)nvn_DeviceFinalize},
    {"nvnDeviceFinalizeCommandHandle", (nvn_fn)nvn_DeviceFinalizeCommandHandle},
    {"nvnDeviceGetCurrentTimestampInNanoseconds", (nvn_fn)nvn_DeviceGetCurrentTimestampInNanoseconds},
    {"nvnDeviceGetDepthMode", (nvn_fn)nvn_DeviceGetDepthMode},
    {"nvnDeviceGetImageHandle", (nvn_fn)nvn_DeviceGetImageHandle},
    {"nvnDeviceGetInteger", (nvn_fn)nvn_DeviceGetInteger},
    {"nvnDeviceGetProcAddress", (nvn_fn)nvn_DeviceGetProcAddress},
    {"nvnDeviceGetSeparateSamplerHandle", (nvn_fn)nvn_DeviceGetSeparateSamplerHandle},
    {"nvnDeviceGetSeparateTextureHandle", (nvn_fn)nvn_DeviceGetSeparateTextureHandle},
    {"nvnDeviceGetTexelFetchHandle", (nvn_fn)nvn_DeviceGetTexelFetchHandle},
    {"nvnDeviceGetTextureHandle", (nvn_fn)nvn_DeviceGetTextureHandle},
    {"nvnDeviceGetTimestampInNanoseconds", (nvn_fn)nvn_DeviceGetTimestampInNanoseconds},
    {"nvnDeviceGetWindowOriginMode", (nvn_fn)nvn_DeviceGetWindowOriginMode},
    {"nvnDeviceInitialize", (nvn_fn)nvn_DeviceInitialize},
    {"nvnDeviceInstallDebugCallback", (nvn_fn)nvn_DeviceInstallDebugCallback},
    {"nvnDeviceRegisterFastClearColor", (nvn_fn)nvn_DeviceRegisterFastClearColor},
    {"nvnDeviceRegisterFastClearColori", (nvn_fn)nvn_DeviceRegisterFastClearColori},
    {"nvnDeviceRegisterFastClearColorui", (nvn_fn)nvn_DeviceRegisterFastClearColorui},
    {"nvnDeviceRegisterFastClearDepth", (nvn_fn)nvn_DeviceRegisterFastClearDepth},
    {"nvnDeviceSetDebugLabel", (nvn_fn)nvn_DeviceSetDebugLabel},
    {"nvnDeviceSetDepthMode", (nvn_fn)nvn_DeviceSetDepthMode},
    {"nvnDeviceSetIntermediateShaderCache", (nvn_fn)nvn_DeviceSetIntermediateShaderCache},
    {"nvnDeviceSetWindowOriginMode", (nvn_fn)nvn_DeviceSetWindowOriginMode},
    {"nvnDeviceWalkDebugDatabase", (nvn_fn)nvn_DeviceWalkDebugDatabase},
    {"nvnLoadCProcs", (nvn_fn)nvn_unimplemented},
    {"nvnMemoryPoolBuilderGetFlags", (nvn_fn)nvn_MemoryPoolBuilderGetFlags},
    {"nvnMemoryPoolBuilderGetMemory", (nvn_fn)nvn_MemoryPoolBuilderGetMemory},
    {"nvnMemoryPoolBuilderGetSize", (nvn_fn)nvn_MemoryPoolBuilderGetSize},
    {"nvnMemoryPoolBuilderSetDefaults", (nvn_fn)nvn_MemoryPoolBuilderSetDefaults},
    {"nvnMemoryPoolBuilderSetDevice", (nvn_fn)nvn_MemoryPoolBuilderSetDevice},
    {"nvnMemoryPoolBuilderSetFlags", (nvn_fn)nvn_MemoryPoolBuilderSetFlags},
    {"nvnMemoryPoolBuilderSetStorage", (nvn_fn)nvn_MemoryPoolBuilderSetStorage},
    {"nvnMemoryPoolFinalize", (nvn_fn)nvn_MemoryPoolFinalize},
    {"nvnMemoryPoolFlushMappedRange", (nvn_fn)nvn_MemoryPoolFlushMappedRange},
    {"nvnMemoryPoolGetBufferAddress", (nvn_fn)nvn_MemoryPoolGetBufferAddress},
    {"nvnMemoryPoolGetFlags", (nvn_fn)nvn_MemoryPoolGetFlags},
    {"nvnMemoryPoolGetSize", (nvn_fn)nvn_MemoryPoolGetSize},
    {"nvnMemoryPoolInitialize", (nvn_fn)nvn_MemoryPoolInitialize},
    {"nvnMemoryPoolInitializeEP13NVNmemoryPoolPK20", (nvn_fn)nvn_MemoryPoolInitializeEP13NVNmemoryPoolPK20},
    {"nvnMemoryPoolInvalidateMappedRange", (nvn_fn)nvn_MemoryPoolInvalidateMappedRange},
    {"nvnMemoryPoolMap", (nvn_fn)nvn_MemoryPoolMap},
    {"nvnMemoryPoolMapVirtual", (nvn_fn)nvn_MemoryPoolMapVirtual},
    {"nvnMemoryPoolSetDebugLabel", (nvn_fn)nvn_MemoryPoolSetDebugLabel},
    {"nvnMultisampleStateGetAlphaToCoverageDither", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateGetAlphaToCoverageEnable", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateGetCoverageModulationMode", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateGetCoverageToColorEnable", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateGetCoverageToColorOutput", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateGetMultisampleEnable", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateGetRasterSamples", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateGetSampleLocationsEnable", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateGetSampleLocationsGrid", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateGetSampleLocationsGridEna", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateGetSamples", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetAlphaToCoverageDither", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetAlphaToCoverageEnable", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetCoverageModulationMode", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetCoverageToColorEnable", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetCoverageToColorOutput", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetDefaults", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetMultisampleEnable", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetRasterSamples", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetSampleLocations", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetSampleLocationsEnable", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetSampleLocationsGridEna", (nvn_fn)nvn_unimplemented},
    {"nvnMultisampleStateSetSamples", (nvn_fn)nvn_unimplemented},
    {"nvnPolygonStateGetCullFace", (nvn_fn)nvn_unimplemented},
    {"nvnPolygonStateGetFrontFace", (nvn_fn)nvn_unimplemented},
    {"nvnPolygonStateGetPolygonMode", (nvn_fn)nvn_unimplemented},
    {"nvnPolygonStateGetPolygonOffsetEnables", (nvn_fn)nvn_unimplemented},
    {"nvnPolygonStateSetCullFace", (nvn_fn)nvn_unimplemented},
    {"nvnPolygonStateSetDefaults", (nvn_fn)nvn_unimplemented},
    {"nvnPolygonStateSetFrontFace", (nvn_fn)nvn_unimplemented},
    {"nvnPolygonStateSetPolygonMode", (nvn_fn)nvn_unimplemented},
    {"nvnPolygonStateSetPolygonOffsetEnables", (nvn_fn)nvn_unimplemented},
    {"nvnProgramFinalize", (nvn_fn)nvn_ProgramFinalize},
    {"nvnProgramInitialize", (nvn_fn)nvn_ProgramInitialize},
    {"nvnProgramSetDebugLabel", (nvn_fn)nvn_ProgramSetDebugLabel},
    {"nvnProgramSetShaders", (nvn_fn)nvn_ProgramSetShaders},
    {"nvnProgramSetSubroutineLinkage", (nvn_fn)nvn_ProgramSetSubroutineLinkage},
    {"nvnQueueAcquireTexture", (nvn_fn)nvn_QueueAcquireTexture},
    {"nvnQueueBuilderGetQueueMemorySize", (nvn_fn)nvn_QueueBuilderGetQueueMemorySize},
    {"nvnQueueBuilderSetCommandFlushThreshold", (nvn_fn)nvn_QueueBuilderSetCommandFlushThreshold},
    {"nvnQueueBuilderSetCommandMemorySize", (nvn_fn)nvn_QueueBuilderSetCommandMemorySize},
    {"nvnQueueBuilderSetComputeMemorySize", (nvn_fn)nvn_QueueBuilderSetComputeMemorySize},
    {"nvnQueueBuilderSetControlMemorySize", (nvn_fn)nvn_QueueBuilderSetControlMemorySize},
    {"nvnQueueBuilderSetDefaults", (nvn_fn)nvn_QueueBuilderSetDefaults},
    {"nvnQueueBuilderSetDevice", (nvn_fn)nvn_QueueBuilderSetDevice},
    {"nvnQueueBuilderSetFlags", (nvn_fn)nvn_QueueBuilderSetFlags},
    {"nvnQueueBuilderSetQueueMemory", (nvn_fn)nvn_QueueBuilderSetQueueMemory},
    {"nvnQueueFenceSync", (nvn_fn)nvn_QueueFenceSync},
    {"nvnQueueFinalize", (nvn_fn)nvn_QueueFinalize},
    {"nvnQueueFinish", (nvn_fn)nvn_QueueFinish},
    {"nvnQueueFlush", (nvn_fn)nvn_QueueFlush},
    {"nvnQueueGetError", (nvn_fn)nvn_QueueGetError},
    {"nvnQueueGetTotalCommandMemoryUsed", (nvn_fn)nvn_QueueGetTotalCommandMemoryUsed},
    {"nvnQueueGetTotalComputeMemoryUsed", (nvn_fn)nvn_QueueGetTotalComputeMemoryUsed},
    {"nvnQueueGetTotalControlMemoryUsed", (nvn_fn)nvn_QueueGetTotalControlMemoryUsed},
    {"nvnQueueInitialize", (nvn_fn)nvn_QueueInitialize},
    {"nvnQueuePresentTexture", (nvn_fn)nvn_QueuePresentTexture},
    {"nvnQueueResetMemoryUsageCounts", (nvn_fn)nvn_QueueResetMemoryUsageCounts},
    {"nvnQueueSetDebugLabel", (nvn_fn)nvn_QueueSetDebugLabel},
    {"nvnQueueSubmitCommands", (nvn_fn)nvn_QueueSubmitCommands},
    {"nvnQueueWaitSync", (nvn_fn)nvn_QueueWaitSync},
    {"nvnSamplerBuilderGetBorderColor", (nvn_fn)nvn_SamplerBuilderGetBorderColor},
    {"nvnSamplerBuilderGetBorderColori", (nvn_fn)nvn_SamplerBuilderGetBorderColori},
    {"nvnSamplerBuilderGetBorderColorui", (nvn_fn)nvn_SamplerBuilderGetBorderColorui},
    {"nvnSamplerBuilderGetCompare", (nvn_fn)nvn_SamplerBuilderGetCompare},
    {"nvnSamplerBuilderGetLodBias", (nvn_fn)nvn_SamplerBuilderGetLodBias},
    {"nvnSamplerBuilderGetLodClamp", (nvn_fn)nvn_SamplerBuilderGetLodClamp},
    {"nvnSamplerBuilderGetMaxAnisotropy", (nvn_fn)nvn_SamplerBuilderGetMaxAnisotropy},
    {"nvnSamplerBuilderGetMinMagFilter", (nvn_fn)nvn_SamplerBuilderGetMinMagFilter},
    {"nvnSamplerBuilderGetReductionFilter", (nvn_fn)nvn_SamplerBuilderGetReductionFilter},
    {"nvnSamplerBuilderGetWrapMode", (nvn_fn)nvn_SamplerBuilderGetWrapMode},
    {"nvnSamplerBuilderSetBorderColor", (nvn_fn)nvn_SamplerBuilderSetBorderColor},
    {"nvnSamplerBuilderSetBorderColori", (nvn_fn)nvn_SamplerBuilderSetBorderColori},
    {"nvnSamplerBuilderSetBorderColorui", (nvn_fn)nvn_SamplerBuilderSetBorderColorui},
    {"nvnSamplerBuilderSetCompare", (nvn_fn)nvn_SamplerBuilderSetCompare},
    {"nvnSamplerBuilderSetDefaults", (nvn_fn)nvn_SamplerBuilderSetDefaults},
    {"nvnSamplerBuilderSetDevice", (nvn_fn)nvn_SamplerBuilderSetDevice},
    {"nvnSamplerBuilderSetLodBias", (nvn_fn)nvn_SamplerBuilderSetLodBias},
    {"nvnSamplerBuilderSetLodClamp", (nvn_fn)nvn_SamplerBuilderSetLodClamp},
    {"nvnSamplerBuilderSetMaxAnisotropy", (nvn_fn)nvn_SamplerBuilderSetMaxAnisotropy},
    {"nvnSamplerBuilderSetMinMagFilter", (nvn_fn)nvn_SamplerBuilderSetMinMagFilter},
    {"nvnSamplerBuilderSetReductionFilter", (nvn_fn)nvn_SamplerBuilderSetReductionFilter},
    {"nvnSamplerBuilderSetWrapMode", (nvn_fn)nvn_SamplerBuilderSetWrapMode},
    {"nvnSamplerCompare", (nvn_fn)nvn_SamplerCompare},
    {"nvnSamplerFinalize", (nvn_fn)nvn_SamplerFinalize},
    {"nvnSamplerGetBorderColor", (nvn_fn)nvn_SamplerGetBorderColor},
    {"nvnSamplerGetBorderColori", (nvn_fn)nvn_SamplerGetBorderColori},
    {"nvnSamplerGetBorderColorui", (nvn_fn)nvn_SamplerGetBorderColorui},
    {"nvnSamplerGetCompare", (nvn_fn)nvn_SamplerGetCompare},
    {"nvnSamplerGetDebugID", (nvn_fn)nvn_SamplerGetDebugID},
    {"nvnSamplerGetLodBias", (nvn_fn)nvn_SamplerGetLodBias},
    {"nvnSamplerGetLodClamp", (nvn_fn)nvn_SamplerGetLodClamp},
    {"nvnSamplerGetMaxAnisotropy", (nvn_fn)nvn_SamplerGetMaxAnisotropy},
    {"nvnSamplerGetMinMagFilter", (nvn_fn)nvn_SamplerGetMinMagFilter},
    {"nvnSamplerGetReductionFilter", (nvn_fn)nvn_SamplerGetReductionFilter},
    {"nvnSamplerGetWrapMode", (nvn_fn)nvn_SamplerGetWrapMode},
    {"nvnSamplerInitialize", (nvn_fn)nvn_SamplerInitialize},
    {"nvnSamplerPoolFinalize", (nvn_fn)nvn_SamplerPoolFinalize},
    {"nvnSamplerPoolGetMemoryOffset", (nvn_fn)nvn_SamplerPoolGetMemoryOffset},
    {"nvnSamplerPoolGetMemoryPool", (nvn_fn)nvn_SamplerPoolGetMemoryPool},
    {"nvnSamplerPoolGetSize", (nvn_fn)nvn_SamplerPoolGetSize},
    {"nvnSamplerPoolInitialize", (nvn_fn)nvn_SamplerPoolInitialize},
    {"nvnSamplerPoolRegisterSampler", (nvn_fn)nvn_SamplerPoolRegisterSampler},
    {"nvnSamplerPoolRegisterSamplerBuilder", (nvn_fn)nvn_SamplerPoolRegisterSamplerBuilder},
    {"nvnSamplerPoolSetDebugLabel", (nvn_fn)nvn_SamplerPoolSetDebugLabel},
    {"nvnSamplerSetDebugLabel", (nvn_fn)nvn_SamplerSetDebugLabel},
    {"nvnSyncFinalize", (nvn_fn)nvn_SyncFinalize},
    {"nvnSyncInitialize", (nvn_fn)nvn_SyncInitialize},
    {"nvnSyncSetDebugLabel", (nvn_fn)nvn_SyncSetDebugLabel},
    {"nvnSyncWait", (nvn_fn)nvn_SyncWait},
    {"nvnTextureBuilderGetDepth", (nvn_fn)nvn_TextureBuilderGetDepth},
    {"nvnTextureBuilderGetDepthStencilMode", (nvn_fn)nvn_TextureBuilderGetDepthStencilMode},
    {"nvnTextureBuilderGetFlags", (nvn_fn)nvn_TextureBuilderGetFlags},
    {"nvnTextureBuilderGetFormat", (nvn_fn)nvn_TextureBuilderGetFormat},
    {"nvnTextureBuilderGetGLTextureName", (nvn_fn)nvn_TextureBuilderGetGLTextureName},
    {"nvnTextureBuilderGetHeight", (nvn_fn)nvn_TextureBuilderGetHeight},
    {"nvnTextureBuilderGetLevels", (nvn_fn)nvn_TextureBuilderGetLevels},
    {"nvnTextureBuilderGetMemoryOffset", (nvn_fn)nvn_TextureBuilderGetMemoryOffset},
    {"nvnTextureBuilderGetMemoryPool", (nvn_fn)nvn_TextureBuilderGetMemoryPool},
    {"nvnTextureBuilderGetPackagedTextureData", (nvn_fn)nvn_TextureBuilderGetPackagedTextureData},
    {"nvnTextureBuilderGetSamples", (nvn_fn)nvn_TextureBuilderGetSamples},
    {"nvnTextureBuilderGetSparseTileLayout", (nvn_fn)nvn_TextureBuilderGetSparseTileLayout},
    {"nvnTextureBuilderGetStorageAlignment", (nvn_fn)nvn_TextureBuilderGetStorageAlignment},
    {"nvnTextureBuilderGetStorageClass", (nvn_fn)nvn_TextureBuilderGetStorageClass},
    {"nvnTextureBuilderGetStorageSize", (nvn_fn)nvn_TextureBuilderGetStorageSize},
    {"nvnTextureBuilderGetStride", (nvn_fn)nvn_TextureBuilderGetStride},
    {"nvnTextureBuilderGetSwizzle", (nvn_fn)nvn_TextureBuilderGetSwizzle},
    {"nvnTextureBuilderGetTarget", (nvn_fn)nvn_TextureBuilderGetTarget},
    {"nvnTextureBuilderGetWidth", (nvn_fn)nvn_TextureBuilderGetWidth},
    {"nvnTextureBuilderGetZCullStorageSize", (nvn_fn)nvn_TextureBuilderGetZCullStorageSize},
    {"nvnTextureBuilderSetDefaults", (nvn_fn)nvn_TextureBuilderSetDefaults},
    {"nvnTextureBuilderSetDepth", (nvn_fn)nvn_TextureBuilderSetDepth},
    {"nvnTextureBuilderSetDepthStencilMode", (nvn_fn)nvn_TextureBuilderSetDepthStencilMode},
    {"nvnTextureBuilderSetDevice", (nvn_fn)nvn_TextureBuilderSetDevice},
    {"nvnTextureBuilderSetFlags", (nvn_fn)nvn_TextureBuilderSetFlags},
    {"nvnTextureBuilderSetFormat", (nvn_fn)nvn_TextureBuilderSetFormat},
    {"nvnTextureBuilderSetGLTextureName", (nvn_fn)nvn_TextureBuilderSetGLTextureName},
    {"nvnTextureBuilderSetHeight", (nvn_fn)nvn_TextureBuilderSetHeight},
    {"nvnTextureBuilderSetLevels", (nvn_fn)nvn_TextureBuilderSetLevels},
    {"nvnTextureBuilderSetPackagedTextureData", (nvn_fn)nvn_TextureBuilderSetPackagedTextureData},
    {"nvnTextureBuilderSetPackagedTextureLayout", (nvn_fn)nvn_TextureBuilderSetPackagedTextureLayout},
    {"nvnTextureBuilderSetSamples", (nvn_fn)nvn_TextureBuilderSetSamples},
    {"nvnTextureBuilderSetSize1D", (nvn_fn)nvn_TextureBuilderSetSize1D},
    {"nvnTextureBuilderSetSize2D", (nvn_fn)nvn_TextureBuilderSetSize2D},
    {"nvnTextureBuilderSetSize3D", (nvn_fn)nvn_TextureBuilderSetSize3D},
    {"nvnTextureBuilderSetStorage", (nvn_fn)nvn_TextureBuilderSetStorage},
    {"nvnTextureBuilderSetStride", (nvn_fn)nvn_TextureBuilderSetStride},
    {"nvnTextureBuilderSetSwizzle", (nvn_fn)nvn_TextureBuilderSetSwizzle},
    {"nvnTextureBuilderSetTarget", (nvn_fn)nvn_TextureBuilderSetTarget},
    {"nvnTextureBuilderSetWidth", (nvn_fn)nvn_TextureBuilderSetWidth},
    {"nvnTextureCompare", (nvn_fn)nvn_TextureCompare},
    {"nvnTextureFinalize", (nvn_fn)nvn_TextureFinalize},
    {"nvnTextureFlushTexels", (nvn_fn)nvn_TextureFlushTexels},
    {"nvnTextureGetDebugID", (nvn_fn)nvn_TextureGetDebugID},
    {"nvnTextureGetDepth", (nvn_fn)nvn_TextureGetDepth},
    {"nvnTextureGetDepthStencilMode", (nvn_fn)nvn_TextureGetDepthStencilMode},
    {"nvnTextureGetFlags", (nvn_fn)nvn_TextureGetFlags},
    {"nvnTextureGetFormat", (nvn_fn)nvn_TextureGetFormat},
    {"nvnTextureGetHeight", (nvn_fn)nvn_TextureGetHeight},
    {"nvnTextureGetLevels", (nvn_fn)nvn_TextureGetLevels},
    {"nvnTextureGetMemoryOffset", (nvn_fn)nvn_TextureGetMemoryOffset},
    {"nvnTextureGetMemoryPool", (nvn_fn)nvn_TextureGetMemoryPool},
    {"nvnTextureGetSamples", (nvn_fn)nvn_TextureGetSamples},
    {"nvnTextureGetSparseTileLayout", (nvn_fn)nvn_TextureGetSparseTileLayout},
    {"nvnTextureGetStorageClass", (nvn_fn)nvn_TextureGetStorageClass},
    {"nvnTextureGetStorageSize", (nvn_fn)nvn_TextureGetStorageSize},
    {"nvnTextureGetStride", (nvn_fn)nvn_TextureGetStride},
    {"nvnTextureGetSwizzle", (nvn_fn)nvn_TextureGetSwizzle},
    {"nvnTextureGetTarget", (nvn_fn)nvn_TextureGetTarget},
    {"nvnTextureGetTextureAddress", (nvn_fn)nvn_TextureGetTextureAddress},
    {"nvnTextureGetViewOffset", (nvn_fn)nvn_TextureGetViewOffset},
    {"nvnTextureGetWidth", (nvn_fn)nvn_TextureGetWidth},
    {"nvnTextureGetZCullStorageSize", (nvn_fn)nvn_TextureGetZCullStorageSize},
    {"nvnTextureInitialize", (nvn_fn)nvn_TextureInitialize},
    {"nvnTextureInvalidateTexels", (nvn_fn)nvn_TextureInvalidateTexels},
    {"nvnTexturePoolFinalize", (nvn_fn)nvn_TexturePoolFinalize},
    {"nvnTexturePoolGetMemoryOffset", (nvn_fn)nvn_TexturePoolGetMemoryOffset},
    {"nvnTexturePoolGetMemoryPool", (nvn_fn)nvn_TexturePoolGetMemoryPool},
    {"nvnTexturePoolGetSize", (nvn_fn)nvn_TexturePoolGetSize},
    {"nvnTexturePoolInitialize", (nvn_fn)nvn_TexturePoolInitialize},
    {"nvnTexturePoolRegisterImage", (nvn_fn)nvn_TexturePoolRegisterImage},
    {"nvnTexturePoolRegisterTexture", (nvn_fn)nvn_TexturePoolRegisterTexture},
    {"nvnTexturePoolSetDebugLabel", (nvn_fn)nvn_TexturePoolSetDebugLabel},
    {"nvnTextureReadTexels", (nvn_fn)nvn_TextureReadTexels},
    {"nvnTextureReadTexelsStrided", (nvn_fn)nvn_TextureReadTexelsStrided},
    {"nvnTextureSetDebugLabel", (nvn_fn)nvn_TextureSetDebugLabel},
    {"nvnTextureViewCompare", (nvn_fn)nvn_TextureViewCompare},
    {"nvnTextureViewGetDepthStencilMode", (nvn_fn)nvn_TextureViewGetDepthStencilMode},
    {"nvnTextureViewGetFormat", (nvn_fn)nvn_TextureViewGetFormat},
    {"nvnTextureViewGetLayers", (nvn_fn)nvn_TextureViewGetLayers},
    {"nvnTextureViewGetLevels", (nvn_fn)nvn_TextureViewGetLevels},
    {"nvnTextureViewGetSwizzle", (nvn_fn)nvn_TextureViewGetSwizzle},
    {"nvnTextureViewGetTarget", (nvn_fn)nvn_TextureViewGetTarget},
    {"nvnTextureViewSetDefaults", (nvn_fn)nvn_TextureViewSetDefaults},
    {"nvnTextureViewSetDepthStencilMode", (nvn_fn)nvn_TextureViewSetDepthStencilMode},
    {"nvnTextureViewSetFormat", (nvn_fn)nvn_TextureViewSetFormat},
    {"nvnTextureViewSetLayers", (nvn_fn)nvn_TextureViewSetLayers},
    {"nvnTextureViewSetLevels", (nvn_fn)nvn_TextureViewSetLevels},
    {"nvnTextureViewSetSwizzle", (nvn_fn)nvn_TextureViewSetSwizzle},
    {"nvnTextureViewSetTarget", (nvn_fn)nvn_TextureViewSetTarget},
    {"nvnTextureWriteTexels", (nvn_fn)nvn_TextureWriteTexels},
    {"nvnTextureWriteTexelsStrided", (nvn_fn)nvn_TextureWriteTexelsStrided},
    {"nvnVertexAttribStateGetFormat", (nvn_fn)nvn_unimplemented},
    {"nvnVertexAttribStateGetStreamIndex", (nvn_fn)nvn_unimplemented},
    {"nvnVertexAttribStateSetDefaults", (nvn_fn)nvn_unimplemented},
    {"nvnVertexAttribStateSetFormat", (nvn_fn)nvn_unimplemented},
    {"nvnVertexAttribStateSetStreamIndex", (nvn_fn)nvn_unimplemented},
    {"nvnVertexStreamStateGetDivisor", (nvn_fn)nvn_unimplemented},
    {"nvnVertexStreamStateGetStride", (nvn_fn)nvn_unimplemented},
    {"nvnVertexStreamStateSetDefaults", (nvn_fn)nvn_unimplemented},
    {"nvnVertexStreamStateSetDivisor", (nvn_fn)nvn_unimplemented},
    {"nvnVertexStreamStateSetStride", (nvn_fn)nvn_unimplemented},
    {"nvnWindowAcquireTexture", (nvn_fn)nvn_WindowAcquireTexture},
    {"nvnWindowBuilderGetNativeWindow", (nvn_fn)nvn_WindowBuilderGetNativeWindow},
    {"nvnWindowBuilderGetPresentInterval", (nvn_fn)nvn_WindowBuilderGetPresentInterval},
    {"nvnWindowBuilderSetDefaults", (nvn_fn)nvn_WindowBuilderSetDefaults},
    {"nvnWindowBuilderSetDevice", (nvn_fn)nvn_WindowBuilderSetDevice},
    {"nvnWindowBuilderSetNativeWindow", (nvn_fn)nvn_WindowBuilderSetNativeWindow},
    {"nvnWindowBuilderSetPresentInterval", (nvn_fn)nvn_WindowBuilderSetPresentInterval},
    {"nvnWindowBuilderSetTextures", (nvn_fn)nvn_WindowBuilderSetTextures},
    {"nvnWindowFinalize", (nvn_fn)nvn_WindowFinalize},
    {"nvnWindowGetCrop", (nvn_fn)nvn_WindowGetCrop},
    {"nvnWindowGetNativeWindow", (nvn_fn)nvn_WindowGetNativeWindow},
    {"nvnWindowGetPresentInterval", (nvn_fn)nvn_WindowGetPresentInterval},
    {"nvnWindowInitialize", (nvn_fn)nvn_WindowInitialize},
    {"nvnWindowSetCrop", (nvn_fn)nvn_WindowSetCrop},
    {"nvnWindowSetDebugLabel", (nvn_fn)nvn_WindowSetDebugLabel},
    {"nvnWindowSetPresentInterval", (nvn_fn)nvn_WindowSetPresentInterval},
};
const unsigned g_nvn_table_n = 469;
const unsigned g_nvn_impl_n = 248;
