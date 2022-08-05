// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class HelloTexturedCubeGame : public Singleton<HelloTexturedCubeGame>, public BaseGame
		{
			struct Vertex
			{
				float _x, _y, _z;
				UINT32 _color;
			};
			VERUS_TYPEDEFS(Vertex);

			VERUS_UBUFFER UB_ShaderVS
			{
				mataff _matW;
				matrix _matVP;
			};

			VERUS_UBUFFER UB_ShaderFS
			{
				float _phase;
			};

			Vector3          _rotation = Vector3(0);
			CGI::GeometryPwn _geo;
			CGI::ShaderPwn   _shader;
			CGI::PipelinePwn _pipe;
			CGI::TexturePwn  _tex;
			CSZ              _shaderCode;
			CGI::CSHandle    _csh; // Complex set handle. Simple set has one uniform buffer. Complex set additionally has textures.
			UB_ShaderVS      _ubShaderVS;
			UB_ShaderFS      _ubShaderFS;
			int              _vertCount = 0;
			int              _indexCount = 0;

		public:
			HelloTexturedCubeGame();
			~HelloTexturedCubeGame();

			virtual void BaseGame_UpdateSettings(App::Window::RDesc windowDesc) override;
			virtual void BaseGame_LoadContent() override;
			virtual void BaseGame_UnloadContent() override;
			virtual void BaseGame_Update() override;
			virtual void BaseGame_Draw() override;
			virtual void BaseGame_DrawView(CGI::RcViewDesc viewDesc) override;
		};
		VERUS_TYPEDEFS(HelloTexturedCubeGame);
	}
}

#define VERUS_QREF_GAME Game::RHelloTexturedCubeGame game = Game::HelloTexturedCubeGame::I()
