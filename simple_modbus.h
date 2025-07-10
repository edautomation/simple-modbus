/*
 * simple-modbus: Minimal Modbus RTU server implementation in C/C++
 *
 * This module provides a lightweight, single-instance Modbus RTU server core,
 * designed for embedded and bare-metal applications. It supports basic Modbus
 * function codes for reading and writing registers, and is platform-agnostic:
 * you can use it on any platform by providing your own transport and register
 * access callbacks.
 *
 * Usage:
 *   - Implement the smb_transport_if_t and smb_server_if_t interfaces.
 *   - Call smb_server_config() to initialize the server.
 *   - Periodically call smb_server_poll() to process requests and send responses.
 *
 * Limitations:
 *   - Only one server instance is supported per application.
 *   - The user must provide register access and transport callbacks.
 *
 * See https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf for detailed
 * information about the Modbus protocol.
 *
 * simple-modbus is licensed under the MIT License. See the LICENSE file in the
 * project's root directory for more information.
 */

#ifndef SIMPLE_MODBUS_H_
#define SIMPLE_MODBUS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Transport interface for Simple Modbus server.
 *
 * Implementations MUST provide functions to read and write complete Modbus frames.
 * These functions may be blocking, but can also be non-blocking by simply returning
 * 0 if no complete frame is available yet.
 */
struct smb_transport_if_t
{
    /**
     * @brief Read a Modbus frame from the transport.
     *
     * A frame comprises the server address, function code, data, and CRC.
     *
     * @param buffer Pointer to the buffer to store the received frame.
     * @param max_length Maximum number of bytes to read into the buffer.
     * @return < 0 on error,
     *         0 if no frame is available or the frame is not complete yet,
     *         otherwise the length of the entire frame (including CRC).
     */
    int16_t (*read_frame)(uint8_t* buffer, uint16_t max_length);

    /**
     * @brief Write a Modbus frame to the transport.
     *
     * A frame comprises the server address, function code, data, and CRC.
     *
     * @param buffer Pointer to the buffer containing the frame to send.
     * @param length Number of bytes to write.
     * @return < 0 on error,
     *        0 if all bytes could be written
     *        any other positive value otherwise
     *
     * @note If write_frame returns a positive value, it will be called again
     *       with the same arguments, until it returns 0 or <0. It is up to
     *       the implementer of this function to manage which bytes must be sent.
     */
    int16_t (*write_frame)(uint8_t* buffer, uint16_t length);
};

/**
 * @brief Server callback interface
 *
 * Implementations MAY provide callbacks for register access.
 * Setting the callbacks to NULL will disable the corresponding functionality.
 * e.g., if read_holding_regs is NULL, the server will reply with exception
 * code 0x01 (Illegal function).
 */
struct smb_server_if_t
{
    /**
     * @brief Callback to read input registers.
     *
     * `n_regs` is guaranteed to be smaller or equal to 125
     *
     * @param[out] buffer Pointer to the buffer to store register values
     * @param[in] length Number of registers to read.
     * @param[in] start_addr Starting register address.
     * @return 0 if busy,
     *         2 * n_regs on success,
     *         any other value if there is an error in the provided data
     *          (e.g., wrong address or number of registers).
     */
    int16_t (*read_input_regs)(uint8_t* const buffer,
                               uint16_t n_regs,
                               uint16_t start_addr);

    /**
     * @brief Callback to read holding registers.
     *
     * `n_regs` is guaranteed to be smaller or equal to 125
     *
     * @param[out] buffer Pointer to the buffer to store register values
     * @param[in] length Number of registers to read.
     * @param[in] start_addr Starting register address.
     * @return 0 if busy,
     *         2 * n_regs on success,
     *         any other value if there is an error in the provided data
     *          (e.g., wrong address or number of registers).
     */
    int16_t (*read_holding_regs)(uint8_t* const buffer,
                                 uint16_t n_regs,
                                 uint16_t start_addr);

    /**
     * @brief Callback to write holding registers.
     *
     * Used by both function codes 0x06 (Write Single Register) and
     * 0x10 (Write Multiple Registers).
     *
     * @param[in] buffer Pointer to the buffer containing register values to write
     * @param[in] length Number of registers to write.
     * @param[in] start_addr Starting register address.
     * @return 0 if busy,
     *         2 * n_regs on success,
     *         any other value if there is an error in the provided data
     *          (e.g., wrong address or number of registers).
     */
    int16_t (*write_regs)(const uint8_t* const buffer,
                          uint16_t n_regs,
                          uint16_t start_addr);
};

/**
 * @brief Configure the Simple Modbus server.
 *
 * This function must be called before using the server.
 *
 * @param server_addr Modbus server address (1-247).
 * @param transport Pointer to the transport interface implementation.
 * @param server_cb Pointer to the server callback interface implementation.
 * @return 0 on success,
 *         negative errno value on error (e.g., -EINVAL for invalid arguments).
 */
int16_t smb_server_config(uint8_t server_addr,
                          const struct smb_transport_if_t* transport,
                          const struct smb_server_if_t* server_cb);

/**
 * @brief Poll the Simple Modbus server.
 *
 * Call this function periodically to process incoming requests and send responses.
 *
 * @return 0 on success or no action,
 *         negative errno value on error,
 *         -EAGAIN if the operation should be retried (e.g., partial write).
 */
int16_t smb_server_poll(void);

#ifdef __cplusplus
}
#endif

#endif  // SIMPLE_MODBUS_H_
