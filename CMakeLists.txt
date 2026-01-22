cmake_minimum_required(VERSION 3.10)
project(tinyhook)

option(TINYHOOK_COMPACT "no error log output" OFF)
option(TINYHOOK_NO_EXPORT "hide all symbols" OFF)

if(TINYHOOK_COMPACT)
    add_definitions(-DCOMPACT)
endif()
if(TINYHOOK_NO_EXPORT)
    add_definitions(-DNO_EXPORT)
endif()

set(TH_SOURCES
    src/memory.c
    src/tinyhook.c
    src/interpose.c
    src/objcrt.c
    src/symsolve/symtable.c
    src/symsolve/symexport.c
    src/exhook.c
)
include_directories(src)
add_compile_options(-fvisibility=hidden -Wall -Wshadow)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

add_library(tinyhook_static STATIC ${TH_SOURCES})
target_include_directories(tinyhook_static PUBLIC include)
set_target_properties(tinyhook_static PROPERTIES OUTPUT_NAME "tinyhook")

add_library(tinyhook_shared SHARED ${TH_SOURCES})
target_include_directories(tinyhook_shared PUBLIC include)
target_link_libraries(tinyhook_shared objc)
set_target_properties(tinyhook_shared PROPERTIES OUTPUT_NAME "tinyhook")
