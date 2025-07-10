#include <gtest/gtest.h>

#include <errno.h>
#include <array>

#include "simple_modbus_rtu.h"

static constexpr std::array<uint32_t, 11> kBaudRates = {
    1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 76800, 115200};

// Above 19200, use fixed T3p5 value of 1.75ms
// see https://modbus.org/docs/Modbus_over_serial_line_V1_02.pdf, p.13
static constexpr std::array<uint16_t, 11> kT3p5_us = {
    32083, 16041, 8020, 4010, 2674, 2005, 1750, 1750, 1750, 1750, 1750};

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

class RtuConfig : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        smb_rtu_reset();
    }
    void TearDown() override
    {
        // Nothing to do
    }
};

TEST_F(RtuConfig, ServerAddressZero_ReturnEINVAL)
{
    int16_t result = smb_rtu_config(0, 9600, &mock_interface);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(RtuConfig, ServerAddress255_ReturnEINVAL)
{
    int16_t result = smb_rtu_config(255, 9600, &mock_interface);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(RtuConfig, InvalidBaudRate_ReturnEINVAL)
{
    for (uint32_t i = 0; i <= 115200; i++)
    {
        int16_t result = smb_rtu_config(1, i, &mock_interface);
        switch (i)
        {
            case 1200:
            case 2400:
            case 4800:
            case 9600:
            case 14400:
            case 19200:
            case 28800:
            case 38400:
            case 57600:
            case 76800:
            case 115200:
                EXPECT_EQ(result, 0);
                break;
            default:
                ASSERT_EQ(result, -EINVAL);
                break;
        }
    }
}

TEST_F(RtuConfig, NullInterface_ReturnEFAULT)
{
    struct smb_rtu_if_t interface = mock_interface;

    int16_t result = smb_rtu_config(1, 9600, nullptr);
    EXPECT_EQ(result, -EFAULT);

    interface.start_counter = nullptr;
    result = smb_rtu_config(1, 9600, &interface);
    interface.start_counter = mock_start_counter;
    EXPECT_EQ(result, -EFAULT);

    interface.write = nullptr;
    result = smb_rtu_config(1, 9600, &interface);
    interface.write = mock_write;
    EXPECT_EQ(result, -EFAULT);

    interface.frame_received = nullptr;
    result = smb_rtu_config(1, 9600, &interface);
    interface.frame_received = mock_frame_received;
    EXPECT_EQ(result, -EFAULT);
}

TEST_F(RtuConfig, ValidConfig_CounterResetAndStartedWithT3p5)
{
    static uint32_t cb_count_duration_us = 0;
    struct smb_rtu_if_t interface = mock_interface;
    interface.start_counter = [](uint16_t count_duration_us) {
        cb_count_duration_us = count_duration_us;
    };

    uint8_t i = 0;
    for (auto baud_rate : kBaudRates)
    {
        int16_t result = smb_rtu_config(1, baud_rate, &interface);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(cb_count_duration_us, kT3p5_us[i]);
        i++;
    }
}

TEST_F(RtuConfig, NotConfigured_CallApiFuncs_ReturnEFAULT)
{
    constexpr uint8_t kBufferSize = 10;
    uint8_t buffer[kBufferSize] = {0};
    EXPECT_EQ(smb_rtu_receive(0), -EFAULT);
    EXPECT_EQ(smb_rtu_timer_timeout(), -EFAULT);
    EXPECT_EQ(smb_rtu_read_pdu(buffer, kBufferSize), -EFAULT);
    EXPECT_EQ(smb_rtu_write_pdu(buffer, kBufferSize), -EFAULT);
}

TEST_F(RtuConfig, Configured_InvalidApiFuncParameters_ReturnsEFAULT)
{
    constexpr uint8_t kBufferSize = 10;
    smb_rtu_config(1, 9600, &mock_interface);
    EXPECT_EQ(smb_rtu_read_pdu(nullptr, kBufferSize), -EFAULT);
    EXPECT_EQ(smb_rtu_write_pdu(nullptr, kBufferSize), -EFAULT);
}
