#pragma once

namespace verus
{
	namespace CGI
	{
		enum class Gapi : int
		{
			unknown,
			vulkan,
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
			BaseRendererDesc   _desc;
			UINT32             _numSwapChainBuffers = 0;
			UINT32             _swapChainBufferIndex = 0;
			UINT32             _ringBufferIndex = 0;

			BaseRenderer();
			virtual ~BaseRenderer();

		public:
			static const int s_ringBufferSize = 3;

			static BaseRenderer* Load(CSZ dll, RBaseRendererDesc desc);

			virtual void ReleaseMe() = 0;

			void SetDesc(RBaseRendererDesc desc) { _desc = desc; }

			UINT32 GetNumSwapChainBuffers() const { return _numSwapChainBuffers; }
			UINT32 GetSwapChainBufferIndex() const { return _swapChainBufferIndex; }
			UINT32 GetRingBufferIndex() const { return _ringBufferIndex; }

			// Which graphics API?
			virtual Gapi GetGapi() = 0;

			// Frame cycle:
			virtual void BeginFrame(bool present = true) = 0;
			virtual void EndFrame(bool present = true) = 0;
			virtual void Present() = 0;
			virtual void Clear(UINT32 flags) = 0;

			virtual void WaitIdle() = 0;

			// Resources:
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

			virtual int CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD) { return 0; }
			virtual int CreateFramebuffer(int renderPassID, std::initializer_list<TexturePtr> il, int w, int h, int swapChainBufferIndex = -1) { return 0; }
			virtual void DeleteRenderPass(int id) {}
			virtual void DeleteFramebuffer(int id) {}
		};
		VERUS_TYPEDEFS(BaseRenderer);
	}

	extern "C"
	{
		typedef CGI::PBaseRenderer(*PFNVERUSCREATERENDERER)(UINT32 version, CGI::BaseRendererDesc* pDesc);
	}
}
