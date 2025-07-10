cmake_minimum_required(VERSION 3.14)

project(tests VERSION 1.0)

add_executable(tests 
                main.cpp 
                test_rtu_config.cpp
				test_rtu_state_machine.cpp
                test_server_config.cpp 
                test_server_read_pdu.cpp 
                test_server_f03.cpp 
                test_server_f04.cpp 
                test_server_f06.cpp 
                test_server_f16.cpp
)

if (MSVC)
    target_compile_options(tests PRIVATE /W4 /WX)
else()
    target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=missing-field-initializers)
endif()

get_filename_component(PARENT_DIR ../ ABSOLUTE)
include_directories(${PARENT_DIR})
target_sources(tests PRIVATE ${PARENT_DIR}/simple_modbus_server.c ${PARENT_DIR}/simple_modbus_rtu.c)

set_property(TARGET tests PROPERTY CXX_STANDARD 20)

# Google Test
include(FetchContent)
FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)
target_link_libraries(tests gtest_main)
gtest_add_tests(TARGET tests)


