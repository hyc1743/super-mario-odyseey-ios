#ifndef SMO_NVN_GPU_H
#define SMO_NVN_GPU_H
#include <stdint.h>
#include "nvn_resources.h"

typedef struct {
    nvn_resource_snapshot texture;
    int32_t crop[4];
    int32_t interval;
} nvn_present_request;

/* submit returns a nonzero submission sequence only after successful encoding
 * and commit. poll returns 1 for real completion, 0 for pending, -1 for error.
 * All queues share one ordered host GPU timeline. */
typedef struct {
    uint64_t (*submit)(int count, const uint64_t *handles);
    int (*poll)(uint64_t sequence);
    uint64_t (*present)(const nvn_present_request *request);
} nvn_gpu_backend;
void nvn_gpu_install_backend(const nvn_gpu_backend *backend);
int nvn_gpu_init(void);
uint64_t nvn_gpu_submit(int count, const uint64_t *handles);
int nvn_gpu_poll(uint64_t sequence);
uint64_t nvn_gpu_present_queue(void *queue,const nvn_present_request *request);
#endif
