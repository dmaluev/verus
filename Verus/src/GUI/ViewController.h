// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace GUI
	{
		class ViewController : public Object, public ViewDelegate
		{
			PView _pView = nullptr;

		public:
			ViewController();
			virtual ~ViewController();

			void Init(CSZ url);
			void Done();

			void Reload();

			PView GetView() { return _pView; }
			PWidget GetWidgetById(CSZ id);

			template<typename T>
			T* Connect(T*& p, CSZ id)
			{
				p = dynamic_cast<T*>(GetWidgetById(id));
				VERUS_RT_ASSERT(p);
				return p;
			}

			void ConnectOnMouseEnter /**/(TFnEvent fn, CSZ id);
			void ConnectOnMouseLeave /**/(TFnEvent fn, CSZ id);
			void ConnectOnClick      /**/(TFnEvent fn, CSZ id);
			void ConnectOnDoubleClick/**/(TFnEvent fn, CSZ id);
			void ConnectOnKey        /**/(TFnEvent fn, CSZ id = nullptr);
			void ConnectOnChar       /**/(TFnEvent fn, CSZ id = nullptr);
			void ConnectOnTimeout    /**/(TFnEvent fn, CSZ id = nullptr);

			void BeginFadeTo();

			virtual void View_SetViewData(PView pView) override {}
			virtual void View_GetViewData(PView pView) override {}

			void OnKey(RcEventArgs args);
		};
		VERUS_TYPEDEFS(ViewController);
	}
}
