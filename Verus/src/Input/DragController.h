// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Input
{
	struct DragControllerDelegate
	{
		virtual void DragController_GetParams(float& x, float& y) = 0;
		virtual void DragController_SetParams(float x, float y) = 0;
		virtual void DragController_GetRatio(float& x, float& y) = 0;
		virtual void DragController_Begin() {}
		virtual void DragController_End() {}
		virtual void DragController_SetScale(float s) {}
	};
	VERUS_TYPEDEFS(DragControllerDelegate);

	class DragController
	{
		PDragControllerDelegate _pDelegate = nullptr;
		float                   _originX = 0;
		float                   _originY = 0;
		int                     _originCoordX = 0;
		int                     _originCoordY = 0;
		float                   _ratioX = 1;
		float                   _ratioY = 1;
		bool                    _holding = false;

	public:
		void SetDelegate(PDragControllerDelegate p);

		void Begin(int x, int y);
		bool DragTo(int x, int y);
		bool DragBy(int x, int y);
		void End();

		bool IsHolding() const { return _holding; }

		void SetScale(float s);
	};
	VERUS_TYPEDEFS(DragController);
}
