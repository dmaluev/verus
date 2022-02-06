// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace GUI
	{
		template<typename T>
		struct Animated
		{
			T      _from = 0;
			T      _to = 0;
			T      _current = 0;
			float  _time = 0;
			float  _invDuration = 0;
			float  _delay = 0;
			Easing _easing = Easing::none;

			T Lerp(const T& a, const T& b, float ratio)
			{
				return Math::Lerp(a, b, ratio);
			}

			void Update(float dt)
			{
				if (_invDuration > 0)
				{
					if (_time >= 0)
						_time += dt;
					const float ratio = Math::Clamp<float>((_time - _delay) * _invDuration, 0, 1);
					const float ratioWithEasing = Math::ApplyEasing(_easing, ratio);
					_current = Lerp(_from, _to, ratioWithEasing);
					if (dt > 0 && ratio >= 1)
						_invDuration = -_invDuration;
				}
			}

			void Reset(float reverseTime)
			{
				_current = _from;
				_time = reverseTime;
				_invDuration = abs(_invDuration);
				Update(0);
			}
		};

		class Widget;
		class Animator
		{
			struct Video
			{
				int	  _columnCount = 1;
				int	  _rowCount = 1;
				int	  _frameCount = 1;
				float _time = 0;
				float _invDuration = 0;
				float _delay = 0;
				float _uScale = 1;
				float _vScale = 1;
				float _uBias = 0;
				float _vBias = 0;

				void Update(float dt);
				void Reset(float reverseTime);
			} _video;

			Animated<Vector4> _animatedColor;
			Animated<Vector4> _animatedRect;
			float             _postAngle = 0;
			float             _angle = 0;
			float             _angleSpeed = 0;
			float             _pulseScale = 1;
			float             _pulseScaleAdd = 0;
			float             _pulseSpeed = 0;
			float             _timeout = 0;
			float             _maxTimeout = -1;
			bool              _reverse = false;

		public:
			Animator();
			~Animator();

			bool Update();
			void Parse(pugi::xml_node node, const Widget* pWidget);

			void Reset(float reverseTime = 0);

			RcVector4 GetColor(RcVector4 original) const;
			RcVector4 SetColor(RcVector4 color);
			RcVector4 SetFromColor(RcVector4 color);

			float GetX(float original) const;
			float GetY(float original) const;
			float GetW(float original) const;
			float GetH(float original) const;

			float SetX(float x);
			float SetY(float y);
			float SetW(float w);
			float SetH(float h);

			bool GetVideoBias(float& ub, float& vb) const;

			float GetAngle() const { return _angle; }
			void  SetAngle(float a) { _angle = a; }
			float GetPostAngle() const { return _postAngle; }
			void  SetPostAngle(float a) { _postAngle = a; }

			float GetPulseScale() const { return _pulseScale; }
			float GetPulseScaleAdd() const { return _pulseScaleAdd; }

			void SetTimeout(float t);
		};
		VERUS_TYPEDEFS(Animator);
	}
}
