cmake_minimum_required (VERSION 3.12.4)
project (Zobbot)

SET ( SOURCE 
	source/dll.cpp
	source/zobbot.cpp
	source/zobbot.h
	)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(Zobbot SHARED ${SOURCE})
#target_link_libraries(Zobbot "C:/bwapi/bwapi/lib/Release/BWAPI-Static.lib")
target_link_libraries(Zobbot "C:/bwapi/bwapi/lib/Debug/BWAPI-Staticd.lib")
target_include_directories(Zobbot PRIVATE "C:/bwapi/bwapi/include")