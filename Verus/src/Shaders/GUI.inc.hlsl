// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_Gui
{
	matrix _matWVP;
	mataff _matTex;
	float4 _tcScaleBias;
	float4 _tcMaskScaleBias;
};

VERUS_UBUFFER_STRUCT UB_GuiFS
{
	float4 _color;
};
