#include "ProcessingCommon.hlsli"
Texture2D<float4> Src : register(t0);
Texture2D<float2> Map : register(t1);
RWTexture2D<float4> Dst : register(u0);
bool IsInside(float2 p) { return p.x >= 0.0 && p.y >= 0.0 && p.x <= float(SrcWidth - 1) && p.y <= float(SrcHeight - 1); }
float4 LoadOrBorder(int2 p)
{
    if (BorderMode == REMAP_BORDER_CONSTANT) {
        if (p.x < 0 || p.y < 0 || p.x >= int(SrcWidth) || p.y >= int(SrcHeight)) return BorderColor;
    }
    p = clamp(p, int2(0,0), int2(int(SrcWidth)-1, int(SrcHeight)-1));
    return ToLogicalRgba(Src.Load(int3(p,0)), SrcFormat);
}
float4 SampleSource(float2 coord)
{
    if (CoordinateMode == REMAP_COORDINATE_NORMALIZED_ZERO_TO_ONE) coord *= float2(max(SrcWidth - 1, 1), max(SrcHeight - 1, 1));
    if (BorderMode == REMAP_BORDER_CONSTANT && !IsInside(coord)) return BorderColor;
    if (Filter == PROCESSING_FILTER_POINT) return LoadOrBorder(int2(round(coord)));
    float2 baseF = floor(coord);
    float2 f = coord - baseF;
    int2 p0 = int2(baseF);
    int2 p1 = p0 + int2(1, 1);
    float4 c00 = LoadOrBorder(p0);
    float4 c10 = LoadOrBorder(int2(p1.x, p0.y));
    float4 c01 = LoadOrBorder(int2(p0.x, p1.y));
    float4 c11 = LoadOrBorder(p1);
    return lerp(lerp(c00, c10, f.x), lerp(c01, c11, f.x), f.y);
}
[numthreads(THREAD_GROUP_X, THREAD_GROUP_Y, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint2 p = tid.xy;
    if (!InRect(p, DstWidth, DstHeight)) return;
    float2 coord = Map.Load(LoadCoord(p));
    Dst[OffsetPosition(DstX, DstY, p)] = FromLogicalRgba(saturate(SampleSource(coord)), DstFormat);
}
