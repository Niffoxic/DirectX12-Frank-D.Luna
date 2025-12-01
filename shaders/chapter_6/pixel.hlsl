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

struct PixelInput
{
    float4 Position : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 Tangent : TANGENT;
    float2 TexCoords : TEXCOORD;
};

float ShouldRenderFrameSide(float position)
{
    float frameLength = 3.0;
    float left = 1.0 - step(frameLength, position);
    float right = step(u_resolution.x - frameLength, position);
    return step(2.0, left + right);
}

float ShouldRenderFrameTops(float position)
{
    float frameLength = 3.0;
    float bottom = 1.0 - step(frameLength, position);
    float top = step(u_resolution.y - frameLength, position);
    return step(2.0, bottom + top);
}

float3 ComputeDirectionalDiffuse(float3 N)
{
    float3 L = normalize(-u_DirLightDirection);
    float NdotL = max(dot(N, L), 0.0f);
    return u_DirLightColor * NdotL;
}

float3 ComputePointDiffuse(float3 N, float3 posW)
{
    float3 toLight = u_PointLightPosition - posW;
    float dist = length(toLight);

    if (dist > u_PointLightRange)
        return 0.0.xxx;

    float3 L = toLight / max(dist, 1e-3f);
    float NdotL = max(dot(N, L), 0.0f);

    float atten = 1.0f / (1.0f + dist * dist * 0.05f);

    return u_PointLightColor * NdotL * atten;
}

float4 main(PixelInput input) : SV_Target
{
    float2 uv = (input.PosW.xy - 0.5 * u_resolution) / u_resolution.y;
    float r = length(uv);
    float freq = 5.0 * sin(u_time);
    float angle = r * freq * 2.0 * 3.14;
    float color_t = (sin(angle) + 1.0) * 0.5;

    float3 baseColor = float3(input.NormalW * 0.9f);

    float3 N = normalize(input.NormalW);

    float3 dirDiffuse = ComputeDirectionalDiffuse(N);
    float3 pointDiffuse = ComputePointDiffuse(N, input.PosW);

    float3 lightingFactor = dirDiffuse + pointDiffuse;

    float3 ambient = baseColor * 0.4f;
    float3 diffuse = baseColor * lightingFactor * 1.2f;

    float3 finalColor = ambient + diffuse;

    float t = step(
        2.0,
        ShouldRenderFrameTops(input.Position.y) +
        ShouldRenderFrameSide(input.Position.x)
    );

    float3 frameColor = float3(sin(u_time), cos(u_time), 0.75);

    finalColor = max(frameColor * t, finalColor * (1.0 - t));
    finalColor = saturate(finalColor);

    return float4(finalColor, 1.0f);
}
