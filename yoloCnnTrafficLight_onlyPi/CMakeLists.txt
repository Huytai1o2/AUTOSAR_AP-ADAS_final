cmake_minimum_required(VERSION 3.16)
project(TrafficLightDetector CXX)

# ─── C++ standard ───────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Build type ─────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ─── RPi 5 (Cortex-A76 / ARMv8.2-A) optimisation flags ─────────────────────
# -mcpu=cortex-a76 enables architecture + micro-arch tuning
# -O3 + -ffast-math for throughput; remove -ffast-math if NaN safety matters
set(CMAKE_CXX_FLAGS_RELEASE
    "-O3 -march=armv8.2-a -mcpu=cortex-a76 -ffast-math -DNDEBUG"
    CACHE STRING "C++ Release flags" FORCE)

# ─── LiteRT / TFLite SDK ────────────────────────────────────────────────────
set(LITERT_SDK "${CMAKE_SOURCE_DIR}/litert_arm_sdk_native_pi")
set(LITERT_INC "${LITERT_SDK}/include")
set(LITERT_LIB "${LITERT_SDK}/lib")

# ─── OpenCV (system install: sudo apt install libopencv-dev) ─────────────────
find_package(OpenCV REQUIRED)

# ─── Threads (pthreads on Linux) ─────────────────────────────────────────────
find_package(Threads REQUIRED)

# ─── Executable ─────────────────────────────────────────────────────────────
add_executable(traffic_light_detector main.cpp)

target_include_directories(traffic_light_detector PRIVATE
    ${LITERT_INC}
    ${OpenCV_INCLUDE_DIRS}
)

target_link_directories(traffic_light_detector PRIVATE
    ${LITERT_LIB}
)

# Collect all absl .so libraries from the SDK (libtensorflow-lite.so needs them)
file(GLOB ABSL_LIBS "${LITERT_LIB}/libabsl_*.so")
foreach(lib ${ABSL_LIBS})
    get_filename_component(lib_name ${lib} NAME_WE)
    string(REPLACE "lib" "" lib_name ${lib_name})
    list(APPEND ABSL_LINK_LIBS ${lib_name})
endforeach()

target_link_libraries(traffic_light_detector PRIVATE
    ${OpenCV_LIBS}
    tensorflow-lite     # libtensorflow-lite.so from LiteRT SDK
    ${ABSL_LINK_LIBS}
    Threads::Threads
    dl
)

# ─── RPATH: find .so files next to the binary at runtime ────────────────────
set_target_properties(traffic_light_detector PROPERTIES
    BUILD_RPATH  "${LITERT_LIB}"
    INSTALL_RPATH "${LITERT_LIB}"
    INSTALL_RPATH_USE_LINK_PATH ON
)

# ─── Print summary ──────────────────────────────────────────────────────────
message(STATUS "Build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenCV ver  : ${OpenCV_VERSION}")
message(STATUS "LiteRT SDK  : ${LITERT_SDK}")
message(STATUS "CXX flags   : ${CMAKE_CXX_FLAGS_RELEASE}")
