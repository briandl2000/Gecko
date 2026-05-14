struct PSInput
{
    float4 Position : SV_Position;
    float2 UV    : COLOR;
};

float4 main(PSInput input) : SV_Target
{
    return float4(input.UV, 0.0, 1.0);
}
