// Overlay fragment shader for mousecircle (D3D12/DXIL)
// Fixed for SDL3 D3D12 backend compatibility

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
};

float4 main(VertexOutput input) : SV_Target0
{
    return input.color;
}
