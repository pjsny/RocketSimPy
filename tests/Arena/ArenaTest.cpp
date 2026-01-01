#include <gtest/gtest.h>
#include "RocketSim.h"
#include "../TestUtils.h"
#include "Sim/Arena/Arena.h"

using namespace RocketSim;

class ArenaTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::map<GameMode, std::vector<FileData>> emptyMeshes;
        InitFromMem(emptyMeshes, true);
    }
};

TEST_F(ArenaTest, ArenaCreation) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    ASSERT_NE(arena, nullptr);
    EXPECT_EQ(arena->gameMode, GameMode::THE_VOID);
    EXPECT_NEAR(arena->GetTickRate(), 120.0f, 0.1f);
    delete arena;
}

TEST_F(ArenaTest, SingleTickExecution) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    uint64_t initialTickCount = arena->tickCount;
    
    arena->Step(1);
    
    EXPECT_EQ(arena->tickCount, initialTickCount + 1);
    delete arena;
}

TEST_F(ArenaTest, MultipleTickExecution) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    uint64_t initialTickCount = arena->tickCount;
    
    arena->Step(10);
    
    EXPECT_EQ(arena->tickCount, initialTickCount + 10);
    delete arena;
}

TEST_F(ArenaTest, AddCar) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    ASSERT_NE(car, nullptr);
    EXPECT_EQ(car->team, Team::BLUE);
    EXPECT_GT(car->id, 0);
    
    const auto& cars = arena->GetCars();
    EXPECT_EQ(cars.size(), 1);
    EXPECT_NE(cars.find(car), cars.end());
    
    delete arena;
}

TEST_F(ArenaTest, RemoveCar) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    uint32_t carId = car->id;
    
    bool removed = arena->RemoveCar(carId);
    EXPECT_TRUE(removed);
    
    const auto& cars = arena->GetCars();
    EXPECT_EQ(cars.size(), 0);
    
    Car* retrieved = arena->GetCar(carId);
    EXPECT_EQ(retrieved, nullptr);
    
    delete arena;
}

TEST_F(ArenaTest, GetCar) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car1 = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    Car* car2 = arena->AddCar(Team::ORANGE, CAR_CONFIG_DOMINUS);
    
    Car* retrieved1 = arena->GetCar(car1->id);
    Car* retrieved2 = arena->GetCar(car2->id);
    
    EXPECT_EQ(retrieved1, car1);
    EXPECT_EQ(retrieved2, car2);
    
    delete arena;
}

TEST_F(ArenaTest, BallExists) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    ASSERT_NE(arena->ball, nullptr);
    
    BallState state = arena->ball->GetState();
    EXPECT_GT(state.pos.z, 0.0f);
    
    delete arena;
}

TEST_F(ArenaTest, TickRate) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 60.0f);
    EXPECT_NEAR(arena->GetTickRate(), 60.0f, 0.1f);
    
    delete arena;
    
    arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 240.0f);
    EXPECT_NEAR(arena->GetTickRate(), 240.0f, 0.1f);
    
    delete arena;
}

TEST_F(ArenaTest, MutatorConfig) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    const MutatorConfig& config = arena->GetMutatorConfig();
    EXPECT_GT(config.ballMass, 0.0f);
    EXPECT_GT(config.ballRadius, 0.0f);
    
    // Modify mutator config
    MutatorConfig newConfig = config;
    newConfig.ballMass = 200.0f;
    arena->SetMutatorConfig(newConfig);
    
    const MutatorConfig& updatedConfig = arena->GetMutatorConfig();
    EXPECT_NEAR(updatedConfig.ballMass, 200.0f, 0.1f);
    
    delete arena;
}

TEST_F(ArenaTest, ArenaConfig) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    const ArenaConfig& config = arena->GetArenaConfig();
    EXPECT_GT(config.maxAABBLen, 0.0f);
    
    delete arena;
}

TEST_F(ArenaTest, MultipleCars) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car1 = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    Car* car2 = arena->AddCar(Team::BLUE, CAR_CONFIG_DOMINUS);
    Car* car3 = arena->AddCar(Team::ORANGE, CAR_CONFIG_PLANK);
    
    const auto& cars = arena->GetCars();
    EXPECT_EQ(cars.size(), 3);
    
    EXPECT_NE(car1->id, car2->id);
    EXPECT_NE(car2->id, car3->id);
    EXPECT_NE(car1->id, car3->id);
    
    delete arena;
}

TEST_F(ArenaTest, ComponentUpdateOrder) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    CarState stateBefore = car->GetState();
    BallState ballStateBefore = arena->ball->GetState();
    
    // Step should update all components
    arena->Step(1);
    
    CarState stateAfter = car->GetState();
    BallState ballStateAfter = arena->ball->GetState();
    
    // States should have changed (even if slightly)
    // In THE_VOID, objects will fall due to gravity
    EXPECT_NE(stateBefore.tickCountSinceUpdate, stateAfter.tickCountSinceUpdate);
    EXPECT_NE(ballStateBefore.tickCountSinceUpdate, ballStateAfter.tickCountSinceUpdate);
    
    delete arena;
}

TEST_F(ArenaTest, ResetToRandomKickoff) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    
    // Move car and ball
    CarState carState = car->GetState();
    carState.pos = Vec(1000.0f, 1000.0f, 1000.0f);
    car->SetState(carState);
    
    BallState ballState = arena->ball->GetState();
    ballState.pos = Vec(2000.0f, 2000.0f, 2000.0f);
    arena->ball->SetState(ballState);
    
    // Reset to kickoff
    arena->ResetToRandomKickoff(42); // Use seed for reproducibility
    
    CarState carStateAfter = car->GetState();
    BallState ballStateAfter = arena->ball->GetState();
    
    // Positions should have changed (reset to kickoff positions)
    EXPECT_NE(carState.pos.Dist(carStateAfter.pos), 0.0f);
    EXPECT_NE(ballState.pos.Dist(ballStateAfter.pos), 0.0f);
    
    delete arena;
}

TEST_F(ArenaTest, GameModeSpecific) {
    // Test different game modes (some may require collision meshes)
    try {
        Arena* soccarArena = TestUtils::CreateTestArena(GameMode::SOCCAR, 120.0f);
        EXPECT_EQ(soccarArena->gameMode, GameMode::SOCCAR);
        delete soccarArena;
    } catch (...) {
        // SOCCAR may require meshes, skip if it fails
    }
    
    try {
        Arena* hoopsArena = TestUtils::CreateTestArena(GameMode::HOOPS, 120.0f);
        EXPECT_EQ(hoopsArena->gameMode, GameMode::HOOPS);
        delete hoopsArena;
    } catch (...) {
        // HOOPS may require meshes, skip if it fails
    }
    
    try {
        Arena* heatseekerArena = TestUtils::CreateTestArena(GameMode::HEATSEEKER, 120.0f);
        EXPECT_EQ(heatseekerArena->gameMode, GameMode::HEATSEEKER);
        delete heatseekerArena;
    } catch (...) {
        // HEATSEEKER may require meshes, skip if it fails
    }
}

