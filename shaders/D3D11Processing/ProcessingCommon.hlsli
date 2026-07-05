#ifndef D3D11_PROCESSING_COMMON_HLSLI
#define D3D11_PROCESSING_COMMON_HLSLI

#define THREAD_GROUP_X 16
#define THREAD_GROUP_Y 16

static const uint DXGI_FORMAT_R16G16B16A16_FLOAT_VALUE = 10;
static const uint DXGI_FORMAT_R8G8B8A8_UNORM_VALUE = 28;
static const uint DXGI_FORMAT_B8G8R8A8_UNORM_VALUE = 87;
static const uint DXGI_FORMAT_NV12_VALUE = 103;
static const uint DXGI_FORMAT_P010_VALUE = 104;
static const uint DXGI_FORMAT_R32G32_FLOAT_VALUE = 16;

static const uint PROCESSING_FILTER_POINT = 0;
static const uint PROCESSING_FILTER_LINEAR = 1;
static const uint PROCESSING_MATRIX_IDENTITY = 0;
static const uint PROCESSING_MATRIX_BT601 = 1;
static const uint PROCESSING_MATRIX_BT709 = 2;
static const uint PROCESSING_MATRIX_BT2020 = 3;
static const uint PROCESSING_RANGE_FULL = 0;
static const uint PROCESSING_RANGE_LIMITED = 1;
static const uint REMAP_COORDINATE_ABSOLUTE_PIXELS = 0;
static const uint REMAP_COORDINATE_NORMALIZED_ZERO_TO_ONE = 1;
static const uint REMAP_BORDER_CLAMP = 0;
static const uint REMAP_BORDER_CONSTANT = 1;
static const uint COMPOSITE_COPY = 0;
static const uint COMPOSITE_ALPHA_BLEND = 1;
static const uint COMPOSITE_PREMULTIPLIED_ALPHA = 2;
static const uint COMPOSITE_ADD = 3;

cbuffer ProcessingConstants : register(b0)
{
    uint SrcWidth;
    uint SrcHeight;
    uint DstWidth;
    uint DstHeight;
    int SrcX;
    int SrcY;
    int DstX;
    int DstY;
    uint SrcFormat;
    uint DstFormat;
    uint SrcMatrix;
    uint SrcRange;
    uint DstMatrix;
    uint DstRange;
    uint Filter;
    uint AlphaMode;
    float ScaleX;
    float ScaleY;
    uint MapFormat;
    uint CoordinateMode;
    uint BorderMode;
    uint BlendMode;
    float Opacity;
    uint Reserved0;
    float4 BorderColor;
    uint BaseWidth;
    uint BaseHeight;
    uint OverlayWidth;
    uint OverlayHeight;
    int BaseX;
    int BaseY;
    int OverlayX;
    int OverlayY;
};

bool InRect(uint2 p, uint w, uint h) { return p.x < w && p.y < h; }
uint2 OffsetPosition(int x, int y, uint2 p) { return uint2((uint)x, (uint)y) + p; }
int3 LoadCoord(uint2 p) { return int3((int)p.x, (int)p.y, 0); }
float4 ToLogicalRgba(float4 v, uint format) { return v; }
float4 FromLogicalRgba(float4 v, uint format) { return v; }

float4 LoadRgbaLike(Texture2D<float4> tex, uint2 p, uint format) {
    return ToLogicalRgba(tex.Load(LoadCoord(p)), format);
}

#endif
