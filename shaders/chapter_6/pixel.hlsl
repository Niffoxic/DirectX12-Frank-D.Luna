cbuffer Primiary : register(b0)
{
    float  u_time;
    float3 padding;
    float2 u_resolution;
    float2 u_mouse;
    float4x4 u_WorldViewProjectMatrix;
};

struct PixelInput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

float4 main(PixelInput input) : SV_Target
{
    float2 pixelPos = input.Position.xy;

    float2 L = normalize(u_mouse - pixelPos);

    float3 baseColor = input.Color.rgb;

    float2 N2 = normalize(baseColor.rg * 2.0f - 1.0f);

    float NdotL = saturate(dot(N2, L));

    float dist = length(pixelPos - u_mouse);
    float radius = 40.0f;
    float falloff = saturate(1.0f - dist / radius);

    float lighting = NdotL * falloff;

    // Final color = baseColor * ambient + highlight * light
    float3 ambient = baseColor * 0.4f;
    float3 diffuse = baseColor * lighting * 1.2f;

    float3 finalColor = ambient + diffuse;

    return float4(finalColor, 1.0f);
}
