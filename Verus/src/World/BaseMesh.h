// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	class BaseMesh : public Object, public IO::AsyncDelegate, public AllocatorAware
	{
	protected:
		struct VertexInputBinding0 // 16 bytes, common.
		{
			short _pos[4];
			short _tc0[2];
			char  _nrm[4];
		};
		VERUS_TYPEDEFS(VertexInputBinding0);

		struct VertexInputBinding1 // 16 bytes, skinned mesh.
		{
			short _bw[4];
			short _bi[4];
		};
		VERUS_TYPEDEFS(VertexInputBinding1);

		struct VertexInputBinding2 // 16 bytes, per-pixel lighting / normal maps.
		{
			short _tan[4];
			short _bin[4];
		};
		VERUS_TYPEDEFS(VertexInputBinding2);

		struct VertexInputBinding3 // 8 bytes, static light maps / extra color.
		{
			short _tc1[2];
			BYTE  _clr[4];
		};
		VERUS_TYPEDEFS(VertexInputBinding3);

		Vector<UINT16>              _vIndices;
		Vector<UINT32>              _vIndices32;
		Vector<VertexInputBinding0> _vBinding0;
		Vector<VertexInputBinding1> _vBinding1;
		Vector<VertexInputBinding2> _vBinding2;
		Vector<VertexInputBinding3> _vBinding3;
		Anim::Skeleton              _skeleton;
		Anim::Warp                  _warp;
		String                      _warpURL;
		String                      _url;
		btBvhTriangleMeshShape* _pShape = nullptr;
		int                         _vertCount = 0;
		int                         _faceCount = 0;
		int                         _indexCount = 0;
		int                         _boneCount = 0;
		float                       _posDeq[6];
		float                       _tc0Deq[4];
		float                       _tc1Deq[4];
		bool                        _loadOnly = false;
		bool                        _robotic = false;
		bool                        _initShape = false;

	public:
		struct SourceBuffers
		{
			Vector<UINT16>    _vIndices;
			Vector<glm::vec3> _vPos;
			Vector<glm::vec2> _vTc0;
			Vector<glm::vec3> _vNrm;
			bool              _recalculateTangentSpace = true;
		};
		VERUS_TYPEDEFS(SourceBuffers);

		BaseMesh();
		virtual ~BaseMesh();

		void Init(CSZ url);
		void Init(RcSourceBuffers sourceBuffers);
		void Done();

		Str GetURL() const { return _C(_url); }

		int GetVertCount() const { return _vertCount; }
		int GetFaceCount() const { return _faceCount; }
		int GetIndexCount() const { return _indexCount; }
		int GetBoneCount() const { return _boneCount; }

		virtual void Async_WhenLoaded(CSZ url, RcBlob blob) override;

		VERUS_P(void Load(RcBlob blob));
		VERUS_P(void LoadX3D3(RcBlob blob));
		bool IsLoaded() const { return !!_vertCount; }

		// Load extra:
		VERUS_P(void LoadRig());
		VERUS_P(void LoadWarp());

		// Data:
		const UINT16* GetIndices() const { return _vIndices.data(); }
		const UINT32* GetIndices32() const { return _vIndices32.data(); }
		PcVertexInputBinding0 GetVertexInputBinding0() const { return _vBinding0.data(); }
		PcVertexInputBinding1 GetVertexInputBinding1() const { return _vBinding1.data(); }
		PcVertexInputBinding2 GetVertexInputBinding2() const { return _vBinding2.data(); }
		PcVertexInputBinding3 GetVertexInputBinding3() const { return _vBinding3.data(); }

		// Quantization / dequantization:
		static void ComputeDeq(glm::vec3& scale, glm::vec3& bias, const glm::vec3& extents, const glm::vec3& minPos);
		static void QuantizeV(glm::vec3& v, const glm::vec3& extents, const glm::vec3& minPos);
		static void DequantizeUsingDeq2D(const short* in, const float* deq, glm::vec2& out);
		static void DequantizeUsingDeq2D(const short* in, const float* deq, RPoint3 out);
		static void DequantizeUsingDeq3D(const short* in, const float* deq, glm::vec3& out);
		static void DequantizeUsingDeq3D(const short* in, const float* deq, RPoint3 out);
		static void DequantizeNrm(const char* in, glm::vec3& out);
		static void DequantizeNrm(const char* in, RVector3 out);
		static void DequantizeTanBin(const short* in, RVector3 out);
		void  CopyPosDeqScale(float* p) const { memcpy(p, _posDeq + 0, sizeof(float) * 3); }
		void   CopyPosDeqBias(float* p) const { memcpy(p, _posDeq + 3, sizeof(float) * 3); }
		void CopyTexCoord0Deq(float* p) const { memcpy(p, _tc0Deq, sizeof(float) * 4); }
		void CopyTexCoord1Deq(float* p) const { memcpy(p, _tc1Deq, sizeof(float) * 4); }

		void RecalculateTangentSpace();

		// GPU:
		virtual void CreateDeviceBuffers() {}
		virtual void UpdateVertexBuffer(const void* p, int binding) {}
		int GetPipelineIndex(bool instanced = false) const;

		// Physics:
		btBvhTriangleMeshShape* GetShape() const { return _pShape; }
		btBvhTriangleMeshShape* InitShape(RcTransform3 tr, CSZ url = nullptr);
		void DoneShape();

		// Bounds:
		void GetBounds(RPoint3 mn, RPoint3 mx) const;
		Math::Bounds GetBounds() const;

		template<typename T>
		void ForEachVertex(const T& fn, bool boneIndices = false) const
		{
			VERUS_FOR(i, _vertCount)
			{
				Point3 pos, tc;
				Vector3 nrm;
				DequantizeUsingDeq3D(_vBinding0[i]._pos, _posDeq, pos);
				DequantizeNrm(_vBinding0[i]._nrm, nrm);
				DequantizeUsingDeq2D(_vBinding0[i]._tc0, _tc0Deq, tc);
				if (_vBinding1.empty() || !boneIndices)
				{
					if (Continue::no == fn(i, pos, nrm, tc))
						break;
				}
				else
				{
					bool done = false;
					VERUS_FOR(j, 4)
					{
						if (_vBinding1[i]._bw[j] > 0)
						{
							if (Continue::no == fn(_vBinding1[i]._bi[j], pos, nrm, tc))
							{
								done = true;
								break;
							}
						}
					}
					if (done)
						break;
				}
			}
		}

		Anim::RSkeleton GetSkeleton() { return _skeleton; }
		Anim::RcSkeleton GetSkeleton() const { return _skeleton; }
		Anim::RWarp GetWarp() { return _warp; }

		String ToXmlString() const;
		String ToObjString() const;
	};
	VERUS_TYPEDEFS(BaseMesh);
}
