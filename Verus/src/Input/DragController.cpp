// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Input;

void DragController::SetDelegate(PDragControllerDelegate p)
{
	_pDelegate = p;
	if (_pDelegate)
		_pDelegate->DragController_GetRatio(_ratioX, _ratioY);
}

void DragController::Begin(int x, int y)
{
	if (!_pDelegate)
		return;
	_holding = true;
	_originCoordX = x;
	_originCoordY = y;
	_pDelegate->DragController_Begin();
	_pDelegate->DragController_GetParams(_originX, _originY);
}

bool DragController::DragTo(int x, int y)
{
	if (!_pDelegate || !_holding)
		return false;
	const float newX = _originX + (x - _originCoordX) * _ratioX;
	const float newY = _originY + (y - _originCoordY) * _ratioY;
	_pDelegate->DragController_SetParams(newX, newY);
	return true;
}

bool DragController::DragBy(int x, int y)
{
	if (!_pDelegate || !_holding)
		return false;
	_originX += x * _ratioX;
	_originY += y * _ratioY;
	_pDelegate->DragController_SetParams(_originX, _originY);
	return true;
}

void DragController::End()
{
	if (_pDelegate)
		_pDelegate->DragController_End();
	_holding = false;
}

void DragController::SetScale(float s)
{
	if (_pDelegate)
		_pDelegate->DragController_SetScale(s);
}
