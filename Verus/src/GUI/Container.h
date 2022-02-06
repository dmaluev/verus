// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace GUI
	{
		class Container
		{
			typedef PWidget(*PFNCREATOR)();
			typedef Map<String, PFNCREATOR> TMapCreators;

			TMapCreators    _mapCreators;
			Vector<PWidget> _vWidgets;

		public:
			Container();
			virtual ~Container();

			VERUS_P(void RegisterAll());
			VERUS_P(void RegisterWidget(CSZ type, PFNCREATOR pCreator));
			VERUS_P(PWidget CreateWidget(CSZ type));

			void UpdateWidgets();
			void DrawWidgets();

			void ParseWidgets(pugi::xml_node node, CSZ sizerID);

			void ResetAnimators(float reverseTime = 0);

			virtual bool IsSizer() { return false; }

			PWidget GetHovered(float x, float y);
			PWidget GetWidgetById(CSZ id);
			int GetWidgetCount() const { return Utils::Cast32(_vWidgets.size()); }
			int GetWidgetIndex(PWidget p);
		};
		VERUS_TYPEDEFS(Container);
	}
}
