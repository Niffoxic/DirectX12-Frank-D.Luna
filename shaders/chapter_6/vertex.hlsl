cbuffer Primiary : register(b0)
{
    float u_time;
    float3 padding;
    float2 u_resolution;
    float2 u_mouse;
    float4x4 u_WorldViewProjectMatrix;

    float3 u_EyePosW;
    float padEye;

    float3 u_DirLightDirection;
    float padDir0;
    float3 u_DirLightColor;
    float padDir1;

    float3 u_PointLightPosition;
    float u_PointLightRange;
    float3 u_PointLightColor;
    float padPoint;
};

struct VertexInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float2 TexCoords : TEXCOORD;
};

struct VertexOutput
{
    float4 Position : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 Tangent : TANGENT;
    float2 TexCoords : TEXCOORD;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;

    float4 posW = float4(input.Position, 1.0f);
    posW.y = sin(posW.y * u_time);

    output.Position = mul(posW, u_WorldViewProjectMatrix);
    output.PosW = input.Position;
    output.NormalW = normalize(input.Normal);
    output.TexCoords = input.TexCoords;
    output.Tangent = input.Tangent;

    return output;
}
