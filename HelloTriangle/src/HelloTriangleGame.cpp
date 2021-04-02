// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::Game;

HelloTriangleGame::HelloTriangleGame()
{
	Utils::I().SetWritableFolderName("HelloTriangle");
}

HelloTriangleGame::~HelloTriangleGame()
{
	CGI::Renderer::I()->WaitIdle();
}

void HelloTriangleGame::BaseGame_UpdateSettings()
{
	VERUS_QREF_SETTINGS;
	settings._displayOffscreenDraw = false; // Draw directly to swap chain.
	GetEngineInit().ReducedFeatureSet(); // Don't try to load any resources.
}

void HelloTriangleGame::BaseGame_LoadContent()
{
	VERUS_QREF_RENDERER;

	// Create geometry:
	{
		const Vertex triangle[] =
		{
			{    0,  0.5f, 0, VERUS_COLOR_RGBA(255, 0, 0, 255)}, // Red vertex (on top).
			{-0.5f, -0.5f, 0, VERUS_COLOR_RGBA(0, 255, 0, 255)}, // Green vertex.
			{ 0.5f, -0.5f, 0, VERUS_COLOR_RGBA(0, 0, 255, 255)}, // Blue vertex.
		};
		_vertCount = VERUS_COUNT_OF(triangle);

		CGI::GeometryDesc geoDesc;
		geoDesc._name = "HelloTriangle.Geo"; // Optional name, for RenderDoc, etc.
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
		_geo->UpdateVertexBuffer(triangle, 0); // Copy command. When BaseGame_LoadContent is called, the command buffer is already recording commands.
	}

	// Create shader:
	{
		// Shader code can be provided as a string.
		_shaderCode =
#include "HelloTriangleShader.hlsl"
			;
		CSZ branches[] =
		{
			"main:#",
			nullptr
		};

		CGI::ShaderDesc shaderDesc;
		shaderDesc._source = _shaderCode;
		shaderDesc._branches = branches;
		_shader.Init(shaderDesc);
		_shader->CreateDescriptorSet(0, &_ubShaderVS, sizeof(_ubShaderVS), 100, {}, CGI::ShaderStageFlags::vs); // Uniform buffer used only by vertex shader.
		_shader->CreateDescriptorSet(1, &_ubShaderFS, sizeof(_ubShaderFS), 100, {}, CGI::ShaderStageFlags::fs); // Uniform buffer used only by fragment shader.
		_shader->CreatePipelineLayout();
	}

	// Create pipeline:
	{
		// What to draw? How to draw? Where to draw?
		CGI::PipelineDesc pipeDesc(_geo, _shader, "#", renderer.GetRenderPassHandle_AutoWithDepth()); // Reuse existing render pass.
		pipeDesc.DisableDepthTest();
		_pipe.Init(pipeDesc);
	}
}

void HelloTriangleGame::BaseGame_UnloadContent()
{
}

void HelloTriangleGame::BaseGame_HandleInput()
{
	//_stateMachine.HandleInput();
}

void HelloTriangleGame::BaseGame_Update()
{
	//_stateMachine.Update();
}

void HelloTriangleGame::BaseGame_Draw()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_TIMER;

	//ImGui::Text("HelloTriangle");

	auto cb = renderer.GetCommandBuffer(); // Default command buffer for this frame.

	const Matrix4 matWVP = Matrix4::scale(
		Vector3(
			0.9f + 0.2f * sin(timer.GetTime() * VERUS_PI * 0.41f), // Wobble.
			0.9f + 0.2f * sin(timer.GetTime() * VERUS_PI * 0.47f),
			1));
	_ubShaderVS._matWVP = matWVP.UniformBufferFormat();
	_ubShaderFS._phase = pow(abs(0.5f - glm::fract(timer.GetTime())) * 2, 4.f); // Blink.

	cb->BeginRenderPass(
		renderer.GetRenderPassHandle_AutoWithDepth(),
		renderer.GetFramebufferHandle_AutoWithDepth(renderer->GetSwapChainBufferIndex()),
		{ Vector4(0), Vector4(1) });

	cb->BindPipeline(_pipe);
	cb->BindVertexBuffers(_geo);
	_shader->BeginBindDescriptors(); // Map.
	cb->BindDescriptors(_shader, 0); // Update uniform buffer for set=0. (Copy from _ubShaderVS)
	cb->BindDescriptors(_shader, 1); // Update uniform buffer for set=1. (Copy from _ubShaderFS)
	_shader->EndBindDescriptors(); // Unmap.
	cb->Draw(_vertCount);

	renderer->ImGuiRenderDrawData(); // Also draw Dear ImGui, please.

	cb->EndRenderPass();
}
