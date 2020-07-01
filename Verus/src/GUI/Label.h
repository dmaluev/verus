#pragma once

namespace verus
{
	namespace GUI
	{
		class Label : public Widget, public InputFocus
		{
			String         _font;
			Vector<String> _vChoices;
			float          _fontScale = 1;
			UINT32         _shadowColor = VERUS_COLOR_RGBA(0, 0, 0, 64);
			bool           _center = false;

		public:
			Label();
			virtual ~Label();

			static PWidget Make();

			virtual void Update() override;
			virtual void Draw() override;
			virtual void Parse(pugi::xml_node node) override;
			virtual PInputFocus AsInputFocus() override { return this; }

			void SetShadowColor(UINT32 color) { _shadowColor = color; }

			CSZ GetFont() const { return _C(_font); }
			float GetFontScale() const { return _fontScale; }
			float GetFontH() const;

			void SetChoice(CSZ s);
			void SetChoiceValue(CWSZ s);
			void NextChoiceValue();
			int GetChoiceIndex(CWSZ s = nullptr);
			void SetChoiceIndex(int index);

			virtual bool InvokeOnClick(float x, float y) override;
		};
		VERUS_TYPEDEFS(Label);
	}
}
