#ifndef DS5_BRIDGE_PLAYER_LED_MODE_H
#define DS5_BRIDGE_PLAYER_LED_MODE_H

#include <cstdint>

#include "dualsense_output.h"

enum PlayerLedMode : uint8_t {
    PlayerLedModeOff = 0,
    PlayerLedModeFollowGame = 1,
    PlayerLedModeP1 = 2,
    PlayerLedModeP2 = 3,
    PlayerLedModeP3 = 4,
    PlayerLedModeP4 = 5,
};

inline bool player_led_mode_is_valid(uint8_t value) {
    return value <= PlayerLedModeP4;
}

inline bool player_led_mode_follows_game(PlayerLedMode mode) {
    return mode == PlayerLedModeFollowGame;
}

inline uint8_t player_led_mode_pattern(PlayerLedMode mode) {
    using namespace ds5::output;
    switch (mode) {
        case PlayerLedModeP1:
            return kPlayerLed1Instant;
        case PlayerLedModeP2:
            return kPlayerLed2Instant;
        case PlayerLedModeP3:
            return kPlayerLed3Instant;
        case PlayerLedModeP4:
            return kPlayerLed4Instant;
        case PlayerLedModeOff:
        case PlayerLedModeFollowGame:
        default:
            return kPlayerLedOff;
    }
}

#endif // DS5_BRIDGE_PLAYER_LED_MODE_H
