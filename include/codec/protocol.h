#pragma once
#include <cstdint>

namespace rpc {
namespace codec {

// Wire format:
//  +----------+----------+----------+
//  | Magic(4) | Length(4)| Body(N)  |
//  +----------+----------+----------+
// Magic  = 0xCAFEBABE (network byte order)
// Length = byte length of Body (network byte order)
// Body   = serialized RpcRequest or RpcResponse protobuf

static constexpr uint32_t kMagicNumber  = 0xCAFEBABE;
static constexpr size_t   kHeaderSize   = 8;  // 4 (magic) + 4 (length)
static constexpr size_t   kMaxBodySize  = 64 * 1024 * 1024;  // 64 MB sanity cap

} // namespace codec
} // namespace rpc
