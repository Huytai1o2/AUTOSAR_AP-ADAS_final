# Install script for directory: /root/workspace

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/root/workspace/build/install")
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
  
                        file(MAKE_DIRECTORY "/root/workspace/build/install/bin")
                    
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  
                    file(GLOB FILES "/opt/para-sdk_raspi-raspios64_bookworm/bin/*")
                    foreach(FILE ${FILES})
                        get_filename_component(FILENAME ${FILE} NAME)
                        file(CREATE_LINK ${FILE}
                             "/root/workspace/build/install/bin/${FILENAME}"
                             SYMBOLIC)
                    endforeach()
                
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  
                        file(MAKE_DIRECTORY "/root/workspace/build/install/external/bin")
                    
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  
                    file(GLOB FILES "/opt/para-sdk_raspi-raspios64_bookworm/external/bin/*")
                    foreach(FILE ${FILES})
                        get_filename_component(FILENAME ${FILE} NAME)
                        file(CREATE_LINK ${FILE}
                             "/root/workspace/build/install/external/bin/${FILENAME}"
                             SYMBOLIC)
                    endforeach()
                
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/root/workspace/build/install/etc/")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/root/workspace/build/install/etc" TYPE DIRECTORY FILES "/opt/para-sdk_raspi-raspios64_bookworm/etc/")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/root/workspace/build/install/etc/")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/root/workspace/build/install/etc" TYPE DIRECTORY FILES "/root/workspace/config/")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/root/workspace/build/install/para-exec.sh")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/root/workspace/build/install" TYPE PROGRAM PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE FILES "/root/workspace/build/para-exec.sh")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/root/workspace/build/CM/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/root/workspace/build/ClientSensor/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/root/workspace/build/EM/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/root/workspace/build/ProviderSensor/cmake_install.cmake")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  
    file(GLOB PARA_LIBS "/opt/para-sdk_raspi-raspios64_bookworm/lib/*.so*")
    foreach(LIB ${PARA_LIBS})
        get_filename_component(LIB_NAME ${LIB} NAME)
        set(DEST "${CMAKE_INSTALL_PREFIX}/lib/${LIB_NAME}")
        execute_process(COMMAND cp -L "${LIB}" "${DEST}")
    endforeach()

endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  
    file(GLOB EXT_LIBS "/opt/para-sdk_raspi-raspios64_bookworm/external/lib/*.so*")
    foreach(LIB ${EXT_LIBS})
        get_filename_component(LIB_NAME ${LIB} NAME)
        set(DEST "${CMAKE_INSTALL_PREFIX}/lib/${LIB_NAME}")
        execute_process(COMMAND cp -L "${LIB}" "${DEST}")
    endforeach()

endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  
    foreach(PROG CM EM DM IDSM PHM SIGCM TBM)
        set(SRC "/opt/para-sdk_raspi-raspios64_bookworm/bin/${PROG}")
        set(DEST "${CMAKE_INSTALL_PREFIX}/bin/${PROG}")
        if(EXISTS "${SRC}")
            message(STATUS "Overwriting symlink with binary (dereferencing): ${PROG}")
            file(REMOVE "${DEST}")
            # Use cp -L to copy the file and dereference if it is a symlink
            execute_process(COMMAND cp -L "${SRC}" "${DEST}")
        endif()
    endforeach()

    # Install External binaries (Force copy to replace symlinks)
    foreach(PROG dlt-daemon iox-roudi)
        set(SRC "/opt/para-sdk_raspi-raspios64_bookworm/external/bin/${PROG}")
        set(DEST "${CMAKE_INSTALL_PREFIX}/external/bin/${PROG}")
        if(EXISTS "${SRC}")
            message(STATUS "Overwriting symlink with binary (dereferencing): ${PROG}")
            file(REMOVE "${DEST}")
            execute_process(COMMAND cp -L "${SRC}" "${DEST}")
        endif()
    endforeach()

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/root/workspace/build/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
