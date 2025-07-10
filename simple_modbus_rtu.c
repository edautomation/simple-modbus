#include "simple_modbus_rtu.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define MODBUS_RTU_BUFFER_SIZE 256

#define RETURN_IF(x, err) \
    do                    \
    {                     \
        if (x)            \
        {                 \
            return err;   \
        }                 \
    } while (0)

enum rtu_state_t
{
    RTU_STATE_INIT,
    RTU_STATE_IDLE,
    RTU_STATE_EMIT,
    RTU_STATE_RECEIVE,
    RTU_STATE_CONTROL_AND_WAIT,
    RTU_STATE_PROCESS_RX_FRAME,
    RTU_STATE_WAIT_FOR_TX_COMPLETE,
    RTU_STATE_TX_TIMEOUT,
};

enum rtu_action_t
{
    RTU_ACTION_NONE,
    RTU_ACTION_RX,
    RTU_ACTION_TX,
    RTU_ACTION_TIMEOUT,
    RTU_ACTION_PROCESS_RX,
};

struct rtu_event_t
{
    enum rtu_action_t action;
    uint8_t* bytes;
    uint16_t n_bytes;
};

struct rtu_t
{
    uint8_t addr;
    const struct smb_rtu_if_t* interface;
    enum rtu_state_t state;
    uint16_t t_1_5char_us;
    uint16_t t_3_5char_us;
    uint16_t buffer_index;
    uint8_t rx_buffer[MODBUS_RTU_BUFFER_SIZE];
    uint8_t* current_tx_buffer;
    uint16_t current_tx_length;
};

// NOLINTNEXTLINE (false negative)
static struct rtu_t rtu_ = {
    .addr = 0,
    .state = RTU_STATE_INIT,
    .interface = NULL,
    .t_1_5char_us = 0,
    .t_3_5char_us = 0,
    .buffer_index = 0,
    .rx_buffer = {0},
};

static int16_t exec_sm(const struct rtu_event_t* event);
static int16_t exec_init(const struct rtu_event_t* event);
static int16_t exec_idle(const struct rtu_event_t* event);
static int16_t exec_emitting(const struct rtu_event_t* event);
static int16_t exec_receiving(const struct rtu_event_t* event);
static int16_t exec_waiting(const struct rtu_event_t* event);
static int16_t exec_process(const struct rtu_event_t* event);
static int16_t exec_wait_for_tx_complete(const struct rtu_event_t* event);
static int16_t exec_tx_timeout(const struct rtu_event_t* event);

void smb_rtu_reset(void)
{
    rtu_.addr = 0;
    rtu_.state = RTU_STATE_INIT;
    rtu_.interface = NULL;
    rtu_.t_1_5char_us = 0;
    rtu_.t_3_5char_us = 0;
    rtu_.buffer_index = 0;
    rtu_.current_tx_buffer = NULL;
    rtu_.current_tx_length = 0;

    // Do not use memset, as it is not safe.
    // and memset_s is not available in all compilers
    for (size_t i = 0; i < sizeof(rtu_.rx_buffer); i++)
    {
        rtu_.rx_buffer[i] = 0;
    }
}

int16_t smb_rtu_config(uint8_t addr,
                       uint32_t baud_rate,
                       const struct smb_rtu_if_t* interface)
{
    // sanity checks
    RETURN_IF(0 == addr, -EINVAL);
    RETURN_IF(UINT8_MAX == addr, -EINVAL);
    RETURN_IF(NULL == interface, -EFAULT);
    RETURN_IF(NULL == interface->start_counter, -EFAULT);
    RETURN_IF(NULL == interface->write, -EFAULT);
    RETURN_IF(NULL == interface->frame_received, -EFAULT);

    // only a specific set of baud rates are supported
    switch (baud_rate)
    {
        case 1200:
            rtu_.t_1_5char_us = 13750;
            rtu_.t_3_5char_us = 32083;
            break;
        case 2400:
            rtu_.t_1_5char_us = 6875;
            rtu_.t_3_5char_us = 16041;
            break;
        case 4800:
            rtu_.t_1_5char_us = 3437;
            rtu_.t_3_5char_us = 8020;
            break;
        case 9600:
            rtu_.t_1_5char_us = 1719;
            rtu_.t_3_5char_us = 4010;
            break;
        case 14400:
            rtu_.t_1_5char_us = 1146;
            rtu_.t_3_5char_us = 2674;
            break;
        case 19200:
            rtu_.t_1_5char_us = 859;
            rtu_.t_3_5char_us = 2005;
            break;
        case 28800:
        case 38400:
        case 57600:
        case 76800:
        case 115200:
            rtu_.t_1_5char_us = 750;
            rtu_.t_3_5char_us = 1750;
            break;
        default:
            return -EINVAL;
    }

    // Do not use memset, as it is not safe.
    // and memset_s is not available in all compilers
    for (size_t i = 0; i < sizeof(rtu_.rx_buffer); i++)
    {
        rtu_.rx_buffer[i] = 0;
    }
    rtu_.addr = addr;
    rtu_.buffer_index = 0;
    rtu_.interface = interface;
    rtu_.interface->start_counter(rtu_.t_3_5char_us);

    return 0;
}

int16_t smb_rtu_receive(uint8_t byte)
{
    RETURN_IF(NULL == rtu_.interface, -EFAULT);

    struct rtu_event_t event = {
        .action = RTU_ACTION_RX,
        .bytes = &byte,
        .n_bytes = 1,
    };
    int16_t ret = exec_sm(&event);

    return ret;
}

int16_t smb_rtu_timer_timeout(void)
{
    RETURN_IF(NULL == rtu_.interface, -EFAULT);

    struct rtu_event_t event = {
        .action = RTU_ACTION_TIMEOUT,
        .bytes = NULL,
        .n_bytes = 0,
    };
    int16_t ret = exec_sm(&event);

    return ret;
}

int16_t smb_rtu_read_pdu(uint8_t* buffer, uint16_t length)
{
    RETURN_IF(NULL == rtu_.interface, -EFAULT);
    RETURN_IF(NULL == buffer, -EFAULT);

    struct rtu_event_t event = {
        .action = RTU_ACTION_PROCESS_RX,
        .bytes = buffer,
        .n_bytes = length,
    };
    int16_t ret = exec_sm(&event);

    return ret;
}

int16_t smb_rtu_write_pdu(uint8_t* buffer, uint16_t length)
{
    RETURN_IF(NULL == rtu_.interface, -EFAULT);
    RETURN_IF(NULL == buffer, -EFAULT);
    RETURN_IF(length > MODBUS_RTU_BUFFER_SIZE, -EINVAL);

    struct rtu_event_t event = {
        .action = RTU_ACTION_TX,
        .bytes = buffer,
        .n_bytes = length,
    };
    int16_t ret = exec_sm(&event);

    return ret;
}

static int16_t exec_sm(const struct rtu_event_t* event)
{
    RETURN_IF(NULL == event, -EFAULT);

    int16_t ret = 0;
    switch (rtu_.state)
    {
        case RTU_STATE_INIT:
            ret = exec_init(event);
            break;
        case RTU_STATE_IDLE:
            ret = exec_idle(event);
            break;
        case RTU_STATE_EMIT:
            ret = exec_emitting(event);
            break;
        case RTU_STATE_RECEIVE:
            ret = exec_receiving(event);
            break;
        case RTU_STATE_CONTROL_AND_WAIT:
            ret = exec_waiting(event);
            break;
        case RTU_STATE_PROCESS_RX_FRAME:
            ret = exec_process(event);
            break;
        case RTU_STATE_WAIT_FOR_TX_COMPLETE:
            ret = exec_wait_for_tx_complete(event);
            break;
        case RTU_STATE_TX_TIMEOUT:
            ret = exec_tx_timeout(event);
            break;
        default:
            ret = -EFAULT;  // Developer error, should not happen.
            break;
    }
    return ret;
}

static int16_t exec_init(const struct rtu_event_t* event)
{
    RETURN_IF(NULL == event, -EFAULT);
    RETURN_IF(NULL == rtu_.interface, -EFAULT);
    RETURN_IF(NULL == rtu_.interface->start_counter, -EFAULT);

    int16_t ret = 0;
    if (RTU_ACTION_TIMEOUT == event->action)
    {
        rtu_.state = RTU_STATE_IDLE;
        rtu_.buffer_index = 0;
    }
    else if (RTU_ACTION_PROCESS_RX == event->action)
    {
        ret = 0;  // read pdu returns 0 when frame not complete yet.
    }
    else
    {
        rtu_.interface->start_counter(rtu_.t_3_5char_us);
        ret = -EAGAIN;
    }

    return ret;
}

static int16_t exec_idle(const struct rtu_event_t* event)
{
    RETURN_IF(NULL == event, -EFAULT);
    RETURN_IF(NULL == rtu_.interface, -EFAULT);
    RETURN_IF(NULL == rtu_.interface->start_counter, -EFAULT);

    int16_t ret = 0;
    if (RTU_ACTION_RX == event->action)
    {
        if ((NULL == event->bytes) || (event->n_bytes > 1) || (event->n_bytes == 0))
        {
            ret = -EFAULT;
        }
        else
        {
            rtu_.rx_buffer[0] = event->bytes[0];
            rtu_.buffer_index = 1;
            rtu_.interface->start_counter(rtu_.t_1_5char_us);
            rtu_.state = RTU_STATE_RECEIVE;
        }
    }
    else if (RTU_ACTION_PROCESS_RX == event->action)
    {
        ret = 0;  // read pdu returns 0 when frame not complete yet.
    }
    else if (RTU_ACTION_TX == event->action)
    {
        if ((NULL == event->bytes) || (event->n_bytes == 0) ||
            (NULL == rtu_.interface->write) ||
            (event->n_bytes > MODBUS_RTU_BUFFER_SIZE))
        {
            ret = -EFAULT;
        }
        else
        {
            int16_t n_bytes = rtu_.interface->write(event->bytes, event->n_bytes);
            if (n_bytes < 0)
            {
                ret = n_bytes;  // propagate error to caller
            }
            else if (n_bytes < event->n_bytes)
            {
                // not all bytes were written, continue later
                rtu_.buffer_index = n_bytes;
                rtu_.state = RTU_STATE_EMIT;
                rtu_.current_tx_buffer = event->bytes;
                rtu_.current_tx_length = event->n_bytes;
                rtu_.interface->start_counter(rtu_.t_1_5char_us);
                ret = -EAGAIN;
            }
            else
            {
                rtu_.state = RTU_STATE_WAIT_FOR_TX_COMPLETE;
                rtu_.interface->start_counter(rtu_.t_3_5char_us);
            }
        }
    }
    else
    {
        ret = -EFAULT;
    }
    return ret;
}

static int16_t exec_emitting(const struct rtu_event_t* event)
{
    RETURN_IF(NULL == event, -EFAULT);
    RETURN_IF(NULL == rtu_.interface, -EFAULT);
    RETURN_IF(NULL == rtu_.interface->write, -EFAULT);

    int16_t ret = 0;
    if (RTU_ACTION_TX == event->action)
    {
        if ((NULL == event->bytes) || (rtu_.buffer_index >= event->n_bytes))
        {
            ret = -EFAULT;
        }
        else if (rtu_.current_tx_buffer != event->bytes)
        {
            // Not ready for a new frame yet, waiting for 3.5chars to pass
            ret = -EBUSY;
        }
        else if (rtu_.current_tx_length != event->n_bytes)
        {
            // The same parameters must be used when calling smb_rtu_write_pdu again
            ret = -EINVAL;
        }
        else
        {
            int16_t n_remaining_bytes = event->n_bytes - rtu_.buffer_index;
            uint8_t* bytes = &event->bytes[rtu_.buffer_index];
            int16_t n_bytes = rtu_.interface->write(bytes, n_remaining_bytes);
            if (n_bytes < 0)
            {
                rtu_.state = RTU_STATE_WAIT_FOR_TX_COMPLETE;
                rtu_.interface->start_counter(rtu_.t_3_5char_us);
                ret = n_bytes;
            }
            else if (n_bytes < n_remaining_bytes)
            {
                rtu_.buffer_index += n_bytes;
                rtu_.interface->start_counter(rtu_.t_1_5char_us);
                ret = -EAGAIN;
            }
            else if (n_bytes == n_remaining_bytes)
            {
                rtu_.state = RTU_STATE_WAIT_FOR_TX_COMPLETE;
                rtu_.interface->start_counter(rtu_.t_3_5char_us);
                ret = 0;
            }
            else
            {
                ret = -EFAULT;  // Developer error, should not happen.
            }
        }
    }
    else if (RTU_ACTION_TIMEOUT == event->action)
    {
        rtu_.state = RTU_STATE_TX_TIMEOUT;
    }
    else
    {
        ret = -EBUSY;
    }

    return ret;
}

static int16_t exec_receiving(const struct rtu_event_t* event)
{
    RETURN_IF(NULL == event, -EFAULT);
    RETURN_IF(NULL == rtu_.interface, -EFAULT);
    RETURN_IF(NULL == rtu_.interface->start_counter, -EFAULT);

    int16_t ret = 0;
    if (RTU_ACTION_RX == event->action)
    {
        if (NULL == event->bytes || event->n_bytes > 1 || event->n_bytes == 0)
        {
            ret = -EFAULT;
        }
        else if (rtu_.buffer_index >= MODBUS_RTU_BUFFER_SIZE)
        {
            ret = -ENOBUFS;
        }
        else
        {
            rtu_.rx_buffer[rtu_.buffer_index] = event->bytes[0];
            rtu_.buffer_index++;
            rtu_.interface->start_counter(rtu_.t_1_5char_us);
        }
    }
    else if (RTU_ACTION_TIMEOUT == event->action)
    {
        rtu_.state = RTU_STATE_CONTROL_AND_WAIT;
        rtu_.interface->start_counter(rtu_.t_3_5char_us - rtu_.t_1_5char_us);
    }
    else if (RTU_ACTION_PROCESS_RX == event->action)
    {
        ret = 0;  // read pdu returns 0 when frame not complete yet.
    }
    else if (RTU_ACTION_TX == event->action)
    {
        ret = -EBUSY;
    }
    else
    {
        ret = -EFAULT;  // Developer error, should not happen.
    }

    return ret;
}

static int16_t exec_waiting(const struct rtu_event_t* event)
{
    RETURN_IF(NULL == event, -EFAULT);
    RETURN_IF(NULL == rtu_.interface, -EFAULT);
    RETURN_IF(NULL == rtu_.interface->start_counter, -EFAULT);
    RETURN_IF(NULL == rtu_.interface->frame_received, -EFAULT);

    int16_t ret = 0;
    if (RTU_ACTION_RX == event->action)
    {
        rtu_.interface->start_counter(rtu_.t_3_5char_us);
        ret = -EBUSY;
    }
    else if (RTU_ACTION_TIMEOUT == event->action)
    {
        uint8_t addr = rtu_.rx_buffer[0];
        if (0 == addr || rtu_.addr == addr)
        {
            rtu_.state = RTU_STATE_PROCESS_RX_FRAME;
            rtu_.interface->frame_received();
        }
        else
        {
            // Frame not for us, ignore it.
            rtu_.state = RTU_STATE_IDLE;
        }
    }
    else if (RTU_ACTION_PROCESS_RX == event->action)
    {
        // Read pdu returns 0 when frame not complete yet.
        ret = 0;
    }
    else if (RTU_ACTION_TX == event->action)
    {
        ret = -EBUSY;
    }
    else
    {
        ret = -EFAULT;
    }

    return ret;
}

static int16_t exec_process(const struct rtu_event_t* event)
{
    RETURN_IF(NULL == event, -EFAULT);

    int16_t ret = 0;
    if (RTU_ACTION_PROCESS_RX == event->action)
    {
        if (rtu_.buffer_index >= event->n_bytes)
        {
            ret = -EINVAL;
        }
        else
        {
            int16_t n_rx_bytes = rtu_.buffer_index;
            for (int16_t i = 0; i < n_rx_bytes; i++)
            {
                event->bytes[i] = rtu_.rx_buffer[i];
            }
            ret = n_rx_bytes;

            // Frame was process, we can receive or transmit again.
            rtu_.state = RTU_STATE_IDLE;
        }
    }
    else if ((RTU_ACTION_RX == event->action) ||
             (RTU_ACTION_TX == event->action))
    {
        ret = -EBUSY;
    }
    else
    {
        ret = -EFAULT;
    }
    return ret;
}

static int16_t exec_wait_for_tx_complete(const struct rtu_event_t* event)
{
    RETURN_IF(NULL == event, -EFAULT);

    int16_t ret = 0;
    if (RTU_ACTION_TIMEOUT == event->action)
    {
        rtu_.state = RTU_STATE_IDLE;
    }
    else
    {
        ret = -EBUSY;
    }

    return ret;
}

static int16_t exec_tx_timeout(const struct rtu_event_t* event)
{
    RETURN_IF(NULL == event, -EFAULT);
    RETURN_IF(NULL == rtu_.interface, -EFAULT);
    RETURN_IF(NULL == rtu_.interface->start_counter, -EFAULT);

    int16_t ret = 0;
    if (RTU_ACTION_TX == event->action)
    {
        if (NULL == event->bytes)
        {
            ret = -EFAULT;
        }
        else
        {
            if (rtu_.current_tx_buffer == event->bytes)
            {
                // Error, wait 3.5 chars before sending a new frame
                rtu_.state = RTU_STATE_WAIT_FOR_TX_COMPLETE;
                rtu_.interface->start_counter(rtu_.t_3_5char_us);
                ret = -ETIMEDOUT;
            }
            else
            {
                // Error must be read out before sending a new frame
                ret = -EBUSY;
            }
        }
    }
    else
    {
        ret = -EBUSY;
    }

    return ret;
}
