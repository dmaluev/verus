// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	// SoundNode adds sound to some area of the world.
	// * can control some sound parameters like gain and looping
	class SoundNode : public BaseNode
	{
		Audio::SoundPwn  _sound;
		Audio::SourcePtr _soundSource;
		float            _gain = 0.8f;
		float            _pitch = 1;
		bool             _looping = true;

	public:
		struct Desc : BaseNode::Desc
		{
			CSZ   _url = nullptr;
			float _gain = 0.8f;
			float _pitch = 1;
			bool  _looping = true;

			Desc(CSZ url = nullptr) : _url(url) {}
		};
		VERUS_TYPEDEFS(Desc);

		SoundNode();
		virtual ~SoundNode();

		void Init(RcDesc desc);
		void Done();

		virtual void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication) override;

		virtual void Update() override;

		virtual void Disable(bool disable) override;

		virtual void Serialize(IO::RSeekableStream stream) override;
		virtual void Deserialize(IO::RStream stream) override;

		// <Resources>
		bool IsLoaded() const { return _sound && _sound->IsLoaded(); }
		void Load(CSZ url);
		Str GetURL() const { return _sound ? _sound->GetURL() : ""; }

		Audio::SoundPtr GetSound() const { return _sound; }
		Audio::SourcePtr GetSoundSource() const { return _soundSource; }
		// </Resources>

		float GetGain() const { return _gain; }
		void SetGain(float gain);

		float GetPitch() const { return _pitch; }
		void SetPitch(float pitch);

		bool IsLooping() const { return _looping; }
		void SetLooping(bool b = true);

		void Play();
		void Stop();
	};
	VERUS_TYPEDEFS(SoundNode);

	class SoundNodePtr : public Ptr<SoundNode>
	{
	public:
		void Init(SoundNode::RcDesc desc);
		void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication);
	};
	VERUS_TYPEDEFS(SoundNodePtr);

	class SoundNodePwn : public SoundNodePtr
	{
	public:
		~SoundNodePwn() { Done(); }
		void Done();
	};
	VERUS_TYPEDEFS(SoundNodePwn);
}
