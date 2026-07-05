#include "ProcessingCommon.hlsli"
#include "ColorSpace.hlsli"
Texture2D<float> YPlane : register(t0);
Texture2D<float2> UVPlane : register(t1);
RWTexture2D<float4> Dst : register(u0);
float3 LoadRgbAt(uint2 logicalSrc)
{
    uint2 srcPos = OffsetPosition(SrcX, SrcY, logicalSrc);
    float y = YPlane.Load(LoadCoord(srcPos));
    float2 uv = UVPlane.Load(LoadCoord(srcPos / 2));
    return DecodeYuv(y, uv, SrcRange, SrcMatrix);
}
float3 SamplePointYuv(uint2 p)
{
    float2 srcF = (float2(p) + 0.5) * float2(ScaleX, ScaleY) - 0.5;
    int2 ip = int2(round(srcF));
    ip = clamp(ip, int2(0,0), int2(int(SrcWidth)-1, int(SrcHeight)-1));
    return LoadRgbAt(uint2((uint)ip.x, (uint)ip.y));
}
float3 SampleLinearYuv(uint2 p)
{
    float2 srcF = (float2(p) + 0.5) * float2(ScaleX, ScaleY) - 0.5;
    float2 baseF = floor(srcF);
    float2 f = srcF - baseF;
    int2 p0 = int2(baseF);
    int2 p1 = p0 + int2(1, 1);
    p0 = clamp(p0, int2(0,0), int2(int(SrcWidth)-1, int(SrcHeight)-1));
    p1 = clamp(p1, int2(0,0), int2(int(SrcWidth)-1, int(SrcHeight)-1));
    float3 c00 = LoadRgbAt(uint2((uint)p0.x, (uint)p0.y));
    float3 c10 = LoadRgbAt(uint2((uint)p1.x, (uint)p0.y));
    float3 c01 = LoadRgbAt(uint2((uint)p0.x, (uint)p1.y));
    float3 c11 = LoadRgbAt(uint2((uint)p1.x, (uint)p1.y));
    return lerp(lerp(c00, c10, f.x), lerp(c01, c11, f.x), f.y);
}
[numthreads(THREAD_GROUP_X, THREAD_GROUP_Y, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint2 p = tid.xy;
    if (!InRect(p, DstWidth, DstHeight)) return;
    float3 rgb = (Filter == PROCESSING_FILTER_POINT) ? SamplePointYuv(p) : SampleLinearYuv(p);
    Dst[OffsetPosition(DstX, DstY, p)] = FromLogicalRgba(float4(rgb, 1.0), DstFormat);
}
