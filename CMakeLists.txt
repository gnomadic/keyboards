# CMakeLists.txt
message(STATUS "=== GC9A01 Driver Check ===")

# Check if we should build the driver
if(CONFIG_DISPLAY_GC9A01)
    message(STATUS "GC9A01 driver requested")
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/drivers/display/gc9a01.c")
        message(STATUS "Adding GC9A01 driver to build")
        target_sources(app PRIVATE drivers/display/gc9a01.c)
    else()
        message(FATAL_ERROR "GC9A01 driver file not found: ${CMAKE_CURRENT_SOURCE_DIR}/drivers/display/gc9a01.c")
    endif()
else()
    message(STATUS "GC9A01 driver not requested")
endif()