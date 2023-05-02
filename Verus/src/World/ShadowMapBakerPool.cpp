// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// ShadowMapBakerPool::Block:

ShadowMapBakerPool::Block::Block()
{
	VERUS_ZERO_MEM(_presence);
}

ShadowMapBakerPool::Block::~Block()
{
	Done();
}

void ShadowMapBakerPool::Block::Init(int side)
{
	VERUS_INIT();
	VERUS_QREF_BLUR;
	VERUS_QREF_RENDERER;

	// This texture will contain average linear depth and average linear depth squared (see VSM specs):
	CGI::TextureDesc texDesc;
	texDesc._clearValue = Vector4(1);
	texDesc._name = "ShadowMapBakerPool.Block.Tex";
	texDesc._format = CGI::Format::floatR16G16;
	texDesc._width = side;
	texDesc._height = side;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;
	_tex.Init(texDesc);

	_fbhPong = renderer->CreateFramebuffer(blur.GetRenderPassHandleForVsmV(),
		{
			_tex
		},
		side,
		side);

	_csh = renderer.GetDS().BindDescriptorSetTexturesForVSM(_tex);
}

void ShadowMapBakerPool::Block::Done()
{
	if (CGI::Renderer::IsLoaded())
	{
		VERUS_QREF_RENDERER;
		renderer.GetDS().GetLightShader()->FreeDescriptorSet(_csh);
		renderer->DeleteFramebuffer(_fbhPong);
	}
	VERUS_DONE(Block);
}

void ShadowMapBakerPool::Block::Update()
{
	VERUS_QREF_TIMER;
	VERUS_FOR(i, s_blockSide * s_blockSide)
	{
		if (IsReserved(i) && ((_baked >> i) & 0x1) && _presence[i] < 1)
			_presence[i] = Math::Min(1.f, _presence[i] + dt);
	}
}

bool ShadowMapBakerPool::Block::HasFreeCells() const
{
	return _reserved != UINT16_MAX;
}

int ShadowMapBakerPool::Block::FindFreeCell() const
{
	VERUS_FOR(i, s_blockSide * s_blockSide)
	{
		if (!IsReserved(i))
			return i;
	}
	return -1;
}

bool ShadowMapBakerPool::Block::IsUnused() const
{
	return !_reserved;
}

bool ShadowMapBakerPool::Block::IsReserved(int cellIndex) const
{
	return !!((_reserved >> cellIndex) & 0x1);
}

void ShadowMapBakerPool::Block::Reserve(int cellIndex)
{
	VERUS_RT_ASSERT(!IsReserved(cellIndex));
	VERUS_BITMASK_SET(_reserved, 1 << cellIndex);
	VERUS_BITMASK_UNSET(_baked, 1 << cellIndex);
	_presence[cellIndex] = 0;
}

void ShadowMapBakerPool::Block::Free(int cellIndex)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(IsReserved(cellIndex));
	VERUS_BITMASK_UNSET(_reserved, 1 << cellIndex);
	if (IsUnused())
		_doneFrame = renderer.GetFrameCount() + CGI::BaseRenderer::s_ringBufferSize + 1;
}

void ShadowMapBakerPool::Block::MarkBaked(int cellIndex)
{
	VERUS_BITMASK_SET(_baked, 1 << cellIndex);
}

// ShadowMapBakerPool:

ShadowMapBakerPool::ShadowMapBakerPool()
{
}

ShadowMapBakerPool::~ShadowMapBakerPool()
{
	Done();
}

void ShadowMapBakerPool::Init(int side)
{
	VERUS_INIT();
	VERUS_QREF_BLUR;
	VERUS_QREF_CSMB;
	VERUS_QREF_RENDERER;

	_side = side;

	CGI::TextureDesc texDesc;
	texDesc._clearValue = Vector4(1);
	texDesc._name = "ShadowMapBakerPool.TexDepth";
	texDesc._format = CGI::Format::unormD24uintS8;
	texDesc._width = _side;
	texDesc._height = _side;
	texDesc._flags = CGI::TextureDesc::Flags::depthSampledR;
	_tex[TEX_DEPTH].Init(texDesc);

	texDesc.Reset();
	texDesc._clearValue = Vector4(1);
	texDesc._name = "ShadowMapBakerPool.Tex2F";
	texDesc._format = CGI::Format::floatR16G16; // Same as in blocks.
	texDesc._width = _side;
	texDesc._height = _side;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;
	_tex[TEX_2CH].Init(texDesc);

	_fbh = renderer->CreateFramebuffer(csmb.GetRenderPassHandle(),
		{
			_tex[TEX_DEPTH]
		},
		_side,
		_side);
	_fbhPing = renderer->CreateFramebuffer(blur.GetRenderPassHandleForVsmU(),
		{
			_tex[TEX_2CH]
		},
		_side,
		_side);

	// For blur:
	_cshPing = blur.GetShader()->BindDescriptorSetTextures(1, { _tex[TEX_DEPTH] });
	_cshPong = blur.GetShader()->BindDescriptorSetTextures(1, { _tex[TEX_2CH] });

	_vBlocks.reserve(8);
}

void ShadowMapBakerPool::Done()
{
	for (auto& x : _vBlocks)
		x.Done();
	_vBlocks.clear();
	if (Effects::Blur::IsValidSingleton() && Effects::Blur::I().IsInitialized())
	{
		VERUS_QREF_BLUR;
		blur.GetShader()->FreeDescriptorSet(_cshPong);
		blur.GetShader()->FreeDescriptorSet(_cshPing);
	}
	if (CGI::Renderer::IsLoaded())
	{
		VERUS_QREF_RENDERER;
		renderer->DeleteFramebuffer(_fbhPing);
		renderer->DeleteFramebuffer(_fbh);
	}
	VERUS_DONE(ShadowMapBakerPool);
}

void ShadowMapBakerPool::Update()
{
	GarbageCollection();

	for (auto& x : _vBlocks)
		x.Update();
}

void ShadowMapBakerPool::Begin(RLightNode lightNode)
{
	VERUS_QREF_CSMB;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	_activeBlockHandle = lightNode.GetShadowMapHandle();
	VERUS_RT_ASSERT(_activeBlockHandle.IsSet());

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(80, 80, 80, 255), "ShadowMapBakerPool");

	lightNode.SetupShadowMapCamera(_passCamera);
	_pPrevPassCamera = wm.SetPassCamera(&_passCamera);
	_pPrevHeadCamera = wm.SetHeadCamera(&_passCamera); // For correct layout.

	cb->BeginRenderPass(csmb.GetRenderPassHandle(), _fbh, { _tex[TEX_DEPTH]->GetClearValue() },
		CGI::ViewportScissorFlags::setAllForFramebuffer);

	VERUS_RT_ASSERT(!_baking);
	_baking = true;
}

void ShadowMapBakerPool::End()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	auto cb = renderer.GetCommandBuffer();

	cb->EndRenderPass();

	VERUS_PROFILER_END_EVENT(cb);

	_pPrevPassCamera = wm.SetPassCamera(_pPrevPassCamera);
	VERUS_RT_ASSERT(&_passCamera == _pPrevPassCamera); // Check camera's integrity.
	_pPrevPassCamera = nullptr;
	_pPrevHeadCamera = wm.SetHeadCamera(_pPrevHeadCamera);
	VERUS_RT_ASSERT(&_passCamera == _pPrevHeadCamera); // Check camera's integrity.
	_pPrevHeadCamera = nullptr;

	VERUS_RT_ASSERT(_baking);
	_baking = false;

	const UINT32 blockIndex = _activeBlockHandle.GetBlockIndex();
	const UINT32 cellIndex = _activeBlockHandle.GetCellIndex();
	const Vector4 rc( // For viewport and scissor.
		static_cast<float>(_side * (cellIndex & 0x3)),
		static_cast<float>(_side * (cellIndex >> 2)),
		static_cast<float>(_side),
		static_cast<float>(_side));
	// This is where the magic happens:
	Effects::Blur::I().GenerateForVSM(_fbhPing, _vBlocks[blockIndex].GetFramebufferHandle(), _cshPing, _cshPong, rc, _passCamera.GetZNearFarEx());

	_vBlocks[blockIndex].MarkBaked(cellIndex);
}

Matrix4 ShadowMapBakerPool::GetOffsetMatrix(ShadowMapHandle handle) const
{
	const UINT32 cellIndex = handle.GetCellIndex();
	const float x = 1.f / s_blockSide;
	const Vector3 s(x, x, 1);
	return VMath::appendScale(Matrix4::translation(Vector3(x * (cellIndex & 0x3), x * (cellIndex >> 2), 0)), s);
}

CGI::CSHandle ShadowMapBakerPool::GetComplexSetHandle(ShadowMapHandle handle) const
{
	const UINT32 blockIndex = handle.GetBlockIndex();
	VERUS_RT_ASSERT(blockIndex < _vBlocks.size());
	return _vBlocks[blockIndex].GetComplexSetHandle();
}

float ShadowMapBakerPool::GetPresence(ShadowMapHandle handle) const
{
	const UINT32 blockIndex = handle.GetBlockIndex();
	VERUS_RT_ASSERT(blockIndex < _vBlocks.size());
	return _vBlocks[blockIndex].GetPresence(handle.GetCellIndex());
}

ShadowMapHandle ShadowMapBakerPool::ReserveCell()
{
	UINT32 blockIndex = 0;
	for (auto& block : _vBlocks)
	{
		if (block.HasFreeCells())
		{
			const int cellIndex = block.FindFreeCell();
			block.Reserve(cellIndex);
			return ShadowMapHandle::Make((blockIndex << 16) | cellIndex);
		}
		blockIndex++;
	}

	if (true)
	{
		_vBlocks.resize(_vBlocks.size() + 1);
		_vBlocks.back().Init(_side * s_blockSide);
		_vBlocks.back().Reserve(0);
		return ShadowMapHandle::Make(blockIndex << 16);
	}

	return ShadowMapHandle();
}

void ShadowMapBakerPool::FreeCell(ShadowMapHandle handle)
{
	const UINT32 blockIndex = handle.GetBlockIndex();
	VERUS_RT_ASSERT(blockIndex < _vBlocks.size());
	_vBlocks[blockIndex].Free(handle.GetCellIndex());
}

void ShadowMapBakerPool::GarbageCollection()
{
	VERUS_QREF_RENDERER;

	const size_t blockCount = _vBlocks.size();
	if (blockCount >= 2 &&
		_vBlocks[blockCount - 1].IsUnused() &&
		_vBlocks[blockCount - 2].IsUnused() &&
		renderer.GetFrameCount() >= _vBlocks.back().GetDoneFrame())
		_vBlocks.pop_back();
}
