#include <gtest/gtest.h>

#include "simple_modbus.h"
#include "test_common.h"

TEST(ServerF16, NoCallbackDefined_Reply01Return0)
{
    static bool was_write_called = false;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x00;  // Start address high byte
        buffer[3] = 0x00;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x01;  // One register to write
        buffer[6] = 0x02;  // 2 bytes for one register
        buffer[7] = 0x00;  // Register value high
        buffer[8] = 0x2A;  // Register value low
        buffer[9] = 0x27;
        buffer[10] = 0x8F;
        return 11;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        EXPECT_EQ(length, 5);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kWriteMultipleRegisters | kErrorFlag);
        EXPECT_EQ(buffer[2], 0x01);
        EXPECT_EQ(buffer[3], 0x8D);
        EXPECT_EQ(buffer[4], 0xC0);
        was_write_called = true;
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = nullptr,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_TRUE(was_write_called);
}

TEST(ServerF16, PduLengthTooSmall_Reply03Return0)
{
    static bool was_write_called = false;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x00;  // Start address high byte
        buffer[3] = 0x00;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x01;  // One register to write
        buffer[6] = 0x01;  // Register value high
        buffer[7] = 0x41;  // Register value low
        buffer[8] = 0x00;  // Already crc, missing one byte
        buffer[9] = 0x66;
        return 10;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        EXPECT_EQ(length, 5);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kWriteMultipleRegisters | kErrorFlag);
        EXPECT_EQ(buffer[2], 0x03);
        EXPECT_EQ(buffer[3], 0x0C);
        EXPECT_EQ(buffer[4], 0x01);
        was_write_called = true;
        return 0;
    };
    auto write_regs = [](const uint16_t*, uint16_t, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = write_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_TRUE(was_write_called);
}

TEST(ServerF16, PduLengthIncorrect_Reply03Return0)
{
    static bool was_write_called = false;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x00;  // Start address high byte
        buffer[3] = 0x00;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x01;  // One register to write
        buffer[6] = 0x02;  // Two bytes for one register
        buffer[7] = 0x00;  // Register value high
        buffer[8] = 0x2A;  // Register value low
        buffer[9] = 0x00;  // This byte has nothing to do here
        buffer[10] = 0xCF;
        buffer[11] = 0x1A;
        return 12;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        EXPECT_EQ(length, 5);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kWriteMultipleRegisters | kErrorFlag);
        EXPECT_EQ(buffer[2], 0x03);
        EXPECT_EQ(buffer[3], 0x0C);
        EXPECT_EQ(buffer[4], 0x01);
        was_write_called = true;
        return 0;
    };
    auto write_regs = [](const uint16_t*, uint16_t, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = write_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_TRUE(was_write_called);
}

TEST(ServerF16, InconsistentNbytesInPdu_Reply03Return0)
{
    static bool was_write_called = false;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x00;  // Start address high byte
        buffer[3] = 0x00;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x01;  // One register to write
        buffer[6] = 0x03;  // Wrong number of bytes
        buffer[7] = 0x10;  // Register value high
        buffer[8] = 0x2A;  // Register value low
        buffer[9] = 0x7B;
        buffer[10] = 0x8F;
        return 11;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        EXPECT_EQ(length, 5);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kWriteMultipleRegisters | kErrorFlag);
        EXPECT_EQ(buffer[2], 0x03);
        EXPECT_EQ(buffer[3], 0x0C);
        EXPECT_EQ(buffer[4], 0x01);
        was_write_called = true;
        return 0;
    };
    auto write_regs = [](const uint16_t*, uint16_t, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = write_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_TRUE(was_write_called);
}

TEST(ServerF16, ValidRequest_CallbackReturnsError_Reply02Return0)
{
    static bool was_write_called = false;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x00;  // Start address high byte
        buffer[3] = 0x00;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x01;  // One register to write
        buffer[6] = 0x02;  // Two bytes for one register
        buffer[7] = 0x00;  // Register value high
        buffer[8] = 0x2A;  // Register value low
        buffer[9] = 0x27;
        buffer[10] = 0x8F;
        return 11;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        EXPECT_EQ(length, 5);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kWriteMultipleRegisters | kErrorFlag);
        EXPECT_EQ(buffer[2], 0x02);
        EXPECT_EQ(buffer[3], 0xCD);
        EXPECT_EQ(buffer[4], 0xC1);
        was_write_called = true;
        return 0;
    };
    auto write_regs = [](const uint16_t*, uint16_t, uint16_t) -> int16_t {
        return -1;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = write_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_TRUE(was_write_called);
}

TEST(ServerF16, ValidRequest_CallbackReturnsZero_NoReplyReturnEAGAIN)
{
    static uint16_t writes = 0;
    static uint16_t cb_writes = 0;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x00;  // Start address high byte
        buffer[3] = 0x00;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x01;  // One register to write
        buffer[6] = 0x02;  // Two bytes for one register
        buffer[7] = 0x00;  // Register value high
        buffer[8] = 0x2A;  // Register value low
        buffer[9] = 0x27;
        buffer[10] = 0x8F;
        return 11;
    };
    auto write_frame = [](uint8_t*, uint16_t) -> int16_t {
        writes++;
        return 0;
    };
    auto write_regs = [](const uint16_t*, uint16_t length, uint16_t) -> int16_t {
        cb_writes++;
        if (cb_writes == 1)
        {
            return 0;
        }
        return length;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = write_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), -EAGAIN);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_EQ(cb_writes, 2);
    EXPECT_EQ(writes, 1);
}

TEST(ServerF16, ValidRequest_WritePduReturnsLength_Return0)
{
    static uint16_t writes = 0;
    static uint16_t cb_writes = 0;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x42;  // Start address high byte
        buffer[3] = 0x73;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x01;  // One register to write
        buffer[6] = 0x02;  // Two bytes for one register
        buffer[7] = 0x40;  // Register value high
        buffer[8] = 0x2A;  // Register value low
        buffer[9] = 0x7F;
        buffer[10] = 0x48;
        return 11;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        writes++;
        EXPECT_EQ(length, 8);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kWriteMultipleRegisters);
        EXPECT_EQ(buffer[2], 0x42);  // Start address high byte
        EXPECT_EQ(buffer[3], 0x73);  // Start address low byte
        EXPECT_EQ(buffer[4], 0x00);
        EXPECT_EQ(buffer[5], 0x01);  // One register to write
        EXPECT_EQ(buffer[6], 0xE4);
        EXPECT_EQ(buffer[7], 0x6A);
        return 0;
    };
    auto write_regs = [](const uint16_t* buffer, uint16_t length, uint16_t start_addr) -> int16_t {
        EXPECT_EQ(start_addr, (0x42 << 8) | 0x73);
        EXPECT_EQ(length, 1);
        EXPECT_EQ(buffer[0], 0x2A40);
        cb_writes++;
        return length;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = write_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_EQ(cb_writes, 1);
    EXPECT_EQ(writes, 1);
}

TEST(ServerF16, ValidRequest_WritePduReturnsLessThanLength_ReturnEAGAIN)
{
    static uint16_t writes = 0;
    static uint16_t cb_write_regs = 0;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x42;  // Start address high byte
        buffer[3] = 0x73;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x01;  // One register to write
        buffer[6] = 0x02;  // Two bytes for one register
        buffer[7] = 0x40;  // Register value high
        buffer[8] = 0x2A;  // Register value low
        buffer[9] = 0x7F;
        buffer[10] = 0x48;
        return 11;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        writes++;
        EXPECT_EQ(length, 8);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kWriteMultipleRegisters);
        EXPECT_EQ(buffer[2], 0x42);  // Start address high byte
        EXPECT_EQ(buffer[3], 0x73);  // Start address low byte
        EXPECT_EQ(buffer[4], 0x00);
        EXPECT_EQ(buffer[5], 0x01);  // One register to write
        EXPECT_EQ(buffer[6], 0xE4);
        EXPECT_EQ(buffer[7], 0x6A);
        if (writes == 1)
        {
            return length - 1;
        }
        else
        {
            return 0;
        }
    };
    auto write_regs = [](const uint16_t*, uint16_t length, uint16_t) -> int16_t {
        cb_write_regs++;
        return length;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = write_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), -EAGAIN);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_EQ(cb_write_regs, 1);
    EXPECT_EQ(writes, 2);
}

TEST(ServerF16, ValidRequest_WritePduReturnsError_ReturnError)
{
    static uint16_t writes = 0;
    static uint16_t cb_writes = 0;
    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x42;  // Start address high byte
        buffer[3] = 0x73;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x01;  // One register to write
        buffer[6] = 0x02;  // Two bytes for one register
        buffer[7] = 0x40;  // Register value high
        buffer[8] = 0x2A;  // Register value low
        buffer[9] = 0x7F;
        buffer[10] = 0x48;
        return 11;
    };
    auto write_frame = [](uint8_t*, uint16_t) -> int16_t {
        writes++;
        return -1;
    };
    auto write_regs = [](const uint16_t*, uint16_t length, uint16_t) -> int16_t {
        cb_writes++;
        return length;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = write_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), -1);
    EXPECT_EQ(cb_writes, 1);
    EXPECT_EQ(writes, 1);
}

TEST(ServerF16, ValidRequest123Bytes_WritePduReturnsLength_Return0)
{
    static uint16_t writes = 0;
    static uint16_t cb_writes = 0;

    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x42;  // Start address high byte
        buffer[3] = 0x73;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x7B;  // One register to write
        buffer[6] = 0xF6;  // Twice number of registers
        for (uint8_t i = 7; i < 253; i++)
        {
            buffer[i] = i;
        }
        buffer[253] = 0xF7;  // CRC high
        buffer[254] = 0x85;  // CRC low
        return 255;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        writes++;
        EXPECT_EQ(length, 8);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kWriteMultipleRegisters);
        EXPECT_EQ(buffer[2], 0x42);  // Start address high byte
        EXPECT_EQ(buffer[3], 0x73);  // Start address low byte
        EXPECT_EQ(buffer[4], 0x00);
        EXPECT_EQ(buffer[5], 0x7B);  // 123 registers
        EXPECT_EQ(buffer[6], 0x65);
        EXPECT_EQ(buffer[7], 0x89);
        return 0;
    };
    auto write_regs = [](const uint16_t* buffer, uint16_t length, uint16_t start_addr) -> int16_t {
        EXPECT_EQ(start_addr, (0x42 << 8) | 0x73);
        EXPECT_EQ(length, 123);
        for (uint16_t i = 0; i < length; i++)
        {
            uint16_t high_byte = ((2 * i) + 7);
            uint16_t low_byte = ((2 * i) + 1 + 7);
            EXPECT_EQ(buffer[i], (low_byte << 8) | high_byte);
        }
        cb_writes++;
        return length;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = write_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_EQ(cb_writes, 1);
    EXPECT_EQ(writes, 1);
}

TEST(ServerF16, TooManyBytesRequested_Reply03Return0)
{
    static uint16_t writes = 0;
    static uint16_t cb_writes = 0;

    auto read_frame = [](uint8_t* buffer, uint16_t) -> int16_t {
        buffer[0] = kServerAddr;
        buffer[1] = kWriteMultipleRegisters;
        buffer[2] = 0x42;  // Start address high byte
        buffer[3] = 0x73;  // Start address low byte
        buffer[4] = 0x00;
        buffer[5] = 0x7C;  // One register to write
        buffer[6] = 0xF8;  // Twice number of registers
        for (uint8_t i = 7; i < 253; i++)
        {
            buffer[i] = i;
        }
        buffer[253] = 0x03;  // CRC high
        buffer[254] = 0xA5;  // CRC low
        return 255;
    };
    auto write_frame = [](uint8_t* buffer, uint16_t length) -> int16_t {
        EXPECT_EQ(length, 5);
        EXPECT_EQ(buffer[0], kServerAddr);
        EXPECT_EQ(buffer[1], kWriteMultipleRegisters | kErrorFlag);
        EXPECT_EQ(buffer[2], 0x03);
        EXPECT_EQ(buffer[3], 0x0C);
        EXPECT_EQ(buffer[4], 0x01);
        writes++;
        return 0;
    };
    auto write_regs = [](const uint16_t*, uint16_t, uint16_t) -> int16_t {
        cb_writes++;
        return 0;
    };
    smb_transport_if_t interface = {read_frame, write_frame};
    smb_server_if_t callback = {
        .write_regs = write_regs,
    };
    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
    EXPECT_EQ(smb_server_poll(), 0);
    EXPECT_EQ(cb_writes, 0);
    EXPECT_EQ(writes, 1);
}
