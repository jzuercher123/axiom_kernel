cmake_minimum_required(VERSION 3.20)

project(axiom_kernel
        VERSION 0.1.0
        DESCRIPTION "Universal Sensor-to-AI Metrology Protocol"
        LANGUAGES CXX
)

# Enforce C++20 baseline (C++23 compatible)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Compile-Time Feature Flags (Section 7)
# ==============================================================================
option(AXIOM_BUILD_TESTS      "Build unit, concurrency, and integration tests" ON)
option(AXIOM_ENABLE_OPENCV    "Enable OpenCvSensor for IMX462 / V4L2 capture"  OFF)
option(AXIOM_ENABLE_OPENSSL "Enable OpenSSL software crypto backend" ON)
option(AXIOM_ENABLE_CRYPTO_HW "Enable ATECC608A hardware I2C signing backend"  OFF)
option(AXIOM_ENABLE_ZMQ       "Enable ZeroMQ publisher socket adapter"         ON)
option(AXIOM_ENABLE_FITS      "Enable FITS scientific format writer"           OFF)
option(AXIOM_BUILD_SDA_NODE   "Build reference sda_node executable"            ON)

# Load helper modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

# ==============================================================================
# LAYER 0: axiom_core (Zero External Dependencies)
# ==============================================================================
add_library(axiom_core INTERFACE)
add_library(axiom::core ALIAS axiom_core)

target_include_directories(axiom_core
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(axiom_core INTERFACE cxx_std_20)
axiom_set_project_warnings(axiom_core)

# ==============================================================================
# Tests
# ==============================================================================
if(AXIOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ==============================================================================
# Layer 2: Reference sda_node Deployment
# ==============================================================================
if(AXIOM_BUILD_SDA_NODE)
    add_executable(sda_node
            deployments/sda_node/main.cpp
    )
    target_link_libraries(sda_node
            PRIVATE
            axiom::core
            OpenSSL::Crypto
            cppzmq
    )
endif()