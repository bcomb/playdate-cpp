cmake_minimum_required(VERSION 3.19)
project("Shadertoy")

# C++ 20 and up is required in order to include the playdate headers
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)

# As this is part of the repo, we can't demonstrate this aptly, but for a full
# project, you will want to include this project in order to use the build
# system, which might looks something like this:
#
# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/submodules/playdate-cpp)

# Now we can declare our application
add_playdate_application(${PROJECT_NAME})

set(SOURCES
    src/main.cpp

    inc/Application.h
    src/Application.cpp

    inc/Shadertoy.h
    src/Shadertoy.cpp
)

target_sources(${PROJECT_NAME} PUBLIC ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_link_libraries(${PROJECT_NAME} PUBLIC pdcpp_core common)