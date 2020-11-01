// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class Emitter : public SceneNode
		{
			Vector4           _flowColor = Vector4::Replicate(1);
			Vector3           _flowOffset = Vector3(0);
			SceneParticlesPwn _particles;
			Audio::SoundPwn   _sound;
			Audio::SourcePtr  _src;
			float             _flow = 0;
			float             _flowScale = 1;

		public:
			struct Desc
			{
				pugi::xml_node _node;
				CSZ            _name = nullptr;
				CSZ            _urlParticles = nullptr;
				CSZ            _urlSound = nullptr;
			};
			VERUS_TYPEDEFS(Desc);

			Emitter();
			~Emitter();

			void Init(RcDesc desc);
			void Done();

			virtual void Update() override;

			virtual String GetUrl() override { return _C(_particles->GetParticles().GetUrl()); }

			SceneParticlesPtr GetParticles() { return _particles; }

			// Serialization:
			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;
			virtual void SaveXML(pugi::xml_node node) override;
			virtual void LoadXML(pugi::xml_node node) override;
		};
		VERUS_TYPEDEFS(Emitter);

		class EmitterPtr : public Ptr<Emitter>
		{
		public:
			void Init(Emitter::RcDesc desc);
		};
		VERUS_TYPEDEFS(EmitterPtr);

		class EmitterPwn : public EmitterPtr
		{
		public:
			~EmitterPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(EmitterPwn);
	}
}
