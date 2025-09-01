# CMakeLists.txt - Debug version
message(STATUS "=== Debug: CMakeLists.txt executing ===")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/drivers/display/gc9a01.c")
    message(STATUS "✅ gc9a01.c EXISTS at expected location")
else()
    message(STATUS "❌ gc9a01.c NOT FOUND")
    message(STATUS "Looking in: ${CMAKE_CURRENT_SOURCE_DIR}/drivers/display/")
endif()

# Only add if file exists AND config is enabled
if(CONFIG_DISPLAY_GC9A01 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/drivers/display/gc9a01.c")
    message(STATUS "Adding gc9a01.c to build")
    zephyr_library_sources(drivers/display/gc9a01.c)
else()
    message(STATUS "NOT adding gc9a01.c - either config disabled or file missing")
    message(STATUS "CONFIG_DISPLAY_GC9A01: ${CONFIG_DISPLAY_GC9A01}")
endif()