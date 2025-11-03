// Overlay vertex shader for mousecircle (D3D12/DXIL)
// Fixed for SDL3 D3D12 backend compatibility

struct VertexInput {
    float2 position : TEXCOORD0;
    float4 color : TEXCOORD1;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.color = input.color;
    return output;
}
