#ifndef DS5_BRIDGE_HOST_AUDIO_RUNTIME_H
#define DS5_BRIDGE_HOST_AUDIO_RUNTIME_H

#include <cstdint>

#include "audio.h"

struct HostAudioRuntimeState {
    volatile bool requested = false;
    volatile bool stream_active = false;
    volatile bool duplex_requested = false;
    volatile uint32_t last_heartbeat_us = 0;
    volatile uint32_t stream_started_us = 0;
    volatile uint32_t last_frame_us = 0;
    volatile uint32_t request_started_us = 0;
    uint16_t stream_generation = 0;
    AudioRuntimeMode mode = AudioRuntimeFallbackPicoLocal;
    AudioFallbackReason fallback_reason = AudioFallbackHostDisabled;

    bool heartbeat_healthy(uint32_t now, uint32_t timeout_us) const {
        return last_heartbeat_us != 0
            && static_cast<uint32_t>(now - last_heartbeat_us) < timeout_us;
    }

    bool start_grace_active(uint32_t now, uint32_t grace_us) const {
        const uint32_t started = stream_active ? stream_started_us : request_started_us;
        return started != 0 && static_cast<uint32_t>(now - started) < grace_us;
    }

    uint32_t last_contact_us() const {
        uint32_t contact = last_heartbeat_us;
        if (last_frame_us != 0 && (contact == 0 || static_cast<int32_t>(last_frame_us - contact) > 0)) {
            contact = last_frame_us;
        }
        if (stream_started_us != 0 && (contact == 0 || static_cast<int32_t>(stream_started_us - contact) > 0)) {
            contact = stream_started_us;
        }
        return contact;
    }

    void bump_generation() {
        stream_generation++;
        if (stream_generation == 0) {
            stream_generation = 1;
        }
    }
};

#endif // DS5_BRIDGE_HOST_AUDIO_RUNTIME_H
