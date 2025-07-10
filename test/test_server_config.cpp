#include <gtest/gtest.h>
#include <cstdint>

#include "simple_modbus.h"

constexpr uint8_t kServerAddr = 0x01;

TEST(ServerConfig, Success)
{
    auto mock_read_pdu = [](uint8_t*, uint16_t) -> int16_t {
        return 0;
    };
    auto mock_write_pdu = [](uint8_t*, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {mock_read_pdu, mock_write_pdu};
    smb_server_if_t callback = {};

    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), 0);
}

TEST(ServerConfig, NullTransport)
{
    smb_server_if_t callback = {};

    EXPECT_EQ(smb_server_config(kServerAddr, nullptr, &callback), -EFAULT);
    EXPECT_EQ(smb_server_poll(), -EFAULT);
}

TEST(ServerConfig, NullCallback)
{
    auto mock_read_pdu = [](uint8_t*, uint16_t) -> int16_t {
        return 0;
    };
    auto mock_write_pdu = [](uint8_t*, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {mock_read_pdu, mock_write_pdu};

    EXPECT_EQ(smb_server_config(kServerAddr, &interface, nullptr), -EFAULT);
    EXPECT_EQ(smb_server_poll(), -EFAULT);
}

TEST(ServerConfig, NullReadPdu)
{
    auto mock_write_pdu = [](uint8_t*, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {nullptr, mock_write_pdu};
    smb_server_if_t callback = {};

    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), -EFAULT);
    EXPECT_EQ(smb_server_poll(), -EFAULT);
}

TEST(ServerConfig, NullWritePdu)
{
    auto mock_read_pdu = [](uint8_t*, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {mock_read_pdu, nullptr};
    smb_server_if_t callback = {};

    EXPECT_EQ(smb_server_config(kServerAddr, &interface, &callback), -EFAULT);
    EXPECT_EQ(smb_server_poll(), -EFAULT);
}

TEST(ServerConfig, BroadcastAddress)
{
    auto mock_read_pdu = [](uint8_t*, uint16_t) -> int16_t {
        return 0;
    };
    auto mock_write_pdu = [](uint8_t*, uint16_t) -> int16_t {
        return 0;
    };
    smb_transport_if_t interface = {mock_read_pdu, mock_write_pdu};
    smb_server_if_t callback = {};

    EXPECT_EQ(smb_server_config(0, &interface, &callback), -EINVAL);
}

TEST(ServerPoll, NotConfigured_BadAddress)
{
    EXPECT_EQ(smb_server_poll(), -EFAULT);
}
