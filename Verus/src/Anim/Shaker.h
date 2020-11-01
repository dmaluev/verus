// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Anim
	{
		//! Shaker can load standard wav file, 8-bit mono, and use it as a function of time.

		//! Works well for camera's shaking effects.
		//!
		class Shaker
		{
			Vector<float> _vData;
			float         _speed = 0;
			float         _offset = 0;
			float         _value = 0;
			float         _scale = 1;
			float         _bias = 0;
			bool          _loop = false;

		public:
			Shaker(float speed = 400, bool loop = true) : _speed(speed), _loop(loop) {}
			bool IsLoaded() const { return !_vData.empty(); }
			void Load(CSZ url);
			void Update();
			void Reset() { _offset = 0; }
			void Randomize();
			//! Returns the current value in the range [-1 to 1].
			float Get();
			void SetScaleBias(float s, float b);
		};
		VERUS_TYPEDEFS(Shaker);
	}
}
