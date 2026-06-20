#include "controller_output_state.h"

#include <algorithm>
#include <cstring>

#include "dualsense_output.h"

using namespace ds5::output;

namespace {

uint8_t state_data[kAudioStateSnapshotSize] = {
    0xfd, 0xe3, 0x0, 0x0,
    0x7f, 0x64,
    0xff, 0x9, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xa,
    0x4, 0x0, 0x0, 0x0, 0x1,
    0x00,
    0x00, 0x00, 0xff,
};

uint8_t cached_right_trigger[kTriggerEffectSize]{};
uint8_t cached_left_trigger[kTriggerEffectSize]{};
bool cached_right_trigger_valid = false;
bool cached_left_trigger_valid = false;
uint8_t cached_trigger_power = 0;
bool cached_trigger_power_valid = false;
bool player_led_enabled = true;
uint8_t cached_player_leds = 0;
bool cached_player_leds_valid = false;

void clamp_speaker_volume() {
    if (state_data[kSpeakerVolumeOffset] > kSpeakerVolumeMax) {
        state_data[kSpeakerVolumeOffset] = kSpeakerVolumeMax;
    }
}

void clear_mic_control(uint8_t *payload) {
    if (payload == nullptr) {
        return;
    }

    payload[kValidFlag0Offset] = static_cast<uint8_t>(
        payload[kValidFlag0Offset] & static_cast<uint8_t>(~kFlag0MicVolumeEnable)
    );
    payload[kValidFlag1Offset] = static_cast<uint8_t>(
        payload[kValidFlag1Offset] & static_cast<uint8_t>(~kFlag1MicMuteLedControlEnable)
    );
    if ((payload[kValidFlag1Offset] & kFlag1PowerSaveControlEnable) != 0) {
        payload[kPowerSaveControlOffset] = static_cast<uint8_t>(
            payload[kPowerSaveControlOffset] & static_cast<uint8_t>(~kPowerSaveControlMicMute)
        );
        if (payload[kPowerSaveControlOffset] == 0) {
            payload[kValidFlag1Offset] = static_cast<uint8_t>(
                payload[kValidFlag1Offset] & static_cast<uint8_t>(~kFlag1PowerSaveControlEnable)
            );
        }
    }
    payload[kMicVolumeOffset] = 0;
    payload[kMuteLedOffset] = 0;
}

uint8_t scale_lightbar_channel(uint8_t channel, uint8_t brightness_percent) {
    return scaled_percent(channel, brightness_percent);
}

void apply_player_led_policy(uint8_t *payload) {
    if (payload == nullptr) {
        return;
    }
    if (player_led_enabled) {
        return;
    }
    payload[kValidFlag1Offset] |= kFlag1PlayerIndicatorControlEnable;
    payload[kPlayerLedsOffset] = 0;
}

void cache_player_leds_from_payload(uint8_t const *payload, uint8_t len) {
    if (payload == nullptr || len <= kValidFlag1Offset) {
        return;
    }
    const uint8_t flag1 = payload[kValidFlag1Offset];
    if ((flag1 & kFlag1ReleaseLeds) != 0) {
        cached_player_leds = 0;
        cached_player_leds_valid = false;
        return;
    }
    if (len <= kPlayerLedsOffset || (flag1 & kFlag1PlayerIndicatorControlEnable) == 0) {
        return;
    }

    cached_player_leds = payload[kPlayerLedsOffset];
    cached_player_leds_valid = true;
}

void release_player_led_policy(uint8_t *payload) {
    if (payload == nullptr) {
        return;
    }
    payload[kValidFlag1Offset] = static_cast<uint8_t>(
        payload[kValidFlag1Offset] & static_cast<uint8_t>(~kFlag1PlayerIndicatorControlEnable)
    );
    payload[kPlayerLedsOffset] = 0;
}

void restore_player_led_policy(uint8_t *payload) {
    if (payload == nullptr) {
        return;
    }
    if (!cached_player_leds_valid) {
        release_player_led_policy(payload);
        return;
    }

    payload[kValidFlag1Offset] = static_cast<uint8_t>(
        (payload[kValidFlag1Offset] & static_cast<uint8_t>(~kFlag1ReleaseLeds))
        | kFlag1PlayerIndicatorControlEnable
    );
    payload[kPlayerLedsOffset] = cached_player_leds;
}

void apply_current_player_led_policy(uint8_t *payload) {
    if (player_led_enabled) {
        restore_player_led_policy(payload);
    } else {
        apply_player_led_policy(payload);
    }
}

void clear_adaptive_trigger_effects(uint8_t *payload) {
    if (payload == nullptr) {
        return;
    }

    payload[kValidFlag0Offset] = static_cast<uint8_t>(
        payload[kValidFlag0Offset]
        & static_cast<uint8_t>(~(kFlag0RightTriggerEffect | kFlag0LeftTriggerEffect))
    );
    std::memset(payload + kTriggerEffectRightOffset, 0, kTriggerEffectSize);
    std::memset(payload + kTriggerEffectLeftOffset, 0, kTriggerEffectSize);
}

} // namespace

void controller_output_state_clear_triggers(uint8_t *payload) {
    if (payload == nullptr) {
        return;
    }

    clear_adaptive_trigger_effects(payload);
    payload[kValidFlag1Offset] = static_cast<uint8_t>(
        payload[kValidFlag1Offset] & static_cast<uint8_t>(~kFlag1MotorPowerLevelEnable)
    );
    payload[kTriggerPowerOffset] = 0;
}

void controller_output_state_clear_zero_rumble(uint8_t *payload) {
    if (payload == nullptr) {
        return;
    }

    const uint8_t rumble_flag0 = payload[kValidFlag0Offset] & static_cast<uint8_t>(
        kFlag0CompatibleVibration | kFlag0HapticsSelect
    );
    const uint8_t rumble_flag2 = payload[kValidFlag2Offset] & kFlag2CompatibleVibration2;
    if ((rumble_flag0 | rumble_flag2) == 0) {
        return;
    }
    if ((payload[kMotorRightOffset] | payload[kMotorLeftOffset]) != 0) {
        return;
    }

    payload[kValidFlag0Offset] = static_cast<uint8_t>(payload[kValidFlag0Offset] & static_cast<uint8_t>(~rumble_flag0));
    payload[kValidFlag2Offset] = static_cast<uint8_t>(payload[kValidFlag2Offset] & static_cast<uint8_t>(~rumble_flag2));
}

void controller_output_state_reset_cached_triggers() {
    cached_right_trigger_valid = false;
    cached_left_trigger_valid = false;
    cached_trigger_power = 0;
    cached_trigger_power_valid = false;
}

void controller_output_state_reset_cached_player_leds() {
    cached_player_leds = 0;
    cached_player_leds_valid = false;
}

void controller_output_state_apply_host_payload(uint8_t const *data, uint8_t len) {
    if (data == nullptr) {
        return;
    }

    const uint8_t copy_len = len > sizeof(state_data) ? sizeof(state_data) : len;
    std::memcpy(state_data, data, copy_len);
    if (copy_len < sizeof(state_data)) {
        std::memset(state_data + copy_len, 0, sizeof(state_data) - copy_len);
    }
    cache_player_leds_from_payload(state_data, copy_len);
    controller_output_state_clear_zero_rumble(state_data);
    clear_mic_control(state_data);

    if (
        (state_data[kValidFlag0Offset] & kFlag0RightTriggerEffect) != 0
        && copy_len > kTriggerEffectRightOffset + kTriggerEffectSize - 1
    ) {
        std::memcpy(cached_right_trigger, state_data + kTriggerEffectRightOffset, sizeof(cached_right_trigger));
        cached_right_trigger_valid = true;
    } else if (cached_right_trigger_valid) {
        state_data[kValidFlag0Offset] |= kFlag0RightTriggerEffect;
        std::memcpy(state_data + kTriggerEffectRightOffset, cached_right_trigger, sizeof(cached_right_trigger));
    }

    if (
        (state_data[kValidFlag0Offset] & kFlag0LeftTriggerEffect) != 0
        && copy_len > kTriggerEffectLeftOffset + kTriggerEffectSize - 1
    ) {
        std::memcpy(cached_left_trigger, state_data + kTriggerEffectLeftOffset, sizeof(cached_left_trigger));
        cached_left_trigger_valid = true;
    } else if (cached_left_trigger_valid) {
        state_data[kValidFlag0Offset] |= kFlag0LeftTriggerEffect;
        std::memcpy(state_data + kTriggerEffectLeftOffset, cached_left_trigger, sizeof(cached_left_trigger));
    }

    if (
        (state_data[kValidFlag1Offset] & kFlag1MotorPowerLevelEnable) != 0
        && copy_len > kTriggerPowerOffset
    ) {
        cached_trigger_power = state_data[kTriggerPowerOffset];
        cached_trigger_power_valid = true;
    } else if (cached_trigger_power_valid) {
        state_data[kValidFlag1Offset] |= kFlag1MotorPowerLevelEnable;
        state_data[kTriggerPowerOffset] = cached_trigger_power;
    }

    clamp_speaker_volume();
    apply_current_player_led_policy(state_data);
}

void controller_output_state_set_adaptive_trigger(
    uint8_t const *right_trigger,
    bool right_valid,
    uint8_t const *left_trigger,
    bool left_valid,
    uint8_t motor_power,
    bool motor_power_valid
) {
    if (right_valid && right_trigger != nullptr) {
        std::memcpy(cached_right_trigger, right_trigger, sizeof(cached_right_trigger));
        cached_right_trigger_valid = true;
        state_data[kValidFlag0Offset] |= kFlag0RightTriggerEffect;
        std::memcpy(state_data + kTriggerEffectRightOffset, right_trigger, kTriggerEffectSize);
    }
    if (left_valid && left_trigger != nullptr) {
        std::memcpy(cached_left_trigger, left_trigger, sizeof(cached_left_trigger));
        cached_left_trigger_valid = true;
        state_data[kValidFlag0Offset] |= kFlag0LeftTriggerEffect;
        std::memcpy(state_data + kTriggerEffectLeftOffset, left_trigger, kTriggerEffectSize);
    }
    if (motor_power_valid) {
        cached_trigger_power = motor_power;
        cached_trigger_power_valid = true;
        state_data[kValidFlag1Offset] |= kFlag1MotorPowerLevelEnable;
        state_data[kTriggerPowerOffset] = motor_power;
    }
}

void controller_output_state_set_lightbar(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness_percent) {
    const uint8_t brightness = std::min<uint8_t>(brightness_percent, 100);
    state_data[kValidFlag1Offset] = static_cast<uint8_t>(
        (
            state_data[kValidFlag1Offset]
            & static_cast<uint8_t>(~kFlag1ReleaseLeds)
        )
        | kFlag1LightbarControlEnable
    );
    apply_current_player_led_policy(state_data);
    state_data[kValidFlag2Offset] = static_cast<uint8_t>(
        state_data[kValidFlag2Offset] & static_cast<uint8_t>(~kLightbarSetupControlMask)
    );
    state_data[kLedBrightnessOffset] = 0x01;
    state_data[kLightbarRedOffset] = scale_lightbar_channel(red, brightness);
    state_data[kLightbarGreenOffset] = scale_lightbar_channel(green, brightness);
    state_data[kLightbarBlueOffset] = scale_lightbar_channel(blue, brightness);
}

void controller_output_state_set_player_led_enabled(bool enabled) {
    player_led_enabled = enabled;
    apply_current_player_led_policy(state_data);
}

bool controller_output_state_copy_player_led_report(uint8_t *destination, uint16_t len) {
    if (destination == nullptr || len <= kPlayerLedsOffset) {
        return false;
    }

    if (!player_led_enabled) {
        destination[kValidFlag1Offset] |= kFlag1PlayerIndicatorControlEnable;
        destination[kPlayerLedsOffset] = 0;
        return true;
    }

    if (!cached_player_leds_valid) {
        return false;
    }

    destination[kValidFlag1Offset] = static_cast<uint8_t>(
        (destination[kValidFlag1Offset] & static_cast<uint8_t>(~kFlag1ReleaseLeds))
        | kFlag1PlayerIndicatorControlEnable
    );
    destination[kPlayerLedsOffset] = cached_player_leds;
    return true;
}

void controller_output_state_copy_audio_snapshot(uint8_t *destination, bool headset_plugged) {
    if (destination == nullptr) {
        return;
    }

    std::memcpy(destination, state_data, sizeof(state_data));
    controller_output_state_clear_zero_rumble(destination);
    clear_mic_control(destination);
    apply_current_player_led_policy(destination);
    if (headset_plugged) {
        destination[kValidFlag0Offset] = static_cast<uint8_t>(
            (destination[kValidFlag0Offset] | kFlag0AudioControlEnable)
            & static_cast<uint8_t>(~kFlag0SpeakerVolumeEnable)
        );
        destination[kValidFlag1Offset] = static_cast<uint8_t>(
            destination[kValidFlag1Offset] & static_cast<uint8_t>(~kFlag1AudioControl2Enable)
        );
        destination[kHeadphoneVolumeOffset] = kHeadphoneVolumeMax;
        destination[kSpeakerVolumeOffset] = 0x00;
        destination[kAudioControlOffset] = kAudioFlagsOutputPathHeadphones;
        destination[kAudioControl2Offset] = 0x00;
        clear_adaptive_trigger_effects(destination);
        return;
    }

    destination[kValidFlag0Offset] |= static_cast<uint8_t>(
        kFlag0AudioControlEnable | kFlag0SpeakerVolumeEnable
    );
    destination[kHeadphoneVolumeOffset] = kHeadphoneVolumeMax;
    destination[kValidFlag1Offset] |= kFlag1AudioControl2Enable;
    destination[kSpeakerVolumeOffset] = kSpeakerVolumeMax;
    destination[kAudioControlOffset] = kAudioFlagsOutputPathSpeaker;
    destination[kAudioControl2Offset] = kAudioFlags2SpeakerPreampGain;
    clear_adaptive_trigger_effects(destination);
}
