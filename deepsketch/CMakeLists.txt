cmake_minimum_required (VERSION 3.16)

#set(CMAKE_CXX_COMPILER clang++-13)

project(deepSketch)


find_package(Torch REQUIRED)
message(STATUS "Torch_FOUND: ${Torch_FOUND}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

add_executable(deepsketch deepsketch_cmp.cpp)
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
