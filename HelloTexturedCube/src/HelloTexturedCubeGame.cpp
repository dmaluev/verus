// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::Game;

HelloTexturedCubeGame::HelloTexturedCubeGame()
{
	Utils::I().SetWritableFolderName("HelloTexturedCube");
}

HelloTexturedCubeGame::~HelloTexturedCubeGame()
{
	CGI::Renderer::I()->WaitIdle();
}

void HelloTexturedCubeGame::BaseGame_UpdateSettings(App::Window::RDesc windowDesc)
{
	VERUS_QREF_SETTINGS;
	settings._displayOffscreenDraw = false; // Draw directly to swap chain.
	GetEngineInit().ReducedFeatureSet(); // Don't try to load any resources.
}

void HelloTexturedCubeGame::BaseGame_LoadContent()
{
	VERUS_QREF_RENDERER;

	GetCameraSpirit().MoveTo(Point3::Replicate(3));
	GetCameraSpirit().LookAt(Point3(0), true);

	// Create geometry:
	{
		const Vertex cubeVerts[] =
		{
			{-1, -1, -1, VERUS_COLOR_RGBA(0,   0, 0, 255)},
			{+1, -1, -1, VERUS_COLOR_RGBA(255,   0, 0, 255)},
			{-1, +1, -1, VERUS_COLOR_RGBA(0, 255, 0, 255)},
			{+1, +1, -1, VERUS_COLOR_RGBA(255, 255, 0, 255)},
			{-1, -1, +1, VERUS_COLOR_RGBA(0,   0, 255, 255)},
			{+1, -1, +1, VERUS_COLOR_RGBA(255,   0, 255, 255)},
			{-1, +1, +1, VERUS_COLOR_RGBA(0, 255, 255, 255)},
			{+1, +1, +1, VERUS_COLOR_RGBA(255, 255, 255, 255)}
		};
		_vertCount = VERUS_COUNT_OF(cubeVerts);

		const UINT16 cubeIndices[] =
		{
			// -X
			2, 0, 6,
			6, 0, 4,

			// +X
			1, 3, 5,
			5, 3, 7,

			// -Y
			0, 1, 4,
			4, 1, 5,

			// +Y
			2, 6, 3,
			3, 6, 7,

			// -Z
			5, 7, 4,
			4, 7, 6,

			// +Z
			1, 0, 3,
			3, 0, 2
		};
		_indexCount = VERUS_COUNT_OF(cubeIndices);

		CGI::GeometryDesc geoDesc;
		geoDesc._name = "HelloTexturedCube.Geo"; // Optional name, for RenderDoc, etc.
		const CGI::VertexInputAttrDesc viaDesc[] =
		{
			{0, offsetof(Vertex, _x),     CGI::ViaType::floats, 3, CGI::ViaUsage::position, 0}, // 3 floats for position.
			{0, offsetof(Vertex, _color), CGI::ViaType::ubytes, 4, CGI::ViaUsage::color, 0}, // Color can be stored as 32-bit unsigned integer.
			CGI::VertexInputAttrDesc::End()
		};
		geoDesc._pVertexInputAttrDesc = viaDesc;
		const int strides[] = { sizeof(Vertex), 0 };
		geoDesc._pStrides = strides;
		_geo.Init(geoDesc);
		_geo->CreateVertexBuffer(_vertCount, 0);
		_geo->UpdateVertexBuffer(cubeVerts, 0); // Copy command. When BaseGame_LoadContent is called, the command buffer is already recording commands.
		_geo->CreateIndexBuffer(_indexCount);
		_geo->UpdateIndexBuffer(cubeIndices); // Copy command.
	}

	// Create shader:
	{
		_shader.Init("[Shaders]:HelloTexturedCubeShader.hlsl"); // HLSL file was copied by Post-Build Event.
		_shader->CreateDescriptorSet(0, &_ubShaderVS, sizeof(_ubShaderVS), 100, {}, CGI::ShaderStageFlags::vs); // Uniform buffer used only by vertex shader.
		_shader->CreateDescriptorSet(1, &_ubShaderFS, sizeof(_ubShaderFS), 100,
			{
				CGI::Sampler::aniso // Immutable sampler for this set.
			}, CGI::ShaderStageFlags::fs); // Uniform buffer used only by fragment shader.
		_shader->CreatePipelineLayout();
	}

	// Create pipeline:
	{
		// What to draw? How to draw? Where to draw?
		CGI::PipelineDesc pipeDesc(_geo, _shader, "#", renderer.GetRenderPassHandle_AutoWithDepth()); // Reuse existing render pass.
		_pipe.Init(pipeDesc);
	}

	_tex.Init("[Textures]:HelloTexturedCube.dds"); // Texture file was copied by Post-Build Event.
}

void HelloTexturedCubeGame::BaseGame_UnloadContent()
{
}

void HelloTexturedCubeGame::BaseGame_Update()
{
	VERUS_QREF_TIMER;

	_rotation += Vector3(dt * 0.03f, dt * 0.05f, dt * 0.07f);
}

void HelloTexturedCubeGame::BaseGame_Draw()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_TIMER;

	if (!_csh.IsSet() && _tex->IsLoaded())
		_csh = _shader->BindDescriptorSetTextures(1, { _tex });

	ImGui::Text("HelloTexturedCube");
	ImGui::Text("Use Mouse and WASD keys");

	Scene::RCamera camera = GetCamera();
	Transform3 matW = Transform3::rotationZYX(_rotation);

	auto cb = renderer.GetCommandBuffer(); // Default command buffer for this frame.

	_ubShaderVS._matW = matW.UniformBufferFormat();
	_ubShaderVS._matVP = camera.GetMatrixVP().UniformBufferFormat();
	_ubShaderFS._phase = pow(abs(0.5f - glm::fract(timer.GetTime() * 0.25f)) * 2, 4.f); // Blink.

	cb->BeginRenderPass(
		renderer.GetRenderPassHandle_AutoWithDepth(),
		renderer.GetFramebufferHandle_AutoWithDepth(renderer->GetSwapChainBufferIndex()),
		{ Vector4(0), Vector4(1) });

	if (_csh.IsSet()) // Texture is loaded asynchronously. Don't draw the cube, while the texture is loading.
	{
		cb->BindPipeline(_pipe);
		cb->BindVertexBuffers(_geo);
		cb->BindIndexBuffer(_geo);
		_shader->BeginBindDescriptors(); // Map.
		cb->BindDescriptors(_shader, 0); // Update uniform buffer for set=0. (Copy from _ubShaderVS)
		cb->BindDescriptors(_shader, 1, _csh); // Update uniform buffer for set=1. (Copy from _ubShaderFS). Also set textures.
		_shader->EndBindDescriptors(); // Unmap.
		cb->DrawIndexed(_indexCount);
	}

	renderer->ImGuiRenderDrawData(); // Also draw Dear ImGui, please.

	cb->EndRenderPass();
}
