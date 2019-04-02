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
			BaseRendererDesc _desc;
			UINT32           _numSwapChainBuffers = 0;
			UINT32           _swapChainBufferIndex = 0;
			UINT32           _ringBufferIndex = 0;

			BaseRenderer();
			virtual ~BaseRenderer();

		public:
			enum { ringBufferSize = 3 };

			static BaseRenderer* Load(CSZ dll, RBaseRendererDesc desc);

			virtual void ReleaseMe() = 0;

			void SetDesc(RBaseRendererDesc desc) { _desc = desc; }

			UINT32 GetNumSwapChainBuffers() const { return _numSwapChainBuffers; }
			UINT32 GetSwapChainBufferIndex() const { return _swapChainBufferIndex; }
			UINT32 GetRingBufferIndex() const { return _ringBufferIndex; }

			// Which graphics API?
			virtual Gapi GetGapi() = 0;

			// Frame cycle:
			virtual void BeginFrame() = 0;
			virtual void EndFrame() = 0;
			virtual void Present() = 0;
			virtual void Clear(UINT32 flags) = 0;

			// Resources:
			virtual PBaseCommandBuffer InsertCommandBuffer() { return nullptr; }
			virtual PBaseGeometry      InsertGeometry() { return nullptr; }
			virtual PBasePipeline      InsertPipeline() { return nullptr; }
			virtual PBaseShader        InsertShader() { return nullptr; }
			virtual PBaseTexture       InsertTexture() { return nullptr; }

			virtual void DeleteCommandBuffer(PBaseCommandBuffer p) {}
			virtual void DeleteGeometry(PBaseGeometry p) {}
			virtual void DeletePipeline(PBasePipeline p) {}
			virtual void DeleteShader(PBaseShader p) {}
			virtual void DeleteTexture(PBaseTexture p) {}
		};
		VERUS_TYPEDEFS(BaseRenderer);
	}
}

extern "C"
{
	typedef verus::CGI::PBaseRenderer(*PFNVERUSCREATERENDERER)(UINT32 version, verus::CGI::BaseRendererDesc* pDesc);
}
