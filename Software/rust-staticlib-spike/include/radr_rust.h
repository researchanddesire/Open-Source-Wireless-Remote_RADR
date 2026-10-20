#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Spawns the Rust runtime thread (PSRAM stack, pinned to core 1).
// Returns 0 on success, 1 if already started, negative on failure.
int radr_rust_start(void);

// Number of heartbeats the Rust runtime has emitted.
uint32_t radr_rust_heartbeats(void);

#ifdef __cplusplus
}
#endif
