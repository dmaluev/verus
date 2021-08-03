// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Anim
	{
		// Warp manages sphere-based deformations to create such effects like dangling and speech.
		// Any other part of the mesh can also be deformed.
		// Warp's zone objects can use motion object or can simulate spring physics,
		// for example to simulate female's breast.
		// Warp class includes a function to convert special audio files to motion.
		// If there is a filename.wav, containing character's speech, some additional files must be created:
		// 1. filename_Jaw.wav; effects: Stretch 400, Dynamic 10 Gate, 8000 8-bit.
		// 2. filename_O.wav; effects: Low pass 5000Hz, Keep O noise, Dynamic 10 Gate, 8000 8-bit.
		// 3. filename_S.wav; effects: Keep S noise, Dynamic 10 Gate, Stretch 400, 8000 8-bit.
		// Motion should be applied to the skeleton before it will be used in Update() method.
		// Then _skeletonReady can be set to true for the mesh.
		class Warp : public Object
		{
		public:
			typedef Map<String, Vector3> TMapOffsets;
			class Zone
			{
			public:
				Point3      _pos = Point3(0);
				Point3      _posW = Point3(0);
				Vector3     _vel = Vector3(0);
				Vector3     _off = Vector3(0);
				String      _name;
				String      _bone;
				TMapOffsets _mapOffsets;
				float       _radius = 0;
				float       _spring = 0;
				float       _damping = 5;
				float       _maxOffset = 0;
				char        _type = 0;
			};
			VERUS_TYPEDEFS(Zone);

		private:
			Vector<Zone> _vZones;
			String       _preview;
			float        _yMin = FLT_MAX;
			float        _jawScale = 1;

		public:
			Warp();
			~Warp();

			void Init();
			void Done();

			void Update(RSkeleton skeleton);
			void DrawLines() const;
			void DrawZones() const;

			void Load(CSZ url);
			void LoadFromPtr(SZ p);

			void Fill(PVector4 p);

			void ApplyMotion(RMotion motion, float time);

			static void ComputeLipSyncFromAudio(RcBlob blob, RMotion motion, float jawScale = 1);

			float GetJawScale() const { return _jawScale; }

			template<typename T>
			void ForEachZone(const T& fn)
			{
				for (auto& zone : _vZones)
				{
					if (Continue::no == fn(zone))
						break;
				}
			}

			void SetPreview(CSZ preview) { _preview = preview; }
		};
		VERUS_TYPEDEFS(Warp);
	}
}
