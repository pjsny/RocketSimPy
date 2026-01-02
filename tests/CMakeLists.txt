# Test executable
add_executable(RocketSimTests
    main.cpp
    TestUtils.cpp
    Math/MathTest.cpp
    Car/CarPhysicsTest.cpp
    Ball/BallPhysicsTest.cpp
    Arena/ArenaTest.cpp
    Integration/IntegrationTest.cpp
    StateManagement/StateManagementTest.cpp
)

target_link_libraries(RocketSimTests
    PRIVATE
    RocketSim
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(RocketSimTests PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/libsrc/bullet3-3.24"
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(RocketSimTests)

# Performance benchmark executable (optional)
option(BUILD_BENCHMARK "Build the performance benchmark" OFF)

if(BUILD_BENCHMARK)
    add_executable(PerformanceBenchmark
        Benchmark/PerformanceBenchmark.cpp
        Benchmark/READMEBenchmark.cpp
        Benchmark/StressBenchmark.cpp
        Benchmark/PhaseProfiler.cpp
        Benchmark/ProfilerUtils.cpp
    )

    target_link_libraries(PerformanceBenchmark
        PRIVATE
        RocketSim
    )

    target_include_directories(PerformanceBenchmark PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_SOURCE_DIR}/libsrc/bullet3-3.24"
    )
endif()

