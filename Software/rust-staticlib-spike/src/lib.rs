//! Feasibility spike: a std Rust runtime linked into the Arduino RADR firmware
//! as a static library. Arduino keeps `app_main`, NimBLE, and every driver;
//! Rust runs on its own FreeRTOS-backed pthread behind a small C API.

use core::ffi::c_int;
use std::{
    sync::atomic::{AtomicBool, AtomicU32, Ordering},
    thread,
    time::{Duration, Instant},
};

use esp_idf_sys as sys;

mod psram_allocator;

#[cfg(feature = "buttplug")]
mod buttplug_probe;

/// Buttplug's embedded catalog parse needs a deep stack; the plain heartbeat
/// does not. Both stacks are allocated from PSRAM.
#[cfg(feature = "buttplug")]
const RUST_TASK_STACK_BYTES: usize = 96 * 1024;
#[cfg(not(feature = "buttplug"))]
const RUST_TASK_STACK_BYTES: usize = 16 * 1024;
const RUST_TASK_PRIORITY: usize = 5;
const RUST_TASK_CORE: c_int = 1;
const HEARTBEAT_PERIOD: Duration = Duration::from_secs(5);

static STARTED: AtomicBool = AtomicBool::new(false);
static HEARTBEATS: AtomicU32 = AtomicU32::new(0);

/// Spawns the Rust runtime thread. Returns 0 on success, 1 if it was already
/// started, or a negative value if the thread could not be created.
#[unsafe(no_mangle)]
pub extern "C" fn radr_rust_start() -> c_int {
    if STARTED.swap(true, Ordering::AcqRel) {
        return 1;
    }

    // ESP-IDF's pthread layer reads this per-caller configuration when std
    // creates the thread, which lets the stack live in PSRAM.
    let previous = unsafe { sys::esp_pthread_get_default_config() };
    let mut config = previous;
    config.stack_size = RUST_TASK_STACK_BYTES;
    config.prio = RUST_TASK_PRIORITY;
    config.pin_to_core = RUST_TASK_CORE;
    config.thread_name = c"rust".as_ptr();
    config.stack_alloc_caps = sys::MALLOC_CAP_SPIRAM | sys::MALLOC_CAP_8BIT;
    if unsafe { sys::esp_pthread_set_cfg(&config) } != sys::ESP_OK {
        STARTED.store(false, Ordering::Release);
        return -2;
    }

    let spawned = thread::Builder::new()
        .name("rust".into())
        .stack_size(RUST_TASK_STACK_BYTES)
        .spawn(run);

    unsafe { sys::esp_pthread_set_cfg(&previous) };

    match spawned {
        Ok(_) => 0,
        Err(error) => {
            println!("[rust] could not spawn runtime thread: {error}");
            STARTED.store(false, Ordering::Release);
            -1
        }
    }
}

/// Number of heartbeats the Rust runtime has emitted.
#[unsafe(no_mangle)]
pub extern "C" fn radr_rust_heartbeats() -> u32 {
    HEARTBEATS.load(Ordering::Acquire)
}

fn run() {
    log_memory("thread start");

    let runtime = match tokio::runtime::Builder::new_current_thread()
        .enable_time()
        .build()
    {
        Ok(runtime) => runtime,
        Err(error) => {
            println!("[rust] could not create Tokio runtime: {error}");
            return;
        }
    };
    log_memory("tokio runtime built");

    runtime.block_on(async {
        #[cfg(feature = "buttplug")]
        let _server = buttplug_probe::start().await;

        let started = Instant::now();
        let mut interval = tokio::time::interval(HEARTBEAT_PERIOD);
        loop {
            interval.tick().await;
            let count = HEARTBEATS.fetch_add(1, Ordering::AcqRel) + 1;
            println!(
                "[rust] heartbeat {count} uptime_ms={}",
                started.elapsed().as_millis()
            );
            log_memory("heartbeat");
        }
    });
}

pub(crate) fn log_memory(stage: &str) {
    let (internal, psram, minimum_internal, stack_headroom) = unsafe {
        (
            sys::heap_caps_get_free_size(sys::MALLOC_CAP_INTERNAL),
            sys::heap_caps_get_free_size(sys::MALLOC_CAP_SPIRAM),
            sys::heap_caps_get_minimum_free_size(sys::MALLOC_CAP_INTERNAL),
            sys::uxTaskGetStackHighWaterMark(core::ptr::null_mut()),
        )
    };
    println!(
        "[rust] memory at {stage}: internal_free={internal} psram_free={psram} \
         internal_min={minimum_internal} stack_headroom={stack_headroom}"
    );
}
