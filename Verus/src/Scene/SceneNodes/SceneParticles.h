// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class SceneParticles : public Object
		{
			Effects::Particles _particles;
			int                _refCount = 0;

		public:
			struct Desc
			{
				CSZ _url = nullptr;

				Desc(CSZ url = nullptr) : _url(url) {}
			};
			VERUS_TYPEDEFS(Desc);

			SceneParticles();
			~SceneParticles();

			void Init(RcDesc desc);
			bool Done();

			void AddRef() { _refCount++; }
			Effects::RParticles GetParticles() { return _particles; }

			void Update();
			void Draw();
		};
		VERUS_TYPEDEFS(SceneParticles);

		class SceneParticlesPtr : public Ptr<SceneParticles>
		{
		public:
			void Init(SceneParticles::RcDesc desc);
		};
		VERUS_TYPEDEFS(SceneParticlesPtr);

		class SceneParticlesPwn : public SceneParticlesPtr
		{
		public:
			~SceneParticlesPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(SceneParticlesPwn);
	}
}
