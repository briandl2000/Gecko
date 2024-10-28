#include "../ShaderDefines.h"

RW_Texture2D_Arr<float4> Texture : register(u0);

struct BloomData
{
	uint Width;
	uint Height;
	float Threshold;
};

DYNAMIC_CALL_DATA(BloomData, bloomData, b0);

#if defined (COMPUTE)
[N_THREADS(8, 8, 1)]
void main(uint3 DTid : S_INPUT_DISPATCH_ID)
{
	Texture[DTid] = float4(
		max(Texture[DTid].xyz - ( (1.f / (1.f - bloomData.Threshold)) -1.f), 0.f), 
		1.f
	);
}
#endif