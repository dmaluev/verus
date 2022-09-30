// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Anim
	{
		// Shaker can load standard wav file, 8-bit mono, and use it as a function of time.
		// Works well for camera's shaking effects.
		class Shaker
		{
			String        _url;
			Vector<float> _vData;
			float         _speed = 0;
			float         _offset = 0;
			float         _value = 0;
			float         _scale = 1;
			float         _bias = 0;
			bool          _looping = false;

		public:
			Shaker(float speed = 400, bool looping = true) : _speed(speed), _looping(looping) {}

			bool IsLoaded() const { return !_vData.empty(); }
			void Load(CSZ url);
			Str GetURL() const { return _C(_url); }

			void Update();

			void Reset() { _offset = 0; }
			void Randomize();
			// Returns the current value in the range [-1 to 1].
			float Get();

			float GetSpeed() const { return _speed; }
			void SetSpeed(float speed) { _speed = speed; }

			float GetScale() const { return _scale; }
			void SetScale(float scale) { _scale = scale; }

			float GetBias() const { return _bias; }
			void SetBias(float bias) { _bias = bias; }

			bool IsLooping() const { return _looping; }
			void SetLooping(float looping) { _looping = looping; }
		};
		VERUS_TYPEDEFS(Shaker);
	}
}
