"""Build the Rust static library and link it into the Arduino firmware.

Spike only: used by the `development_rust` environment. Requires the `esp`
Rust toolchain (espup) and builds `rust-staticlib-spike` with Cargo before
linking. Set `custom_rust_features` in platformio.ini to pass Cargo features
and `custom_rust_cargo_args` for extra Cargo arguments (e.g. profile tuning).
Each environment builds into its own Cargo target directory.
"""

Import("env")

import os
import shlex
import subprocess
from pathlib import Path

project_dir = Path(env.subst("$PROJECT_DIR"))
crate_dir = project_dir / "rust-staticlib-spike"
target_dir = crate_dir / "target" / env.subst("$PIOENV")
lib_dir = target_dir / "xtensa-esp32s3-espidf" / "release"
features = env.GetProjectOption("custom_rust_features", "").strip()
extra_args = shlex.split(env.GetProjectOption("custom_rust_cargo_args", ""))

command = ["cargo", "+esp", "build", "--release", "--target-dir", str(target_dir)]
if features:
    command += ["--features", features]
command += extra_args

# PlatformIO's Python environment must not leak into esp-idf-sys, which runs
# its own ESP-IDF CMake build and Python virtual environment.
cargo_env = {
    key: value
    for key, value in os.environ.items()
    if not key.startswith(("PLATFORMIO", "PIO", "IDF_", "VIRTUAL_ENV", "PYTHON"))
}

print("Rust staticlib: " + " ".join(command))
result = subprocess.run(command, cwd=crate_dir, env=cargo_env)
if result.returncode != 0:
    env.Exit(result.returncode)

# Ahead of the framework libraries so the ESP-IDF symbols Rust needs
# (pthread, newlib, heap_caps, FreeRTOS) resolve from them.
env.Prepend(LIBPATH=[str(lib_dir)], LIBS=["radr_rust"])
env.Depends(
    os.path.join("$BUILD_DIR", "${PROGNAME}.elf"), str(lib_dir / "libradr_rust.a")
)
