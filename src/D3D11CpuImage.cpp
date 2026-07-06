//
// D3D11CpuImage.cpp
//
#include <D3D11Helper/D3D11Gpu/D3D11CpuImage.hpp>
#include <D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp>

#include <cstring>
#include <stdexcept>
#include <string>

namespace D3D11CoreLib {
namespace {

[[noreturn]] void ThrowCpuImageError(const char* functionName, const std::string& message) {
    throw std::invalid_argument(std::string(functionName ? functionName : "D3D11CpuImage") + ": " + message);
}

void ValidateDimensions(UINT width, UINT height, const char* functionName) {
    if (width == 0) {
        ThrowCpuImageError(functionName, "width must be > 0");
    }
    if (height == 0) {
        ThrowCpuImageError(functionName, "height must be > 0");
    }
}

void ValidateRowArguments(
    const void* dst,
    UINT dstRowPitch,
    const void* src,
    UINT srcRowPitch,
    UINT rowBytes,
    UINT height,
    const char* functionName) {

    if (height == 0 || rowBytes == 0) {
        return;
    }
    if (!dst) {
        ThrowCpuImageError(functionName, "destination pointer must not be null");
    }
    if (!src) {
        ThrowCpuImageError(functionName, "source pointer must not be null");
    }
    if (dstRowPitch < rowBytes) {
        ThrowCpuImageError(functionName, "destination row pitch is smaller than row bytes");
    }
    if (srcRowPitch < rowBytes) {
        ThrowCpuImageError(functionName, "source row pitch is smaller than row bytes");
    }
}

} // unnamed namespace

bool D3D11CpuImage::Empty() const noexcept {
    return width == 0 || height == 0 || format == DXGI_FORMAT_UNKNOWN || planes.empty() || pixels.empty();
}

UINT D3D11CpuImage::PlaneCount() const noexcept {
    return static_cast<UINT>(planes.size());
}

UINT64 D3D11CpuImage::SizeBytes() const noexcept {
    return static_cast<UINT64>(pixels.size());
}

bool IsSinglePlaneCpuImageFormat(DXGI_FORMAT format) noexcept {
    if (format == DXGI_FORMAT_UNKNOWN) {
        return false;
    }
    if (FormatUtil::IsTypelessFormat(format) ||
        FormatUtil::IsDepthFormat(format) ||
        FormatUtil::IsBlockCompressedFormat(format) ||
        FormatUtil::IsPlanarFormat(format)) {
        return false;
    }
    return FormatUtil::BytesPerPixel(format) != 0;
}

UINT GetPackedRowPitch(UINT width, DXGI_FORMAT format) {
    constexpr const char* kFn = "GetPackedRowPitch";
    if (width == 0) {
        ThrowCpuImageError(kFn, "width must be > 0");
    }
    if (!IsSinglePlaneCpuImageFormat(format)) {
        ThrowCpuImageError(kFn, "unsupported format");
    }

    const UINT bpp = FormatUtil::BytesPerPixel(format);
    return width * bpp;
}

UINT64 GetRequiredCpuImageSize(UINT width, UINT height, DXGI_FORMAT format, UINT rowPitch) {
    constexpr const char* kFn = "GetRequiredCpuImageSize";
    ValidateDimensions(width, height, kFn);

    const UINT packedRowPitch = GetPackedRowPitch(width, format);
    const UINT effectiveRowPitch = (rowPitch == 0) ? packedRowPitch : rowPitch;
    if (effectiveRowPitch < packedRowPitch) {
        ThrowCpuImageError(kFn, "row pitch is smaller than packed row pitch");
    }

    return static_cast<UINT64>(effectiveRowPitch) * static_cast<UINT64>(height);
}

D3D11CpuImage CreateCpuImage(UINT width, UINT height, DXGI_FORMAT format, UINT rowPitch) {
    constexpr const char* kFn = "CreateCpuImage";
    ValidateDimensions(width, height, kFn);

    const UINT packedRowPitch = GetPackedRowPitch(width, format);
    const UINT effectiveRowPitch = (rowPitch == 0) ? packedRowPitch : rowPitch;
    if (effectiveRowPitch < packedRowPitch) {
        ThrowCpuImageError(kFn, "row pitch is smaller than packed row pitch");
    }

    const UINT64 sizeBytes = GetRequiredCpuImageSize(width, height, format, effectiveRowPitch);
    if (sizeBytes > static_cast<UINT64>(static_cast<size_t>(-1))) {
        ThrowCpuImageError(kFn, "image is too large for this platform");
    }

    D3D11CpuImage image;
    image.width = width;
    image.height = height;
    image.format = format;
    image.planes.push_back(D3D11CpuImagePlane{ width, height, effectiveRowPitch, 0 });
    image.pixels.resize(static_cast<size_t>(sizeBytes), 0);
    return image;
}

void ValidateCpuImage(const D3D11CpuImage& image, const char* functionName) {
    ValidateDimensions(image.width, image.height, functionName);
    if (!IsSinglePlaneCpuImageFormat(image.format)) {
        ThrowCpuImageError(functionName, "unsupported format");
    }
    if (image.planes.size() != 1) {
        ThrowCpuImageError(functionName, "expected exactly one plane");
    }

    const auto& plane = image.planes[0];
    if (plane.width != image.width || plane.height != image.height) {
        ThrowCpuImageError(functionName, "plane dimensions do not match image dimensions");
    }

    const UINT packedRowPitch = GetPackedRowPitch(image.width, image.format);
    if (plane.rowPitch < packedRowPitch) {
        ThrowCpuImageError(functionName, "plane row pitch is smaller than packed row pitch");
    }

    const UINT64 requiredSize = plane.offsetBytes + static_cast<UINT64>(plane.rowPitch) * static_cast<UINT64>(plane.height);
    if (requiredSize > image.pixels.size()) {
        ThrowCpuImageError(functionName, "pixel buffer is smaller than required plane storage");
    }
}

void CopyRows(void* dst, UINT dstRowPitch, const void* src, UINT srcRowPitch, UINT rowBytes, UINT height) {
    constexpr const char* kFn = "CopyRows";
    ValidateRowArguments(dst, dstRowPitch, src, srcRowPitch, rowBytes, height, kFn);

    if (height == 0 || rowBytes == 0) {
        return;
    }

    auto* dstBytes = static_cast<uint8_t*>(dst);
    const auto* srcBytes = static_cast<const uint8_t*>(src);
    for (UINT y = 0; y < height; ++y) {
        std::memcpy(dstBytes + static_cast<size_t>(y) * dstRowPitch,
                    srcBytes + static_cast<size_t>(y) * srcRowPitch,
                    rowBytes);
    }
}

std::vector<uint8_t> PackRows(const void* src, UINT srcRowPitch, UINT rowBytes, UINT height) {
    constexpr const char* kFn = "PackRows";
    if (height == 0 || rowBytes == 0) {
        return {};
    }
    if (!src) {
        ThrowCpuImageError(kFn, "source pointer must not be null");
    }
    if (srcRowPitch < rowBytes) {
        ThrowCpuImageError(kFn, "source row pitch is smaller than row bytes");
    }

    std::vector<uint8_t> packed(static_cast<size_t>(rowBytes) * height);
    CopyRows(packed.data(), rowBytes, src, srcRowPitch, rowBytes, height);
    return packed;
}

void UnpackRows(void* dst, UINT dstRowPitch, const void* packed, UINT rowBytes, UINT height) {
    constexpr const char* kFn = "UnpackRows";
    if (height == 0 || rowBytes == 0) {
        return;
    }
    if (!dst) {
        ThrowCpuImageError(kFn, "destination pointer must not be null");
    }
    if (!packed) {
        ThrowCpuImageError(kFn, "packed source pointer must not be null");
    }
    if (dstRowPitch < rowBytes) {
        ThrowCpuImageError(kFn, "destination row pitch is smaller than row bytes");
    }

    CopyRows(dst, dstRowPitch, packed, rowBytes, rowBytes, height);
}

} // namespace D3D11CoreLib
