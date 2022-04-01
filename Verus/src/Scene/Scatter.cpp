// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

Scatter::Scatter()
{
}

Scatter::~Scatter()
{
	Done();
}

void Scatter::Init(int side, int typeCount, PcTypeDesc pTypes, int seed)
{
	VERUS_INIT();
	VERUS_RT_ASSERT(side >= 16 && Math::IsPowerOfTwo(side));

	_side = side;
	_shift = Math::HighestBit(_side);

	Random random(seed);

	// Fill 2D array of instances:
	_vInstances.resize(_side * _side);
	const int instanceCount = Utils::Cast32(_vInstances.size());
	int type = 0;
	int maxPermille = (typeCount >= 1) ? pTypes[0]._permille : 1000;
	VERUS_FOR(i, instanceCount)
	{
		RInstance instance = _vInstances[i];
		const int permille = i * 1000 / instanceCount;
		if (permille < maxPermille)
		{
			instance._type = pTypes[type]._type;
		}
		else if (type + 1 < typeCount)
		{
			type++;
			maxPermille += pTypes[type]._permille;

			instance._type = pTypes[type]._type;
		}
		else
		{
			instance._type = -1;
			type = typeCount;
		}

		if (type < typeCount)
		{
			instance._x = pTypes[type]._minOffset;
			instance._z = pTypes[type]._maxOffset;
			instance._scale = random.NextFloat(pTypes[type]._minScale, pTypes[type]._maxScale);
			instance._angle = random.NextFloat(pTypes[type]._minAngle, pTypes[type]._maxAngle);
			instance._rand = random.Next();
		}
	}

	std::shuffle(_vInstances.begin(), _vInstances.end(), random.GetGenerator());

	VERUS_FOR(i, _side)
	{
		const int rowOffset = i * _side;
		VERUS_FOR(j, _side)
		{
			const int index = rowOffset + j;
			RInstance instance = _vInstances[index];
			const float mn = instance._x;
			const float mx = instance._z;
			instance._x = (j & 0xF) + random.NextFloat(mn, mx);
			instance._z = (i & 0xF) + random.NextFloat(mn, mx);
		}
	}
}

void Scatter::Done()
{
	VERUS_DONE(Scatter);
}

void Scatter::QuadtreeIntegral_ProcessVisibleNode(const short ij[2], RcPoint3 center)
{
	VERUS_QREF_SM;

	RcPoint3 eyePos = sm.GetMainCamera()->GetEyePosition();
	const float distSq = VMath::distSqr(eyePos, center);
	if (distSq >= _maxDistSq)
		return;

	const int mask = _side - 1;
	VERUS_FOR(i, 16)
	{
		VERUS_FOR(j, 16)
		{
			const int ijGlobal[2] =
			{
				i + ij[0],
				j + ij[1]
			};

			// Coordinates inside 2D instance array:
			const int iLookup = ijGlobal[0] & mask;
			const int jLookup = ijGlobal[1] & mask;

			RcInstance instance = _vInstances[(iLookup << _shift) + jLookup];

			if (instance._type >= 0) // Hit some non-empty instance?
			{
				_pDelegate->Scatter_AddInstance(
					ijGlobal,
					instance._type,
					instance._x + center.getX() - 8,
					instance._z + center.getZ() - 8,
					instance._scale,
					instance._angle,
					instance._rand);
			}
		}
	}
}

void Scatter::QuadtreeIntegral_GetHeights(const short ij[2], float height[2])
{
	VERUS_RT_FAIL(__FUNCTION__);
}

Scatter::RcInstance Scatter::GetInstanceAt(const int ij[2]) const
{
	const int mask = _side - 1;
	const int i = ij[0] & mask;
	const int j = ij[1] & mask;
	return _vInstances[(i << _shift) + j];
}
