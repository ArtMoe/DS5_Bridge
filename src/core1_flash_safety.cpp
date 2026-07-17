#include "core1_flash_safety.h"

#include <atomic>
#include <cstdint>

#include "hardware/sync.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "pico/time.h"

namespace {

enum Core1FlashPauseState : uint8_t {
    Core1FlashPauseIdle = 0,
    Core1FlashPauseRequested = 1,
    Core1FlashPausePaused = 2,
    Core1FlashPauseResumeRequested = 3,
};

std::atomic<uint8_t> core1_flash_pause_state{Core1FlashPauseIdle};
std::atomic_bool core1_flash_safety_ready{false};
static_assert(std::atomic<uint8_t>::is_always_lock_free);
static_assert(std::atomic_bool::is_always_lock_free);

uint32_t core0_interrupt_state = 0;
bool core0_safe_zone_active = false;

bool cooperative_core_init_deinit(bool init) {
    if (get_core_num() != 1u) {
        return true;
    }
    if (!init && core1_flash_pause_state.load(std::memory_order_acquire) != Core1FlashPauseIdle) {
        return false;
    }
    core1_flash_safety_ready.store(init, std::memory_order_release);
    return true;
}

int cooperative_enter_safe_zone(uint32_t timeout_ms) {
    if (
        get_core_num() != 0u
        || core0_safe_zone_active
        || !core1_flash_safety_ready.load(std::memory_order_acquire)
    ) {
        return PICO_ERROR_NOT_PERMITTED;
    }

    uint8_t expected_state = Core1FlashPauseIdle;
    if (!core1_flash_pause_state.compare_exchange_strong(
        expected_state,
        Core1FlashPauseRequested,
        std::memory_order_acq_rel,
        std::memory_order_acquire
    )) {
        return PICO_ERROR_NOT_PERMITTED;
    }

    const absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (core1_flash_pause_state.load(std::memory_order_acquire) != Core1FlashPausePaused) {
        if (time_reached(deadline)) {
            expected_state = Core1FlashPauseRequested;
            if (core1_flash_pause_state.compare_exchange_strong(
                expected_state,
                Core1FlashPauseIdle,
                std::memory_order_acq_rel,
                std::memory_order_acquire
            )) {
                return PICO_ERROR_TIMEOUT;
            }
        }
        tight_loop_contents();
    }

    core0_interrupt_state = save_and_disable_interrupts();
    core0_safe_zone_active = true;
    return PICO_OK;
}

int cooperative_exit_safe_zone(uint32_t timeout_ms) {
    if (get_core_num() != 0u || !core0_safe_zone_active) {
        return PICO_ERROR_NOT_PERMITTED;
    }

    restore_interrupts_from_disabled(core0_interrupt_state);
    core0_safe_zone_active = false;
    core1_flash_pause_state.store(Core1FlashPauseResumeRequested, std::memory_order_release);

    const absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (core1_flash_pause_state.load(std::memory_order_acquire) != Core1FlashPauseIdle) {
        if (time_reached(deadline)) {
            return PICO_ERROR_TIMEOUT;
        }
        tight_loop_contents();
    }
    return PICO_OK;
}

flash_safety_helper_t cooperative_flash_safety_helper{
    .core_init_deinit = cooperative_core_init_deinit,
    .enter_safe_zone_timeout_ms = cooperative_enter_safe_zone,
    .exit_safe_zone_timeout_ms = cooperative_exit_safe_zone,
};

} // namespace

extern "C" flash_safety_helper_t *get_flash_safety_helper() {
    return &cooperative_flash_safety_helper;
}

void __not_in_flash_func(core1_flash_safety_poll)() {
    if (core1_flash_pause_state.load(std::memory_order_acquire) != Core1FlashPauseRequested) {
        return;
    }

    const uint32_t interrupt_state = save_and_disable_interrupts();
    uint8_t expected_state = Core1FlashPauseRequested;
    if (!core1_flash_pause_state.compare_exchange_strong(
        expected_state,
        Core1FlashPausePaused,
        std::memory_order_acq_rel,
        std::memory_order_acquire
    )) {
        restore_interrupts_from_disabled(interrupt_state);
        return;
    }

    while (core1_flash_pause_state.load(std::memory_order_acquire) == Core1FlashPausePaused) {
        tight_loop_contents();
    }
    restore_interrupts_from_disabled(interrupt_state);
    core1_flash_pause_state.store(Core1FlashPauseIdle, std::memory_order_release);
}
