#include "controller_output_rumble_state.h"

#include "dualsense_output.h"

using namespace ds5::output;

namespace {

bool payload_has_common_motor_bytes(uint8_t const *payload, uint16_t len) {
    return payload != nullptr && len > kMotorLeftOffset;
}

bool payload_motors_active(uint8_t const *payload, uint16_t len) {
    return payload_has_common_motor_bytes(payload, len)
        && (payload[kMotorRightOffset] | payload[kMotorLeftOffset]) != 0;
}

} // namespace

bool controller_output_rumble_payload_uses_classic_selector(uint8_t const *payload, uint16_t len) {
    if (payload == nullptr || len <= kValidFlag1Offset) {
        return false;
    }

    const uint8_t flag0 = payload[kValidFlag0Offset];
    const uint8_t flag2 = len > kValidFlag2Offset ? payload[kValidFlag2Offset] : 0;
    return (flag0 & kFlag0HapticsSelect) != 0
        || (flag2 & kFlag2UseRumbleNotHaptics2) != 0;
}

bool controller_output_rumble_payload_requires_immediate_send(
    ControllerOutputRumbleStateMachine const &state,
    uint8_t const *payload,
    uint16_t len
) {
    if (!payload_has_common_motor_bytes(payload, len)) {
        return false;
    }

    if (controller_output_rumble_payload_uses_classic_selector(payload, len)) {
        return true;
    }

    return state.classic_rumble_active;
}

void controller_output_rumble_state_apply_payload(
    ControllerOutputRumbleStateMachine &state,
    uint8_t const *payload,
    uint16_t len
) {
    if (!payload_has_common_motor_bytes(payload, len)) {
        return;
    }

    if (!controller_output_rumble_payload_uses_classic_selector(payload, len)) {
        state.classic_rumble_active = false;
        return;
    }

    state.classic_rumble_active = payload_motors_active(payload, len);
}
