// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class Site;

		class Draft
		{
			Draft* _pParent = nullptr;
			PSceneNode _p = nullptr;
			BlockPwn   _block;
			LightPwn   _light;
			PrefabPwn  _prefab;

		public:
			struct Desc
			{
				pugi::xml_node _node;
				Draft* _pParent = nullptr;
				CSZ            _url = nullptr;
				int            _matIndex = 0;
				bool           _collide = false;
			};
			VERUS_TYPEDEFS(Desc);

			Draft();
			~Draft();

			void Init(Site* p, RcDesc desc);
			void Done();

			Draft* GetParent() { return _pParent; }
			PSceneNode Get() { return _p; }
			BlockPtr GetBlock() { return _block; }
			LightPtr GetLight() { return _light; }
			PrefabPtr GetPrefab() { return _prefab; }
		};
		VERUS_TYPEDEFS(Draft);

		class DraftPtr : public Ptr<Draft>
		{
		public:
			void Init(Site* p, Draft::RcDesc desc);
			void Done(Site* p);
		};
		VERUS_TYPEDEFS(DraftPtr);

		class SiteParams
		{
		public:
			Vector3  _offset = Vector3(0);
			PTerrain _pTerrain = nullptr;
			float    _radius = 0;
			float    _turn = 0;
			bool     _align = true;
			bool     _tilt = false;
		};
		VERUS_TYPEDEFS(SiteParams);

		class SiteDelegate
		{
		public:
			virtual Transform3 Site_GetTransform(RcSiteParams params) = 0;
			virtual Vector3 Site_TransformOffset(RcVector3 offset) = 0;
		};
		VERUS_TYPEDEFS(SiteDelegate);

		typedef Store<Draft> TStoreDrafts;
		class Site : public SceneNode, private TStoreDrafts
		{
			PSiteDelegate _pDelegate = nullptr;
			DraftPtr      _activeDraft;
			DraftPtr      _linkedDraft;
			String        _prevUrl;
			int           _prevMatIndex = 0;
			int           _refCount = 0;
			SiteParams    _params;

		public:
			struct Desc
			{
				CSZ _name = nullptr;
			};
			VERUS_TYPEDEFS(Desc);

			Site();
			~Site();

			void Init(RcDesc desc);
			bool Done();

			void AddRef() { _refCount++; }

			virtual void Update() override;
			virtual void Draw() override;
			virtual void DrawImmediate() override;

			// Drafts:
			PDraft InsertDraft();
			void DeleteDraft(PDraft p);
			void DeleteAllDrafts();
			DraftPtr GetActiveDraft() const { return _activeDraft; }
			DraftPtr GetLinkedDraft() const { return _linkedDraft; }

			void Save();
			void Load(CSZ pathname = nullptr);

			// Commands:
			void Add(CSZ url, int matIndex); // +
			void Delete(); // -
			void Edit(bool link = false); // !
			void Duplicate(); // *
			void Put(); // =
			PSceneNode FindNearest(PDraftPtr pAttach = nullptr);

			void Dir(RcVector3 dir);
			void DirFromPoint(float radiusScale = 0);
			void ConeFromPoint(bool coneIn = false);

			void Commit();

			PSiteDelegate SetDelegate(PSiteDelegate p);

			// Parameters:
			void SetTerrain(PTerrain p) { _params._pTerrain = p; }
			RcVector3 GetOffset() const { return _params._offset; }
			void SetOffset(RcVector3 v) { _params._offset = v; }
			float GetRadius() const { return _params._radius; }
			void SetRadius(float x) { _params._radius = x; }
			float GetTurn() const { return _params._turn; }
			void SetTurn(float x) { _params._turn = x; }
			bool GetAlign() const { return _params._align; }
			void SetAlign(bool b) { _params._align = b; }
			bool GetTilt() const { return _params._tilt; }
			void SetTilt(bool b) { _params._tilt = b; }
		};
		VERUS_TYPEDEFS(Site);

		class SitePtr : public Ptr<Site>
		{
		public:
			void Init(Site::RcDesc desc);
		};
		VERUS_TYPEDEFS(SitePtr);

		class SitePwn : public SitePtr
		{
		public:
			~SitePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(SitePwn);
	}
}
