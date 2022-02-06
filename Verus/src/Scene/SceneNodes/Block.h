// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class BlockLight
		{
		public:
			Transform3 _tr = Transform3::identity();
			LightPwn   _light;
		};
		VERUS_TYPEDEFS(BlockLight);

		class BlockEmitter
		{
		public:
			Transform3 _tr = Transform3::identity();
			EmitterPwn _emitter;
		};
		VERUS_TYPEDEFS(BlockEmitter);

		// Block is a scene node which has a model and material info.
		// Most blocks form the scene's so called static or immovable geometry.
		class Block : public SceneNode
		{
			Vector4              _userColor = Vector4(0);
			Vector<BlockLight>   _vLights;
			Vector<BlockEmitter> _vEmitters;
			ModelPwn             _model;
			MaterialPwn          _material;
			int                  _matIndex = 0;
			bool                 _collide = true;
			bool                 _async_loadedModel = false;

		public:
			struct Desc
			{
				pugi::xml_node _node;
				CSZ            _name = nullptr;
				CSZ            _model = nullptr;
				CSZ            _modelMat = nullptr;
				CSZ            _blockMat = nullptr;
				int            _matIndex = 0;
				bool           _collide = true;
			};
			VERUS_TYPEDEFS(Desc);

			Block();
			~Block();

			void Init(RcDesc desc);
			void Done();

			VERUS_P(void LoadExtra(SZ xml));

			virtual void Update() override;

			virtual String GetUrl() override { return _C(_model->GetMesh().GetUrl()); }

			bool IsLoadedModel() const { return _model->IsLoaded(); }

			virtual void SetColor(RcVector4 color) override { _userColor = color; }
			virtual Vector4 GetColor() override { return _userColor; }
			ModelPtr GetModel() { return _model; }
			MaterialPtr GetMaterial(bool orModelMat = true);
			int GetMaterialIndex() const { return _matIndex; }

			virtual void UpdateBounds() override;

			// Serialization:
			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;
			virtual void SaveXML(pugi::xml_node node) override;
			virtual void LoadXML(pugi::xml_node node) override;
		};
		VERUS_TYPEDEFS(Block);

		class BlockPtr : public Ptr<Block>
		{
		public:
			void Init(Block::RcDesc desc);
		};
		VERUS_TYPEDEFS(BlockPtr);

		class BlockPwn : public BlockPtr
		{
		public:
			~BlockPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(BlockPwn);
	}
}
