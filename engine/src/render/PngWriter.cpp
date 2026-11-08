#include "zypher/render/PngWriter.h"

#include "zypher/assets/PackFile.h" // crc32
#include "zypher/core/FileSystem.h"
#include "zypher/core/Log.h"

#include <array>
#include <cstring>

namespace zy {
namespace {

void appendU32BigEndian(std::vector<u8>& output, u32 value) {
    output.push_back(static_cast<u8>((value >> 24) & 0xFF));
    output.push_back(static_cast<u8>((value >> 16) & 0xFF));
    output.push_back(static_cast<u8>((value >> 8) & 0xFF));
    output.push_back(static_cast<u8>(value & 0xFF));
}

void appendU16LittleEndian(std::vector<u8>& output, u16 value) {
    output.push_back(static_cast<u8>(value & 0xFF));
    output.push_back(static_cast<u8>((value >> 8) & 0xFF));
}

void appendChunk(std::vector<u8>& output, const char type[5], std::span<const u8> data) {
    appendU32BigEndian(output, static_cast<u32>(data.size()));
    const usize typeOffset = output.size();
    output.insert(output.end(), type, type + 4);
    output.insert(output.end(), data.begin(), data.end());
    const u32 checksum = crc32(ByteSpan{output.data() + typeOffset, 4 + data.size()});
    appendU32BigEndian(output, checksum);
}

[[nodiscard]] u32 adler32(std::span<const u8> data) {
    constexpr u32 kModulus = 65521;
    u32 a = 1;
    u32 b = 0;
    for (const u8 value : data) {
        a = (a + value) % kModulus;
        b = (b + a) % kModulus;
    }
    return (b << 16) | a;
}

} // namespace

std::vector<u8> encodePng(u32 width, u32 height, std::span<const u8> rgba) {
    std::vector<u8> output;
    if (width == 0 || height == 0 || rgba.size() < static_cast<usize>(width) * height * 4) {
        return output;
    }

    static constexpr std::array<u8, 8> kSignature{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    output.insert(output.end(), kSignature.begin(), kSignature.end());

    std::vector<u8> header;
    appendU32BigEndian(header, width);
    appendU32BigEndian(header, height);
    header.push_back(8); // bit depth
    header.push_back(6); // colour type: truecolour with alpha
    header.push_back(0); // compression
    header.push_back(0); // filter method
    header.push_back(0); // no interlacing
    appendChunk(output, "IHDR", header);

    // Raw scanlines with filter byte 0.
    std::vector<u8> raw;
    raw.reserve((static_cast<usize>(width) * 4 + 1) * height);
    for (u32 y = 0; y < height; ++y) {
        raw.push_back(0);
        const auto* row = rgba.data() + static_cast<usize>(y) * width * 4;
        raw.insert(raw.end(), row, row + static_cast<usize>(width) * 4);
    }

    // zlib stream using stored (uncompressed) deflate blocks.
    std::vector<u8> zlib;
    zlib.push_back(0x78);
    zlib.push_back(0x01);
    constexpr usize kMaxBlock = 65535;
    for (usize offset = 0; offset < raw.size(); offset += kMaxBlock) {
        const usize remaining = raw.size() - offset;
        const usize blockSize = remaining > kMaxBlock ? kMaxBlock : remaining;
        const bool last = offset + blockSize >= raw.size();
        // Deflate stored block: BFINAL/BTYPE byte, then 16-bit LEN and NLEN.
        // Writing these as 32-bit values corrupts the stream (and the PNG).
        zlib.push_back(last ? 0x01 : 0x00);
        appendU16LittleEndian(zlib, static_cast<u16>(blockSize));
        appendU16LittleEndian(zlib, static_cast<u16>(~static_cast<u16>(blockSize)));
        zlib.insert(zlib.end(), raw.begin() + static_cast<isize>(offset),
                    raw.begin() + static_cast<isize>(offset + blockSize));
    }
    if (raw.empty()) {
        zlib.push_back(0x01);
        appendU16LittleEndian(zlib, 0);
        appendU16LittleEndian(zlib, 0xFFFFu);
    }
    appendU32BigEndian(zlib, adler32(raw));
    appendChunk(output, "IDAT", zlib);
    appendChunk(output, "IEND", {});
    return output;
}

bool writePng(const std::string& path, u32 width, u32 height, std::span<const u8> rgba) {
    const auto png = encodePng(width, height, rgba);
    if (png.empty()) {
        ZY_LOG_ERROR("render", "cannot encode PNG for '", path, "'");
        return false;
    }
    if (const auto status = FileSystem::writeBinary(path, ByteSpan{png.data(), png.size()});
        status.hasError()) {
        ZY_LOG_ERROR("render", "cannot write PNG '", path, "': ", status.error().message);
        return false;
    }
    return true;
}

} // namespace zy
