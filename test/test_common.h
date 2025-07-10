#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

constexpr unsigned char kServerAddr = 0x01;
constexpr unsigned char kReadHoldingRegsFunctionCode = 0x03;
constexpr unsigned char kReadInputRegsFunctionCode = 0x04;
constexpr unsigned char kWriteSingleRegister = 0x06;
constexpr unsigned char kWriteMultipleRegisters = 0x10;

constexpr unsigned char kErrorFlag = 0x80;
constexpr unsigned char kErrorIllegalFunctionCode = 0x01;

constexpr uint8_t kMaxNumberOfRegisters = 0x7D;

#endif  // TEST_COMMON_H_
