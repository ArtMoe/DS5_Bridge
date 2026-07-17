#ifndef DS5_BRIDGE_CORE1_FLASH_SAFETY_H
#define DS5_BRIDGE_CORE1_FLASH_SAFETY_H

// Called from the RAM-resident core-1 loop. Flash operations are initiated on
// core 0 only; this parks core 1 in RAM with interrupts disabled until core 0
// restores XIP.
void core1_flash_safety_poll();

#endif // DS5_BRIDGE_CORE1_FLASH_SAFETY_H
