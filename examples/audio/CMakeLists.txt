cmake_minimum_required(VERSION 3.19)
project("Audio")

# C++ 20 and up is required in order to include the playdate headers
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)


# Now we can declare our application
add_playdate_application(${PROJECT_NAME})

set(SOURCES
    src/main.cpp

    inc/Application.h
    src/Application.cpp

    inc/Audio.h
    src/Audio.cpp
)

# Add its sources, and you're good to go!
target_sources(${PROJECT_NAME} PUBLIC ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_link_libraries(${PROJECT_NAME} PUBLIC pdcpp_core common)
