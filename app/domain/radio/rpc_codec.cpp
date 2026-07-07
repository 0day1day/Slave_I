#include "domain/radio/rpc_codec.hpp"

#include <cstring>

namespace spectra5::domain {

namespace {

uint32_t read_u32_le(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t read_u16_le(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

void write_u32_le(uint8_t* p, uint32_t value)
{
    p[0] = static_cast<uint8_t>(value);
    p[1] = static_cast<uint8_t>(value >> 8);
    p[2] = static_cast<uint8_t>(value >> 16);
    p[3] = static_cast<uint8_t>(value >> 24);
}

void write_u16_le(uint8_t* p, uint16_t value)
{
    p[0] = static_cast<uint8_t>(value);
    p[1] = static_cast<uint8_t>(value >> 8);
}

RpcFrameHeader header_from_bytes(const uint8_t* data)
{
    RpcFrameHeader header;
    header.magic           = read_u16_le(data + 0);
    header.protocol_version = data[2];
    header.message_type    = data[3];
    header.sequence        = read_u32_le(data + 4);
    header.payload_size    = read_u32_le(data + 8);
    header.checksum        = read_u32_le(data + 12);
    return header;
}

void header_to_bytes(const RpcFrameHeader& header, uint8_t* out)
{
    write_u16_le(out + 0, header.magic);
    out[2] = header.protocol_version;
    out[3] = header.message_type;
    write_u32_le(out + 4, header.sequence);
    write_u32_le(out + 8, header.payload_size);
    write_u32_le(out + 12, header.checksum);
}

}  // namespace

uint32_t rpc_checksum(const RpcFrameHeader& header_without_checksum, const uint8_t* payload,
                        std::size_t payload_size)
{
    uint32_t sum = 0;
    sum += header_without_checksum.magic;
    sum += header_without_checksum.protocol_version;
    sum += header_without_checksum.message_type;
    sum += header_without_checksum.sequence;
    sum += header_without_checksum.payload_size;

    for (std::size_t i = 0; i < payload_size; ++i) {
        sum = (sum * 131u) + payload[i];
    }
    return sum;
}

std::vector<uint8_t> rpc_encode(const RpcFrame& frame)
{
    RpcFrameHeader header = frame.header;
    header.payload_size   = static_cast<uint32_t>(frame.payload.size());
    header.checksum       = 0;
    header.checksum =
        rpc_checksum(header, frame.payload.data(), frame.payload.size());

    std::vector<uint8_t> out(kRpcHeaderSize + frame.payload.size());
    header_to_bytes(header, out.data());
    if (!frame.payload.empty()) {
        std::memcpy(out.data() + kRpcHeaderSize, frame.payload.data(), frame.payload.size());
    }
    return out;
}

RpcDecodeResult rpc_decode(const uint8_t* data, std::size_t size)
{
    RpcDecodeResult result;
    if (data == nullptr || size < kRpcHeaderSize) {
        result.error = RpcCodecError::BufferTooSmall;
        return result;
    }

    result.frame.header = header_from_bytes(data);
    if (result.frame.header.magic != kRpcMagic) {
        result.error = RpcCodecError::BadMagic;
        return result;
    }
    if (result.frame.header.protocol_version != kRpcProtocolVersion) {
        result.error = RpcCodecError::BadVersion;
        return result;
    }

    const std::size_t expected = kRpcHeaderSize + result.frame.header.payload_size;
    if (size != expected) {
        result.error = RpcCodecError::PayloadSizeMismatch;
        return result;
    }

    if (result.frame.header.payload_size > 0) {
        result.frame.payload.resize(result.frame.header.payload_size);
        std::memcpy(result.frame.payload.data(), data + kRpcHeaderSize,
                    result.frame.header.payload_size);
    } else {
        result.frame.payload.clear();
    }

    const uint32_t expected_checksum = result.frame.header.checksum;
    RpcFrameHeader verify_header   = result.frame.header;
    verify_header.checksum         = 0;
    const uint32_t actual_checksum =
        rpc_checksum(verify_header, result.frame.payload.data(), result.frame.payload.size());
    if (actual_checksum != expected_checksum) {
        result.error = RpcCodecError::BadChecksum;
        return result;
    }

    result.error = RpcCodecError::Ok;
    return result;
}

}  // namespace spectra5::domain
