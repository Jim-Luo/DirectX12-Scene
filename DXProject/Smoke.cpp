#include "stdafx.h"
#include "Smoke.h"




Smoke::Smoke(int count, XMFLOAT3 position, XMFLOAT4 color, float dt, int cond)
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
			mSolutions[i][j].position = XMFLOAT3(position.x + randPos(), position.y + randPos(), position.z + randPos());
		}
	}
}

Smoke::~Smoke()
{
}

void Smoke::Update(float dt)
{
	static float t_base = 0.0f;
	t_base += dt;

	UINT solutionIndex = (int)(t_base / mTimeStep) % mState;
	mCurrSolution = mSolutions[solutionIndex];
}

float Smoke::randPos()
{
	float ret = static_cast<float>((rand() % 10000) - 5000);
	return ret / 5000.0f;
}

