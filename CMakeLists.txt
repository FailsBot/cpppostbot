cmake_minimum_required(VERSION 3.2.2)
project(postbot)

find_package(CURL REQUIRED)

# todo: make some Android-specific conditional statements
option(USE_TO_STRING_HACK "Use the std::to_string hackish implementation on Android NDK" OFF)

if(USE_TO_STRING_HACK)
    add_definitions(-DUSE_TO_STRING_HACK)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(rcnbot
    main.cpp
    bot_easy_api.cpp
    bot_network.cpp
    writefn_data.c
)
target_include_directories(rcnbot PRIVATE ${CURL_INCLUDE_DIRS})
target_link_libraries(rcnbot PRIVATE ${CURL_LIBRARIES})

# Enable C++14 support for msvc (@todo this should be done in cmake - use ver 3.10 or above)
if(WIN32)
    target_compile_options(rcnbot PRIVATE
        /std:c++14
        /EHsc   # Catch C++ exceptions only, extern C functions never throw a C++ exception.
    )
endif()
