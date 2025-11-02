cmake_minimum_required(VERSION 3.19)
project("common")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)

set(SOURCES
    inc/playdate_cpp_app.hcpp

    inc/SimpleMath.h

    inc/ImageLoader.h
    src/ImageLoader.cpp
    inc/SvgLoader.h
    src/SvgLoader.cpp
    inc/Globals.h
    src/Globals.cpp

    inc/SoundFx.h
    src/SoundFx.cpp
)

# static lib
add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_link_libraries(${PROJECT_NAME} PUBLIC pdcpp_core)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)