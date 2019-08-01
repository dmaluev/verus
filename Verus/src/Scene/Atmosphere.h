#pragma once

namespace verus
{
	namespace Scene
	{
		class Atmosphere : public Singleton<Atmosphere>, public Object
		{
			struct Fog
			{
				Vector3 _color = Vector3(0);
				Vector3 _colorInit = Vector3(0);
				float   _start = 10000;
				float   _end = 100000;
				float   _startEx = 0;
				float   _endEx = 0;
				float   _density = 1 / 1500.f;
			};

			struct Sun
			{
				Matrix3 _matTilt;
				Vector3 _dirToMidnight = Vector3(0, -1, 0);
				Vector3 _dirTo = Vector3(0, 1, 0);
				Vector3 _color = Vector3(0);
			};

			Fog     _fog;
			Sun     _sun;
			Vector3 _colorAmbient = Vector3(0);
			float   _time = 0.5f;
			float   _timeSpeed = 1 / 300.f;

		public:
			Atmosphere();
			virtual ~Atmosphere();

			void Init();
			void Done();

			void Update();

			// Time:
			float GetTime() const { return _time; }
			void SetTime(float x) { _time = x; }
			void SetTimeSpeed(float x) { _timeSpeed = x; }

			// Sun:
			RcVector3 GetDirToSun() const;
			RcVector3 GetSunColor() const;
		};
		VERUS_TYPEDEFS(Atmosphere);
	}
}
