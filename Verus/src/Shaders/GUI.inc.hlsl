// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

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
