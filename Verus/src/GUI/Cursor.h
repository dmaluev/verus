// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace GUI
	{
		class Cursor : public Object
		{
			World::TexturePwn _tex;
			CGI::CSHandle     _csh;
			float             _x = 0.5f;
			float             _y = 0.5f;
			float             _hotspotX = 0;
			float             _hotspotY = 0;

		public:
			Cursor();
			~Cursor();

			void Init();
			void Done();

			void Draw();

			void MoveHotspotTo(float x, float y);
			void MoveBy(float x, float y);

			float GetX() const { return _x; }
			float GetY() const { return _y; }
		};
		VERUS_TYPEDEFS(Cursor);
	}
}
