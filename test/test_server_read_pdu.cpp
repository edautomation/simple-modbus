#include <gtest/gtest.h>

#include <cstdint>

#include "simple_modbus.h"
#include "test_common.h"

TEST(ServerReadPdu, NoMessage_Return0)
{
    auto read_frame = [](uint8_t*, uint16_t) -> int16_t {
        return 0;  // No message available
    };
    auto write_frame = [](uint8_t*, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_input_regs = nullptr,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
}

TEST(ServerReadPdu, MessageAvailable_TooShort_ReturnEBADMSG)
{
    auto read_frame = [](uint8_t*, uint16_t) -> int16_t {
        return 3;  // No message available
    };
    auto write_frame = [](uint8_t*, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_input_regs = nullptr,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), -EBADMSG);
}

TEST(ServerReadPdu, MessageAvailable_WrongAddress_NoReplyReturn0)
{
    static int16_t read_calls = 0;
    static bool was_write_called = false;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        read_calls++;
        buffer[0] = kServerAddr + 1;
        buffer[1] = 0x00;
        buffer[2] = 0x00;
        buffer[3] = 0xD0;
        return 4;
    };
    auto write_frame = [](uint8_t*, uint16_t) -> int16_t {
        was_write_called = true;
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_input_regs = nullptr,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_EQ(read_calls, 1);
    EXPECT_FALSE(was_write_called);
}

TEST(ServerReadPdu, MessageAvailable_WrongCrc_NoReplyReturnEBADMSG)
{
    static int16_t read_calls = 0;
    static bool was_write_called = false;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        read_calls++;
        buffer[0] = kServerAddr;
        buffer[1] = kReadInputRegsFunctionCode;
        buffer[2] = 0x00;
        buffer[3] = 0x00;  // Wrong CRC
        return 4;
    };
    auto write_frame = [](uint8_t*, uint16_t) -> int16_t {
        was_write_called = true;
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_input_regs = nullptr,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), -EBADMSG);
    EXPECT_EQ(read_calls, 1);
    EXPECT_FALSE(was_write_called);
}

TEST(ServerReadPdu, MessageAvailable_CorrectAddress_UnsupportedFunctionCode_Reply01Return0)
{
    static bool was_read_called = false;
    static bool was_write_called = false;

    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kReadInputRegsFunctionCode;
        buffer[2] = 0x01;
        buffer[3] = 0xE3;
        was_read_called = true;
        return 4;
    };

    auto write_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kReadInputRegsFunctionCode | kErrorFlag);
        EXPECT_EQ(buffer[2], kErrorIllegalFunctionCode);
        EXPECT_EQ(buffer[3], 0x82);
        EXPECT_EQ(buffer[4], 0xC0);
        was_write_called = true;
        return 0;
    };

    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_input_regs = nullptr,  // No callback -> unsupported function code
    };

    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_TRUE(was_read_called);
    EXPECT_TRUE(was_write_called);
}

TEST(ServerReadPdu, ErrorReplyReturnsEAGAIN_ReturnEAGAIN)
{
    static bool was_read_called = false;
    static uint16_t write_calls = 0;

    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kReadInputRegsFunctionCode;
        buffer[2] = 0x01;
        buffer[3] = 0xE3;
        was_read_called = true;
        return 4;
    };

    auto write_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kReadInputRegsFunctionCode | kErrorFlag);
        EXPECT_EQ(buffer[2], kErrorIllegalFunctionCode);
        EXPECT_EQ(buffer[3], 0x82);
        EXPECT_EQ(buffer[4], 0xC0);
        write_calls++;
        if (write_calls == 1)
        {
            return 2;
        }
        else
        {
            return 0;
        }
    };

    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_input_regs = nullptr,  // No callback -> unsupported function code
    };

    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), -EAGAIN);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_TRUE(was_read_called);
    EXPECT_EQ(write_calls, 2);
}

// TODO write pdu returns < 0 -> forward error

TEST(ServerReadPdu, ErrorReplyReturnsError_ForwardError)
{
    static bool was_read_called = false;
    static uint16_t write_calls = 0;

    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kReadInputRegsFunctionCode;
        buffer[2] = 0x01;
        buffer[3] = 0xE3;
        was_read_called = true;
        return 4;
    };

    auto write_frame = [](uint8_t*, uint16_t) -> int16_t {
        write_calls++;
        return -1;
    };

    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_input_regs = nullptr,  // No callback -> unsupported function code
    };

    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), -1);
    EXPECT_TRUE(was_read_called);
    EXPECT_EQ(write_calls, 1);
}
// TODO: test broadcast function codes (only write operations)
