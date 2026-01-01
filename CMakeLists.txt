cmake_minimum_required(VERSION 3.10)

project("RocketSim" 
    VERSION 2.1.0
    LANGUAGES CXX
    DESCRIPTION "A C++ library for simulating Rocket League games at maximum efficiency"
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type defaults to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Add all headers and code files
file(GLOB_RECURSE FILES_ROCKETSIM "${PROJECT_SOURCE_DIR}/src/*.cpp" "${PROJECT_SOURCE_DIR}/src/*.h")

# Only include bullet headers when using MSVC, otherwise just code files
file(GLOB_RECURSE FILES_BULLET "${PROJECT_SOURCE_DIR}/libsrc/bullet3-3.24/*.cpp" "${PROJECT_SOURCE_DIR}/libsrc/bullet3-3.24/*.h")

add_library(RocketSim STATIC ${FILES_ROCKETSIM} ${FILES_BULLET})

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # Enable useful warnings for our code
    target_compile_options(RocketSim PRIVATE -Wall -Wextra)
    # Suppress warnings from Bullet library (third-party code)
    # These warnings are from old C99-style code in Bullet and are harmless
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

set_target_properties(RocketSim PROPERTIES
    LINKER_LANGUAGE CXX
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME RocketSim
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
# Mark Bullet as SYSTEM to suppress warnings from third-party headers
target_include_directories(RocketSim PUBLIC
    "${PROJECT_SOURCE_DIR}/src"
)
target_include_directories(RocketSim PRIVATE
    SYSTEM "${PROJECT_SOURCE_DIR}/libsrc/bullet3-3.24"
)

# Install target
include(GNUInstallDirs)

install(TARGETS RocketSim
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/src/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RocketSim
    FILES_MATCHING PATTERN "*.h"
)