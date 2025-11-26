cbuffer Primiary : register(b0)
{
    float    u_time;
    float3   padding;
    float2   u_resolution;
    float2   u_mouse;
    float4x4 u_WorldViewProjectMatrix;
};

struct VertexInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct VertexOutput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;

    float3 p = input.Position;

    float2 mouseNorm = u_mouse / u_resolution;

    float scaleX = lerp(0.5f, 1.5f, mouseNorm.x);
    float scaleY = lerp(0.5f, 1.5f, mouseNorm.y);

    float3 pDeformed = p;
    pDeformed.x *= scaleX;
    pDeformed.y *= scaleY;

    output.Position = mul(float4(pDeformed, 1.0f), u_WorldViewProjectMatrix);
    output.Color = input.Color;

    return output;
}
