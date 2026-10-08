cmake_minimum_required(VERSION 3.16)

project(MedialAxisWithHoles LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE TRUE)
find_package(CGAL 6.0 REQUIRED)
find_package(GDAL REQUIRED)

# Core library: all sources in src/ (medial axis graph, pruning, I/O)
file(GLOB_RECURSE LIB_SRC CONFIGURE_DEPENDS "src/*.cpp")
add_library(medial_axis_graph_lib STATIC ${LIB_SRC})

target_include_directories(medial_axis_graph_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external
    ${PROJECT_SOURCE_DIR}/external/tomlplusplus/include
)

target_link_libraries(medial_axis_graph_lib PUBLIC
    CGAL::CGAL
    GDAL::GDAL
)

# Command line tool
add_executable(MedialAxisWithHoles apps/main.cpp)
target_link_libraries(MedialAxisWithHoles PRIVATE medial_axis_graph_lib)
