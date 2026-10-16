#ifndef SMO_NVN_CMD_H
#define SMO_NVN_CMD_H
#include <stddef.h>
#include <stdint.h>

/* Host opcodes live in guest-supplied command memory. A handle remains valid
 * until that memory is reused; finalizing the recorder does not free handles.
 * Payloads containing values (e.g. uniform updates) are copied at record time.
 * Resource addresses retain the NVN caller-managed resource lifetime. */
enum nvn_cmd_op {
    NVN_CMD_BEGIN = 1, NVN_CMD_END, NVN_CMD_JUMP, NVN_CMD_CALL,
    NVN_CMD_COPY_BUFFER, NVN_CMD_UPDATE_BUFFER, NVN_CMD_CLEAR_BUFFER,
    NVN_CMD_BARRIER, NVN_CMD_UNSUPPORTED
};
typedef struct { uint32_t op, size; } nvn_cmd_header;
typedef struct { uint64_t src, dst, size, flags; } nvn_cmd_copy;
typedef struct { uint64_t dst, size; /* followed by copied bytes */ } nvn_cmd_update;
typedef struct { uint64_t dst, size; uint32_t value, reserved; } nvn_cmd_clear;
typedef struct { const char *name; uint64_t args[8]; } nvn_cmd_missing;
uint64_t nvn_cmd_record_missing(const char *name, const uint64_t args[8]);

/* Visits executable commands in submission order, expanding CallCommands.
 * Returning zero rejects the stream. It never performs GPU work itself. */
typedef int (*nvn_cmd_visit)(void *context, uint32_t op, const void *payload, size_t size);
int nvn_cmd_walk(uint64_t handle, nvn_cmd_visit visit, void *context);
void *nvn_cmd_alloc(void *recorder, uint32_t op, size_t payload_size);
#endif
