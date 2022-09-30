// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// Managed by DeferredShading:
		class DeferredLights
		{
			Mesh _meshDir;
			Mesh _meshOmni;
			Mesh _meshSpot;

		public:
			RMesh Get(CGI::LightType type)
			{
				switch (type)
				{
				case CGI::LightType::omni: return _meshOmni;
				case CGI::LightType::spot: return _meshSpot;
				}
				return _meshDir;
			}
		};
		VERUS_TYPEDEFS(DeferredLights);

		class WorldUtils : public Singleton<WorldUtils>, public Object
		{
		private:
			DeferredLights _deferredLights;
			bool           _editorMode = false;

		public:
			WorldUtils();
			~WorldUtils();

			void Init();
			void Done();

			RDeferredLights GetDeferredLights() { return _deferredLights; }

			bool IsEditorMode() const { return _editorMode; }
			void SetEditorMode(bool b = true) { _editorMode = b; }
		};
		VERUS_TYPEDEFS(WorldUtils);
	}
}
