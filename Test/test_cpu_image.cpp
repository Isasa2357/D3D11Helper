//
// test_cpu_image.cpp - D3D11CpuImage / row-pitch utility tests
//
// This test is CPU-only. It verifies the DirectX/DXGI-only memory bridge used
// by v1.2.0 texture transfer APIs. It intentionally does not perform file I/O.
//
#include "TestUtil.hpp"
#include <D3D11Helper/D3D11Gpu/D3D11CpuImage.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

using namespace D3D11CoreLib;

namespace {

void ExpectThrow(const char* name, const std::function<void()>& fn) {
    bool threw = false;
    try {
        fn();
    } catch (...) {
        threw = true;
    }
    if (!threw) {
        throw std::runtime_error(std::string(name) + " did not throw");
    }
}

void ExpectEqU64(UINT64 actual, UINT64 expected, const char* what) {
    if (actual != expected) {
        throw std::runtime_error(std::string(what) + ": expected " +
                                 std::to_string(expected) + ", got " +
                                 std::to_string(actual));
    }
}

} // namespace

int main() {
    TEST_RUN("supported single-plane formats", {
        if (!IsSinglePlaneCpuImageFormat(DXGI_FORMAT_R8_UNORM)) {
            throw std::runtime_error("R8_UNORM should be supported");
        }
        if (!IsSinglePlaneCpuImageFormat(DXGI_FORMAT_R8G8B8A8_UNORM)) {
            throw std::runtime_error("RGBA8 should be supported");
        }
        if (!IsSinglePlaneCpuImageFormat(DXGI_FORMAT_B8G8R8A8_UNORM)) {
            throw std::runtime_error("BGRA8 should be supported");
        }
    });

    TEST_RUN("unsupported CPU image formats", {
        if (IsSinglePlaneCpuImageFormat(DXGI_FORMAT_UNKNOWN)) {
            throw std::runtime_error("UNKNOWN should not be supported");
        }
        if (IsSinglePlaneCpuImageFormat(DXGI_FORMAT_NV12)) {
            throw std::runtime_error("NV12 should not be supported by v1.2.0 CpuImage");
        }
        if (IsSinglePlaneCpuImageFormat(DXGI_FORMAT_BC1_UNORM)) {
            throw std::runtime_error("BC1 should not be supported");
        }
        if (IsSinglePlaneCpuImageFormat(DXGI_FORMAT_D32_FLOAT)) {
            throw std::runtime_error("depth format should not be supported");
        }
        if (IsSinglePlaneCpuImageFormat(DXGI_FORMAT_R8G8B8A8_TYPELESS)) {
            throw std::runtime_error("typeless format should not be supported");
        }
    });

    TEST_RUN("packed row pitch", {
        ExpectEqU64(GetPackedRowPitch(4, DXGI_FORMAT_R8G8B8A8_UNORM), 16, "RGBA8 row pitch");
        ExpectEqU64(GetPackedRowPitch(5, DXGI_FORMAT_R8_UNORM), 5, "R8 row pitch");
        ExpectEqU64(GetPackedRowPitch(2, DXGI_FORMAT_R16G16_FLOAT), 8, "RG16F row pitch");
    });

    TEST_RUN("required CPU image size", {
        ExpectEqU64(GetRequiredCpuImageSize(4, 3, DXGI_FORMAT_R8G8B8A8_UNORM), 48, "packed size");
        ExpectEqU64(GetRequiredCpuImageSize(4, 3, DXGI_FORMAT_R8G8B8A8_UNORM, 20), 60, "padded size");
    });

    TEST_RUN("CreateCpuImage packed", {
        auto image = CreateCpuImage(3, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
        if (image.Empty()) throw std::runtime_error("image should not be empty");
        ExpectEqU64(image.width, 3, "width");
        ExpectEqU64(image.height, 2, "height");
        if (image.format != DXGI_FORMAT_R8G8B8A8_UNORM) throw std::runtime_error("format mismatch");
        ExpectEqU64(image.PlaneCount(), 1, "plane count");
        ExpectEqU64(image.planes[0].rowPitch, 12, "row pitch");
        ExpectEqU64(image.SizeBytes(), 24, "size bytes");
        ValidateCpuImage(image, "test");
        if (!std::all_of(image.pixels.begin(), image.pixels.end(), [](uint8_t v) { return v == 0; })) {
            throw std::runtime_error("new image should be zero-initialized");
        }
    });

    TEST_RUN("CreateCpuImage padded", {
        auto image = CreateCpuImage(3, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 16);
        ExpectEqU64(image.planes[0].rowPitch, 16, "padded row pitch");
        ExpectEqU64(image.SizeBytes(), 32, "padded size");
        ValidateCpuImage(image, "test");
    });

    TEST_RUN("invalid CpuImage arguments throw", {
        ExpectThrow("GetPackedRowPitch zero width", [] {
            (void)GetPackedRowPitch(0, DXGI_FORMAT_R8G8B8A8_UNORM);
        });
        ExpectThrow("GetRequiredCpuImageSize zero height", [] {
            (void)GetRequiredCpuImageSize(4, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        });
        ExpectThrow("CreateCpuImage small rowPitch", [] {
            (void)CreateCpuImage(4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 8);
        });
        ExpectThrow("CreateCpuImage unsupported format", [] {
            (void)CreateCpuImage(4, 4, DXGI_FORMAT_NV12);
        });
    });

    TEST_RUN("ValidateCpuImage catches malformed image", {
        auto image = CreateCpuImage(4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
        image.planes[0].rowPitch = 8;
        ExpectThrow("ValidateCpuImage small rowPitch", [&] {
            ValidateCpuImage(image, "test");
        });

        image = CreateCpuImage(4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
        image.pixels.resize(4);
        ExpectThrow("ValidateCpuImage small pixel buffer", [&] {
            ValidateCpuImage(image, "test");
        });
    });

    TEST_RUN("CopyRows with different pitches", {
        const UINT height = 3;
        const UINT rowBytes = 4;
        const UINT srcPitch = 6;
        const UINT dstPitch = 8;

        std::vector<uint8_t> src(srcPitch * height, 0xEE);
        for (UINT y = 0; y < height; ++y) {
            for (UINT x = 0; x < rowBytes; ++x) {
                src[y * srcPitch + x] = static_cast<uint8_t>(y * 10 + x);
            }
        }

        std::vector<uint8_t> dst(dstPitch * height, 0xCC);
        CopyRows(dst.data(), dstPitch, src.data(), srcPitch, rowBytes, height);

        for (UINT y = 0; y < height; ++y) {
            for (UINT x = 0; x < rowBytes; ++x) {
                const uint8_t expected = static_cast<uint8_t>(y * 10 + x);
                if (dst[y * dstPitch + x] != expected) {
                    throw std::runtime_error("copied byte mismatch");
                }
            }
            for (UINT x = rowBytes; x < dstPitch; ++x) {
                if (dst[y * dstPitch + x] != 0xCC) {
                    throw std::runtime_error("destination padding was modified");
                }
            }
        }
    });

    TEST_RUN("PackRows and UnpackRows", {
        const UINT height = 2;
        const UINT rowBytes = 3;
        const UINT srcPitch = 5;
        const UINT dstPitch = 6;

        std::vector<uint8_t> src = {
            1, 2, 3, 99, 99,
            4, 5, 6, 88, 88,
        };

        auto packed = PackRows(src.data(), srcPitch, rowBytes, height);
        if (packed != std::vector<uint8_t>({ 1, 2, 3, 4, 5, 6 })) {
            throw std::runtime_error("packed rows mismatch");
        }

        std::vector<uint8_t> dst(dstPitch * height, 0xAA);
        UnpackRows(dst.data(), dstPitch, packed.data(), rowBytes, height);

        if (dst[0] != 1 || dst[1] != 2 || dst[2] != 3 ||
            dst[dstPitch] != 4 || dst[dstPitch + 1] != 5 || dst[dstPitch + 2] != 6) {
            throw std::runtime_error("unpacked rows mismatch");
        }
        if (dst[3] != 0xAA || dst[4] != 0xAA || dst[5] != 0xAA ||
            dst[dstPitch + 3] != 0xAA || dst[dstPitch + 4] != 0xAA || dst[dstPitch + 5] != 0xAA) {
            throw std::runtime_error("unpack modified destination padding");
        }
    });

    TEST_RUN("CopyRows invalid arguments throw", {
        uint8_t data[16] = {};
        ExpectThrow("CopyRows null dst", [&] {
            CopyRows(nullptr, 4, data, 4, 4, 1);
        });
        ExpectThrow("CopyRows null src", [&] {
            CopyRows(data, 4, nullptr, 4, 4, 1);
        });
        ExpectThrow("CopyRows small dst pitch", [&] {
            CopyRows(data, 2, data, 4, 4, 1);
        });
        ExpectThrow("CopyRows small src pitch", [&] {
            CopyRows(data, 4, data, 2, 4, 1);
        });
    });

    return TestUtil::Result("CpuImage");
}
