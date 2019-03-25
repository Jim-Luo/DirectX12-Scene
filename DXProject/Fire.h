#pragma once

#include "DXUtil.h"

struct Particle
{
	XMFLOAT3 position;
	XMFLOAT4 color;

};

class Fire
{
public:
	Fire();
	/*
	count: particle count
	color: particle color
	dt: updating time delta
	cond: solution count
	 */
	Fire(int count,XMFLOAT3 position, XMFLOAT4 color, float dt, int cond);
	~Fire();

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
	//std::vector<DirectX::XMFLOAT3> mCurrSolution;

};

