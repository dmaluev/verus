#pragma once

namespace verus
{
	namespace Input
	{
		struct KeyMapperDelegate;
	}
	namespace CGI
	{
		struct RendererDelegate;
	}
}

namespace verus
{
	class EngineInit
	{
	public:
		bool _makeGlobal = true;
		bool _makeNet = true;
		bool _makeIO = true;
		bool _makeInput = true;
		bool _makeAudio = true;
		bool _makeCGI = true;
		bool _makePhysics = true;
		bool _makeEffects = true;
		bool _makeExtra = false;
		bool _makeScene = true;
		bool _makeGUI = true;

		void Make();
		void Free();

		void Init(Input::KeyMapperDelegate* pKeyMapperDelegate, CGI::RendererDelegate* pRendererDelegate);
	};
	VERUS_TYPEDEFS(EngineInit);
}
