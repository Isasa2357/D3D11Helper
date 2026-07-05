#include "TestCommon.hpp"

using namespace D3D11CoreLib;

TEST(FormatUtil, YuvAndPlanarTraits) {
    CHECK(FormatUtil::IsYuvFormat(DXGI_FORMAT_NV12));
    CHECK(FormatUtil::IsPlanarFormat(DXGI_FORMAT_NV12));
    CHECK(FormatUtil::RequiresEvenSize(DXGI_FORMAT_NV12));
    CHECK_EQ(FormatUtil::GetKnownPlaneCount(DXGI_FORMAT_NV12), 2u);
    CHECK_EQ(FormatUtil::BitsPerPixel(DXGI_FORMAT_NV12), 12u);

    CHECK(FormatUtil::IsYuvFormat(DXGI_FORMAT_P010));
    CHECK(FormatUtil::IsPlanarFormat(DXGI_FORMAT_P010));
    CHECK(FormatUtil::RequiresEvenSize(DXGI_FORMAT_P010));
    CHECK_EQ(FormatUtil::GetKnownPlaneCount(DXGI_FORMAT_P010), 2u);
    CHECK_EQ(FormatUtil::BitsPerPixel(DXGI_FORMAT_P010), 24u);

    CHECK(FormatUtil::IsYuvFormat(DXGI_FORMAT_YUY2));
    CHECK(!FormatUtil::IsPlanarFormat(DXGI_FORMAT_YUY2));
    CHECK(!FormatUtil::RequiresEvenSize(DXGI_FORMAT_YUY2));
    CHECK_EQ(FormatUtil::GetKnownPlaneCount(DXGI_FORMAT_YUY2), 1u);
}

TEST(FormatUtil, NonYuvDefaults) {
    CHECK(!FormatUtil::IsYuvFormat(DXGI_FORMAT_R8G8B8A8_UNORM));
    CHECK(!FormatUtil::IsPlanarFormat(DXGI_FORMAT_R8G8B8A8_UNORM));
    CHECK(!FormatUtil::RequiresEvenSize(DXGI_FORMAT_R8G8B8A8_UNORM));
    CHECK_EQ(FormatUtil::GetKnownPlaneCount(DXGI_FORMAT_R8G8B8A8_UNORM), 1u);
    CHECK_EQ(FormatUtil::GetKnownPlaneCount(DXGI_FORMAT_UNKNOWN), 0u);
}
