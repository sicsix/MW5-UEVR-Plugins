Texture2D    Tex0 : register(t0);
SamplerState Samp0 : register(s0);

struct PSIn {
    float4 SVPosition : SV_Position;
    float2 UV : TEXCOORD0;
};

float4 Main(PSIn i) : SV_Target {
    return Tex0.Sample(Samp0, i.UV);
}
