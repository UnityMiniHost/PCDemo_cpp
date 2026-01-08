cmake_minimum_required(VERSION 3.15)
project(webglhost_native_demo VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories - simplified structure
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set output directories for each configuration (MSVC Multi-config generator)
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG_TYPE})
endforeach()

# Create demo executable
add_executable(demo
    demo.cpp
)

# Include directories
target_include_directories(demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link required Windows libraries
target_link_libraries(demo PRIVATE
    ws2_32  # Winsock for network communication
)

# Set Windows subsystem to console
if(WIN32)
    set_target_properties(demo PROPERTIES
        WIN32_EXECUTABLE FALSE  # Show console window
    )
endif()

# Display configuration information
message(STATUS "========================================")
message(STATUS "WebGLHost Native Demo Configuration")
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "========================================")
