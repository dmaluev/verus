// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_Gui
{
	mataff _matW;
	mataff _matV;
	float4 _tcScaleBias;
	float4 _tcMaskScaleBias;
};

VERUS_UBUFFER UB_GuiFS
{
	float4 _color;
};
