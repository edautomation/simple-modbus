cmake_minimum_required(VERSION 3.14)

project(SimpleModbus VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /WX")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Werror")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -g0")
endif()

# Define the Simple Modbus library as a static library
add_library(SimpleModbus STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/simple_modbus_server.c
	${CMAKE_CURRENT_SOURCE_DIR}/simple_modbus_rtu.c
)

# Specify the include directory for the Simple Modbus library
target_include_directories(SimpleModbus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Optionally, set C standard if required
set_property(TARGET SimpleModbus PROPERTY C_STANDARD 99)

# Export the library for use in other projects
export(PACKAGE SimpleModbus)