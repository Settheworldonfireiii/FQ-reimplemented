cmake_minimum_required(VERSION 3.22)
project(FQ_reimplemented)

set(CMAKE_CXX_STANDARD 14)

add_executable(FQ_reimplemented main.cpp)


find_library (
        ISAL
        NAMES isal isa-l #
        HINTS "../../../../../../usr/local/Cellar/isa-l/2.30.0" # UGLY
)

message(STATUS "ISAL:: [${ISAL}]")

if (NOT ISAL)
    message(SEND_ERROR "Did not find lib isa-l")
endif()


#target_link_directories(FQ_reimplemented ../../../../../../usr/local/Cellar/isa-l/2.30.0/lib)

#link_directories(../../../../../../usr/local/Cellar/isa-l/2.30.0/lib)

target_link_libraries(FQ_reimplemented ${ISAL})
