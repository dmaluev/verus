// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace GUI
	{
		// Ultimate GUI manager.
		// Contains views ordered by z-depth.
		// Contains fonts, cursor and GUI utility functions.
		typedef StoreUniqueWithNoRefCount<String, Font> TStoreFonts;
		class ViewManager : public Singleton<ViewManager>, public Object, private TStoreFonts
		{
		public:
#include "../Shaders/GUI.inc.hlsl"

			enum PIPE
			{
				PIPE_MAIN,
				PIPE_MAIN_ADD,
				PIPE_MASK,
				PIPE_MASK_ADD,
				PIPE_SOLID_COLOR,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_DEBUG,
				TEX_DUMMY,
				TEX_COUNT
			};

		private:
			PView                         _pCurrentParseView = nullptr;
			Vector<PView>                 _vViews;
			Cursor                        _cursor;
			CGI::ShaderPwn                _shader;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwns<TEX_COUNT>   _tex;
			CGI::CSHandle                 _cshDefault;
			CGI::CSHandle                 _cshDebug;
			String                        _fadeToView;
			UB_Gui                        _ubGui;
			UB_GuiFS                      _ubGuiFS;

		public:
			ViewManager();
			~ViewManager();

			void Init(bool hasCursor = true, bool canDebug = true);
			void Done();

			void HandleInput();
			bool Update();
			void Draw();

			PView ParseView(CSZ url);
			void DeleteView(PView pView);
			void DeleteAllViews();

			PFont LoadFont(CSZ url);
			PFont FindFont(CSZ name = nullptr);
			void DeleteFont(CSZ name);
			void DeleteAllFonts();

			RCursor GetCursor() { return _cursor; }

			void MsgBox(CSZ txt, int data);
			void MsgBoxOK(CSZ id);

			PView GetViewByName(CSZ name);
			PView GetViewByZ(int index) { return _vViews[index]; }

			PView MoveToFront	/**/(CSZ viewName);
			PView MoveToBack	/**/(CSZ viewName);
			PView BeginFadeTo	/**/(CSZ viewName = ":VOID:");
			void BeginFadeOut();
			PView Activate(CSZ viewName);
			PView Deactivate(CSZ viewName);
			bool HasAllViewsInDoneState();
			bool HasSomeViewsInFadeState();
			VERUS_P(bool SwitchView());

			CGI::ShaderPtr GetShader() { return _shader; }
			void BindPipeline(PIPE pipe, CGI::CommandBufferPtr cb);
			UB_Gui& GetUbGui() { return _ubGui; }
			UB_GuiFS& GetUbGuiFS() { return _ubGuiFS; }
			CGI::TexturePtr GetDebugTexture();
			CGI::CSHandle GetDefaultComplexSetHandle() const { return _cshDefault; }
			CGI::CSHandle GetDebugComplexSetHandle() const { return _cshDebug; }

			PView GetCurrentParseView() { return _pCurrentParseView; }

			static float ParseCoordX(CSZ coord, float def = 0);
			static float ParseCoordY(CSZ coord, float def = 0);

			void OnKey(int scancode);
			void OnChar(wchar_t c);

			static CSZ GetSingletonFailMessage() { return "Make_GUI(); // FAIL.\r\n"; }
		};
		VERUS_TYPEDEFS(ViewManager);
	}
}
