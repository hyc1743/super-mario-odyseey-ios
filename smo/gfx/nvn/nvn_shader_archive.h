#ifndef SMO_NVN_SHADER_ARCHIVE_H
#define SMO_NVN_SHADER_ARCHIVE_H
#include <stddef.h>
/* Indexed single-volume SMOA. Opening scans headers, never shader payloads.
 * Reads allocate a NUL-terminated buffer and verify its stored CRC32. */
int nvn_shader_archive_open(const char *path);
int nvn_shader_archive_active(void);
void *nvn_shader_archive_read(const char *name, size_t *size);
void nvn_shader_archive_close(void);
#endif
