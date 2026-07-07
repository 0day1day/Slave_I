#pragma once

#include <cstdint>
#include <vector>

namespace spectra5::domain {

constexpr uint16_t kRpcMagic            = 0x5C01;
constexpr uint8_t kRpcProtocolVersion = 1;
constexpr std::size_t kRpcHeaderSize  = 16;

enum class RpcMessageType : uint8_t {
    HandshakeRequest  = 1,
    HandshakeResponse = 2,
    Capabilities      = 3,
    Ping              = 4,
    Pong              = 5,
    Event             = 6,
    Error             = 7,
    Command           = 8,  // offensive RadioCommand payload (P4 -> C6); see offensive.hpp
};

struct RpcFrameHeader {
    uint16_t magic             = kRpcMagic;
    uint8_t protocol_version   = kRpcProtocolVersion;
    uint8_t message_type       = 0;
    uint32_t sequence          = 0;
    uint32_t payload_size      = 0;
    uint32_t checksum          = 0;
};

struct RpcFrame {
    RpcFrameHeader header;
    std::vector<uint8_t> payload;
};

enum class RpcCodecError {
    Ok,
    BufferTooSmall,
    BadMagic,
    BadVersion,
    BadChecksum,
    PayloadSizeMismatch,
};

inline const char* rpc_codec_error_name(RpcCodecError err)
{
    switch (err) {
        case RpcCodecError::Ok:                   return "ok";
        case RpcCodecError::BufferTooSmall:       return "buffer_too_small";
        case RpcCodecError::BadMagic:             return "bad_magic";
        case RpcCodecError::BadVersion:           return "bad_version";
        case RpcCodecError::BadChecksum:          return "bad_checksum";
        case RpcCodecError::PayloadSizeMismatch:  return "payload_size_mismatch";
    }
    return "unknown";
}

uint32_t rpc_checksum(const RpcFrameHeader& header_without_checksum,
                      const uint8_t* payload, std::size_t payload_size);

}  // namespace spectra5::domain
