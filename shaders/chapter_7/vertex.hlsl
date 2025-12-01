cbuffer Primiary : register(b0)
{
    float    u_time;
    float3   padding;
    float2   u_resolution;
    float2   u_mouse;
    float4x4 u_World;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};

struct VertexInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct VertexOutput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
	
    float3 pos = input.Position;
    float4 posW = mul(float4(pos, 1.0f), u_World);
    output.Position = mul(posW, gViewProj);
    output.Color = input.Color;
    
    return output;
}
