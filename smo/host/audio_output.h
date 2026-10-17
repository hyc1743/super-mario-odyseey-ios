#pragma once
#include <stddef.h>
#include <stdint.h>
/* Interleaved stereo PCM16 at 48 kHz. Called on the guest mixer thread;
 * consumers must copy synchronously, never retain the SDK work buffer. */
typedef void (*smo_audio_output_fn)(const int16_t *, size_t);
void smo_audio_set_output(smo_audio_output_fn output);
/* App background only. Audio interruptions keep guest pacing alive. */
void smo_audio_set_suspended(int suspended);
