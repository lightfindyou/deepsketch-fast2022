cmake_minimum_required (VERSION 3.16)

#set(CMAKE_CXX_COMPILER clang++-13)

project(deepSketch)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_PREFIX_PATH /home/xzjin/.local/lib/python3.8/site-packages/torch)
find_package(Torch REQUIRED)
#find_package(xxhash REQUIRED)
message(STATUS "Torch_FOUND: ${Torch_FOUND}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
link_directories(/opt/intel/oneapi/lib/intel64)


SET (PROJECT_SOURCE_DIR "/home/xzjin/src/deepsketch-fast2022")
file( GLOB LIB_SOURCES ${PROJECT_SOURCE_DIR}/NGT/*.cpp)

#include_directories("${PROJECT_SOURCE_DIR}/deepsketch/NGT")
#link_directories("${PROJECT_SOURCE_DIR}/deepsketch/NGT")

add_subdirectory("${PROJECT_SOURCE_DIR}/deepsketch/NGT")



#file( GLOB LIB_HEADERS lib/*.h )
add_executable(deepsketch ${PROJECT_SOURCE_DIR}/deepsketch/deepsketch_comp.cpp
 ${PROJECT_SOURCE_DIR}/xdelta3/xdelta3.c
 ${PROJECT_SOURCE_DIR}/lz4.c
# /usr/local/include/NGT/Index.h
# ${PROJECT_SOURCE_DIR}/NGT/Tree.cpp
# ${PROJECT_SOURCE_DIR}/NGT/Thread.cpp
# ${PROJECT_SOURCE_DIR}/NGT/Index.cpp
# ${PROJECT_SOURCE_DIR}/NGT/SharedMemoryAllocator.cpp
# ${PROJECT_SOURCE_DIR}/NGT/Command.cpp
# ${PROJECT_SOURCE_DIR}/NGT/Capi.cpp
# ${PROJECT_SOURCE_DIR}/NGT/Version.cpp
# ${PROJECT_SOURCE_DIR}/NGT/Graph.cpp
# ${PROJECT_SOURCE_DIR}/NGT/MmapManager.cpp
# ${PROJECT_SOURCE_DIR}/NGT/Node.cpp
# ${PROJECT_SOURCE_DIR}/NGT/ArrayFile.cpp
)


add_dependencies(deepsketch ngt)

target_link_libraries(deepsketch "${TORCH_LIBRARIES}" "xxhash" ngt pthread)
#add_subdirectory("load_data")
#
#
#set(EXECUTABLE_NAME main)
#
#add_executable(${EXECUTABLE_NAME} src/main.cpp)
#
#
#target_link_libraries(${EXECUTABLE_NAME}
#    PRIVATE load_data
#)
#
#set_target_properties(${EXECUTABLE_NAME} PROPERTIES
#  CXX_STANDARD 20
#  CXX_STANDARD_REQUIRED YES
#)
