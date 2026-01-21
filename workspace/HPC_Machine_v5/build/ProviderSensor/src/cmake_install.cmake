# Install script for directory: /root/workspace/HPC_Machine_v5/ProviderSensor/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/root/workspace/HPC_Machine_v5/build/install")
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
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/opt/cross-pi-gcc-12.2.0-64/bin/aarch64-linux-gnu-objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/root/workspace/HPC_Machine_v5/build/install/opt/ProviderSensor/bin/ProviderSensor" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/root/workspace/HPC_Machine_v5/build/install/opt/ProviderSensor/bin/ProviderSensor")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/root/workspace/HPC_Machine_v5/build/install/opt/ProviderSensor/bin/ProviderSensor"
         RPATH "\$ORIGIN/../../../lib")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/root/workspace/HPC_Machine_v5/build/install/opt/ProviderSensor/bin/ProviderSensor")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/root/workspace/HPC_Machine_v5/build/install/opt/ProviderSensor/bin" TYPE EXECUTABLE FILES "/root/workspace/HPC_Machine_v5/build/ProviderSensor/src/ProviderSensor")
  if(EXISTS "$ENV{DESTDIR}/root/workspace/HPC_Machine_v5/build/install/opt/ProviderSensor/bin/ProviderSensor" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/root/workspace/HPC_Machine_v5/build/install/opt/ProviderSensor/bin/ProviderSensor")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/root/workspace/HPC_Machine_v5/build/install/opt/ProviderSensor/bin/ProviderSensor"
         OLD_RPATH "/opt/litert_arm_sdk_native_pi/lib:/opt/para-sdk_raspi-raspios64_bookworm/lib:"
         NEW_RPATH "\$ORIGIN/../../../lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/opt/cross-pi-gcc-12.2.0-64/bin/aarch64-linux-gnu-strip" "$ENV{DESTDIR}/root/workspace/HPC_Machine_v5/build/install/opt/ProviderSensor/bin/ProviderSensor")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/opt/ProviderSensor/bin" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES "/root/workspace/HPC_Machine_v5/ProviderSensor/src/providersensor/rootswcomponent/image.jpg")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/opt/ProviderSensor/bin" TYPE DIRECTORY PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES "/root/workspace/HPC_Machine_v5/bestYoloMobilenet_saved_model")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/etc/exec" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES "/root/workspace/HPC_Machine_v5/ProviderSensor/src/../manifest/exec/ProviderSensor.json")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/ProviderSensor" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/ProviderSensor")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/ProviderSensor"
         RPATH "\$ORIGIN/../../../lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/root/workspace/HPC_Machine_v5/build/ProviderSensor/src/ProviderSensor")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/ProviderSensor" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/ProviderSensor")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/ProviderSensor"
         OLD_RPATH "/opt/litert_arm_sdk_native_pi/lib:/opt/para-sdk_raspi-raspios64_bookworm/lib:"
         NEW_RPATH "\$ORIGIN/../../../lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/opt/cross-pi-gcc-12.2.0-64/bin/aarch64-linux-gnu-strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/ProviderSensor")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE DIRECTORY FILES "/opt/litert_arm_sdk_native_pi/lib/" FILES_MATCHING REGEX "/[^/]*\\.so[^/]*$" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endif()

