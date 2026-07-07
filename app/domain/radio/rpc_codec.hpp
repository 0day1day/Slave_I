#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "domain/radio/rpc.hpp"

namespace spectra5::domain {

struct RpcDecodeResult {
    RpcFrame frame;
    RpcCodecError error = RpcCodecError::Ok;
};

std::vector<uint8_t> rpc_encode(const RpcFrame& frame);

RpcDecodeResult rpc_decode(const uint8_t* data, std::size_t size);

inline std::optional<RpcFrame> rpc_decode_frame(const uint8_t* data, std::size_t size)
{
    const RpcDecodeResult result = rpc_decode(data, size);
    if (result.error != RpcCodecError::Ok) {
        return std::nullopt;
    }
    return result.frame;
}

}  // namespace spectra5::domain
