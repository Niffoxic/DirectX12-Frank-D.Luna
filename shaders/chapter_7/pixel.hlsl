cbuffer Primiary : register(b0)
{
    float  u_time;
    float3 padding;
    float2  u_resolution;
    float2   u_mouse;
    float4x4 u_WorldViewProjectMatrix;
};

struct PixelInput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

float4 main(PixelInput input) : SV_Target
{
    float2 uv = (input.Position / u_resolution);
    return input.Color * max(0.2, sin(u_time));
}
