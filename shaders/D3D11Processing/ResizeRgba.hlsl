#include "ProcessingCommon.hlsli"
#include "YuvPrimitives.hlsli"

Texture2D<float4> Src : register(t0);
RWTexture2D<float4> Dst : register(u0);

[numthreads(THREAD_GROUP_X, THREAD_GROUP_Y, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    const uint2 p = tid.xy;
    if (!InRect(p, DstWidth, DstHeight)) {
        return;
    }

    const uint2 sourceOrigin = uint2((uint)SrcX, (uint)SrcY);
    const uint2 sourceSize = uint2(SrcWidth, SrcHeight);
    const float2 scale = float2(ScaleX, ScaleY);

    const float4 color = (Filter == PROCESSING_FILTER_POINT)
        ? D3D11SampleLogicalRgbaPoint(
            Src,
            p,
            sourceOrigin,
            sourceSize,
            scale,
            SrcFormat)
        : D3D11SampleLogicalRgbaLinear(
            Src,
            p,
            sourceOrigin,
            sourceSize,
            scale,
            SrcFormat);

    Dst[OffsetPosition(DstX, DstY, p)] =
        FromLogicalRgba(saturate(color), DstFormat);
}
