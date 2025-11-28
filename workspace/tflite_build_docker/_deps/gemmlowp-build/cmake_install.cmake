# Install script for directory: /root/workspace/tflite_build_docker/gemmlowp/contrib

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/root/workspace/tflite_build_docker/install")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/gemmlowp/eight_bit_int_gemm" TYPE FILE FILES "/root/workspace/tflite_build_docker/gemmlowp/eight_bit_int_gemm/eight_bit_int_gemm.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/gemmlowp/meta" TYPE FILE FILES
    "/root/workspace/tflite_build_docker/gemmlowp/meta/base.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/legacy_multi_thread_common.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/legacy_multi_thread_gemm.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/legacy_multi_thread_gemv.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/legacy_operations_common.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/legacy_single_thread_gemm.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/multi_thread_common.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/multi_thread_gemm.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/multi_thread_transform.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/quantized_mul_kernels.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/quantized_mul_kernels_arm_32.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/quantized_mul_kernels_arm_64.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/single_thread_gemm.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/single_thread_transform.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/streams.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/streams_arm_32.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/streams_arm_64.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/transform_kernels.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/transform_kernels_arm_32.h"
    "/root/workspace/tflite_build_docker/gemmlowp/meta/transform_kernels_arm_64.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/gemmlowp/public" TYPE FILE FILES
    "/root/workspace/tflite_build_docker/gemmlowp/public/bit_depth.h"
    "/root/workspace/tflite_build_docker/gemmlowp/public/gemmlowp.h"
    "/root/workspace/tflite_build_docker/gemmlowp/public/map.h"
    "/root/workspace/tflite_build_docker/gemmlowp/public/output_stages.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/gemmlowp/profiling" TYPE FILE FILES
    "/root/workspace/tflite_build_docker/gemmlowp/profiling/instrumentation.h"
    "/root/workspace/tflite_build_docker/gemmlowp/profiling/profiler.h"
    "/root/workspace/tflite_build_docker/gemmlowp/profiling/pthread_everywhere.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/gemmlowp/internal" TYPE FILE FILES
    "/root/workspace/tflite_build_docker/gemmlowp/internal/allocator.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/block_params.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/common.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/compute.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/detect_platform.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/dispatch_gemm_shape.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/kernel.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/kernel_avx.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/kernel_default.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/kernel_msa.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/kernel_neon.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/kernel_reference.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/kernel_sse.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/multi_thread_gemm.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/output.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/output_avx.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/output_msa.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/output_neon.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/output_sse.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/pack.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/pack_avx.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/pack_msa.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/pack_neon.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/pack_sse.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/platform.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/simd_wrappers.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/simd_wrappers_common_neon_sse.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/simd_wrappers_msa.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/simd_wrappers_neon.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/simd_wrappers_sse.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/single_thread_gemm.h"
    "/root/workspace/tflite_build_docker/gemmlowp/internal/unpack.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/gemmlowp/fixedpoint" TYPE FILE FILES
    "/root/workspace/tflite_build_docker/gemmlowp/fixedpoint/fixedpoint.h"
    "/root/workspace/tflite_build_docker/gemmlowp/fixedpoint/fixedpoint_avx.h"
    "/root/workspace/tflite_build_docker/gemmlowp/fixedpoint/fixedpoint_msa.h"
    "/root/workspace/tflite_build_docker/gemmlowp/fixedpoint/fixedpoint_neon.h"
    "/root/workspace/tflite_build_docker/gemmlowp/fixedpoint/fixedpoint_sse.h"
    "/root/workspace/tflite_build_docker/gemmlowp/fixedpoint/fixedpoint_wasmsimd.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libeight_bit_int_gemm.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libeight_bit_int_gemm.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libeight_bit_int_gemm.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/root/workspace/tflite_build_docker/_deps/gemmlowp-build/libeight_bit_int_gemm.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libeight_bit_int_gemm.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libeight_bit_int_gemm.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libeight_bit_int_gemm.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/gemmlowp/gemmlowp-config.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/gemmlowp/gemmlowp-config.cmake"
         "/root/workspace/tflite_build_docker/_deps/gemmlowp-build/CMakeFiles/Export/lib/cmake/gemmlowp/gemmlowp-config.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/gemmlowp/gemmlowp-config-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/gemmlowp/gemmlowp-config.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/gemmlowp" TYPE FILE FILES "/root/workspace/tflite_build_docker/_deps/gemmlowp-build/CMakeFiles/Export/lib/cmake/gemmlowp/gemmlowp-config.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/gemmlowp" TYPE FILE FILES "/root/workspace/tflite_build_docker/_deps/gemmlowp-build/CMakeFiles/Export/lib/cmake/gemmlowp/gemmlowp-config-release.cmake")
  endif()
endif()

