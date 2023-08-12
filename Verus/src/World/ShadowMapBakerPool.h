// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	class ShadowMapBakerPool : public Singleton<ShadowMapBakerPool>, public Object
	{
		enum TEX
		{
			TEX_DEPTH,
			TEX_2CH,
			TEX_COUNT
		};

		static const int s_blockSide = 4;

		class Block : public Object
		{
			CGI::TexturePwn _tex;
			CGI::FBHandle   _fbhPong;
			CGI::CSHandle   _csh;
			UINT64          _doneFrame = 0;
			UINT16          _reserved = 0;
			UINT16          _baked = 0;
			float           _presence[s_blockSide * s_blockSide];

		public:
			Block();
			~Block();

			void Init(int side);
			void Done();

			void Update();

			bool HasFreeCells() const;
			int FindFreeCell() const;
			bool IsUnused() const;
			bool IsReserved(int cellIndex) const;
			void Reserve(int cellIndex);
			void Free(int cellIndex);
			void MarkBaked(int cellIndex);

			CGI::TexturePtr GetTexture() const { return _tex; }
			CGI::FBHandle GetFramebufferHandle() const { return _fbhPong; }
			CGI::CSHandle GetComplexSetHandle() const { return _csh; }
			float GetPresence(int cellIndex) const { return _presence[cellIndex]; }

			UINT64 GetDoneFrame() const { return _doneFrame; }
		};
		VERUS_TYPEDEFS(Block);

		Vector<Block>               _vBlocks;
		CGI::TexturePwns<TEX_COUNT> _tex;
		CGI::FBHandle               _fbh;
		CGI::FBHandle               _fbhPing;
		CGI::CSHandle               _cshPing;
		CGI::CSHandle               _cshPong;
		MainCamera                  _passCamera;
		PCamera                     _pPrevPassCamera = nullptr;
		PMainCamera                 _pPrevHeadCamera = nullptr;
		ShadowMapHandle             _activeBlockHandle;
		int                         _side = 0;
		bool                        _baking = false;

	public:
		ShadowMapBakerPool();
		~ShadowMapBakerPool();

		void Init(int side = 512);
		void Done();

		void Update();

		// <Bake>
		void Begin(RLightNode lightNode);
		void End();
		bool IsBaking() const { return _baking; }
		// </Bake>

		Matrix4 GetOffsetMatrix(ShadowMapHandle handle) const;

		CGI::CSHandle GetComplexSetHandle(ShadowMapHandle handle) const;
		float GetPresence(ShadowMapHandle handle) const;

		ShadowMapHandle ReserveCell();
		void FreeCell(ShadowMapHandle handle);

		void GarbageCollection();
	};
	VERUS_TYPEDEFS(ShadowMapBakerPool);
}
