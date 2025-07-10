#include <gtest/gtest.h>

#include "simple_modbus.h"
#include "test_common.h"

TEST(ServerF03, PduLengthIncorrect_Reply03Return0)
{
    static bool was_write_called = false;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kReadHoldingRegsFunctionCode;
        buffer[2] = 0x00;  // Start address high byte
        buffer[3] = 0x00;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x01;  // One byte to read
        buffer[6] = 0x00;  // This byte has nothing to do here
        buffer[7] = 0x0A;
        buffer[8] = 0x63;
        return 9;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        EXPECT_EQ(length, 5);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kReadHoldingRegsFunctionCode | kErrorFlag);
        EXPECT_EQ(buffer[2], 0x03);
        EXPECT_EQ(buffer[3], 0x01);
        EXPECT_EQ(buffer[4], 0x31);
        was_write_called = true;
        return 0;
    };
    auto read_holding_regs = [](uint8_t*, uint16_t, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_holding_regs = read_holding_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_TRUE(was_write_called);
}

TEST(ServerF03, WrongQuantityOfRegisters_Reply03Return0)
{
    static bool was_write_called = false;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kReadHoldingRegsFunctionCode;
        buffer[2] = 0x00;  // Start address high byte
        buffer[3] = 0x00;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = kMaxNumberOfRegisters + 1;
        buffer[6] = 0xC5;
        buffer[7] = 0xEA;
        return 8;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        EXPECT_EQ(length, 5);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kReadHoldingRegsFunctionCode | kErrorFlag);
        EXPECT_EQ(buffer[2], 0x03);
        EXPECT_EQ(buffer[3], 0x01);
        EXPECT_EQ(buffer[4], 0x31);
        was_write_called = true;
        return 0;
    };
    auto read_holding_regs = [](uint8_t*, uint16_t, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_holding_regs = read_holding_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_TRUE(was_write_called);
}

TEST(ServerF03, ValidRequest_CallbackReturnsError_Reply02Return0)
{
    static bool was_write_called = false;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kReadHoldingRegsFunctionCode;
        buffer[2] = 0x00;
        buffer[3] = 0x00;
        buffer[4] = 0x00;
        buffer[5] = 0x04;
        buffer[6] = 0x44;
        buffer[7] = 0x09;
        return 8;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        EXPECT_EQ(length, 5);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kReadHoldingRegsFunctionCode | kErrorFlag);
        EXPECT_EQ(buffer[2], 0x02);
        EXPECT_EQ(buffer[3], 0xC0);
        EXPECT_EQ(buffer[4], 0xF1);
        was_write_called = true;
        return 0;
    };
    auto read_holding_regs = [](uint8_t*, uint16_t, uint16_t) -> int16_t {
        return -1;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_holding_regs = read_holding_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_TRUE(was_write_called);
}

TEST(ServerF03, ValidRequest_CallbackReturns_NoReplyReturnEAGAIN)
{
    static uint16_t writes = 0;
    static uint16_t cb_reads = 0;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kReadHoldingRegsFunctionCode;
        buffer[2] = 0x00;
        buffer[3] = 0x00;
        buffer[4] = 0x00;
        buffer[5] = 0x04;
        buffer[6] = 0x44;
        buffer[7] = 0x09;
        return 8;
    };
    auto write_frame = [](uint8_t*, uint16_t) -> int16_t {
        writes++;
        return 0;
    };
    auto read_holding_regs = [](uint8_t*, uint16_t length, uint16_t) -> int16_t {
        cb_reads++;
        if (cb_reads == 1)
        {
            return 0;
        }
        return 2 * length;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_holding_regs = read_holding_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), -EAGAIN);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_EQ(cb_reads, 2);
    EXPECT_EQ(writes, 1);
}

TEST(ServerF03, ValidRequest_WritePduReturnsLength_Return0)
{
    static uint16_t writes = 0;
    static uint16_t cb_reads = 0;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kReadHoldingRegsFunctionCode;
        buffer[2] = 0x00;
        buffer[3] = 0x00;
        buffer[4] = 0x00;
        buffer[5] = 0x04;
        buffer[6] = 0x44;
        buffer[7] = 0x09;
        return 8;
    };
    auto write_frame = [](uint8_t*, uint16_t) -> int16_t {
        writes++;
        return 0;
    };
    auto read_holding_regs = [](uint8_t*, uint16_t length, uint16_t) -> int16_t {
        cb_reads++;
        return 2 * length;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_holding_regs = read_holding_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_EQ(cb_reads, 1);
    EXPECT_EQ(writes, 1);
}

TEST(ServerF03, ValidRequest_WritePduReturnsLessThanLength_ReturnEAGAIN)
{
    static uint16_t writes = 0;
    static uint16_t cb_reads = 0;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kReadHoldingRegsFunctionCode;
        buffer[2] = 0x00;
        buffer[3] = 0x00;
        buffer[4] = 0x00;
        buffer[5] = 0x02;
        buffer[6] = 0xC4;
        buffer[7] = 0x0B;
        return 8;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        writes++;
        EXPECT_EQ(length, 9);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kReadHoldingRegsFunctionCode);
        EXPECT_EQ(buffer[2], 0x04);  // 2*2
        EXPECT_EQ(buffer[3], 0x00);
        EXPECT_EQ(buffer[4], 0x01);
        EXPECT_EQ(buffer[5], 0x02);
        EXPECT_EQ(buffer[6], 0x03);
        EXPECT_EQ(buffer[7], 0xEA);
        EXPECT_EQ(buffer[8], 0x92);
        if (writes == 1)
        {
            return length - 1;
        }
        else
        {
            return 0;
        }
    };
    auto read_holding_regs = [](uint8_t* buffer, uint16_t length, uint16_t) -> int16_t {
        buffer[0] = 0x00;
        buffer[1] = 0x01;
        buffer[2] = 0x02;
        buffer[3] = 0x03;
        cb_reads++;
        return 2 * length;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_holding_regs = read_holding_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), -EAGAIN);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_EQ(cb_reads, 1);
    EXPECT_EQ(writes, 2);
}

TEST(ServerF03, ValidRequest_WritePduReturnsError_ReturnError)
{
    static uint16_t writes = 0;
    static uint16_t cb_reads = 0;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kReadHoldingRegsFunctionCode;
        buffer[2] = 0x00;
        buffer[3] = 0x00;
        buffer[4] = 0x00;
        buffer[5] = 0x04;
        buffer[6] = 0x44;
        buffer[7] = 0x09;
        return 8;
    };
    auto write_frame = [](uint8_t*, uint16_t) -> int16_t {
        writes++;
        return -1;
    };
    auto read_holding_regs = [](uint8_t*, uint16_t length, uint16_t) -> int16_t {
        cb_reads++;
        return 2 * length;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .read_holding_regs = read_holding_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), -1);
    EXPECT_EQ(cb_reads, 1);
    EXPECT_EQ(writes, 1);
}
