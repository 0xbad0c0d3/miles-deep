cmake_minimum_required(VERSION 3.0)
project(miles-deep)

set(CMAKE_CXX_FLAGS "-std=c++11 -Wno-sign-compare -Wall -pthread -fPIC -O2")

if (NOT APPLE)
    link_directories(/usr/lib/x86_64-linux-gnu/)
endif ()

add_definitions(-DUSE_OPENCV)
add_definitions(-DNDEBUG)
add_definitions(-DCPU_ONLY)


if (NOT APPLE)
    set(BLAS Open)
else ()
    find_library(VEC_LIBRARY NAMES vecLib)
    if (NOT VEC_LIBRARY)
        message(FATAL_ERROR "libvec not found")
    else ()
        message("libvec found ${VEC_LIBRARY}")
        include_directories(${VEC_LIBRARY}/Headers)
    endif ()
endif ()

include(Libraries.cmake)

set(BUILD_python OFF)
set(CPU_ONLY YES)
set(USE_LEVELDB 0)
set(USE_LMDB 0)

add_subdirectory(caffe)

include_directories(caffe/include ${CMAKE_CURRENT_BINARY_DIR}/caffe/include .)
file(GLOB CLASSES classes/*.cpp)
file(GLOB SRCFILES *.cpp)
list(APPEND SRCFILES ${CLASSES})

add_executable(miles-deep ${SRCFILES})
find_file(INOTIFY sys/inotify.h)
if (INOTIFY)
    target_compile_definitions(miles-deep -DHAS_INOTIFY_H)
else ()
    message("We don't have inotify ((")
endif ()
target_link_libraries(miles-deep caffe m stdc++ ${MILES_DEEP_LIBRARIES})

add_dependencies(miles-deep caffe)

set_target_properties(miles-deep
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")