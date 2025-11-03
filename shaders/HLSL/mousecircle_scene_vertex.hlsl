// Manually fixed scene vertex shader for mousecircle (D3D12/DXIL)

struct SceneUniforms {
    row_major float4x4 mvp;
    float4 cameraPos;
    float4 fogColor;
};

struct VertexInput {
    float3 position : LOC0;
    float surfaceType : LOC1;
    float2 uv : LOC2;
    float3 normal : LOC3;
};

struct VertexOutput {
    float surfaceType : LOC0;
    float2 uv : LOC1;
    float3 normal : LOC2;
    float3 worldPos : LOC3;
    float4 position : SV_Position;
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

