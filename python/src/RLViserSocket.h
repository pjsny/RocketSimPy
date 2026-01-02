#pragma once

#include "RocketSim.h"
#include "Sim/Arena/Arena.h"
#include "Sim/Car/Car.h"
#include "Sim/Ball/Ball.h"
#include "Sim/BoostPad/BoostPad.h"

#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <optional>

// Platform-specific includes for sockets
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    inline int close_socket(SOCKET s) { return closesocket(s); }
    #define SOCKET_ERRNO WSAGetLastError()
    #define SOCKET_WOULDBLOCK WSAEWOULDBLOCK
    #define SOCKET_INVALID INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    typedef int SOCKET;
    inline int close_socket(SOCKET s) { return ::close(s); }
    #define SOCKET_ERRNO errno
    #define SOCKET_WOULDBLOCK EWOULDBLOCK
    #define SOCKET_INVALID -1
#endif

using namespace RocketSim;

namespace RLViser {

// Port configuration (matching rlviser-py)
constexpr uint16_t RLVISER_PORT = 45243;
constexpr uint16_t ROCKETSIM_PORT = 34254;

// UDP packet types (matching rlviser protocol)
enum class UdpPacketType : uint8_t {
    Quit = 0,
    GameState = 1,
    Connection = 2,
    Paused = 3,
    Speed = 4,
    Render = 5,
};

// ============================================================================
// Binary Serialization Utilities (little-endian)
// ============================================================================

class ByteWriter {
public:
    std::vector<uint8_t> data;
    
    void write_u8(uint8_t val) {
        data.push_back(val);
    }
    
    void write_u32(uint32_t val) {
        data.push_back(val & 0xFF);
        data.push_back((val >> 8) & 0xFF);
        data.push_back((val >> 16) & 0xFF);
        data.push_back((val >> 24) & 0xFF);
    }
    
    void write_u64(uint64_t val) {
        for (int i = 0; i < 8; i++) {
            data.push_back((val >> (i * 8)) & 0xFF);
        }
    }
    
    void write_f32(float val) {
        uint32_t bits;
        memcpy(&bits, &val, sizeof(float));
        write_u32(bits);
    }
    
    void write_bool(bool val) {
        write_u8(val ? 1 : 0);
    }
    
    void write_vec(const Vec& v) {
        write_f32(v.x);
        write_f32(v.y);
        write_f32(v.z);
    }
    
    void write_rot_mat(const RotMat& m) {
        write_vec(m.forward);
        write_vec(m.right);
        write_vec(m.up);
    }
};

class ByteReader {
public:
    const uint8_t* data;
    size_t size;
    size_t pos = 0;
    
    ByteReader(const uint8_t* data, size_t size) : data(data), size(size) {}
    
    bool has_remaining(size_t n) const { return pos + n <= size; }
    
    uint8_t read_u8() {
        if (!has_remaining(1)) return 0;
        return data[pos++];
    }
    
    uint32_t read_u32() {
        if (!has_remaining(4)) return 0;
        uint32_t val = 0;
        val |= uint32_t(data[pos++]);
        val |= uint32_t(data[pos++]) << 8;
        val |= uint32_t(data[pos++]) << 16;
        val |= uint32_t(data[pos++]) << 24;
        return val;
    }
    
    uint64_t read_u64() {
        if (!has_remaining(8)) return 0;
        uint64_t val = 0;
        for (int i = 0; i < 8; i++) {
            val |= uint64_t(data[pos++]) << (i * 8);
        }
        return val;
    }
    
    float read_f32() {
        uint32_t bits = read_u32();
        float val;
        memcpy(&val, &bits, sizeof(float));
        return val;
    }
    
    bool read_bool() {
        return read_u8() != 0;
    }
    
    Vec read_vec() {
        float x = read_f32();
        float y = read_f32();
        float z = read_f32();
        return Vec(x, y, z);
    }
    
    RotMat read_rot_mat() {
        Vec forward = read_vec();
        Vec right = read_vec();
        Vec up = read_vec();
        return RotMat(forward, right, up);
    }
};

// ============================================================================
// CarInfo - Complete car information for serialization
// ============================================================================

struct CarInfo {
    uint32_t id;
    Team team;
    CarState state;
    CarConfig config;
    
    // Serialization size (approximate, we compute exact)
    static constexpr size_t NUM_BYTES = 
        4 +     // id
        1 +     // team
        // CarState fields:
        12 +    // pos
        36 +    // rotMat
        12 +    // vel
        12 +    // angVel
        1 +     // isOnGround
        4 +     // wheelsWithContact (4 bools)
        1 +     // hasJumped
        1 +     // hasDoubleJumped
        1 +     // hasFlipped
        12 +    // flipRelTorque
        4 +     // jumpTime
        4 +     // flipTime
        1 +     // isFlipping
        1 +     // isJumping
        4 +     // airTime
        4 +     // airTimeSinceJump
        4 +     // boost
        4 +     // timeSinceBoosted
        1 +     // isBoosting
        4 +     // boostingTime
        1 +     // isSupersonic
        4 +     // supersonicTime
        4 +     // handbrakeVal
        1 +     // isAutoFlipping
        4 +     // autoFlipTimer
        4 +     // autoFlipTorqueScale
        1 +     // worldContact.hasContact
        12 +    // worldContact.contactNormal
        4 +     // carContact.otherCarID
        4 +     // carContact.cooldownTimer
        1 +     // isDemoed
        4 +     // demoRespawnTimer
        // BallHitInfo
        1 +     // isValid
        12 +    // relativePosOnBall
        12 +    // ballPos
        12 +    // extraHitVel
        8 +     // tickCountWhenHit
        8 +     // tickCountWhenExtraImpulseApplied
        // lastControls
        4 + 4 + 4 + 4 + 4 + 1 + 1 + 1 +  // throttle, steer, pitch, yaw, roll, boost, jump, handbrake
        // CarConfig
        12 +    // hitboxSize
        12 +    // hitboxPosOffset
        4 + 4 + 12 +  // frontWheels
        4 + 4 + 12 +  // backWheels
        4;      // dodgeDeadzone
    
    void write(ByteWriter& w) const {
        w.write_u32(id);
        w.write_u8(static_cast<uint8_t>(team));
        
        // CarState
        w.write_vec(state.pos);
        w.write_rot_mat(state.rotMat);
        w.write_vec(state.vel);
        w.write_vec(state.angVel);
        w.write_bool(state.isOnGround);
        for (int i = 0; i < 4; i++) {
            w.write_bool(state.wheelsWithContact[i]);
        }
        w.write_bool(state.hasJumped);
        w.write_bool(state.hasDoubleJumped);
        w.write_bool(state.hasFlipped);
        w.write_vec(state.flipRelTorque);
        w.write_f32(state.jumpTime);
        w.write_f32(state.flipTime);
        w.write_bool(state.isFlipping);
        w.write_bool(state.isJumping);
        w.write_f32(state.airTime);
        w.write_f32(state.airTimeSinceJump);
        w.write_f32(state.boost);
        w.write_f32(state.timeSinceBoosted);
        w.write_bool(state.isBoosting);
        w.write_f32(state.boostingTime);
        w.write_bool(state.isSupersonic);
        w.write_f32(state.supersonicTime);
        w.write_f32(state.handbrakeVal);
        w.write_bool(state.isAutoFlipping);
        w.write_f32(state.autoFlipTimer);
        w.write_f32(state.autoFlipTorqueScale);
        w.write_bool(state.worldContact.hasContact);
        w.write_vec(state.worldContact.contactNormal);
        w.write_u32(state.carContact.otherCarID);
        w.write_f32(state.carContact.cooldownTimer);
        w.write_bool(state.isDemoed);
        w.write_f32(state.demoRespawnTimer);
        
        // BallHitInfo
        w.write_bool(state.ballHitInfo.isValid);
        w.write_vec(state.ballHitInfo.relativePosOnBall);
        w.write_vec(state.ballHitInfo.ballPos);
        w.write_vec(state.ballHitInfo.extraHitVel);
        w.write_u64(state.ballHitInfo.tickCountWhenHit);
        w.write_u64(state.ballHitInfo.tickCountWhenExtraImpulseApplied);
        
        // lastControls
        w.write_f32(state.lastControls.throttle);
        w.write_f32(state.lastControls.steer);
        w.write_f32(state.lastControls.pitch);
        w.write_f32(state.lastControls.yaw);
        w.write_f32(state.lastControls.roll);
        w.write_bool(state.lastControls.boost);
        w.write_bool(state.lastControls.jump);
        w.write_bool(state.lastControls.handbrake);
        
        // CarConfig
        w.write_vec(config.hitboxSize);
        w.write_vec(config.hitboxPosOffset);
        w.write_f32(config.frontWheels.wheelRadius);
        w.write_f32(config.frontWheels.suspensionRestLength);
        w.write_vec(config.frontWheels.connectionPointOffset);
        w.write_f32(config.backWheels.wheelRadius);
        w.write_f32(config.backWheels.suspensionRestLength);
        w.write_vec(config.backWheels.connectionPointOffset);
        w.write_f32(config.dodgeDeadzone);
    }
    
    static CarInfo read(ByteReader& r) {
        CarInfo info;
        info.id = r.read_u32();
        info.team = static_cast<Team>(r.read_u8());
        
        // CarState
        info.state.pos = r.read_vec();
        info.state.rotMat = r.read_rot_mat();
        info.state.vel = r.read_vec();
        info.state.angVel = r.read_vec();
        info.state.isOnGround = r.read_bool();
        for (int i = 0; i < 4; i++) {
            info.state.wheelsWithContact[i] = r.read_bool();
        }
        info.state.hasJumped = r.read_bool();
        info.state.hasDoubleJumped = r.read_bool();
        info.state.hasFlipped = r.read_bool();
        info.state.flipRelTorque = r.read_vec();
        info.state.jumpTime = r.read_f32();
        info.state.flipTime = r.read_f32();
        info.state.isFlipping = r.read_bool();
        info.state.isJumping = r.read_bool();
        info.state.airTime = r.read_f32();
        info.state.airTimeSinceJump = r.read_f32();
        info.state.boost = r.read_f32();
        info.state.timeSinceBoosted = r.read_f32();
        info.state.isBoosting = r.read_bool();
        info.state.boostingTime = r.read_f32();
        info.state.isSupersonic = r.read_bool();
        info.state.supersonicTime = r.read_f32();
        info.state.handbrakeVal = r.read_f32();
        info.state.isAutoFlipping = r.read_bool();
        info.state.autoFlipTimer = r.read_f32();
        info.state.autoFlipTorqueScale = r.read_f32();
        info.state.worldContact.hasContact = r.read_bool();
        info.state.worldContact.contactNormal = r.read_vec();
        info.state.carContact.otherCarID = r.read_u32();
        info.state.carContact.cooldownTimer = r.read_f32();
        info.state.isDemoed = r.read_bool();
        info.state.demoRespawnTimer = r.read_f32();
        
        // BallHitInfo
        info.state.ballHitInfo.isValid = r.read_bool();
        info.state.ballHitInfo.relativePosOnBall = r.read_vec();
        info.state.ballHitInfo.ballPos = r.read_vec();
        info.state.ballHitInfo.extraHitVel = r.read_vec();
        info.state.ballHitInfo.tickCountWhenHit = r.read_u64();
        info.state.ballHitInfo.tickCountWhenExtraImpulseApplied = r.read_u64();
        
        // lastControls
        info.state.lastControls.throttle = r.read_f32();
        info.state.lastControls.steer = r.read_f32();
        info.state.lastControls.pitch = r.read_f32();
        info.state.lastControls.yaw = r.read_f32();
        info.state.lastControls.roll = r.read_f32();
        info.state.lastControls.boost = r.read_bool();
        info.state.lastControls.jump = r.read_bool();
        info.state.lastControls.handbrake = r.read_bool();
        
        // CarConfig
        info.config.hitboxSize = r.read_vec();
        info.config.hitboxPosOffset = r.read_vec();
        info.config.frontWheels.wheelRadius = r.read_f32();
        info.config.frontWheels.suspensionRestLength = r.read_f32();
        info.config.frontWheels.connectionPointOffset = r.read_vec();
        info.config.backWheels.wheelRadius = r.read_f32();
        info.config.backWheels.suspensionRestLength = r.read_f32();
        info.config.backWheels.connectionPointOffset = r.read_vec();
        info.config.dodgeDeadzone = r.read_f32();
        
        return info;
    }
};

// ============================================================================
// BoostPadInfo - Boost pad information for serialization
// ============================================================================

struct BoostPadInfo {
    bool isActive;
    float cooldown;
    Vec pos;
    bool isBig;
    
    static constexpr size_t NUM_BYTES = 1 + 4 + 12 + 1;
    
    void write(ByteWriter& w) const {
        w.write_bool(isActive);
        w.write_f32(cooldown);
        w.write_vec(pos);
        w.write_bool(isBig);
    }
    
    static BoostPadInfo read(ByteReader& r) {
        BoostPadInfo info;
        info.isActive = r.read_bool();
        info.cooldown = r.read_f32();
        info.pos = r.read_vec();
        info.isBig = r.read_bool();
        return info;
    }
};

// ============================================================================
// BallStateInfo - Ball state with heatseeker info
// ============================================================================

struct BallStateInfo {
    BallState state;
    
    void write(ByteWriter& w) const {
        w.write_vec(state.pos);
        w.write_rot_mat(state.rotMat);
        w.write_vec(state.vel);
        w.write_vec(state.angVel);
        
        // Heatseeker info
        w.write_f32(state.hsInfo.yTargetDir);
        w.write_f32(state.hsInfo.curTargetSpeed);
        w.write_f32(state.hsInfo.timeSinceHit);
    }
    
    static BallStateInfo read(ByteReader& r) {
        BallStateInfo info;
        info.state.pos = r.read_vec();
        info.state.rotMat = r.read_rot_mat();
        info.state.vel = r.read_vec();
        info.state.angVel = r.read_vec();
        
        // Heatseeker info
        info.state.hsInfo.yTargetDir = r.read_f32();
        info.state.hsInfo.curTargetSpeed = r.read_f32();
        info.state.hsInfo.timeSinceHit = r.read_f32();
        
        return info;
    }
};

// ============================================================================
// GameState - Full game state for serialization
// ============================================================================

struct GameState {
    uint64_t tickCount;
    float tickRate;
    GameMode gameMode;
    std::vector<BoostPadInfo> pads;
    std::vector<CarInfo> cars;
    BallStateInfo ball;
    
    // Minimum header bytes for reading count info
    static constexpr size_t MIN_NUM_BYTES = 
        8 +     // tick_count
        4 +     // tick_rate
        1 +     // game_mode
        4 +     // num_pads
        4;      // num_cars
    
    std::vector<uint8_t> to_bytes() const {
        ByteWriter w;
        
        // Header
        w.write_u64(tickCount);
        w.write_f32(tickRate);
        w.write_u8(static_cast<uint8_t>(gameMode));
        w.write_u32(static_cast<uint32_t>(pads.size()));
        w.write_u32(static_cast<uint32_t>(cars.size()));
        
        // Ball state (including heatseeker info)
        ball.write(w);
        
        // Pads
        for (const auto& pad : pads) {
            pad.write(w);
        }
        
        // Cars
        for (const auto& car : cars) {
            car.write(w);
        }
        
        return w.data;
    }
    
    static size_t get_num_bytes(const uint8_t* data, size_t size) {
        if (size < MIN_NUM_BYTES) return 0;
        ByteReader r(data, size);
        
        r.read_u64();  // tick_count
        r.read_f32();  // tick_rate
        r.read_u8();   // game_mode
        uint32_t num_pads = r.read_u32();
        uint32_t num_cars = r.read_u32();
        
        // Ball state size: pos(12) + rotMat(36) + vel(12) + angVel(12) + hs(12) = 84
        static constexpr size_t BALL_STATE_BYTES = 12 + 36 + 12 + 12 + 12;
        
        return MIN_NUM_BYTES + 
               BALL_STATE_BYTES +
               num_pads * BoostPadInfo::NUM_BYTES + 
               num_cars * CarInfo::NUM_BYTES;
    }
    
    static GameState from_bytes(const uint8_t* data, size_t size) {
        GameState state;
        ByteReader r(data, size);
        
        // Header
        state.tickCount = r.read_u64();
        state.tickRate = r.read_f32();
        state.gameMode = static_cast<GameMode>(r.read_u8());
        uint32_t num_pads = r.read_u32();
        uint32_t num_cars = r.read_u32();
        
        // Ball state
        state.ball = BallStateInfo::read(r);
        
        // Pads
        state.pads.reserve(num_pads);
        for (uint32_t i = 0; i < num_pads; i++) {
            state.pads.push_back(BoostPadInfo::read(r));
        }
        
        // Cars
        state.cars.reserve(num_cars);
        for (uint32_t i = 0; i < num_cars; i++) {
            state.cars.push_back(CarInfo::read(r));
        }
        
        return state;
    }
    
    // Factory method to create from Arena
    static GameState from_arena(Arena* arena) {
        GameState state;
        state.tickCount = arena->tickCount;
        state.tickRate = arena->GetTickRate();
        state.gameMode = arena->gameMode;
        
        // Ball
        state.ball.state = arena->ball->GetState();
        
        // Pads
        for (BoostPad* pad : arena->GetBoostPads()) {
            BoostPadInfo info;
            BoostPadState padState = pad->GetState();
            info.isActive = padState.isActive;
            info.cooldown = padState.cooldown;
            info.pos = pad->config.pos;
            info.isBig = pad->config.isBig;
            state.pads.push_back(info);
        }
        
        // Cars
        for (Car* car : arena->GetCars()) {
            CarInfo info;
            info.id = car->id;
            info.team = car->team;
            info.state = car->GetState();
            info.config = car->config;
            state.cars.push_back(info);
        }
        
        return state;
    }
};

// ============================================================================
// ReturnMessage - Messages received from RLViser
// ============================================================================

struct ReturnMessage {
    std::optional<GameState> gameState;
    std::optional<float> speed;
    std::optional<bool> paused;
};

// ============================================================================
// RLViserSocket - UDP socket handler for RLViser communication
// ============================================================================

class RLViserSocket {
private:
    SOCKET socket_;
    sockaddr_in rlviser_addr_;
    bool is_initialized_ = false;
    bool is_connected_ = false;
    
    // Buffers for receiving
    std::vector<uint8_t> recv_buffer_;
    uint8_t header_buffer_[GameState::MIN_NUM_BYTES];
    
    // State tracking
    bool is_paused_ = false;
    float game_speed_ = 1.0f;
    
    void set_nonblocking(bool nonblocking) {
#ifdef _WIN32
        u_long mode = nonblocking ? 1 : 0;
        ioctlsocket(socket_, FIONBIO, &mode);
#else
        int flags = fcntl(socket_, F_GETFL, 0);
        if (nonblocking) {
            fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
        } else {
            fcntl(socket_, F_SETFL, flags & ~O_NONBLOCK);
        }
#endif
    }
    
public:
    RLViserSocket() : recv_buffer_(65536) {}
    
    ~RLViserSocket() {
        close();
    }
    
    bool init() {
        if (is_initialized_) return true;
        
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }
#endif
        
        // Create UDP socket
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == SOCKET_INVALID) {
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }
        
        // Bind to RocketSim port
        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(ROCKETSIM_PORT);
        local_addr.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(socket_, (sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            close_socket(socket_);
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }
        
        // Set up RLViser address
        rlviser_addr_.sin_family = AF_INET;
        rlviser_addr_.sin_port = htons(RLVISER_PORT);
        inet_pton(AF_INET, "127.0.0.1", &rlviser_addr_.sin_addr);
        
        is_initialized_ = true;
        return true;
    }
    
    bool connect() {
        if (!is_initialized_ && !init()) {
            return false;
        }
        
        // Send connection packet
        uint8_t conn_packet = static_cast<uint8_t>(UdpPacketType::Connection);
        sendto(socket_, (const char*)&conn_packet, 1, 0, 
               (sockaddr*)&rlviser_addr_, sizeof(rlviser_addr_));
        
        set_nonblocking(true);
        is_connected_ = true;
        return true;
    }
    
    void close() {
        if (is_initialized_) {
            if (is_connected_) {
                // Send quit packet
                uint8_t quit_packet = static_cast<uint8_t>(UdpPacketType::Quit);
                sendto(socket_, (const char*)&quit_packet, 1, 0,
                       (sockaddr*)&rlviser_addr_, sizeof(rlviser_addr_));
            }
            close_socket(socket_);
#ifdef _WIN32
            WSACleanup();
#endif
            is_initialized_ = false;
            is_connected_ = false;
        }
    }
    
    bool is_connected() const { return is_connected_; }
    
    // Send game state to RLViser
    bool send_game_state(const GameState& state) {
        if (!is_connected_) return false;
        
        set_nonblocking(false);
        
        // Send packet type
        uint8_t packet_type = static_cast<uint8_t>(UdpPacketType::GameState);
        sendto(socket_, (const char*)&packet_type, 1, 0,
               (sockaddr*)&rlviser_addr_, sizeof(rlviser_addr_));
        
        // Send game state bytes
        auto bytes = state.to_bytes();
        sendto(socket_, (const char*)bytes.data(), bytes.size(), 0,
               (sockaddr*)&rlviser_addr_, sizeof(rlviser_addr_));
        
        set_nonblocking(true);
        return true;
    }
    
    // Send game state directly from Arena
    bool send_arena_state(Arena* arena) {
        return send_game_state(GameState::from_arena(arena));
    }
    
    // Report game speed
    bool send_game_speed(float speed) {
        if (!is_connected_) return false;
        
        set_nonblocking(false);
        
        uint8_t packet_type = static_cast<uint8_t>(UdpPacketType::Speed);
        sendto(socket_, (const char*)&packet_type, 1, 0,
               (sockaddr*)&rlviser_addr_, sizeof(rlviser_addr_));
        
        uint32_t speed_bits;
        memcpy(&speed_bits, &speed, sizeof(float));
        uint8_t speed_bytes[4];
        speed_bytes[0] = speed_bits & 0xFF;
        speed_bytes[1] = (speed_bits >> 8) & 0xFF;
        speed_bytes[2] = (speed_bits >> 16) & 0xFF;
        speed_bytes[3] = (speed_bits >> 24) & 0xFF;
        sendto(socket_, (const char*)speed_bytes, 4, 0,
               (sockaddr*)&rlviser_addr_, sizeof(rlviser_addr_));
        
        set_nonblocking(true);
        game_speed_ = speed;
        return true;
    }
    
    // Report pause state
    bool send_paused(bool paused) {
        if (!is_connected_) return false;
        
        set_nonblocking(false);
        
        uint8_t packet_type = static_cast<uint8_t>(UdpPacketType::Paused);
        sendto(socket_, (const char*)&packet_type, 1, 0,
               (sockaddr*)&rlviser_addr_, sizeof(rlviser_addr_));
        
        uint8_t paused_byte = paused ? 1 : 0;
        sendto(socket_, (const char*)&paused_byte, 1, 0,
               (sockaddr*)&rlviser_addr_, sizeof(rlviser_addr_));
        
        set_nonblocking(true);
        is_paused_ = paused;
        return true;
    }
    
    // Receive messages from RLViser
    ReturnMessage receive_messages() {
        ReturnMessage result;
        if (!is_connected_) return result;
        
        uint8_t packet_type_buf[1];
        sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        
        // Poll for messages (non-blocking)
        while (recvfrom(socket_, (char*)packet_type_buf, 1, 0,
                        (sockaddr*)&from_addr, &from_len) > 0) {
            
            UdpPacketType packet_type = static_cast<UdpPacketType>(packet_type_buf[0]);
            
            switch (packet_type) {
                case UdpPacketType::GameState: {
                    set_nonblocking(false);
                    
                    // Read header to get size
                    recvfrom(socket_, (char*)header_buffer_, GameState::MIN_NUM_BYTES, MSG_PEEK,
                             (sockaddr*)&from_addr, &from_len);
                    
                    size_t num_bytes = GameState::get_num_bytes(header_buffer_, GameState::MIN_NUM_BYTES);
                    recv_buffer_.resize(num_bytes);
                    
                    recvfrom(socket_, (char*)recv_buffer_.data(), num_bytes, 0,
                             (sockaddr*)&from_addr, &from_len);
                    
                    set_nonblocking(true);
                    
                    result.gameState = GameState::from_bytes(recv_buffer_.data(), num_bytes);
                    break;
                }
                
                case UdpPacketType::Speed: {
                    set_nonblocking(false);
                    
                    uint8_t speed_bytes[4];
                    recvfrom(socket_, (char*)speed_bytes, 4, 0,
                             (sockaddr*)&from_addr, &from_len);
                    
                    set_nonblocking(true);
                    
                    uint32_t bits = speed_bytes[0] | 
                                   (speed_bytes[1] << 8) | 
                                   (speed_bytes[2] << 16) | 
                                   (speed_bytes[3] << 24);
                    float speed;
                    memcpy(&speed, &bits, sizeof(float));
                    result.speed = speed;
                    game_speed_ = speed;
                    break;
                }
                
                case UdpPacketType::Paused: {
                    set_nonblocking(false);
                    
                    uint8_t paused_byte;
                    recvfrom(socket_, (char*)&paused_byte, 1, 0,
                             (sockaddr*)&from_addr, &from_len);
                    
                    set_nonblocking(true);
                    
                    result.paused = (paused_byte != 0);
                    is_paused_ = result.paused.value();
                    break;
                }
                
                case UdpPacketType::Quit:
                    is_connected_ = false;
                    break;
                
                default:
                    break;
            }
            
            from_len = sizeof(from_addr);
        }
        
        return result;
    }
    
    // Getters for state
    bool is_paused() const { return is_paused_; }
    float get_game_speed() const { return game_speed_; }
};

// Global socket instance (singleton pattern like rlviser-py)
inline RLViserSocket& get_socket() {
    static RLViserSocket socket;
    return socket;
}

} // namespace RLViser

