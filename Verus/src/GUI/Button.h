// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace GUI
	{
		class Button : public Widget
		{
			Label _label;
			Image _image;
			Image _icon;
			bool  _hasIcon = false;

		public:
			Button();
			virtual ~Button();

			static PWidget Make();

			virtual void Update() override;
			virtual void Draw() override;
			virtual void Parse(pugi::xml_node node) override;
		};
		VERUS_TYPEDEFS(Button);
	}
}
