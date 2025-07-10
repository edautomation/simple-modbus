/*
 * simple-modbus-rtu: Minimal Modbus RTU frame state machine for embedded systems
 *
 * This module provides a lightweight, single-instance Modbus RTU frame handler,
 * designed for use in embedded and bare-metal applications. It implements the
 * Modbus RTU frame detection state machine, including character timeouts,
 * and provides a simple interface for integrating with UART drivers and timer
 * interrupts.
 *
 * Usage:
 *   - Implement the smb_rtu_if_t interface to connect your UART and timer logic.
 *   - Call smb_rtu_config() to initialize the RTU handler with your server address,
 *     baud rate, and interface implementation.
 *   - From your UART RX interrupt, call smb_rtu_receive() for each received byte,
 *      or better yet, notify the main loop with the received byte and call
 *      smb_rtu_receive() from there.
 *   - From your timer interrupt (configured for 1.5 or 3.5 character times by the
        start_counter() callback), call smb_rtu_timer_timeout(), or better yet,
        notify the main loop and call smb_rtu_timer_timeout() from there.
 *   - Use smb_rtu_read_pdu() to retrieve a received Modbus PDU, and
 *     smb_rtu_write_pdu() to send a response.
 *
 * Limitations:
 *   - Only one RTU handler instance is supported per application.
 *   - The user must provide UART and timer integration via the interface.
 *   - This module does not implement Modbus function code handling; it only
 *     detects and buffers RTU frames.
 *
 * See https://modbus.org/docs/Modbus_over_serial_line_V1_02.pdf for details
 * about the Modbus RTU protocol.
 *
 * simple-modbus-rtu is licensed under the MIT License. See the LICENSE file in the
 * project's root directory for more information.
 */
#ifndef SIMPLE_MODBUS_RTU_H_
#define SIMPLE_MODBUS_RTU_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Interface for Modbus RTU frame handling.
 *
 * Implementations MUST provide functions to start a timer for character times,
 * write bytes to the UART, and notify when a complete frame is received.
 */
struct smb_rtu_if_t
{
    /**
     * @brief Start or restart the 1.5 or 3.5 character time counter.
     *
     * @param count_duration_us Duration in microseconds for 1.5 and
     * 3.5 character times.
     */
    void (*start_counter)(uint16_t count_duration_us);

    /**
     * @brief Write bytes to the UART.
     *
     * @param bytes Pointer to the data to send.
     * @param length Number of bytes to send.
     * @return <0 on error, number of bytes written on success.
     */
    int16_t (*write)(const uint8_t* bytes, uint16_t length);

    /**
     * @brief Callback invoked when a complete frame is received.
     */
    void (*frame_received)(void);
};

/**
 * @brief Reset the Modbus RTU state machine.
 */
void smb_rtu_reset(void);

/**
 * @brief Configure the Modbus RTU handler.
 *
 * @param server_addr Modbus server address (1-247).
 * @param baud_rate UART baud rate.
 * @param interface Pointer to the RTU interface implementation.
 * @return 0 on success,
 *         -EINVAL for an unsupported baud rate or wrong server address
 *         -EFAULT on null pointers
 */
int16_t smb_rtu_config(uint8_t server_addr,
                       uint32_t baud_rate,
                       const struct smb_rtu_if_t* interface);

/**
 * @brief Process a received byte (call from UART RX interrupt).
 *
 * Call this function for each received byte to detect RTU frames using the
 * 1.5 and 3.5 character timeouts.
 *
 * @param bytes The received byte.
 * @return <0 on error, 0 or positive value on success.
 */
int16_t smb_rtu_receive(uint8_t bytes);

/**
 * @brief Handle timer timeout (call from timer interrupt).
 *
 * Call this function when the 1.5 or 3.5 character time has elapsed
 *
 * @return <0 on error, 0 on success.
 */
int16_t smb_rtu_timer_timeout(void);

/**
 * @brief Read a received Modbus PDU.
 *
 * @param buffer Pointer to the buffer to store the PDU.
 * @param length Length of the buffer in bytes.
 * @return 0 if no PDU is available,
 *         otherwise the length of the PDU in bytes,
 *         -EINVAL if the buffer is too small,
 *         <0 on other errors.
 */
int16_t smb_rtu_read_pdu(uint8_t* buffer, uint16_t length);

/**
 * @brief Write a Modbus PDU for transmission.
 *
 * @param buffer Pointer to the buffer containing the PDU to send.
 * @param length Length of the PDU in bytes (must be <= 256).
 * @return 0 if all bytes could be written,
 *         -EAGAIN if bytes still need to be written,
 *         -EBUSY if no bytes can be written at the moment,
 *         <0 on error.
 *
 * !! The user is responsible for calling this function until it returns 0 !!
 */
int16_t smb_rtu_write_pdu(uint8_t* buffer, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif  // V
