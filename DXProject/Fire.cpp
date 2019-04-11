#include "stdafx.h"
#include "Fire.h"



Fire::Fire()
{

}
Fire::Fire(int count, XMFLOAT3 position, XMFLOAT4 color, float dt, int cond)
{

	mVertexCount = count;
	mState = cond;
	mTimeStep = dt;
	mCurrSolution.resize(count);
	mSolutions.resize(cond);
	for (size_t i = 0; i < cond; i++)
	{
		mSolutions[i].resize(count);
		for (size_t j = 0; j < count; j++)
		{
			mSolutions[i][j].color = color;
			mSolutions[i][j].position = XMFLOAT3(position.x + randPos(), position.y + 2 * cos( PI/2 * randPos()), position.z + randPos());
		}
	}
}
Fire::~Fire()
{

}

void Fire::Update(float dt)
{
	static float t_base = 0.0f;
	t_base += dt;

	UINT solutionIndex = (int)(t_base / mTimeStep) % mState;
	mCurrSolution = mSolutions[solutionIndex];
}

float Fire::randPos()
{
	float ret = static_cast<float>((rand() % 10000) - 5000);
	return ret / 5000.0f;
}
