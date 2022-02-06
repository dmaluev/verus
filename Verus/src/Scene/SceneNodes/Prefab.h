// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class Fragment
		{
		public:
			Transform3 _tr = Transform3::identity();
			BlockPwn   _block;
		};
		VERUS_TYPEDEFS(Fragment);

		class Prefab : public SceneNode
		{
			Vector<Fragment> _vFragments;
			String           _url;
			bool             _collide = true;
			bool             _async_loadedAllBlocks = false;

		public:
			struct Desc
			{
				pugi::xml_node _node;
				CSZ            _url = nullptr;
				bool           _collide = true;
			};
			VERUS_TYPEDEFS(Desc);

			Prefab();
			~Prefab();

			void Init(RcDesc desc);
			void Done();

			VERUS_P(void LoadPrefab(SZ xml));

			virtual void Update() override;
			virtual void Draw() override;

			virtual String GetUrl() override { return _url; }

			virtual Vector4 GetColor() override;
			virtual void SetColor(RcVector4 color) override;

			virtual void UpdateBounds() override;

			// Physics engine:
			virtual void SetCollisionGroup(Physics::Group g = Physics::Group::immovable) override;
			virtual void MoveRigidBody() override;

			// Serialization:
			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;
			virtual void SaveXML(pugi::xml_node node) override;
			virtual void LoadXML(pugi::xml_node node) override;
		};
		VERUS_TYPEDEFS(Prefab);

		class PrefabPtr : public Ptr<Prefab>
		{
		public:
			void Init(Prefab::RcDesc desc);
		};
		VERUS_TYPEDEFS(PrefabPtr);

		class PrefabPwn : public PrefabPtr
		{
		public:
			~PrefabPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(PrefabPwn);
	}
}
