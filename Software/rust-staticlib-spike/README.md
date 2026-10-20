# Rust static library in the Arduino firmware (feasibility spike)

**Question:** can a std Rust library built on `esp-idf-sys` link into the
existing C++ Arduino firmware and run upstream Buttplug, without converting the
firmware to Rust?

**Answer:** it builds and links. A std + Tokio static library and the
unmodified upstream Buttplug server and device catalog link into the
pioarduino `development` build with no undefined or duplicate symbols. The
binding layer exactly matches the Arduino core's ESP-IDF version and
sdkconfig. Nothing has been run on hardware: the Motion Lab bench is paused,
and no RADR was attached. The limiting factor is flash size, not linking.

## Versions

| Item | Value |
| --- | --- |
| Platform | pioarduino `55.03.311` (PlatformIO Core 6.2.0 required; the 6.1.19 shim refuses it) |
| Arduino core | `framework-arduinoespressif32` 3.3.11 |
| ESP-IDF in the prebuilt libs | `v5.5.5`, commit `b774170ff46` (`framework-arduinoespressif32-libs` 5.5.5+sha.b774170ff46) |
| C toolchain | `xtensa-esp-elf` 14.2.0+20260121 |
| sdkconfig | `esp32s3/sdkconfig` (`qio_qspi` base) plus the `qio_opi` header delta (octal PSRAM, boot init) |
| Rust | `esp` toolchain (rustc 1.97.0-nightly), `esp-idf-sys` 0.37.2, Tokio 1.53.1 |
| Buttplug | `9571b3db42ee2d7b3342ab9d40eb5c9e45679444`, unmodified, same DashMap patch as PR #137 |

## How it is wired

- `esp-idf-sys` (native builder, `ESP_IDF_VERSION=v5.5.5`) builds a throwaway
  ESP-IDF project only to generate bindings. Its sdkconfig defaults are
  derived from the Arduino libs by `tools/derive_sdkconfig.py`.
  `sdkconfig.bindings` swaps in a stock partition table, which ESP-IDF needs
  only to finish that throwaway build.
- Parity was checked: all 2,174 options known to ESP-IDF match Arduino's
  values, except the partition-table override, which affects bindings only.
  ESP-IDF does not recognise 904 other options. They belong to Arduino and
  its managed components (`ARDUINO_*`, RainMaker, camera, Matter, TinyUSB,
  and so on) and do not affect the ESP-IDF ABI.
- `libradr_rust.a` contains only Rust objects. No ESP-IDF component is
  bundled; every ESP-IDF symbol resolves from Arduino's prebuilt libraries at
  the final link.
- `../scripts/rust_staticlib.py` (`post:` extra script) runs Cargo per
  PlatformIO environment and prepends the library ahead of the framework
  libraries.
- C API (`include/radr_rust.h`): `radr_rust_start()` calls
  `esp_pthread_set_cfg` (PSRAM stack, core 1, priority 5), then
  `std::thread` runs a Tokio `current_thread` runtime. The runtime prints a
  heartbeat and heap figures, and with `buttplug` it first builds
  `ButtplugServer` from the embedded catalog without any communication
  manager. A global allocator keeps every Rust allocation in PSRAM.
  `main.cpp` calls `radr_rust_start()` only under `RADR_RUST_STATICLIB`.

Environments (PlatformIO Core 6.2.0, `-t checkprogsize` so nothing uploads):

```sh
~/.platformio/penv/bin/pio run -e development_rust -t checkprogsize
~/.platformio/penv/bin/pio run -e development_rust_buttplug -t checkprogsize
~/.platformio/penv/bin/pio run -e development_rust_buttplug_small -t checkprogsize
```

## Linker findings

- **No undefined references and no duplicate symbols**, for both the
  heartbeat and Buttplug builds. pthread, newlib, heap_caps and FreeRTOS all
  resolve from the Arduino libs. There is no NimBLE in Rust: `esp32-nimble`
  is not used, so NimBLE-Arduino stays the only host.
- **Silent `atexit` interposition (no effect today).** std references
  `lstat`, which neither newlib nor the Arduino libs define. `esp-idf-sys`
  provides it in its patch object, and that object also defines a strong
  `atexit` that forwards to `esp_register_shutdown_handler`. That strong
  definition replaces FastLED's weak no-op `atexit` stub, and the linker
  reports nothing.
  - **Effect today:** none. No object in the firmware calls `atexit`, and C++
    static destructors use `__cxa_atexit` (`-fuse-cxa-atexit`).
  - **Future risk:** a library that does call `atexit` would have its handlers
    run on `esp_restart()` instead of never.
  - **Fix:** drop `esp-idf-sys` in favour of a few hand-declared externs plus
    our own `lstat` shim.
- **Weak builtins take over some libm functions.** `compiler_builtins`
  exports 220 *weak* symbols (soft float and libm). There are no
  duplicate-symbol errors. However, the Rust archive is searched first, so the
  C++ calls to `floor`, `floorf`, `ceil`, `fmod` and `roundf` now bind to
  Rust's libm port instead of newlib's strong definitions. Soft-float double
  arithmetic still comes from ROM. The results should be identical IEEE
  values, but this is unverified. Linking `-lm` ahead of the Rust library
  should restore newlib's versions (also untested).
- **`esp-idf-hal` 0.46.2 (and so `esp-idf-svc` 0.52.1) does not compile**
  against ESP-IDF 5.5.5 with the Arduino sdkconfig:
  - The `ulp.rs` bug is exposed because Arduino enables ULP.
  - `sdmmc_host_t` gained a field in 5.5.5 that hal 0.46.2 does not set.

  The hybrid does not need either crate, because Arduino owns the peripherals.
  Newer releases (0.47 / 0.53) are untested.

## Size and memory

App slot: 6,553,600 bytes (`partition.csv`). OTA cannot resize the partition
table on units already in the field. Static DRAM is `.dram0.data` + `.dram0.bss`.

| Build | App flash | Δ flash | Slot used | Static DRAM | Δ DRAM |
| --- | ---: | ---: | ---: | ---: | ---: |
| `development` (baseline) | 2,272,382 | — | 34.7% | 86,476 | — |
| `development_rust` (std + Tokio heartbeat, `opt-level=s`) | 2,532,062 | +259,680 | 38.6% | 88,156 | +1,680 |
| `development_rust_buttplug` (`opt-level=s`) | 6,241,094 | +3,968,712 | **95.2%** | 89,460 | +2,984 |
| `development_rust_buttplug_small` (`opt-level=z`, std `optimize_for_size`, no backtrace) | 5,791,390 | +3,519,008 | **88.4%** | 88,004 | +1,528 |

Per-crate attribution comes from `tools/rust_size_from_map.py`, which reads
the link map:

- The heartbeat's Rust code takes 256 KB. Of that, std is 150 KB, Tokio
  28 KB, and ~40 KB is backtrace symbolization (`rustc_demangle`, `gimli`,
  `miniz_oxide`, `addr2line`).
- With Buttplug, the Rust code takes 3.98 MB (`s`) or 3.52 MB (`z`):
  - `buttplug_server_device_config`: 0.80 MB, including 0.67 MB of embedded
    JSON
  - `buttplug_server`: 0.62 MB
- **1.75 MB (`z`) is the `jsonschema` subtree alone**: jsonschema, regex,
  fancy-regex, referencing, ICU/IDNA, and related crates. Upstream uses it at
  boot to validate the built-in catalog and to validate JSON messages. It is
  not optional upstream. Without it the image would be about 4.04 MB (≈62%
  of the slot).
- The hand-ported C++ protocols this would replace (`src/devices/buttplugio`)
  take only 70.5 KB.

RAM:

- **Internal RAM:** about 1.5–3 KB more static DRAM. The thread stack (16 KB
  for the heartbeat; 96 KB with Buttplug, per PR #137) and all Rust heap use
  PSRAM. Only the task control block and pthread bookkeeping use internal RAM.
- **PSRAM heap for Buttplug:** measured on the host (64-bit, so an upper
  bound) with a counting allocator. After the server is built, 2.68 MB stays
  live, across 9.2k blocks. Parsing and schema-validating the catalog
  briefly peaks at 4.96 MB, over 77k allocations. PR #137 already ran this
  on the RADR's 8 MB PSRAM. On-device numbers are still pending: the
  firmware logs them as `[rust] memory at ...`.

## Not verified (needs hardware)

None of the following has been tried on hardware:

- the thread starting
- the PSRAM stack
- `println!` reaching UART0 alongside `Serial`
- Tokio timer behaviour
- running alongside NimBLE and WiFi
- the real heap figures

Also, `radr_rust_start()` runs on the Arduino loop task, which then deletes
itself. ESP-IDF's TLS deletion callbacks will then run std's thread-local
destructors for that task on the idle task. These are expected to be trivial,
but this needs to be seen on hardware. Alternatively, create the task in C++
with `xTaskCreatePinnedToCoreWithCaps(..., MALLOC_CAP_SPIRAM)` and run Rust
inside it.

## Recommendation

**Choose (a), a prebuilt static library inside the existing pioarduino
build.** Moving to ESP-IDF with Arduino as a component (b) would not fix the
real constraint. The flash cost is Buttplug's own code, which is the same
under either build system. (b) would also mean migrating the build system and
revalidating Arduino libraries rebuilt from source. Its one advantage, getting
bindings and sdkconfig from a single build, is already achieved here with a
parity check.

Conditions for (a):

1. **Flash budget first.** With `opt-level=z` there is about 760 KB of
   headroom before the BLE transport and UI glue exist. Before porting, make
   `jsonschema` an optional upstream feature: validating the built-in,
   already-tested catalog at boot is redundant. That saves about 1.75 MB.
   Keep `opt-level=z` and std `optimize_for_size`.
2. **Ship the `.a` prebuilt** from CI, pinned to the pioarduino version, so
   the Build PC and existing CI do not need the esp Rust toolchain or the
   ESP-IDF clone. Add a build-time guard that compares the ESP-IDF version and
   sdkconfig hash the library was built against with the Arduino core's.
3. **Remove the latent `atexit` interposition and the libm takeover:** drop
   `esp-idf-sys` and provide our own `lstat`, and order `-lm` ahead of the
   Rust library.
4. **Start Rust from a task that never exits**, such as a C++-created
   PSRAM-stack task. Keep flash and NVS operations off that stack, because
   tasks with external-RAM stacks must not disable the cache.
