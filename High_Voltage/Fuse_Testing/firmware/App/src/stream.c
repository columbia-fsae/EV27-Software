/**
 * @file stream.c
 * @brief Framed binary stream to the PC over LPUART1 (ST-LINK virtual COM port)
 *
 * Frames go into a ring buffer that the TXE interrupt drains, so the sampling loop never waits
 * on the UART. A frame that does not fit is dropped whole and counted.
 */

#include "stream.h"

#include <stdarg.h>
#include <stdio.h>

#include "app_config.h"
#include "main.h"

extern UART_HandleTypeDef hlpuart1;

static uint8_t tx_buf[PC_TX_BUFFER_SIZE];
static volatile uint32_t tx_head;  // written by main context
static volatile uint32_t tx_tail;  // written by the IRQ
static uint32_t dropped_frames;

static uint16_t crc16_ccitt(uint16_t crc, const uint8_t* data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void stream_init(void) {
    tx_head = 0;
    tx_tail = 0;
    dropped_frames = 0;
    HAL_NVIC_SetPriority(LPUART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(LPUART1_IRQn);
}

static uint32_t tx_free(void) {
    return (PC_TX_BUFFER_SIZE - 1U) - ((tx_head - tx_tail) % PC_TX_BUFFER_SIZE);
}

static void tx_put(uint8_t byte) {
    tx_buf[tx_head] = byte;
    tx_head = (tx_head + 1U) % PC_TX_BUFFER_SIZE;
}

void stream_send(uint8_t type, const void* payload, uint8_t len) {
    const uint32_t frame_len = 2U + 2U + len + 2U;
    if (tx_free() < frame_len) {
        dropped_frames++;
        return;
    }

    uint8_t header[2] = {type, len};
    uint16_t crc = crc16_ccitt(0xFFFFU, header, sizeof(header));
    crc = crc16_ccitt(crc, (const uint8_t*)payload, len);

    tx_put(STREAM_SYNC0);
    tx_put(STREAM_SYNC1);
    tx_put(type);
    tx_put(len);
    for (uint8_t i = 0; i < len; i++) {
        tx_put(((const uint8_t*)payload)[i]);
    }
    tx_put((uint8_t)crc);
    tx_put((uint8_t)(crc >> 8));

    // Start (or keep) the TXE interrupt draining the buffer
    SET_BIT(hlpuart1.Instance->CR1, USART_CR1_TXEIE_TXFNFIE);
}

void stream_printf(const char* fmt, ...) {
    char text[200];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    if (n < 0) {
        return;
    }
    if (n >= (int)sizeof(text)) {
        n = sizeof(text) - 1;
    }
    stream_send(STREAM_TYPE_TEXT, text, (uint8_t)n);
}

uint32_t stream_dropped_frames(void) { return dropped_frames; }

int stream_getc(void) {
    USART_TypeDef* uart = hlpuart1.Instance;
    if (uart->ISR & USART_ISR_ORE) {
        uart->ICR = USART_ICR_ORECF;
    }
    if (uart->ISR & USART_ISR_RXNE_RXFNE) {
        return (int)(uart->RDR & 0xFFU);
    }
    return -1;
}

void stream_irq_handler(void) {
    USART_TypeDef* uart = hlpuart1.Instance;
    if ((uart->CR1 & USART_CR1_TXEIE_TXFNFIE) && (uart->ISR & USART_ISR_TXE_TXFNF)) {
        if (tx_tail != tx_head) {
            uart->TDR = tx_buf[tx_tail];
            tx_tail = (tx_tail + 1U) % PC_TX_BUFFER_SIZE;
        } else {
            CLEAR_BIT(uart->CR1, USART_CR1_TXEIE_TXFNFIE);
        }
    }
}
