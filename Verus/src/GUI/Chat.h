// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::GUI
{
	enum class ChatMessageType : int
	{
		normal,
		system
	};

	class Chat : public Object
	{
		struct Message
		{
			String          _text;
			float           _time = -FLT_MAX;
			ChatMessageType _type = ChatMessageType::normal;
		};
		VERUS_TYPEDEFS(Message);

		Vector<Message> _vMessages;
		String          _normalMsgColor;
		String          _systemMsgColor;
		String          _nameColor;
		String          _compiled;
		WideString      _compiledW;
		float           _keepFor = 0;
		int             _writeAt = 0;
		int             _oldWriteAt = -1;
		int             _oldMsgIndex = -1;

	public:
		Chat();
		~Chat();

		void Init(int keepCount = 4, float keepFor = 20);
		void Done();

		void AddMessage(CSZ txt, CSZ name, ChatMessageType type = ChatMessageType::normal);
		CSZ GetText();
		CWSZ GetTextW();

		void SetNormalMessageColor(UINT32 color);
		void SetSystemMessageColor(UINT32 color);
		void SetNameColor(UINT32 color);
	};
	VERUS_TYPEDEFS(Chat);
}
