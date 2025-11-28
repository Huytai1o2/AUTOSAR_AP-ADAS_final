# Install script for directory: /root/workspace/tflite_build_docker/eigen/unsupported/Eigen

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

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xDevelx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/eigen3/unsupported/Eigen" TYPE FILE FILES
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/AdolcForward"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/AlignedVector3"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/ArpackSupport"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/AutoDiff"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/BVH"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/EulerAngles"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/FFT"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/IterativeSolvers"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/KroneckerProduct"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/LevenbergMarquardt"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/MatrixFunctions"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/MPRealSupport"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/NNLS"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/NonLinearOptimization"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/NumericalDiff"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/OpenGLSupport"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/Polynomials"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/SparseExtra"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/SpecialFunctions"
    "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/Splines"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xDevelx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/eigen3/unsupported/Eigen" TYPE DIRECTORY FILES "/root/workspace/tflite_build_docker/eigen/unsupported/Eigen/src" FILES_MATCHING REGEX "/[^/]*\\.h$")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/root/workspace/tflite_build_docker/_deps/eigen-build/unsupported/Eigen/CXX11/cmake_install.cmake")

endif()

