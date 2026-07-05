#include "ProcessingCommon.hlsli"
Texture2D<float4> Base : register(t0);
Texture2D<float4> Overlay : register(t1);
RWTexture2D<float4> Dst : register(u0);
float4 Blend(float4 b, float4 o)
{
    o.a *= Opacity;
    o.rgb *= Opacity;
    if (BlendMode == COMPOSITE_COPY) return o;
    if (BlendMode == COMPOSITE_ADD) return saturate(float4(b.rgb + o.rgb, max(b.a, o.a)));
    if (BlendMode == COMPOSITE_PREMULTIPLIED_ALPHA) {
        float3 rgb = o.rgb + b.rgb * (1.0 - o.a);
        float a = o.a + b.a * (1.0 - o.a);
        return saturate(float4(rgb, a));
    }
    float a = o.a;
    float3 rgb = lerp(b.rgb, o.rgb, a);
    float outA = o.a + b.a * (1.0 - o.a);
    return saturate(float4(rgb, outA));
}
[numthreads(THREAD_GROUP_X, THREAD_GROUP_Y, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint2 p = tid.xy;
    if (!InRect(p, DstWidth, DstHeight)) return;
    uint2 bp = OffsetPosition(BaseX, BaseY, p);
    uint2 op = OffsetPosition(OverlayX, OverlayY, p);
    uint2 dp = OffsetPosition(DstX, DstY, p);
    float4 b = ToLogicalRgba(Base.Load(LoadCoord(bp)), SrcFormat);
    float4 o = ToLogicalRgba(Overlay.Load(LoadCoord(op)), MapFormat);
    Dst[dp] = FromLogicalRgba(Blend(b, o), DstFormat);
}
