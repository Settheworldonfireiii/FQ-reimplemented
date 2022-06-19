cmake_minimum_required(VERSION 3.22)
project(FQ_reimplemented)

set(CMAKE_CXX_STANDARD 14)

add_executable(FQ_reimplemented main.cpp)


find_library (
        ISAL
        NAMES isal isa-l #
)

message(STATUS "ISAL:: [${ISAL}]")

if (NOT ISAL)
    message(SEND_ERROR "Did not find lib isa-l")
endif()

#[[
find_path(
        ISALINCLUDE
        NAMES igzip_lib.h #
        PATHS /usr/local/Cellar/isa-l/2.30.0/include/isa-l/
)
]]


find_path(
        ISALINCLUDE
        NAMES igzip_lib.h #
        HINTS /include/isa-l/
)


message(STATUS "ISALNCLUDE => ${ISALINCLUDE}")

include_directories(${ISALINCLUDE})

find_path(
        LIBDEFLATEINCLUDE
        NAMES libdeflate.h #
        HINTS /libdeflate/*/include/
)


message(STATUS "LIBDEFLATEINCLUDE => ${LIBDEFLATEINCLUDEE}")

if (NOT LIBDEFLATEINCLUDE)
    message(SEND_ERROR "Did not find include libdeflate")
endif()

include_directories(${LIBDEFLATEINCLUDE})



#file(GLOB_RECURSE ISAL_INCLUDE_DIR "/filesystem/usr/local/Cellar/isa-l/2.30.0/include/isa-l/igzip_lib.h")

target_link_libraries(FQ_reimplemented ${ISAL})
