// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::GUI
{
	class TextBox : public Label
	{
		enum class Rule : int
		{
			none,
			lower = (1 << 0),
			upper = (1 << 1),
			number = (1 << 2)
		};

		WideString _fullText;
		int        _cursor = 0;
		int        _viewOffset = 0;
		int        _maxLength = -1;
		Cooldown   _blinkCooldown = 0.5f;
		Rule       _rule = Rule::none;
		bool       _blinkState = false;

	public:
		TextBox();
		virtual ~TextBox();

		static PWidget Make();

		virtual void Update() override;
		virtual void Draw() override;
		virtual void Parse(pugi::xml_node node) override;
		virtual PInputFocus AsInputFocus() override { return this; }

		void Focus();
		virtual void InputFocus_AddChar(wchar_t c) override;
		virtual void InputFocus_DeleteChar() override;
		virtual void InputFocus_BackspaceChar() override;
		virtual void InputFocus_SetCursor(int pos) override;
		virtual void InputFocus_MoveCursor(int delta) override;
		virtual void InputFocus_Enter() override;
		virtual void InputFocus_Tab() override;
		virtual void InputFocus_Key(int scancode) override;
		virtual void InputFocus_OnFocus() override;

		void UpdateLabelText();

		WideStr GetText() const;
		void    SetText(CWSZ txt);
		void    SetText(CSZ txt);
	};
	VERUS_TYPEDEFS(TextBox);
}
