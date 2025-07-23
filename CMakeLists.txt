cmake_minimum_required(VERSION 3.10)
project(juggernaut_c_modular C)

set(CMAKE_C_STANDARD 11)

# Core sources
set(CORE_SOURCES
    core/hash_algorithms.c
    core/scheduler.c
    core/cpu/md5_simd.c
    core/cpu/bcrypt_cpu.c
)

# CLI sources
set(CLI_SOURCES
    cli/main.c
    cli/commands.c
    cli/auto_detect.c
)

add_executable(juggernaut_cli
    ${CORE_SOURCES}
    ${CLI_SOURCES}
)

# Include directories
include_directories(
    core
    core/cpu
)

# TODO: Add GPU/FPGA, AI, and cloud modules as they are implemented 