#include "ShaderDefines.h"

RW_Texture2D_Arr<float4> Bloom : register(u0);
RW_Texture2D_Arr<float4> InputTexture : register(u1);
RW_Texture2D_Arr<float4> OutputTexture : register(u2);

#if defined (COMPUTE)
[N_THREADS(8, 8, 1)]
void main(uint3 DTid : S_INPUT_DISPATCH_ID)
{
	OutputTexture[DTid] = InputTexture[DTid] + Bloom[DTid];
}
#endif