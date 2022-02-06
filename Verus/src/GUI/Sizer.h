// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace GUI
	{
		class Sizer : public Widget, public Container
		{
			int   _rows = 1;
			int   _cols = 1;
			float _rowHeight = 0;
			float _colWidth = 0;

		public:
			Sizer();
			virtual ~Sizer();

			static PWidget Make();

			virtual void Update() override;
			virtual void Draw() override;
			virtual void Parse(pugi::xml_node node) override;

			virtual PContainer AsContainer() override { return this; }
			virtual bool IsSizer() override { return true; }

			void GetWidgetAbsolutePosition(PWidget pWidget, float& x, float& y);
		};
		VERUS_TYPEDEFS(Sizer);
	}
}
