#include <gtest/gtest.h>

#include <errno.h>
#include <map>

#include "simple_modbus_rtu.h"

static const std::map<uint32_t, uint16_t> kBaudRateToT3p5 = {
    {1200, 32083},
    {2400, 16041},
    {4800, 8020},
    {9600, 4010},
    {14400, 2674},
    {19200, 2005},
    {28800, 1750},
    {38400, 1750},
    {57600, 1750},
    {76800, 1750},
    {115200, 1750},
};

static const std::map<uint32_t, uint16_t> kBaudRateToT1p5 = {
    {1200, 13750},
    {2400, 6875},
    {4800, 3437},
    {9600, 1719},
    {14400, 1146},
    {19200, 859},
    {28800, 750},
    {38400, 750},
    {57600, 750},
    {76800, 750},
    {115200, 750},
};

static void mock_start_counter(uint16_t count_duration_us)
{
    (void)count_duration_us;
}
static int16_t mock_write(const uint8_t* bytes, uint16_t length)
{
    (void)bytes;
    (void)length;
    return 0;
}
static void mock_frame_received(void) {}
static struct smb_rtu_if_t mock_interface = {
    .start_counter = mock_start_counter,
    .write = mock_write,
    .frame_received = mock_frame_received,
};

class RtuStateMachine : public ::testing::Test
{
  protected:
    static constexpr uint8_t kAddr = 1;
    static constexpr uint32_t kBaudRate = 9600;
    void SetUp() override
    {
        smb_rtu_reset();
        ASSERT_EQ(smb_rtu_config(kAddr, kBaudRate, &mock_interface), 0);
    }
    void TearDown() override
    {
        mock_interface.start_counter = mock_start_counter;
        mock_interface.write = mock_write;
        mock_interface.frame_received = mock_frame_received;
    }
};

TEST_F(RtuStateMachine, Startup_NewCharLessThan3p5CharsSinceLastOne_TimerRestartedFor3p5Chars)
{
    static uint32_t cb_count_duration_us = 0;
    auto fake_start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };
    mock_interface.start_counter = fake_start_counter;
    EXPECT_EQ(smb_rtu_receive(0x01), -EAGAIN);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT3p5.at(kBaudRate));
}

TEST_F(RtuStateMachine, Startup_NewCharMoreThan3p5CharsSinceLastOne_TimerRestartedFor1p5Char)
{
    static uint32_t cb_count_duration_us = 0;
    auto fake_start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };
    mock_interface.start_counter = fake_start_counter;
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_EQ(smb_rtu_receive(kAddr), 0);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT1p5.at(9600));
}

TEST_F(RtuStateMachine, FrameReception_Max256Bytes)
{
    static uint32_t cb_count_duration_us = 0;
    mock_interface.start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };

    // Receive the first character
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_EQ(smb_rtu_receive(kAddr), 0);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT1p5.at(kBaudRate));

    // Receive the following character before 1.5 characters have passed
    constexpr uint16_t kMaxBytesInFrame = 256;
    for (auto i = 1; i < kMaxBytesInFrame; i++)
    {
        cb_count_duration_us = 0;  // reset the counter duration
        EXPECT_EQ(smb_rtu_receive(0x02), 0);
        EXPECT_EQ(cb_count_duration_us, kBaudRateToT1p5.at(kBaudRate));
    }

    // Return error when receiving the 257th character
    cb_count_duration_us = 0;  // reset the counter duration
    EXPECT_EQ(smb_rtu_receive(0x02), -ENOBUFS);
    EXPECT_EQ(cb_count_duration_us, 0);
}

TEST_F(RtuStateMachine, FrameReception_EndOfReception)
{
    static uint32_t cb_count_duration_us = 0;
    static bool is_frame_received = false;
    mock_interface.start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };
    mock_interface.frame_received = []() {
        is_frame_received = true;
    };

    // Receive x first characters
    constexpr auto kRxBufSize = 4;
    uint8_t rx_buf[kRxBufSize] = {kAddr, 2, 3, 4};
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    for (auto i = 0; i < kRxBufSize; i++)
    {
        EXPECT_EQ(smb_rtu_receive(rx_buf[i]), 0);
        EXPECT_EQ(cb_count_duration_us, kBaudRateToT1p5.at(kBaudRate));
    }

    // Timeout 1.5Chars
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT3p5.at(kBaudRate) - kBaudRateToT1p5.at(kBaudRate));

    // Ignore characters that are received after that
    constexpr uint8_t kBufSize = 255;
    uint8_t buf[kBufSize] = {0};
    for (uint8_t i = 0; i < kBufSize; i++)
    {
        cb_count_duration_us = 0;
        EXPECT_EQ(smb_rtu_receive(i), -EBUSY);
        EXPECT_EQ(smb_rtu_read_pdu(buf, kBufSize), 0);
        EXPECT_FALSE(is_frame_received);
        EXPECT_EQ(cb_count_duration_us, kBaudRateToT3p5.at(kBaudRate));
    }

    // Wait until timer timeout => frame received cb
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_TRUE(is_frame_received);

    // Receive before reading frame => busy
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_receive(42), -EBUSY);
    EXPECT_EQ(cb_count_duration_us, 0);

    // Read frame
    EXPECT_EQ(smb_rtu_read_pdu(buf, kBufSize), kRxBufSize);
    for (auto i = 0; i < kRxBufSize; i++)
    {
        EXPECT_LE(i, kBufSize);
        EXPECT_EQ(buf[i], rx_buf[i]);
    }

    // Receive after reading frame => timer restarted
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_receive(kAddr), 0);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT1p5.at(kBaudRate));
}

TEST_F(RtuStateMachine, Startup_WrongServerAddr_FrameIgnored)
{
    static uint32_t cb_count_duration_us = 0;
    static bool is_frame_received = false;
    mock_interface.start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };
    ;
    mock_interface.frame_received = []() {
        is_frame_received = true;
    };

    // Receive wrong address
    uint8_t wrong_address = kAddr + 42;
    constexpr auto kRxBufSize = 4;
    uint8_t rx_buf[kRxBufSize] = {wrong_address, 2, 3, 4};
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    for (auto i = 0; i < kRxBufSize; i++)
    {
        EXPECT_EQ(smb_rtu_receive(rx_buf[i]), 0);
        EXPECT_EQ(cb_count_duration_us, kBaudRateToT1p5.at(kBaudRate));
    }

    // Timeout 1.5Chars
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT3p5.at(kBaudRate) - kBaudRateToT1p5.at(kBaudRate));

    // Ignore characters that are received after that
    constexpr uint8_t kBufSize = 255;
    uint8_t buf[kBufSize] = {0};
    for (uint8_t i = 0; i < kBufSize; i++)
    {
        cb_count_duration_us = 0;
        EXPECT_EQ(smb_rtu_receive(i), -EBUSY);
        EXPECT_EQ(smb_rtu_read_pdu(buf, kBufSize), 0);
        EXPECT_FALSE(is_frame_received);
        EXPECT_EQ(cb_count_duration_us, kBaudRateToT3p5.at(kBaudRate));
    }

    // Wait until timer timeout => no frame received because wrong addr
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_EQ(smb_rtu_read_pdu(buf, kBufSize), 0);
    EXPECT_FALSE(is_frame_received);

    // Timer restarted, ready to receive.
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_receive(kAddr), 0);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT1p5.at(kBaudRate));
}

TEST_F(RtuStateMachine, FrameReception_NotEnoughSpaceInBuffer_EINVAL)
{
    static uint32_t cb_count_duration_us = 0;
    static bool is_frame_received = false;
    mock_interface.start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };
    mock_interface.frame_received = []() {
        is_frame_received = true;
    };

    // Receive x first characters
    constexpr auto kRxBufSize = 4;
    uint8_t rx_buf[kRxBufSize] = {kAddr, 2, 3, 4};
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    for (auto i = 0; i < kRxBufSize; i++)
    {
        EXPECT_EQ(smb_rtu_receive(rx_buf[i]), 0);
        EXPECT_EQ(cb_count_duration_us, kBaudRateToT1p5.at(kBaudRate));
    }

    // Timeout 1.5Chars
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT3p5.at(kBaudRate) - kBaudRateToT1p5.at(kBaudRate));

    // Wait until timer timeout => frame received cb
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_TRUE(is_frame_received);

    // Receive before reading frame => busy
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_receive(42), -EBUSY);
    EXPECT_EQ(cb_count_duration_us, 0);

    // Read frame
    constexpr auto kReadBufferSize = kRxBufSize - 1;  // Intentionally smaller than kRxBufSize
    uint8_t buf[kReadBufferSize] = {0};
    EXPECT_EQ(smb_rtu_read_pdu(buf, kReadBufferSize), -EINVAL);
}

TEST_F(RtuStateMachine, WritePduLengthGreaterThan256_EINVAL)
{
    constexpr uint16_t kMaxPduLength = 256;
    uint8_t pdu[kMaxPduLength + 1] = {0};
    EXPECT_EQ(smb_rtu_write_pdu(pdu, kMaxPduLength + 1), -EINVAL);
}

TEST_F(RtuStateMachine, WritePduBefore3chars5timeout_EAGAIN)
{
    constexpr uint16_t kPduLenght = 42;
    uint8_t pdu[kPduLenght] = {0};
    EXPECT_EQ(smb_rtu_write_pdu(pdu, kPduLenght), -EAGAIN);
}

TEST_F(RtuStateMachine, WritePduDuringFrameReception_EBUSY)
{
    static uint32_t cb_count_duration_us = 0;
    static bool is_frame_received = false;
    mock_interface.start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };
    mock_interface.frame_received = []() {
        is_frame_received = true;
    };

    // Receive x first characters
    constexpr auto kRxBufSize = 4;
    uint8_t rx_buf[kRxBufSize] = {kAddr, 2, 3, 4};
    constexpr auto kTxBufSize = 4;
    uint8_t tx_buf[kTxBufSize] = {0, 1, 2, 3};
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    for (auto i = 0; i < kRxBufSize; i++)
    {
        EXPECT_EQ(smb_rtu_receive(rx_buf[i]), 0);
        EXPECT_EQ(cb_count_duration_us, kBaudRateToT1p5.at(kBaudRate));
        EXPECT_EQ(smb_rtu_write_pdu(tx_buf, kTxBufSize), -EBUSY);
    }

    // Timeout 1.5Chars
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT3p5.at(kBaudRate) - kBaudRateToT1p5.at(kBaudRate));
    EXPECT_EQ(smb_rtu_write_pdu(tx_buf, kTxBufSize), -EBUSY);

    // Wait until timer timeout => frame received cb
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_TRUE(is_frame_received);

    // Write before reading frame => busy
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_write_pdu(tx_buf, kTxBufSize), -EBUSY);
    EXPECT_EQ(cb_count_duration_us, 0);

    // Read frame
    constexpr uint8_t kBufSize = 255;
    uint8_t buf[kBufSize] = {0};
    EXPECT_EQ(smb_rtu_read_pdu(buf, kBufSize), kRxBufSize);
    for (auto i = 0; i < kRxBufSize; i++)
    {
        EXPECT_LE(i, kBufSize);
        EXPECT_EQ(buf[i], rx_buf[i]);
    }
}

TEST_F(RtuStateMachine, WritePduLessThanRequested_EAGAIN)
{
    static constexpr uint16_t kPduLength = 42;
    static uint8_t tx_buf[kPduLength] = {0};
    static uint32_t cb_count_duration_us = 0;
    static uint32_t cb_write_cnt = 0;
    mock_interface.start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };
    mock_interface.write = [](const uint8_t* buffer, uint16_t length) -> int16_t {
        if (cb_write_cnt < 1)
        {
            EXPECT_EQ(length, kPduLength);
            memcpy(tx_buf, buffer, length - 1);
            cb_write_cnt++;
            return (int16_t)(length - 1);  // Simulate writing less than requested
        }
        else
        {
            EXPECT_EQ(length, 1);
            tx_buf[kPduLength - 1] = buffer[0];  // Write the last byte
            return length;
        }
    };

    // 3.5chars timeout => ready to write
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);

    // Incomplete write => EAGAIN
    uint8_t pdu[kPduLength] = {195, 3, 254, 169, 121, 221, 218, 120, 78, 250, 102, 143, 113, 141, 19, 182, 233, 90, 13, 75, 125, 204, 10, 240, 84, 217, 141, 28, 250, 17, 40, 83, 164, 224, 135, 185, 136, 146, 199, 70, 156, 49};
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_write_pdu(pdu, kPduLength), -EAGAIN);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT1p5.at(kBaudRate));
    for (auto i = 0; i < kPduLength - 1; i++)
    {
        EXPECT_EQ(tx_buf[i], pdu[i]);
    }

    // Complete write => 0
    // To check that bytes are not written again, change pdu content
    for (auto i = 0; i < kPduLength; i++)
    {
        pdu[i] = 0xFF - pdu[i];
    }
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_write_pdu(pdu, kPduLength), 0);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT3p5.at(kBaudRate));
    for (auto i = 0; i < kPduLength - 1; i++)
    {
        EXPECT_NE(tx_buf[i], pdu[i]);  // Previous bytes should not be overwritten
    }
    EXPECT_EQ(tx_buf[kPduLength - 1], pdu[kPduLength - 1]);  // Last byte should be written correctly
}

TEST_F(RtuStateMachine, ApiCallsDuringWritingSequence)
{
    constexpr uint16_t kPduLength = 42;

    // 3.5chars timeout => ready to write
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);

    // Write PDU
    uint8_t pdu[kPduLength] = {0};
    EXPECT_EQ(smb_rtu_write_pdu(pdu, kPduLength), -EAGAIN);

    // Read PDU during writing sequence
    EXPECT_EQ(smb_rtu_read_pdu(pdu, kPduLength), -EBUSY);

    // Receive during writing sequence
    EXPECT_EQ(smb_rtu_receive(0x01), -EBUSY);
}

TEST_F(RtuStateMachine, TimeoutDuringWritingSequence_ETIMEDOUTmustBeHandled)
{
    static constexpr uint16_t kPduLength = 42;
    static uint8_t tx_buf[kPduLength] = {0};
    static uint32_t cb_write_cnt = 0;
    static uint32_t cb_count_duration_us = 0;
    mock_interface.start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };
    mock_interface.write = [](const uint8_t* buffer, uint16_t length) -> int16_t {
        if (cb_write_cnt < 1)
        {
            EXPECT_EQ(length, kPduLength);
            memcpy(tx_buf, buffer, length - 1);
            cb_write_cnt++;
            return (int16_t)(length - 1);  // Simulate writing less than requested
        }
        else if (cb_write_cnt == 2)
        {
            EXPECT_EQ(length, 1);
            tx_buf[kPduLength - 1] = buffer[0];  // Write the last byte
            return length;
        }
        else
        {
            return length;
        }
    };

    // 3.5chars timeout => ready to write
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);

    // Incomplete write => EAGAIN
    uint8_t pdu[kPduLength] = {0};
    EXPECT_EQ(smb_rtu_write_pdu(pdu, kPduLength), -EAGAIN);

    // Wrong length
    EXPECT_EQ(smb_rtu_write_pdu(pdu, kPduLength + 1), -EINVAL);

    // Timeout (1.5 chars) during writing sequence
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);

    // Write other PDU: still not finished writing the first one => EBUSY
    uint8_t pdu2[kPduLength] = {0};
    EXPECT_EQ(smb_rtu_write_pdu(pdu2, kPduLength), -EBUSY);

    // Finish writing the first PDU, but too late => ETIMEDOUT and wait 3.5 chars
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_write_pdu(pdu, kPduLength), -ETIMEDOUT);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT3p5.at(kBaudRate));

    // Write other PDU before timer timeout => still busy
    EXPECT_EQ(smb_rtu_write_pdu(pdu2, kPduLength), -EBUSY);

    // Timeout 3.5 chars => ready to write
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_EQ(smb_rtu_write_pdu(pdu2, kPduLength), 0);
}

TEST_F(RtuStateMachine, ErrorWritingPdu_ErrorPropagated)
{
    constexpr uint16_t kPduLength = 42;
    constexpr int16_t kWriteErr = -42; 
    mock_interface.write = [](const uint8_t* , uint16_t ) -> int16_t {
        return kWriteErr;
    };

    // 3.5chars timeout => ready to write
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);

    // Incomplete write => EAGAIN
    uint8_t pdu[kPduLength] = {0};
    EXPECT_EQ(smb_rtu_write_pdu(pdu, kPduLength), kWriteErr);
}

TEST_F(RtuStateMachine, LessThan3p5CharsAfterEndOfTransmission_WriteAnotherPdu_Busy)
{
    constexpr uint16_t kPduLength = 42;
    static uint32_t cb_count_duration_us = 0;
    mock_interface.start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };
    mock_interface.write = [](const uint8_t* , uint16_t length ) -> int16_t {
        return length;  
    };

    // 3.5chars timeout => ready to write
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);

    // Write PDU
    uint8_t pdu[kPduLength] = {0};
    cb_count_duration_us = 0;
    EXPECT_EQ(smb_rtu_write_pdu(pdu, kPduLength), 0);
    EXPECT_EQ(cb_count_duration_us, kBaudRateToT3p5.at(kBaudRate));

    // Write PDU too early
    uint8_t new_pdu[kPduLength] = {0};
    EXPECT_EQ(smb_rtu_read_pdu(new_pdu, kPduLength), -EBUSY);

    // Write PDU after 3.5 chars
    EXPECT_EQ(smb_rtu_timer_timeout(), 0);
    EXPECT_EQ(smb_rtu_read_pdu(new_pdu, kPduLength), 0);
}