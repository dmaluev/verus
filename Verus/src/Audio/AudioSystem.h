// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Audio
{
	typedef StoreUnique<String, Sound> TStoreSounds;
	class AudioSystem : public Singleton<AudioSystem>, public Object, private TStoreSounds
	{
		Point3       _listenerPosition = Point3(0);
		Vector3      _listenerDirection = Vector3(0, 0, 1);
		Vector3      _listenerVelocity = Vector3(0);
		Vector3      _listenerUp = Vector3(0, 1, 0);
		ALCdevice* _pDevice = nullptr;
		ALCcontext* _pContext = nullptr;
		String       _version;
		String       _deviceSpecifier;
		StreamPlayer _streamPlayers[4];

	public:
		AudioSystem();
		~AudioSystem();

		void Init();
		void Done();

		void Update();

		RStreamPlayer GetStreamPlayer(int index) { return _streamPlayers[index]; }
		void DeleteAllStreams();

		PSound InsertSound(CSZ url);
		PSound FindSound(CSZ url);
		void DeleteSound(CSZ url);
		void DeleteAllSounds();

		void UpdateListener(RcPoint3 pos, RcVector3 dir, RcVector3 vel, RcVector3 up);
		float ComputeTravelDelay(RcPoint3 pos) const;

		static CSZ GetSingletonFailMessage() { return "Make_Audio(); // FAIL.\r\n"; }
	};
	VERUS_TYPEDEFS(AudioSystem);
}
