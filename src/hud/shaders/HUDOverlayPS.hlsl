Texture2D    Tex0 : register(t0);
SamplerState Samp0 : register(s0);

cbuffer PSConstants : register(b0) {
    float Brightness;
    float Padding[3];
}

struct PSIn {
    float4 SVPosition : SV_Position;
    float2 UV : TEXCOORD0;
};

float4 Main(PSIn i) : SV_Target {
    float4 o = Tex0.Sample(Samp0, i.UV);
    o.rgb *= Brightness * Brightness * 10.0f;
    o.a *= 0.667f;
    return o;
}
