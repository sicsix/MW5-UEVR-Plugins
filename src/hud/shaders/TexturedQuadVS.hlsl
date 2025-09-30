cbuffer VSConstants : register(b0) {
    row_major float4x4 MVP;
}

struct VSIn {
    float2 Position : POSITION;
    float2 UV : TEXCOORD0;
};

struct VSOut {
    float4 SVPosition : SV_Position;
    float2 UV : TEXCOORD0;
};

VSOut Main(VSIn i) {
    VSOut o;
    o.SVPosition = mul(float4(i.Position, 0, 1), MVP);
    o.UV         = i.UV;
    return o;
}
