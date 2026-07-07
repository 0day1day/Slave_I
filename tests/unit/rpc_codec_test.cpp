#include <cassert>
#include <cstring>
#include <vector>

#include "domain/radio/rpc_codec.hpp"

using namespace spectra5::domain;

int main()
{
    RpcFrame frame;
    frame.header.message_type = static_cast<uint8_t>(RpcMessageType::HandshakeRequest);
    frame.header.sequence     = 42;
    frame.payload             = {0x01, 0x02, 0x03, 0x04};

    const std::vector<uint8_t> encoded = rpc_encode(frame);
    assert(encoded.size() == kRpcHeaderSize + frame.payload.size());

    const auto decoded = rpc_decode_frame(encoded.data(), encoded.size());
    assert(decoded.has_value());
    assert(decoded->header.magic == kRpcMagic);
    assert(decoded->header.protocol_version == kRpcProtocolVersion);
    assert(decoded->header.message_type ==
           static_cast<uint8_t>(RpcMessageType::HandshakeRequest));
    assert(decoded->header.sequence == 42);
    assert(decoded->payload == frame.payload);

    auto corrupt = encoded;
    corrupt[corrupt.size() - 1] ^= 0xFF;
    const RpcDecodeResult bad = rpc_decode(corrupt.data(), corrupt.size());
    assert(bad.error == RpcCodecError::BadChecksum);

    uint8_t short_buf[kRpcHeaderSize - 1]{};
    const RpcDecodeResult small = rpc_decode(short_buf, sizeof(short_buf));
    assert(small.error == RpcCodecError::BufferTooSmall);

    RpcFrameHeader bad_magic;
    bad_magic.magic = 0xDEAD;
    bad_magic.payload_size = 0;
    uint8_t hdr[kRpcHeaderSize]{};
    hdr[0] = 0xAD;
    hdr[1] = 0xDE;
    const RpcDecodeResult magic_fail = rpc_decode(hdr, sizeof(hdr));
    assert(magic_fail.error == RpcCodecError::BadMagic);

    return 0;
}
