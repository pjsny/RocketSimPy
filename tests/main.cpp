#include <gtest/gtest.h>
#include "RocketSim.h"

int main(int argc, char **argv) {
    // Initialize RocketSim with collision meshes for all tests
    RocketSim::Init("collision_meshes");
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
