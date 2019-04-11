#pragma once

#include "Fire.h"

class Smoke
{
public:
	Smoke(int count, XMFLOAT3 position, XMFLOAT4 color, float dt, int cond);
	~Smoke();

	int VertexCount()const { return mVertexCount; }

	// Returns the solution at the ith grid point.
	const Particle& Position(int i)const { return mCurrSolution[i]; }


	void Update(float dt);

private:
	int mVertexCount = 0;
	int mState = 0;
	float mTimeStep = 0.0f;

	float randPos();

	std::vector<Particle> mCurrSolution;
	std::vector<std::vector<Particle>> mSolutions;
};

