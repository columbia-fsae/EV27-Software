/**
 * @file stm32g4xx_it.c
 * @brief Interrupt handlers
 */

#include "stm32g4xx_it.h"

#include "main.h"
#include "stream.h"

void NMI_Handler(void) {
    while (1) {
    }
}

void HardFault_Handler(void) {
    while (1) {
    }
}

void MemManage_Handler(void) {
    while (1) {
    }
}

void BusFault_Handler(void) {
    while (1) {
    }
}

void UsageFault_Handler(void) {
    while (1) {
    }
}

void SVC_Handler(void) {}

void DebugMon_Handler(void) {}

void PendSV_Handler(void) {}

void SysTick_Handler(void) { HAL_IncTick(); }

void LPUART1_IRQHandler(void) { stream_irq_handler(); }
