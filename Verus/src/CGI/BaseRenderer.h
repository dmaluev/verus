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

			BaseRenderer();
			virtual ~BaseRenderer();

		public:
			static BaseRenderer* Load(CSZ dll, RBaseRendererDesc desc);

			virtual void ReleaseMe() = 0;

			void SetDesc(RBaseRendererDesc desc) { _desc = desc; }

			// Which graphics API?
			virtual Gapi GetGapi() = 0;

			virtual void PrepareDraw() {}
			virtual void Clear(UINT32 flags) {}
			virtual void Present() {}
		};
		VERUS_TYPEDEFS(BaseRenderer);
	}
}

extern "C"
{
	typedef verus::CGI::PBaseRenderer(*PFNVERUSCREATERENDERER)(UINT32 version, verus::CGI::BaseRendererDesc* pDesc);
}
