// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	enum class Gapi : int
	{
		unknown,
		vulkan,
		direct3D11,
		direct3D12
	};

	struct BaseRendererDesc
	{
		GlobalVarsClipboard _gvc;

		BaseRendererDesc()
		{
			_gvc.Copy();
		}
	};
	VERUS_TYPEDEFS(BaseRendererDesc);

	class BaseRenderer : public Object
	{
	protected:
		Vector<PScheduled> _vScheduled;
		BaseRendererDesc   _desc;
		int                _swapChainBufferCount = 0;
		int                _swapChainBufferIndex = 0;
		int                _ringBufferIndex = 0;

		BaseRenderer();
		virtual ~BaseRenderer();

	public:
		static const int s_ringBufferSize = 3;

		static BaseRenderer* Load(CSZ dll, RBaseRendererDesc desc);
		virtual void ReleaseMe() = 0;

		void SetDesc(RBaseRendererDesc desc) { _desc = desc; }

		int GetSwapChainBufferCount() const { return _swapChainBufferCount; }
		int GetSwapChainBufferIndex() const { return _swapChainBufferIndex; }
		int GetRingBufferIndex() const { return _ringBufferIndex; }

		void Schedule(PScheduled p);
		void Unschedule(PScheduled p);
		void UpdateScheduled();

		virtual void ImGuiInit(RPHandle renderPassHandle) = 0;
		virtual void ImGuiRenderDrawData() = 0;

		virtual void ResizeSwapChain() = 0;

		virtual PBaseExtReality GetExtReality() { return nullptr; }

		// Which graphics API?
		virtual Gapi GetGapi() = 0;

		// <FrameCycle>
		virtual void BeginFrame() = 0;
		virtual void AcquireSwapChainImage() = 0;
		virtual void EndFrame() = 0;
		virtual void WaitIdle() = 0;
		virtual void OnMinimized() = 0;
		// </FrameCycle>

		// <Resources>
		virtual PBaseCommandBuffer InsertCommandBuffer() = 0;
		virtual PBaseGeometry      InsertGeometry() = 0;
		virtual PBasePipeline      InsertPipeline() = 0;
		virtual PBaseShader        InsertShader() = 0;
		virtual PBaseTexture       InsertTexture() = 0;

		virtual void DeleteCommandBuffer(PBaseCommandBuffer p) = 0;
		virtual void DeleteGeometry(PBaseGeometry p) = 0;
		virtual void DeletePipeline(PBasePipeline p) = 0;
		virtual void DeleteShader(PBaseShader p) = 0;
		virtual void DeleteTexture(PBaseTexture p) = 0;

		RPHandle CreateSimpleRenderPass(Format format, RP::Attachment::LoadOp loadOp = RP::Attachment::LoadOp::dontCare, ImageLayout layout = ImageLayout::fsReadOnly);
		RPHandle CreateShadowRenderPass(Format format, RP::Attachment::LoadOp loadOp = RP::Attachment::LoadOp::clear);
		virtual RPHandle CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD) = 0;
		virtual FBHandle CreateFramebuffer(RPHandle renderPassHandle, std::initializer_list<TexturePtr> il, int w, int h,
			int swapChainBufferIndex = -1, CubeMapFace cubeMapFace = CubeMapFace::none) = 0;
		virtual void DeleteRenderPass(RPHandle handle) = 0;
		virtual void DeleteFramebuffer(FBHandle handle) = 0;
		// </Resources>

		static void SetAlphaBlendHelper(
			CSZ sz,
			int& colorBlendOp,
			int& alphaBlendOp,
			int& srcColorBlendFactor,
			int& dstColorBlendFactor,
			int& srcAlphaBlendFactor,
			int& dstAlphaBlendFactor);

		virtual void UpdateUtilization() {}
	};
	VERUS_TYPEDEFS(BaseRenderer);

	extern "C"
	{
		typedef PBaseRenderer(*PFNCREATERENDERER)(UINT32 version, BaseRendererDesc* pDesc);
	}
}
