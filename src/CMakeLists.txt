cmake_minimum_required(VERSION 3.4.1)
project(DPFPIR)

enable_language(C CXX)
set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig)
pkg_check_modules(DPU QUIET dpu)

if(DPU_FOUND)
    message(STATUS "DPU support enabled")
    set(ENABLE_PIM ON)
    add_definitions(-DENABLE_PIM)
else()
    message(WARNING "DPU package not found, disabling DPU support")
    set(ENABLE_PIM OFF)
endif()

set(SRCS
    ../prf/AES.cpp
    ../util/Defines.cpp
    ../util/Log.cpp
    ../prf/PRNG.cpp
    ../dpf/dpf.cpp
    datastore.cpp)

set(CMAKE_C_FLAGS "-ffunction-sections -Wall -maes -msse2 -msse4.1 -mavx2 -mpclmul -Wfatal-errors -pthread -Wno-strict-overflow -fopenmp -fPIC -Wno-ignored-attributes")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++14 -O3")

set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -ggdb -rdynamic")
set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g -ggdb -rdynamic")


add_executable(cpu_bench ${SRCS} cpu_bench.cpp)
target_link_libraries(cpu_bench crypto)

if(ENABLE_PIM)
    add_executable(pim_bench ${SRCS} pim_bench.cpp)
    target_link_libraries       (pim_bench crypto ${DPU_LIBRARIES})
    target_include_directories  (pim_bench PUBLIC ${DPU_INCLUDE_DIRS})
    target_compile_options      (pim_bench PUBLIC ${DPU_CFLAGS_OTHER})
endif()

add_executable(tests ${SRCS} test.cpp)
target_link_libraries(tests crypto)
if(ENABLE_PIM)
    target_link_libraries       (tests ${DPU_LIBRARIES})
    target_include_directories  (tests PUBLIC ${DPU_INCLUDE_DIRS})
    target_compile_options      (tests PUBLIC ${DPU_CFLAGS_OTHER})
endif()

if(ENABLE_PIM)
    set(NR_TASKLETS 12)

    set(DPU_BIN ${CMAKE_BINARY_DIR}/dpu_task)

    add_custom_command(
        OUTPUT  ${DPU_BIN}
        COMMAND dpu-upmem-dpurte-clang
                -DNR_TASKLETS=${NR_TASKLETS}
                -O2
                ${CMAKE_SOURCE_DIR}/dpu/dpu_task.c
                -o ${DPU_BIN}
        DEPENDS ${CMAKE_SOURCE_DIR}/dpu/dpu_task.c
        COMMENT "Compiling DPU kernel (record_words=${RECORD_WORDS})"
        VERBATIM)

    add_custom_target(dpu_kernel ALL DEPENDS ${DPU_BIN})

    add_dependencies(pim_bench dpu_kernel)
endif()

