import os
Import("env")

# Use all 20 cores for parallel compilation
env.SetOption("num_jobs", 20)

# Enable ccache for ESP-IDF CMake build (where most compile time lives)
os.environ["IDF_CCACHE_ENABLE"] = "1"
os.environ["CCACHE_COMPILER_TYPE"] = "gcc"

# Also wrap SCons-level compilers for any non-CMake compilation
env.Replace(
    CC='"ccache" "${CC}"',
    CXX='"ccache" "${CXX}"',
    AS='"ccache" "${AS}"',
)
