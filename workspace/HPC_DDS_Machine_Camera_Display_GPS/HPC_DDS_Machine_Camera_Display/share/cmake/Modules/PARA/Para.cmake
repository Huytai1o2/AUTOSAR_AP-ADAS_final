# ================================================================================================
# Copyright 2024. PopcornSAR Co.,Ltd. All rights reserved.
#
# This software is copyright protected and proprietary to PopcornSAR Co.,Ltd.
# PopcornSAR Co.,Ltd. grants to you only those rights as set out in the license conditions.
#
# More information is available at https://www.autosar.io
# ================================================================================================

#[=======================================================================[.rst:
Para
-------

PARA Application Helper
#]=======================================================================]

#[=======================================================================[.rst:
.. command:: Para_Logo

  Print the PARA logo

  .. code-block:: cmake

    Para_Logo()
#]=======================================================================]
function(Para_Logo)
    message("")
    message("  ${WHITE}██████")
    message("  ${WHITE}██   ██ ${GREEN} █████  ${BLUE}██████  ${RED} █████")
    message("  ${WHITE}██████  ${GREEN}██   ██ ${BLUE}██   ██ ${RED}██   ██")
    message("  ${WHITE}██      ${GREEN}███████ ${BLUE}██████  ${RED}███████")
    message("  ${WHITE}██      ${GREEN}██   ██ ${BLUE}██   ██ ${RED}██   ██")
    message("${RESET}")
endfunction()

#[=======================================================================[.rst:
.. command:: Para_PrintCMakeOptions

  Print the CMake options for PARA adaptive application project

  .. code-block:: cmake

    Para_PrintCMakeOptions()
#]=======================================================================]
function(Para_PrintCMakeOptions)
    message("** [PARA] CMake options:")
    message("   - PARA_SDK ................ : ${PARA_SDK}")
    message("   - PARA version ............ : ${AUTOSAR_RELEASE}_${PARA_VERSION}")
    message("   - CMAKE_BUILD_TYPE ........ : ${CMAKE_BUILD_TYPE}")
    message("   - CMAKE_TOOLCHAIN_FILE .... : ${CMAKE_TOOLCHAIN_FILE}")
    message("   - CMAKE_INSTALL_PREFIX .... : ${CMAKE_INSTALL_PREFIX}")
    message("   - CMake version ........... : ${CMAKE_VERSION}")
endfunction()

#[=======================================================================[.rst:
.. command:: Para_PrintBuildOptions

  Print the build options for PARA adaptive application project

  .. code-block:: cmake

    Para_PrintBuildOptions()
#]=======================================================================]
function(Para_PrintBuildOptions)
    message("** [PARA] Build properties:")
    message("   - Platform ................ : ${CMAKE_SYSTEM_NAME}")
    message("   - C++ compiler ............ : ${CMAKE_CXX_COMPILER}")
    message("   - C++ compiler version .... : ${CMAKE_CXX_COMPILER_VERSION}")
    message("   - C++ standard ............ : ${CMAKE_CXX_STANDARD}")
    message("   - C++ standard library .... : ${CMAKE_CXX_STANDARD_LIBRARIES}")
    message("   - C++ flags ............... : ${CMAKE_CXX_FLAGS}")
endfunction()

#[=======================================================================[.rst:
.. command:: Para_Init

  Initialize PARA

  .. code-block:: cmake

    Para_Init()
#]=======================================================================]
macro(Para_Init)
    if (NOT DEFINED PARA)
        if (NOT WIN32)
            string(ASCII 27 ESC)
            set(RED "${ESC}[31m")
            set(GREEN "${ESC}[32m")
            set(YELLOW "${ESC}[33m")
            set(BLUE "${ESC}[34m")
            set(WHITE "${ESC}[37m")
            set(RESET "${ESC}[39m")
        endif ()

        Para_Logo()

        set(PARA para)
        set(AUTOSAR_RELEASE R20-11 CACHE STRING "AUTOSAR Release" FORCE)
        set(PARA_VERSION 25.10.31 CACHE STRING "PARA Version" FORCE)

        if (NOT CMAKE_BUILD_TYPE)
            set(CMAKE_BUILD_TYPE Release)
        endif ()

        list(APPEND CMAKE_PREFIX_PATH ${PARA_SDK};${PARA_SDK}/external)

        if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
            set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Default installation path" FORCE)
        endif ()

        set(PARA_PLATFORM_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
        set(PARA_PLATFORM_INSTALL_BINDIR ${PARA_PLATFORM_INSTALL_PREFIX}/bin)
        set(PARA_PLATFORM_INSTALL_LIBDIR ${PARA_PLATFORM_INSTALL_PREFIX}/lib)
        set(PARA_PLATFORM_INSTALL_SYSCONFDIR ${PARA_PLATFORM_INSTALL_PREFIX}/etc)
        set(PARA_PLATFORM_INSTALL_LOCALSTATEDIR ${PARA_PLATFORM_INSTALL_PREFIX}/var)

        # C++ Standard: ISO C++14
        set(CMAKE_CXX_STANDARD 14)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
        set(CMAKE_CXX_EXTENSIONS OFF)

        # DDS Implementation
        if (DEFINED PARA_DDS_IMPLEMENTATION)
            set(PARA_DDS_IMPLEMENTATIONS cyclonedds-cxx)
            list(FIND PARA_DDS_IMPLEMENTATIONS ${PARA_DDS_IMPLEMENTATION} PARA_DDS_IMPLEMENTATION_FOUND)
            if (PARA_DDS_IMPLEMENTATION_FOUND EQUAL -1)
                string(REPLACE ";" ", " PARA_DDS_IMPLEMENTATIONS "${PARA_DDS_IMPLEMENTATIONS}")
                message(FATAL_ERROR "${PARA_DDS_IMPLEMENTATION} is not in list of the supported options: [${PARA_DDS_IMPLEMENTATIONS}]")
            endif ()
        endif ()

        if (PARA_DDS_IMPLEMENTATION STREQUAL cyclonedds-cxx)
            find_library(_para_ddsc_shared_lib ddsc REQUIRED)
            add_library(CycloneDDS-Para::ddsc SHARED IMPORTED)
            set_target_properties(CycloneDDS-Para::ddsc
                    PROPERTIES
                    IMPORTED_LOCATION ${_para_ddsc_shared_lib}
                    INTERFACE_INCLUDE_DIRECTORIES ${PARA_SDK}/external/include/ddsc
            )

            find_library(_para_ddscxx_shared_lib ddscxx REQUIRED)
            add_library(CycloneDDS-CXX-Para::ddscxx SHARED IMPORTED)
            set_target_properties(CycloneDDS-CXX-Para::ddscxx
                    PROPERTIES
                    IMPORTED_LOCATION ${_para_ddscxx_shared_lib}
                    INTERFACE_INCLUDE_DIRECTORIES ${PARA_SDK}/external/include/ddscxx
                    INTERFACE_LINK_LIBRARIES CycloneDDS-Para::ddsc
            )

            find_library(_para_idlcxx_shared_lib cycloneddsidlcxx PATHS ${PARA_SDK}/share/cyclonedds/lib NO_DEFAULT_PATH REQUIRED)
            find_program(_idlc_executable idlc PATHS ${PARA_SDK}/share/cyclonedds/bin NO_DEFAULT_PATH REQUIRED)
            add_executable(CycloneDDS::idlc IMPORTED)
            set_target_properties(CycloneDDS::idlc
                    PROPERTIES
                    IMPORTED_LOCATION ${_idlc_executable})
            include(${PARA_SDK}/share/cyclonedds/lib/cmake/CycloneDDS/idlc/Generate.cmake)
        endif ()

        Para_PrintCMakeOptions()
        Para_PrintBuildOptions()

        # Install manifests
        if (CMAKE_INSTALL_PREFIX STREQUAL PARA_SDK)
            message("${YELLOW}** [PARA] WARNING: The installation directory is set to the PARA SDK location. This may corrupt the PARA SDK.${RESET}")
        else ()
            if (EXISTS ${PARA_SDK}/bin)
                if (NOT EXISTS ${PARA_PLATFORM_INSTALL_PREFIX}/bin)
                    install(CODE "
                        file(MAKE_DIRECTORY \"${PARA_PLATFORM_INSTALL_PREFIX}/bin\")
                    ")
                endif ()
                install(CODE "
                    file(GLOB FILES \"${PARA_SDK}/bin/*\")
                    foreach(FILE \${FILES})
                        get_filename_component(FILENAME \${FILE} NAME)
                        file(CREATE_LINK \${FILE}
                             \"${PARA_PLATFORM_INSTALL_PREFIX}/bin/\${FILENAME}\"
                             SYMBOLIC)
                    endforeach()
                ")
            endif ()
            if (EXISTS ${PARA_SDK}/external/bin)
                if (NOT EXISTS ${PARA_PLATFORM_INSTALL_PREFIX}/external/bin)
                    install(CODE "
                        file(MAKE_DIRECTORY \"${PARA_PLATFORM_INSTALL_PREFIX}/external/bin\")
                    ")
                endif ()
                install(CODE "
                    file(GLOB FILES \"${PARA_SDK}/external/bin/*\")
                    foreach(FILE \${FILES})
                        get_filename_component(FILENAME \${FILE} NAME)
                        file(CREATE_LINK \${FILE}
                             \"${PARA_PLATFORM_INSTALL_PREFIX}/external/bin/\${FILENAME}\"
                             SYMBOLIC)
                    endforeach()
                ")
            endif ()
            if (EXISTS ${PARA_SDK}/etc)
                install(
                        DIRECTORY ${PARA_SDK}/etc/
                        DESTINATION ${PARA_PLATFORM_INSTALL_SYSCONFDIR}
                )
            endif ()
        endif ()
        if (EXISTS ${CMAKE_SOURCE_DIR}/config)
            install(
                    DIRECTORY ${CMAKE_SOURCE_DIR}/config/
                    DESTINATION ${PARA_PLATFORM_INSTALL_SYSCONFDIR}
            )
        endif ()

        # Install execute script
        set(PARA_EXEC_FILE para-exec.sh)
        set(PARA_EXEC_SCRIPT [=[
#!/bin/sh

SCRIPT_DIR=$(dirname "$0")
INSTALL_DIR=$(cd "$SCRIPT_DIR" 2>/dev/null && pwd -P)

# configure run environment
export PARA_CORE="$INSTALL_DIR"
export PARA_CONF="$INSTALL_DIR/etc"
export PARA_DATA="$INSTALL_DIR/var"
export PARA_APPL="$INSTALL_DIR/opt"

exec "$PARA_CORE"/bin/EM "$@"
        ]=])
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${PARA_EXEC_FILE} "${PARA_EXEC_SCRIPT}")
        install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${PARA_EXEC_FILE}
                DESTINATION ${PARA_PLATFORM_INSTALL_PREFIX}
                PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
        )
    else ()
        message(WARNING "PARA already initialized")
    endif ()
endmacro()

#[=======================================================================[.rst:
.. command:: Para_App

  Configure a PARA adaptive application

  .. code-block:: cmake

    Para_App()
#]=======================================================================]
macro(Para_App)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs)
    cmake_parse_arguments(PARAMS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT DEFINED PARA_APP_NAME)
        message(FATAL_ERROR "[PARA] ERROR: The variable 'PARA_APP_NAME' is not set")
    endif ()

    if (NOT DEFINED PARA_APP_VERSION)
        message(FATAL_ERROR "[PARA] ERROR: The variable 'PARA_APP_VERSION' is not set")
    endif ()

    message("** [PARA] Configuring the PARA adaptive application: ${PARA_APP_NAME}@${PARA_APP_VERSION}")

    set(PARA_APP_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/opt/${PARA_APP_NAME})
    set(PARA_APP_INSTALL_BINDIR ${PARA_APP_INSTALL_PREFIX}/bin)
    set(PARA_APP_INSTALL_LIBDIR ${PARA_APP_INSTALL_PREFIX}/lib)
    set(PARA_APP_INSTALL_SYSCONFDIR ${PARA_APP_INSTALL_PREFIX}/etc)
    set(PARA_APP_INSTALL_LOCALSTATEDIR ${PARA_APP_INSTALL_PREFIX}/var)

    if (DEFINED PARA_APP_GEN_DIR)
        message("   - Generated artifacts:\n      ${PARA_APP_GEN_DIR}")

        # Install generated manifests
        if (EXISTS ${PARA_APP_GEN_DIR}/manifest)
            if (DEFINED PARA_PLATFORM)
                install(
                        DIRECTORY ${PARA_APP_GEN_DIR}/manifest/
                        DESTINATION ${PARA_PLATFORM_INSTALL_SYSCONFDIR}
                )
            else ()
                install(
                        DIRECTORY ${PARA_APP_GEN_DIR}/manifest/
                        DESTINATION ${PARA_APP_INSTALL_SYSCONFDIR}
                )
            endif ()
        endif ()
    endif ()
endmacro()

#[=======================================================================[.rst:
.. command:: Para_Target

  Configure a binary target declared as add_executable() or add_library()

  .. code-block:: cmake

    Para_Target(<target>
    [[PARA_GEN_DIR <dir>]
     [OUTPUT_NAME <name>]
     [PARA_LIBS_PUBLIC [items1...]]
     [PARA_LIBS_PRIVATE [items1...]]
    ]
    )
#]=======================================================================]
function(Para_Target PARAMS_TARGET)
    set(options)
    set(oneValueArgs PARA_GEN_DIR OUTPUT_NAME)
    set(multiValueArgs PARA_LIBS_PUBLIC PARA_LIBS_PRIVATE)
    cmake_parse_arguments(PARAMS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    message("** [PARA] Configuring the target: ${PARAMS_TARGET}")

    get_target_property(TARGET_COMPILE_OPTIONS ${PARAMS_TARGET} COMPILE_OPTIONS)
    if (NOT TARGET_COMPILE_OPTIONS STREQUAL TARGET_COMPILE_OPTIONS-NOTFOUND)
        message("   - Compiler options ........ : ${TARGET_COMPILE_OPTIONS}")
    endif ()

    if (DEFINED PARAMS_PARA_GEN_DIR)
        # Include generated headers
        target_include_directories(${PARAMS_TARGET}
                PRIVATE
                ${PARAMS_PARA_GEN_DIR}/include
        )

        if (DEFINED PARA_DDS_IMPLEMENTATION)
            target_compile_definitions(${PARAMS_TARGET}
                    PRIVATE
                    PARA_DDS_IMPLEMENTATION=1
            )
        endif ()

        if (PARA_DDS_IMPLEMENTATION STREQUAL cyclonedds-cxx)
            # CycloneDDS
            file(GLOB_RECURSE IDL_FILES "${PARAMS_PARA_GEN_DIR}/idl/*.idl")
            if (IDL_FILES)
                IDLC_GENERATE_GENERIC(
                        TARGET ${PARAMS_TARGET}_idl
                        BACKEND ${_para_idlcxx_shared_lib}
                        FILES
                        ${IDL_FILES}
                        ${PARA_SDK}/include/para/com/dds/idl/dds_rpc_type.idl
                        ${PARA_SDK}/include/para/com/dds/idl/dds_service_announcement_message.idl
                        ${PARA_SDK}/include/para/com/dds/idl/dds_com_types.idl
                        INCLUDES
                        ${PARAMS_PARA_GEN_DIR}/idl
                        ${PARA_SDK}/include/para/com/dds/idl
                        WARNINGS no-implicit-extensibility
                        SUFFIXES .hpp .cpp
                )
                target_link_libraries(${PARAMS_TARGET}
                        PRIVATE
                        ${PARAMS_TARGET}_idl
                        CycloneDDS-CXX-Para::ddscxx
                )
                target_compile_definitions(${PARAMS_TARGET}
                        PRIVATE
                        PARA_DDS_IMPLEMENTATION_CYCLONEDDS_CXX=1
                )
                set_target_properties(${PARAMS_TARGET} PROPERTIES CXX_STANDARD 17)
            endif ()
        endif ()

        # Install binaries
        if (PARA_PLATFORM STREQUAL TRUE)
            install(TARGETS ${PARAMS_TARGET}
                    RUNTIME DESTINATION ${PARA_PLATFORM_INSTALL_BINDIR}
                    LIBRARY DESTINATION ${PARA_PLATFORM_INSTALL_LIBDIR}
            )
        else ()
            install(TARGETS ${PARAMS_TARGET}
                    RUNTIME DESTINATION ${PARA_APP_INSTALL_BINDIR}
                    LIBRARY DESTINATION ${PARA_APP_INSTALL_LIBDIR}
            )
        endif ()
    endif ()

    target_include_directories(${PARAMS_TARGET}
            PRIVATE
            ${PARA_SDK}/external/include
    )

    # Link dependencies as public
    if (DEFINED PARAMS_PARA_LIBS_PUBLIC)
        message("   - Public dependencies:")
        foreach (lib ${PARAMS_PARA_LIBS_PUBLIC})
            message("      ${PARA}-${lib}")
            find_package(${PARA}-${lib} REQUIRED CMAKE_FIND_ROOT_PATH_BOTH)
            target_link_libraries(${PARAMS_TARGET}
                    PUBLIC
                    ${PARA}::${lib}
            )
        endforeach ()
    endif ()

    # Link dependencies as private
    if (DEFINED PARAMS_PARA_LIBS_PRIVATE)
        message("   - Private dependencies:")
        foreach (lib ${PARAMS_PARA_LIBS_PRIVATE})
            message("      ${PARA}-${lib}")
            find_package(${PARA}-${lib} REQUIRED CMAKE_FIND_ROOT_PATH_BOTH)
            target_link_libraries(${PARAMS_TARGET}
                    PRIVATE
                    ${PARA}::${lib}
            )
        endforeach ()
    endif ()

    # Install the application
    if (DEFINED PARAMS_OUTPUT_NAME)
        set_target_properties(${PARAMS_TARGET}
                PROPERTIES
                OUTPUT_NAME ${PARAMS_OUTPUT_NAME}
        )
    endif ()
endfunction()

Para_Init()
