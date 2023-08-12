// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Effects
{
	class Ssr : public Singleton<Ssr>, public Object
	{
#include "../Shaders/Ssr.inc.hlsl"

		enum PIPE
		{
			PIPE_MAIN,
			PIPE_DEBUG_CUBE_MAP,
			PIPE_COUNT
		};

		static UB_SsrVS s_ubSsrVS;
		static UB_SsrFS s_ubSsrFS;

		CGI::ShaderPwn                _shader;
		CGI::PipelinePwns<PIPE_COUNT> _pipe;
		CGI::RPHandle                 _rph;
		CGI::FBHandle                 _fbh;
		CGI::CSHandle                 _csh;
		float                         _radius = 2.2f;
		float                         _depthBias = 0.03f;
		float                         _thickness = 0.3f;
		float                         _equalizeDist = 20;
		bool                          _cubeMapDebugMode = false;
		bool                          _editMode = false;

	public:
		Ssr();
		~Ssr();

		void Init();
		void Done();

		void OnSwapChainResized();

		bool BindDescriptorSetTextures();

		void Generate();

		bool IsCubeMapDebugMode() const { return _cubeMapDebugMode; }
		void ToggleCubeMapDebugMode() { _cubeMapDebugMode = !_cubeMapDebugMode; }

		bool IsEditMode() const { return _editMode; }
		void ToggleEditMode() { _editMode = !_editMode; }
	};
	VERUS_TYPEDEFS(Ssr);
}
