//
// BOOTSEL button gestures.
//

#include "button_functions.h"

#include <cstdint>

#include "bt.h"
#include "kitsune_button_gesture.h"
#include "utils.h"

#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#if PICO_RP2350
#include "hardware/regs/sio.h"
#endif
#include "pico/bootrom.h"
#include "pico/flash.h"
#include "pico/time.h"

// Gesture thresholds, in samples at the 100 ms poll cadence.
static constexpr uint32_t BUTTON_POLL_INTERVAL_MS = 100;
static constexpr uint32_t BUTTON_FLASH_SAFE_TIMEOUT_MS = 5;
static kitsune::ButtonGesture button_gesture({
    5,  // ~500 ms max press for a click.
    10, // ~1000 ms allowed between clicks before a single click dispatches.
    15, // ~1500 ms hold threshold.
});
static uint32_t button_last_check_ms = 0;

static void __no_inline_not_in_flash_func(button_read_cb)(void *param) {
    bool *pressed = static_cast<bool *>(param);
    constexpr uint CS_PIN_INDEX = 1;

    hw_write_masked(
        &ioqspi_hw->io[CS_PIN_INDEX].ctrl,
        GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS
    );

    for (int i = 0; i < 1000; ++i) {
        __asm volatile("nop");
    }

#if PICO_RP2350
    *pressed = !(sio_hw->gpio_hi_in & SIO_GPIO_HI_IN_QSPI_CSN_BITS);
#else
    *pressed = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));
#endif

    hw_write_masked(
        &ioqspi_hw->io[CS_PIN_INDEX].ctrl,
        GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS
    );
}

static bool button_read_bootsel(bool &pressed) {
    return flash_safe_execute(
        button_read_cb,
        &pressed,
        BUTTON_FLASH_SAFE_TIMEOUT_MS
    ) == PICO_OK;
}

static void button_dispatch(kitsune::ButtonGestureEvent event) {
    switch (event) {
        case kitsune::ButtonGestureEvent::Click:
            if (bt_is_controller_connected()) {
                DS5_LOG("[BTN] BOOTSEL click - disconnect controller and preserve pairing\n");
                (void)bt_disconnect_with_intent(BtControllerDisconnectIntentSleep);
                return;
            }
            DS5_LOG("[BTN] BOOTSEL click - request controller scan\n");
            (void)bt_request_scan();
            return;

        case kitsune::ButtonGestureEvent::DoubleClick:
            DS5_LOG("[BTN] BOOTSEL double click - reboot\n");
            watchdog_reboot(0, 0, 0);
            while (true) {
                tight_loop_contents();
            }

        case kitsune::ButtonGestureEvent::TripleClick:
            DS5_LOG("[BTN] BOOTSEL triple click - reboot to BOOTSEL\n");
            reset_usb_boot(0, 0);
            while (true) {
                tight_loop_contents();
            }

        case kitsune::ButtonGestureEvent::Hold:
            DS5_LOG("[BTN] BOOTSEL hold - forget controller pairings\n");
            (void)bt_forget_pairings();
            return;

        default:
            return;
    }
}

void button_check() {
    const uint32_t now = to_ms_since_boot(get_absolute_time());
    if (static_cast<uint32_t>(now - button_last_check_ms) < BUTTON_POLL_INTERVAL_MS) {
        return;
    }
    button_last_check_ms = now;

    bool pressed = false;
    if (!button_read_bootsel(pressed)) {
        // Failure to park the other core is not a physical button release.
        // Preserve the gesture and retry on the next sample boundary.
        return;
    }
    button_dispatch(button_gesture.update(pressed));
}
