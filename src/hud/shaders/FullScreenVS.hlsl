cbuffer VSConstants : register(b0) {
    float2 UVScale;
    float2 UVOffset;
}

struct VSOut {
    float4 SVPosition : SV_Position;
    float2 UV : TEXCOORD0;
};

VSOut Main(uint vertId : SV_VertexID) {
    float2 uv01 = float2((vertId << 1) & 2, vertId & 2);
    float2 ndc  = uv01 * float2(2, -2) + float2(-1, 1);
    uv01.x *= 0.5f;

    VSOut o;
    o.SVPosition = float4(ndc, 0, 1);
    o.UV         = uv01 * UVScale + UVOffset;
    return o;
}
