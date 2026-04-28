struct PSInput
{
    float4 Position : SV_Position;
    float3 Color    : COLOR;
};

float4 main(PSInput input) : SV_Target
{
    return float4(input.Color, 1.0);
}
