cmake_minimum_required(VERSION 3.10)
project(torchlight VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_RELEASE "-O2")

# Find required packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Source files
set(TORCHLIGHT_SOURCES
    torchlight_core.c
    http_parser.c
    route_handler.c
    template_engine.c
    json_api.c
    websocket_handler.c
    utils.c
)

# Create shared library
add_library(torchlight SHARED ${TORCHLIGHT_SOURCES})

# Create static library
add_library(torchlight_static STATIC ${TORCHLIGHT_SOURCES})

# Set properties for static library
set_target_properties(torchlight_static PROPERTIES OUTPUT_NAME torchlight)

# Link libraries
target_link_libraries(torchlight 
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENSSL_LIBRARIES}
)

target_link_libraries(torchlight_static 
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENSSL_LIBRARIES}
)

# Include directories for linking
target_include_directories(torchlight PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(torchlight_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Create test executable
add_executable(test_torchlight test_torchlight.c)
target_link_libraries(test_torchlight torchlight_static)

# Create example executable
add_executable(torchlight_example example.c)
target_link_libraries(torchlight_example torchlight_static)

# Installation
install(TARGETS torchlight torchlight_static
    EXPORT TorchLightTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES torchlight.h DESTINATION include)

install(EXPORT TorchLightTargets
    FILE TorchLightTargets.cmake
    NAMESPACE TorchLight::
    DESTINATION lib/cmake/TorchLight
)

# Create TorchLightConfig.cmake
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    TorchLightConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/TorchLightConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/TorchLightConfig.cmake
    INSTALL_DESTINATION lib/cmake/TorchLight
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/TorchLightConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/TorchLightConfigVersion.cmake
    DESTINATION lib/cmake/TorchLight
)

# Add testing
enable_testing()
add_test(NAME torchlight_tests COMMAND test_torchlight)

# Display configuration
message(STATUS "TorchLight v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")