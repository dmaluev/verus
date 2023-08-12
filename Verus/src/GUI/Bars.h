// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::GUI
{
	class Bars : public Widget
	{
		float _aspectRatio = 21 / 9.f;

	public:
		Bars();
		virtual ~Bars();

		static PWidget Make();

		virtual void Update() override;
		virtual void Draw() override;
		virtual void Parse(pugi::xml_node node) override;
	};
	VERUS_TYPEDEFS(Bars);
}
