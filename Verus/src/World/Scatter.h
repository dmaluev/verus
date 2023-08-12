// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	struct ScatterDelegate
	{
		virtual void Scatter_AddInstance(const int ij[2], int type, float x, float z,
			float scale, float angle, UINT32 r) = 0;
	};
	VERUS_TYPEDEFS(ScatterDelegate);

	class Scatter : public Object, public Math::QuadtreeIntegralDelegate
	{
	public:
		class Instance
		{
		public:
			int    _type = 0;
			float  _x = 0;
			float  _z = 0;
			float  _scale = 1;
			float  _angle = 0;
			UINT32 _rand = 0;
		};
		VERUS_TYPEDEFS(Instance);

	private:
		PScatterDelegate _pDelegate = nullptr;
		Vector<Instance> _vInstances;
		float            _maxDistSq = FLT_MAX;
		int              _side = 0;
		int              _shift = 0;

	public:
		class TypeDesc
		{
		public:
			int   _type = 0;
			int   _permille = 0;
			float _minOffset = 0.25f;
			float _maxOffset = 0.75f;
			float _minScale = 0.8f;
			float _maxScale = 1.25f;
			float _minAngle = 0;
			float _maxAngle = VERUS_2PI;
			TypeDesc(int type, int permille) : _type(type), _permille(permille) {}
		};
		VERUS_TYPEDEFS(TypeDesc);

		Scatter();
		~Scatter();

		void Init(int side, int typeCount, PcTypeDesc pTypes, int seed = 192000);
		void Done();

		void SetMaxDist(float dist) { _maxDistSq = dist * dist; }

		PScatterDelegate SetDelegate(PScatterDelegate p) { return Utils::Swap(_pDelegate, p); }

		virtual void QuadtreeIntegral_OnElementDetected(const short ij[2], RcPoint3 center) override;
		virtual void QuadtreeIntegral_GetHeights(const short ij[2], float height[2]) override;

		RcInstance GetInstanceAt(const int ij[2]) const;
	};
	VERUS_TYPEDEFS(Scatter);
}
