// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	// Managed by DeferredShading:
	class DeferredShadingMeshes
	{
		Mesh _meshDir;
		Mesh _meshOmni;
		Mesh _meshSpot;
		Mesh _meshBox;

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
		RMesh GetBox()
		{
			return _meshBox;
		}
	};
	VERUS_TYPEDEFS(DeferredShadingMeshes);

	class WorldUtils : public Singleton<WorldUtils>, public Object
	{
	private:
		DeferredShadingMeshes _deferredShadingMeshes;
		bool                  _editorMode = false;

	public:
		WorldUtils();
		~WorldUtils();

		void Init();
		void Done();

		RDeferredShadingMeshes GetDeferredShadingMeshes() { return _deferredShadingMeshes; }

		bool IsEditorMode() const { return _editorMode; }
		void SetEditorMode(bool b = true) { _editorMode = b; }
	};
	VERUS_TYPEDEFS(WorldUtils);
}
