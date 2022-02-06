// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

Chat::Chat()
{
}

Chat::~Chat()
{
	Done();
}

void Chat::Init(int keepCount, float keepFor)
{
	VERUS_INIT();

	_keepFor = keepFor;

	_vMessages.resize(keepCount);
}

void Chat::Done()
{
	VERUS_DONE(Chat);
}

void Chat::AddMessage(CSZ txt, CSZ name, ChatMessageType type)
{
	if (!strlen(txt))
		return;

	VERUS_QREF_TIMER_GUI;

	String str;
	if (name)
	{
		str.reserve(strlen(txt) + strlen(name) + 2 + 11 + 11);
		if (!_nameColor.empty())
			str += _nameColor;
		str += name;
		str += ": ";
		if (!_nameColor.empty())
			str += _normalMsgColor.empty() ? "\\RGB=X" : _normalMsgColor;
		str += txt;
		txt = _C(str);
	}
	RMessage msg = _vMessages[_writeAt];
	msg._text = txt;
	msg._time = timer.GetTime();
	msg._type = type;
	_writeAt = VERUS_CIRCULAR_ADD(_writeAt, _vMessages.size());
}

CSZ Chat::GetText()
{
	VERUS_QREF_TIMER_GUI;

	const float minTime = timer.GetTime() - _keepFor;

	// Detect if we need a new string:
	int index = _writeAt;
	int count = static_cast<int>(_vMessages.size());
	do
	{
		index--;
		if (index < 0)
			index = static_cast<int>(_vMessages.size()) - 1;
		RMessage msg = _vMessages[index];
		if (msg._time < minTime)
			break;
		count--;
	} while (count);

	if (index == _oldMsgIndex && _writeAt == _oldWriteAt)
		return _C(_compiled);

	_oldWriteAt = _writeAt; // Confirm new messages.
	_oldMsgIndex = index; // Confirm messages getting old.

	// Build a new string:
	_compiled.clear();
	index = _writeAt;
	count = static_cast<int>(_vMessages.size());
	do
	{
		index--;
		if (index < 0)
			index = static_cast<int>(_vMessages.size()) - 1;
		RMessage msg = _vMessages[index];
		if (msg._time < minTime)
			break;

		if (ChatMessageType::normal == msg._type && !_normalMsgColor.empty())
			_compiled += _normalMsgColor;
		if (ChatMessageType::system == msg._type && !_systemMsgColor.empty())
			_compiled += _systemMsgColor;

		_compiled += msg._text;

		if (ChatMessageType::normal == msg._type && !_normalMsgColor.empty())
			_compiled += "\\RGB=X";
		if (ChatMessageType::system == msg._type && !_systemMsgColor.empty())
			_compiled += "\\RGB=X";

		_compiled += "\n";
		count--;
	} while (count);

	_compiledW = Str::Utf8ToWide(_compiled);
	return _C(_compiled);
}

CWSZ Chat::GetTextW()
{
	GetText();
	return _C(_compiledW);
}

void Chat::SetNormalMessageColor(UINT32 color)
{
	if (color)
	{
		_normalMsgColor = "\\RGB=";
		_normalMsgColor += Convert::ToHex(color).substr(0, 6);
	}
	else
		_normalMsgColor.clear();
}

void Chat::SetSystemMessageColor(UINT32 color)
{
	if (color)
	{
		_systemMsgColor = "\\RGB=";
		_systemMsgColor += Convert::ToHex(color).substr(0, 6);
	}
	else
		_systemMsgColor.clear();
}

void Chat::SetNameColor(UINT32 color)
{
	if (color)
	{
		_nameColor = "\\RGB=";
		_nameColor += Convert::ToHex(color).substr(0, 6);
	}
	else
		_nameColor.clear();
}
