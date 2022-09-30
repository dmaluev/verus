// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::App;

UndoManager::UndoManager()
{
}

UndoManager::~UndoManager()
{
}

void UndoManager::Init(int maxCommands)
{
	VERUS_INIT();

	SetMaxCommands(maxCommands);
}

void UndoManager::Done()
{
	for (auto& p : _vCommands)
		delete p;
	_vCommands.clear();

	VERUS_DONE(UndoManager);
}

void UndoManager::SetMaxCommands(int maxCommands)
{
	_maxCommands = maxCommands;
	_vCommands.reserve(_maxCommands);
}

void UndoManager::Undo()
{
	if (CanUndo())
	{
		_vCommands[_nextUndo]->Undo();
		_nextUndo++;
	}
}

void UndoManager::Redo()
{
	if (CanRedo())
	{
		_nextUndo--;
		_vCommands[_nextUndo]->Redo();
	}
}

bool UndoManager::CanUndo() const
{
	return _nextUndo < _vCommands.size() && _vCommands[_nextUndo]->IsValid() && _vCommands[_nextUndo]->HasRedo();
}

bool UndoManager::CanRedo() const
{
	return _nextUndo && _vCommands[_nextUndo - 1]->IsValid() && _vCommands[_nextUndo - 1]->HasRedo();
}

UndoManager::PCommand UndoManager::BeginChange(PCommand pCommand)
{
	if (_nextUndo)
	{
		VERUS_FOR(i, _nextUndo)
			delete _vCommands[i];
		_vCommands.erase(_vCommands.begin(), _vCommands.begin() + _nextUndo);
		_nextUndo = 0;
	}

	if (_vCommands.size() >= _maxCommands && _vCommands.back()->HasRedo())
		_vCommands.resize(_vCommands.size() - 1);

	_vCommands.insert(_vCommands.begin(), 1, pCommand);

	return _vCommands.front();
}

UndoManager::PCommand UndoManager::EndChange(PCommand pCommand)
{
	if (pCommand)
	{
		auto it = std::find(_vCommands.begin(), _vCommands.end(), pCommand);
		if (it == _vCommands.end())
			pCommand = nullptr;
	}
	else
	{
		pCommand = _vCommands.empty() ? nullptr : _vCommands.front();
	}
	if (pCommand)
	{
		pCommand->SetHasRedo(true);
		pCommand->SaveState();
	}
	return pCommand;
}

void UndoManager::DeleteInvalidCommands()
{
	for (auto& p : _vCommands)
	{
		if (!p->IsValid())
		{
			delete p;
			p = nullptr;
		}
	}
	_vCommands.erase(std::remove(_vCommands.begin(), _vCommands.end(), nullptr), _vCommands.end());
	_nextUndo = 0;
}
