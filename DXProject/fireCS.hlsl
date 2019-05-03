cbuffer cbUpdateSettings
{
	float gConstant0;
	float gConstant1;
	float gConstant2;

};

RWTexture2D<float> gSolutionInput : register(u0);
RWTexture2D<float> gOutput       : register(u1);

[numthreads(16, 16, 1)]
void CS(int3 dispatchThreadID : SV_DispatchThreadID)
{
	// We do not need to do bounds checking because:
	//	 *out-of-bounds reads return 0, which works for us--it just means the boundary of 
	//    our water simulation is clamped to 0 in local space.
	//   *out-of-bounds writes are a no-op.

	int x = dispatchThreadID.x;
	int y = dispatchThreadID.y;

	gOutput[int2(x, y)] =
		gConstant0 * gSolutionInput[int2(x, y)].r +
		gConstant1 * gSolutionInput[int2(x, y)].r +
		gConstant2 * (
			gSolutionInput[int2(x, y + 1)].r +
			gSolutionInput[int2(x, y - 1)].r +
			gSolutionInput[int2(x + 1, y)].r +
			gSolutionInput[int2(x - 1, y)].r);
}
