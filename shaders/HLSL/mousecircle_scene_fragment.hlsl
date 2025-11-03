// Scene fragment shader for mousecircle (D3D12/DXIL)

cbuffer SceneUniforms : register(b0, space3)
{
    row_major float4x4 mvp;
    float4 cameraPos;
    float4 fogColor;
};

struct VertexOutput {
    float4 position : SV_Position;
    float surfaceType : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float3 worldPos : TEXCOORD3;
};

float checker(float2 uv) {
    float2 scaled = floor(uv * 4.0);
    float v = fmod(scaled.x + scaled.y, 2.0);
    return v < 0.5 ? 1.0 : 0.7;
}

float4 main(VertexOutput input) : SV_Target0
{
    float3 baseColor;
    if (input.surfaceType < 0.5) {
        baseColor = float3(0.1, 0.1, 0.9);
    } else if (input.surfaceType < 1.5) {
        baseColor = float3(0.9, 0.2, 0.2);
    } else if (input.surfaceType < 2.5) {
        baseColor = float3(0.9, 0.9, 0.2);
    } else {
        baseColor = float3(0.7, 0.5, 0.3);
    }

    float3 n = normalize(input.normal);
    float3 lightDir = normalize(float3(0.35, 1.0, 0.45));
    float diff = max(dot(n, lightDir), 0.15);
    float fogFactor = exp(-distance(input.worldPos, cameraPos.xyz) * 0.08);
    float3 color = baseColor * checker(input.uv) * diff;
    color = lerp(fogColor.xyz, color, fogFactor);
    return float4(color, 1.0);
}
