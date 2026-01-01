cmake_minimum_required(VERSION 3.15...3.27)
project(RocketSimPy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Python and nanobind
find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)

# Detect the installed nanobind package and import it into CMake
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
find_package(nanobind CONFIG REQUIRED)

# Collect RocketSim source files
file(GLOB_RECURSE ROCKETSIM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp"
)

# Collect all Bullet sources
file(GLOB_RECURSE BULLET_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../libsrc/bullet3-3.24/*.cpp"
)

# Create the nanobind module
nanobind_add_module(
    RocketSim
    STABLE_ABI
    NB_STATIC
    src/bindings.cpp
    ${ROCKETSIM_SOURCES}
    ${BULLET_SOURCES}
)

# Include directories
target_include_directories(RocketSim PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libsrc/bullet3-3.24"
)

# Suppress warnings from Bullet library (third-party code)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(RocketSim PRIVATE
        -Wno-c99-extensions
        -Wno-gnu-statement-expression-from-macro-expansion
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-reorder-ctor
        -Wno-deprecated-copy
        -Wno-delete-non-abstract-non-virtual-dtor
        -Wno-return-type
    )
endif()

# Install the module
install(TARGETS RocketSim LIBRARY DESTINATION .)

