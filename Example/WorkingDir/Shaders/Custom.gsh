#include "ShaderDefines.h"

RW_Texture2D_Arr<float4> Output : register(u0);

#if defined (COMPUTE)
[N_THREADS(8, 8, 1)]
void main(uint3 DTid : S_INPUT_DISPATCH_ID)
{
	Output[DTid] = float4(1., 0., 1., 1.);
}
#endif
