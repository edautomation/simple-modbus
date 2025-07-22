#include "simple_modbus.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MODBUS_MAX_FRAME_SIZE           256
#define MODBUS_MIN_FRAME_SIZE           4  // 4 bytes for: address, function code, CRC (2B)
#define MODBUS_MAX_NUMBER_OF_READ_REGS  0x7D
#define MODBUS_MAX_NUMBER_OF_WRITE_REGS 0x7B

#define MODBUS_FUNC_READ_HOLDING_REGS   0x03
#define MODBUS_FUNC_READ_INPUT_REGS     0x04
#define MODBUS_FUNC_WRITE_SINGLE_REG    0x06
#define MODBUS_FUNC_WRITE_MULTIPLE_REGS 0x10

#define MODBUS_EXC_ILLEGAL_FUNCTION      0x01
#define MODBUS_EXC_ILLEGAL_DATA_ADDRESS  0x02
#define MODBUS_EXC_ILLEGAL_DATA_VALUE    0x03
#define MODBUS_EXC_SERVER_DEVICE_FAILURE 0x04

#define MODBUS_FUNC_READ_INPUT_REGS_FRAME_LENGTH     8   // addr, func code, start addr (2B), quantity of registers (2B), CRC (2B)
#define MODBUS_FUNC_READ_HOLDING_REGS_FRAME_LENGTH   8   // addr, func code, start addr (2B), quantity of registers (2B), CRC (2B)
#define MODBUS_FUNC_WRITE_SINGLE_REG_FRAME_LENGTH    8   // addr, func code, start addr (2B), value (2B), CRC (2B)
#define MODBUS_FUNC_WRITE_MULT_REGS_MIN_FRAME_LENGTH 11  // addr, func code, start addr (2B), quantity (2B), value (2B), CRC (2B)

#define RETURN_IF(x, err) \
    do                    \
    {                     \
        if (x)            \
        {                 \
            return err;   \
        }                 \
    } while (0)

enum server_state_t
{
    SERVER_STATE_IDLE,
    SERVER_STATE_PROCESSING_REQUEST,
    SERVER_STATE_SEND_REPLY,
};

struct server_t
{
    uint8_t addr;
    const struct smb_transport_if_t* transport;
    const struct smb_server_if_t* callbacks;
    enum server_state_t state;
    uint8_t buffer[MODBUS_MAX_FRAME_SIZE];
    uint16_t buffer_index;
    int16_t frame_length;
};

// NOLINTNEXTLINE (false negative)
static struct server_t server_ = {0, NULL, NULL, SERVER_STATE_IDLE, {0}, 0, 0};

static int16_t exec_state_idle(void);
static int16_t process_frame(void);
static int16_t process_read_holding_regs(void);
static int16_t process_read_input_regs(void);
static int16_t process_write_single_reg(void);
static int16_t process_write_multiple_regs(void);
static int16_t process_read_regs(int16_t (*read_func)(uint16_t*, uint16_t, uint16_t));
static int16_t process_write_regs(uint8_t* buffer, uint16_t n_regs);
static void prepare_error_reply(uint8_t addr, uint8_t error_code);
static int16_t send_reply(void);
static void reset_state();
static uint16_t calculate_crc(const uint8_t* data, int16_t length);

int16_t smb_server_config(uint8_t server_addr,
                          const struct smb_transport_if_t* transport,
                          const struct smb_server_if_t* server_cb)
{
    // reset in case of bad arguments
    server_.addr = 0;
    server_.transport = NULL;
    server_.callbacks = NULL;
    server_.state = SERVER_STATE_IDLE;
    server_.buffer_index = 0;
    server_.frame_length = 0;
    // memset is not safe
    // memset_s is not available in all compilers
    for (size_t i = 0; i < sizeof(server_.buffer); i++)
    {
        server_.buffer[i] = 0;
    }

    // sanity check
    RETURN_IF(0 == server_addr, -EINVAL);
    RETURN_IF(NULL == transport, -EFAULT);
    RETURN_IF(NULL == transport->read_frame, -EFAULT);
    RETURN_IF(NULL == transport->write_frame, -EFAULT);
    RETURN_IF(NULL == server_cb, -EFAULT);

    // configure server structure
    server_.addr = server_addr;
    server_.transport = transport;
    server_.callbacks = server_cb;

    return 0;
}

int16_t smb_server_poll(void)
{
    // verify that the server was properly configured
    RETURN_IF(NULL == server_.transport, -EFAULT);
    RETURN_IF(NULL == server_.transport->read_frame, -EFAULT);
    RETURN_IF(NULL == server_.transport->write_frame, -EFAULT);
    RETURN_IF(NULL == server_.callbacks, -EFAULT);

    int16_t ret = 0;
    switch (server_.state)
    {
        case SERVER_STATE_IDLE:
            ret = exec_state_idle();
            break;
        case SERVER_STATE_PROCESSING_REQUEST:
            ret = process_frame();
            break;
        case SERVER_STATE_SEND_REPLY:
            ret = send_reply();
            break;
        default:
            reset_state();
            ret = -EFAULT;
            break;
    }

    return ret;
}

static int16_t exec_state_idle(void)
{
    int16_t ret = 0;
    int16_t read_len = server_.transport->read_frame(server_.buffer, sizeof(server_.buffer));
    if (read_len < 0)
    {
        ret = read_len;  // forward error to caller
    }
    else if (0 == read_len)
    {
        ret = 0;  // no message available, nothing to do
    }
    else if (read_len < MODBUS_MIN_FRAME_SIZE)
    {
        ret = -EBADMSG;
    }
    else
    {
        const int16_t n_crc_byte = (int16_t)2;
        uint16_t crc = calculate_crc(server_.buffer, read_len - n_crc_byte);
        if (crc != (uint16_t)((server_.buffer[read_len - 2] << 8) | server_.buffer[read_len - 1]))
        {
            ret = -EBADMSG;
        }
        else if (server_.buffer[0] == server_.addr)
        {
            server_.frame_length = read_len;
            ret = process_frame();
        }
        else
        {
            // ignore message, not for us
        }
    }

    return ret;
}

static int16_t process_frame()
{
    int16_t ret = 0;
    uint8_t function_code = server_.buffer[1];
    switch (function_code)
    {
        case MODBUS_FUNC_READ_INPUT_REGS:
            ret = process_read_input_regs();
            break;
        case MODBUS_FUNC_READ_HOLDING_REGS:
            ret = process_read_holding_regs();
            break;
        case MODBUS_FUNC_WRITE_SINGLE_REG:
            ret = process_write_single_reg();
            break;
        case MODBUS_FUNC_WRITE_MULTIPLE_REGS:
            ret = process_write_multiple_regs();
            break;
        default:
            prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_FUNCTION);
            ret = send_reply();
            break;
    }
    return ret;
}

static int16_t process_read_holding_regs(void)
{
    int16_t ret = 0;
    if (NULL == server_.callbacks->read_holding_regs)
    {
        prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_FUNCTION);
        ret = send_reply();
    }
    else if (server_.frame_length != MODBUS_FUNC_READ_HOLDING_REGS_FRAME_LENGTH)
    {
        prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_DATA_VALUE);
        ret = send_reply();
    }
    else
    {
        ret = process_read_regs(server_.callbacks->read_holding_regs);
    }
    return ret;
}

static int16_t process_read_input_regs(void)
{
    int16_t ret = 0;
    if (NULL == server_.callbacks->read_input_regs)
    {
        prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_FUNCTION);
        ret = send_reply();
    }
    else if (server_.frame_length != MODBUS_FUNC_READ_INPUT_REGS_FRAME_LENGTH)
    {
        prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_DATA_VALUE);
        ret = send_reply();
    }
    else
    {
        ret = process_read_regs(server_.callbacks->read_input_regs);
    }
    return ret;
}

static int16_t process_write_single_reg(void)
{
    int16_t ret = 0;
    if (NULL == server_.callbacks->write_regs)
    {
        prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_FUNCTION);
        ret = send_reply();
    }
    else if (server_.frame_length != MODBUS_FUNC_WRITE_SINGLE_REG_FRAME_LENGTH)
    {
        prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_DATA_VALUE);
        ret = send_reply();
    }
    else
    {
        ret = process_write_regs(&server_.buffer[4], 1);
    }
    return ret;
}

static int16_t process_write_multiple_regs(void)
{
    int16_t ret = 0;

    if (NULL == server_.callbacks->write_regs)
    {
        prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_FUNCTION);
        ret = send_reply();
    }
    else if (server_.frame_length < MODBUS_FUNC_WRITE_MULT_REGS_MIN_FRAME_LENGTH)
    {
        prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_DATA_VALUE);
        ret = send_reply();
    }
    else
    {
        uint16_t n_regs_high = ((uint16_t)server_.buffer[4] << 8);
        uint16_t n_regs_low = (uint16_t)server_.buffer[5];
        uint16_t n_regs = n_regs_high | n_regs_low;
        uint16_t n_bytes = (uint16_t)server_.buffer[6];

        // addr + func code + start addr (2B) + quantity (2B) +
        // n bytes (1B) + values (2B * n_regs) + CRC (2B)
        uint16_t expected_frame_len = 7 + (2 * n_regs) + 2;

        if ((server_.frame_length != expected_frame_len) ||
            (n_bytes != (2 * n_regs)) ||
            (n_regs > MODBUS_MAX_NUMBER_OF_WRITE_REGS))
        {
            prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_DATA_VALUE);
            ret = send_reply();
        }
        else
        {
            ret = process_write_regs(&server_.buffer[7], n_regs);
        }
    }

    return ret;
}

static int16_t process_read_regs(int16_t (*read_func)(uint16_t*, uint16_t, uint16_t))
{
    int16_t ret = 0;
    uint16_t n_regs_high = ((uint16_t)server_.buffer[4] << 8);
    uint16_t n_regs_low = (uint16_t)server_.buffer[5];
    uint16_t n_regs = n_regs_high | n_regs_low;
    if (n_regs > MODBUS_MAX_NUMBER_OF_READ_REGS)
    {
        prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_DATA_VALUE);
        ret = send_reply();
    }
    else
    {
        uint16_t start_addr_high = ((uint16_t)server_.buffer[2] << 8);
        uint16_t start_addr_low = (uint16_t)server_.buffer[3];
        uint16_t start_addr = start_addr_high | start_addr_low;
        ret = read_func((uint16_t*)&server_.buffer[3], n_regs, start_addr);
        if (ret == 0)
        {
            server_.state = SERVER_STATE_PROCESSING_REQUEST;
            ret = -EAGAIN;
        }
        else if (ret == n_regs)
        {
            uint16_t n_bytes = 2 * n_regs;
            server_.buffer[2] = (uint8_t)n_bytes;  // Already checked bounds above

            static const uint16_t n_header_bytes = 3;
            uint16_t crc = calculate_crc(server_.buffer, n_header_bytes + n_bytes);
            server_.buffer[n_header_bytes + n_bytes] = (crc & 0xFF00) >> 8;
            server_.buffer[n_header_bytes + n_bytes + 1] = (crc & 0x00FF);
            server_.frame_length = n_header_bytes + n_bytes + 2;
            ret = send_reply();
        }
        else
        {
            prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_DATA_ADDRESS);
            ret = send_reply();
        }
    }

    return ret;
}

static int16_t process_write_regs(uint8_t* buffer, uint16_t n_regs)
{
    uint16_t start_addr_high = ((uint16_t)server_.buffer[2] << 8);
    uint16_t start_addr_low = (uint16_t)server_.buffer[3];
    uint16_t start_addr = start_addr_high | start_addr_low;
    int16_t ret = server_.callbacks->write_regs((uint16_t*)buffer, n_regs, start_addr);
    if (ret == 0)
    {
        server_.state = SERVER_STATE_PROCESSING_REQUEST;
        ret = -EAGAIN;
    }
    else if (ret == n_regs)
    {
        // addr + func code + start addr (2B) + quantity (2B)
        static const uint16_t n_response_bytes = 6;
        uint16_t crc = calculate_crc(server_.buffer, n_response_bytes);
        server_.buffer[n_response_bytes] = (crc & 0xFF00) >> 8;
        server_.buffer[n_response_bytes + 1] = (crc & 0x00FF);
        server_.frame_length = n_response_bytes + 2;
        ret = send_reply();
    }
    else
    {
        prepare_error_reply(server_.addr, MODBUS_EXC_ILLEGAL_DATA_ADDRESS);
        ret = send_reply();
    }
    return ret;
}

static void prepare_error_reply(uint8_t addr, uint8_t error_code)
{
    server_.buffer[0] = addr;
    server_.buffer[1] |= 0x80;
    server_.buffer[2] = error_code;

    uint16_t crc = calculate_crc(server_.buffer, 3);
    server_.buffer[3] = (crc & 0xFF00) >> 8;
    server_.buffer[4] = (crc & 0x00FF);

    static const uint16_t n_error_response_bytes = 5;
    server_.frame_length = n_error_response_bytes;
}

static int16_t send_reply(void)
{
    int16_t ret = 0;
    server_.state = SERVER_STATE_SEND_REPLY;
    int16_t write_ret = server_.transport->write_frame(server_.buffer, server_.frame_length);
    if (write_ret < 0)
    {
        reset_state();
        ret = write_ret;  // forward error to caller
    }
    else if (write_ret == 0)
    {
        reset_state();
        ret = 0;
    }
    else
    {
        ret = -EAGAIN;
    }

    return ret;
}

static void reset_state()
{
    server_.buffer_index = 0;
    server_.state = SERVER_STATE_IDLE;
    server_.frame_length = 0;
}

static uint16_t calculate_crc(const uint8_t* data, int16_t length)
{
    uint16_t crc = 0xFFFF;
    for (int16_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i];
        for (int16_t j = 8; j != 0; j--)
        {
            if ((crc & 0x0001) != 0)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return (uint16_t)(crc << 8) | (uint16_t)(crc >> 8);
}
