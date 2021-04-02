// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		// Epigraph
		// "Vulkan – (it takes) 1000 lines to draw a triangle"
		// "- I thought you only needed 3 lines to draw a triangle"
		// https://www.reddit.com/r/linux/comments/58fyqn/vulkan_1000_lines_to_draw_a_triangle/

		class HelloTriangleGame : public Singleton<HelloTriangleGame>, public BaseGame
		{
			struct Vertex
			{
				float _x, _y, _z;
				UINT32 _color;
			};
			VERUS_TYPEDEFS(Vertex);

			VERUS_UBUFFER UB_ShaderVS
			{
				matrix _matWVP;
			};

			VERUS_UBUFFER UB_ShaderFS
			{
				float _phase;
			};

			CGI::GeometryPwn _geo;
			CGI::ShaderPwn   _shader;
			CGI::PipelinePwn _pipe;
			CSZ              _shaderCode;
			UB_ShaderVS      _ubShaderVS;
			UB_ShaderFS      _ubShaderFS;
			int              _vertCount = 0;

		public:
			HelloTriangleGame();
			~HelloTriangleGame();

			virtual void BaseGame_UpdateSettings() override;
			virtual void BaseGame_LoadContent() override;
			virtual void BaseGame_UnloadContent() override;
			virtual void BaseGame_HandleInput() override;
			virtual void BaseGame_Update() override;
			virtual void BaseGame_Draw() override;
		};
		VERUS_TYPEDEFS(HelloTriangleGame);
	}
}

#define VERUS_QREF_GAME Game::RHelloTriangleGame game = Game::HelloTriangleGame::I()
