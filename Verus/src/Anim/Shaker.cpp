// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Anim;

void Shaker::Load(CSZ url)
{
	Vector<BYTE> v;
	IO::FileSystem::LoadResource(url, v);

	// Find data:
	int chunkOffset = 44;
	int chunkSize = 0;
	const int mx = Math::Min<int>(128, Utils::Cast32(v.size())) - 4;
	VERUS_FOR(i, mx)
	{
		if (!memcmp(&v[i], "data", 4))
		{
			chunkOffset = i + 8;
			memcpy(&chunkSize, &v[i + 4], 4);
			break;
		}
	}

	_vData.resize(chunkSize);
	VERUS_FOR(i, chunkSize)
	{
		const float val = v[chunkOffset + i] * (1 / 128.f) - 1;
		_vData[i] = val;
	}
	if (!_loop)
		_offset = static_cast<float>(chunkSize);
}

void Shaker::Update()
{
	VERUS_QREF_TIMER;
	_offset += dt * _speed;
	if (_offset >= _vData.size())
	{
		if (_loop)
		{
			_offset = fmod(_offset, static_cast<float>(_vData.size()));
		}
		else
		{
			_offset = static_cast<float>(_vData.size());
			_value = _bias;
			return;
		}
	}
	const float part = fmod(_offset, 1.f);
	size_t a = static_cast<size_t>(_offset);
	size_t b = a + 1;
	if (b >= _vData.size())
		b = a;
	_value = Math::Lerp(_vData[a], _vData[b], part) * _scale + _bias;
}

void Shaker::Randomize()
{
	VERUS_QREF_UTILS;
	_offset = static_cast<float>(utils.GetRandom().Next() % _vData.size());
}

float Shaker::Get()
{
	return _value;
}

void Shaker::SetScaleBias(float s, float b)
{
	_scale = s;
	_bias = b;
}
