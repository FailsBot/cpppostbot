cmake_minimum_required(VERSION 3.2.2)

project(postbot)

# todo: make some Android-specific conditional statements
option(USE_TO_STRING_HACK "Use the std::to_string hackish implementation on Android NDK" OFF)

if(MSVC)
	# not tested.
      set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -O2 -EHsc")
else()
      set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -O2 -g -std=c++1y")
      # -Wall -Wextra -Werror -pedantic -g -std=c++1y")
endif()

set(SOURCES
      "main.cpp"
      "bot_easy_api.cpp"
      "bot_network.cpp"
      "writefn_data.c"
    )

set(HEADERS
      "writefn_data.h"
      "termcolor.h"
      "bot_easy_api.h"
      "tgtypes.h"
      "json.hpp"
    )

if(USE_TO_STRING_HACK)
	set(HEADERS
		${HEADERS}
		"include/to_string/to_string.h"
	   )
	add_definitions("-DUSE_TO_STRING_HACK")
endif()

find_package(CURL REQUIRED)
add_executable(rcnbot ${SOURCES} ${HEADERS})
target_link_libraries(rcnbot curl)
