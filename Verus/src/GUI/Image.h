// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace GUI
	{
		class Image : public Widget
		{
			World::TexturePwn _tex;
			CGI::CSHandle     _csh;
			Vector4           _tcScale = Vector4(1, 1);
			Linear<Vector4>   _tcBias;
			Vector4           _tcScaleMask = Vector4(1, 1);
			Vector4           _tcBiasMask = Vector4(0, 0);
			bool              _useMask = false;
			bool              _solidColor = false;
			bool              _add = false;

		public:
			Image();
			virtual ~Image();

			static PWidget Make();

			virtual void Update() override;
			virtual void Draw() override;
			virtual void Parse(pugi::xml_node node) override;

			void SetBiasX(float a) { _tcBias.GetValue().setX(a); }
			void SetBiasY(float a) { _tcBias.GetValue().setY(a); }
		};
		VERUS_TYPEDEFS(Image);
	}
}
