
if(NOT "/root/workspace/tflite_build_docker/_deps/flatbuffers-subbuild/flatbuffers-populate-prefix/src/flatbuffers-populate-stamp/flatbuffers-populate-gitinfo.txt" IS_NEWER_THAN "/root/workspace/tflite_build_docker/_deps/flatbuffers-subbuild/flatbuffers-populate-prefix/src/flatbuffers-populate-stamp/flatbuffers-populate-gitclone-lastrun.txt")
  message(STATUS "Avoiding repeated git clone, stamp file is up to date: '/root/workspace/tflite_build_docker/_deps/flatbuffers-subbuild/flatbuffers-populate-prefix/src/flatbuffers-populate-stamp/flatbuffers-populate-gitclone-lastrun.txt'")
  return()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "/root/workspace/tflite_build_docker/flatbuffers"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: '/root/workspace/tflite_build_docker/flatbuffers'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "/usr/bin/git"  clone --no-checkout --progress --config "advice.detachedHead=false" "https://github.com/google/flatbuffers" "flatbuffers"
    WORKING_DIRECTORY "/root/workspace/tflite_build_docker"
    RESULT_VARIABLE error_code
    )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(STATUS "Had to git clone more than once:
          ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://github.com/google/flatbuffers'")
endif()

execute_process(
  COMMAND "/usr/bin/git"  checkout e6463926479bd6b330cbcf673f7e917803fd5831 --
  WORKING_DIRECTORY "/root/workspace/tflite_build_docker/flatbuffers"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: 'e6463926479bd6b330cbcf673f7e917803fd5831'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "/usr/bin/git"  submodule update --recursive --init 
    WORKING_DIRECTORY "/root/workspace/tflite_build_docker/flatbuffers"
    RESULT_VARIABLE error_code
    )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: '/root/workspace/tflite_build_docker/flatbuffers'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
    "/root/workspace/tflite_build_docker/_deps/flatbuffers-subbuild/flatbuffers-populate-prefix/src/flatbuffers-populate-stamp/flatbuffers-populate-gitinfo.txt"
    "/root/workspace/tflite_build_docker/_deps/flatbuffers-subbuild/flatbuffers-populate-prefix/src/flatbuffers-populate-stamp/flatbuffers-populate-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: '/root/workspace/tflite_build_docker/_deps/flatbuffers-subbuild/flatbuffers-populate-prefix/src/flatbuffers-populate-stamp/flatbuffers-populate-gitclone-lastrun.txt'")
endif()

