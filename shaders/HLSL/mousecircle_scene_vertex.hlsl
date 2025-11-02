// Scene vertex shader for mousecircle (D3D12/DXIL)

struct SceneUniforms {
    row_major float4x4 mvp;
    float4 cameraPos;
    float4 fogColor;
};

struct VertexInput {
    float3 position : TEXCOORD0;
    float surfaceType : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 normal : TEXCOORD3;
};

struct VertexOutput {
    float4 position : SV_Position;
    float surfaceType : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float3 worldPos : TEXCOORD3;
};

ConstantBuffer<SceneUniforms> uniforms : register(b0);

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    float4 world = float4(input.position, 1.0);
    output.position = mul(uniforms.mvp, world);
    output.surfaceType = input.surfaceType;
    output.uv = input.uv;
    output.normal = input.normal;
    output.worldPos = input.position;
    return output;
}
