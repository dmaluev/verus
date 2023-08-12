// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	template<typename T>
	class DynamicBuffer : public GeometryPwn
	{
		Vector<T> _vVB;
		int       _maxVerts = 1024;
		int       _vertCount = 0;
		int       _firstVertex = 0;

	public:
		DynamicBuffer()
		{
		}

		~DynamicBuffer()
		{
		}

		void Init(RcGeometryDesc desc, int maxVerts = 1024)
		{
			_maxVerts = maxVerts;
			GeometryDesc dynDesc(desc);
			dynDesc._dynBindingsMask = 0x1;
			GeometryPwn::Init(dynDesc);
		}

		void CreateVertexBuffer()
		{
			_vVB.resize(_maxVerts);
			(*this)->CreateVertexBuffer(_maxVerts, 0);
		}

		void Reset()
		{
			_vertCount = 0;
			_firstVertex = 0;
		}

		void Begin()
		{
			_firstVertex = _vertCount;
		}

		void End(BaseCommandBuffer* pCB = nullptr)
		{
			VERUS_QREF_RENDERER;
			const int vertCount = _vertCount - _firstVertex;
			if (vertCount)
			{
				if (!pCB)
					pCB = renderer.GetCommandBuffer().Get();
				(*this)->UpdateVertexBuffer(&_vVB[_firstVertex], 0, pCB, vertCount, _firstVertex);
				pCB->Draw(vertCount, 1, _firstVertex);
			}
		}

		bool Add(const T& v)
		{
			if (_vertCount + 1 > _maxVerts)
				return false;
			_vVB[_vertCount++] = v;
			return true;
		}

		bool AddQuad(
			const T& a0,
			const T& a1,
			const T& b0,
			const T& b1)
		{
			if (_vertCount + 6 > _maxVerts)
				return false;
			_vVB[_vertCount++] = a0;
			_vVB[_vertCount++] = b0;
			_vVB[_vertCount++] = a1;
			_vVB[_vertCount++] = a1;
			_vVB[_vertCount++] = b0;
			_vVB[_vertCount++] = b1;
			return true;
		}
	};
}
