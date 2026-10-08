cmake_minimum_required(VERSION 3.3)

project(MedialAxisWithHoles)


option(CXX "enable C++ compilation" ON)

if (CXX)
    enable_language(CXX)
endif ()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Visual Studio compiler with static runtime libraries
if (MSVC AND MT)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
endif ()

find_package(CGAL REQUIRED)
find_package(GDAL REQUIRED)

# Collect all .cpp files in src/io and all subdirectories
file(GLOB IO_SRC "src/io/*.cpp")
# Collect all .cpp files in src/pruning and all subdirectories
file(GLOB PRUNING_SRC "src/pruning/*.cpp")

set(SRC_FILES
    src/main.cpp
    src/MedialAxisWithHoles.cpp
    src/run_medial_axis.cpp
    ${IO_SRC}
    ${PRUNING_SRC}
)

# Add the executable
add_executable(${CMAKE_PROJECT_NAME} ${SRC_FILES})

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

#link CGAL
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE 
        CGAL::CGAL
        GDAL::GDAL
)

set(CGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE TRUE)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE 
        # CGAL_VERBOSE
        # CGAL_CHECK_EXPENSIVE
        # CGAL_CHECK_EXACTNESS
        # NDEBUG
        # CGAL_NO_ASSERTIONS
        # CGAL_NO_PRECONDITIONS
        # CGAL_NO_POSTCONDITIONS
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE 
                           ${PROJECT_SOURCE_DIR}/external/tomlplusplus/include
                           ${PROJECT_SOURCE_DIR}/external)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    include(FeatureSummary)
    feature_summary(WHAT ALL)
endif ()
