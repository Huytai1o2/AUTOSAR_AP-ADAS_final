cmake_minimum_required(VERSION 3.25)
project(MyProject LANGUAGES CXX)

find_package(OpenCV REQUIRED)

# SDK prefix
set(LITERT_SDK "${CMAKE_SOURCE_DIR}/litert_arm_sdk")
include_directories(${LITERT_SDK}/include)
link_directories(${LITERT_SDK}/lib)

# Abseil (nằm cùng prefix với SDK TFLite nếu bạn đã build kèm ABSL_ENABLE_INSTALL=ON)
list(APPEND CMAKE_PREFIX_PATH "${LITERT_SDK}")
find_package(absl CONFIG REQUIRED)

add_executable(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/src/main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Thêm cấu hình debug: đặt default build type nếu chưa có và compile flags theo config
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug/Release)" FORCE)
endif()

# Compiler options: dùng generator expressions để áp dụng flags tương ứng cho Debug/Release
target_compile_options(${PROJECT_NAME} PRIVATE
  $<$<CONFIG:Debug>:-g3;-O0;-Wall;-Wextra>
  $<$<CONFIG:Release>:-O3;-DNDEBUG;-Wall;-Wextra>
)

# Thêm define để có thể kiểm tra #ifdef DEBUG trong mã
target_compile_definitions(${PROJECT_NAME} PRIVATE
  $<$<CONFIG:Debug>:DEBUG>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  ${OpenCV_LIBS}
  tensorflow-lite
  absl::log       # <-- quan trọng
  pthread dl
)

# rpath để chạy không cần export LD_LIBRARY_PATH
set_target_properties(${PROJECT_NAME} PROPERTIES
  BUILD_RPATH "${LITERT_SDK}/lib"
  INSTALL_RPATH "${LITERT_SDK}/lib"
)

# copy model into build dir so runtime path exists
configure_file(${CMAKE_SOURCE_DIR}/bestYoloMobilenet_saved_model/bestYoloMobilenet_float16.tflite
               ${CMAKE_CURRENT_BINARY_DIR}/bestYoloMobilenet_saved_model/bestYoloMobilenet_float16.tflite COPYONLY)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
