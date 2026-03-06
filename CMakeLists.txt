cmake_minimum_required(VERSION 3.16)
project(joyson_glove_sdk VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Find dependencies
find_package(Threads REQUIRED)

# Source files
set(SDK_SOURCES
    src/protocol.cpp
    src/udp_client.cpp
    src/motor_controller.cpp
    src/encoder_reader.cpp
    src/imu_reader.cpp
    src/glove_sdk.cpp
)

# Build shared library
add_library(${PROJECT_NAME} SHARED ${SDK_SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Threads::Threads
)

# Set library properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "include/joyson_glove/protocol.hpp;include/joyson_glove/udp_client.hpp;include/joyson_glove/motor_controller.hpp;include/joyson_glove/encoder_reader.hpp;include/joyson_glove/imu_reader.hpp;include/joyson_glove/glove_sdk.hpp"
)

# Examples
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_executable(basic_motor_control examples/basic_motor_control.cpp)
    target_link_libraries(basic_motor_control ${PROJECT_NAME})

    add_executable(read_sensors examples/read_sensors.cpp)
    target_link_libraries(read_sensors ${PROJECT_NAME})

    add_executable(test_imu_reader examples/test_imu_reader.cpp)
    target_link_libraries(test_imu_reader ${PROJECT_NAME})

    add_executable(test_encoder_reader examples/test_encoder_reader.cpp)
    target_link_libraries(test_encoder_reader ${PROJECT_NAME})

    add_executable(test_motor_controller examples/test_motor_controller.cpp)
    target_link_libraries(test_motor_controller ${PROJECT_NAME})
endif()

# Tests
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()

    add_executable(test_protocol tests/test_protocol.cpp)
    target_link_libraries(test_protocol ${PROJECT_NAME} GTest::GTest GTest::Main)
    add_test(NAME test_protocol COMMAND test_protocol)

    add_executable(test_udp_client tests/test_udp_client.cpp)
    target_link_libraries(test_udp_client ${PROJECT_NAME} GTest::GTest GTest::Main)
    add_test(NAME test_udp_client COMMAND test_udp_client)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/joyson_glove
)

install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE joyson_glove::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Generate config files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
