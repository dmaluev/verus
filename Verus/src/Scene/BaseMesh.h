#pragma once

namespace verus
{
	namespace Scene
	{
		class BaseMesh : public Object, public IO::AsyncCallback, public AllocatorAware
		{
		protected:
			struct VertexInputBinding0 // 16 bytes, common.
			{
				short _pos[4];
				short _tc0[2];
				char  _nrm[4];
			};

			struct VertexInputBinding1 // 16 bytes, skinned mesh.
			{
				short _bw[4];
				short _bi[4];
			};

			struct VertexInputBinding2 // 16 bytes, per-pixel lighting / normal maps.
			{
				short _tan[4];
				short _bin[4];
			};

			struct VertexInputBinding3 // 8 bytes, static light maps / extra color.
			{
				short _tc1[2];
				BYTE  _clr[4];
			};

			Vector<UINT16>              _vIndices;
			Vector<UINT32>              _vIndices32;
			Vector<VertexInputBinding0> _vBinding0;
			Vector<VertexInputBinding1> _vBinding1;
			Vector<VertexInputBinding2> _vBinding2;
			Vector<VertexInputBinding3> _vBinding3;
			Anim::Skeleton              _skeleton;
			Anim::Warp                  _warp;
			String                      _warpUrl;
			String                      _url;
			int                         _vertCount = 0;
			int                         _faceCount = 0;
			int                         _indexCount = 0;
			int                         _boneCount = 0;
			float                       _posDeq[6];
			float                       _tc0Deq[4];
			float                       _tc1Deq[4];
			bool                        _loadOnly = false;
			bool                        _rigidSkeleton = false;

		public:
			BaseMesh();
			virtual ~BaseMesh();

			void Init(CSZ url);
			void Done();

			Str GetUrl() const { return _C(_url); }

			int GetVertCount() const { return _vertCount; }
			int GetFaceCount() const { return _faceCount; }
			int GetIndexCount() const { return _indexCount; }
			int GetBoneCount() const { return _boneCount; }

			virtual void Async_Run(CSZ url, RcBlob blob) override;

			VERUS_P(void Load(RcBlob blob));
			VERUS_P(void LoadX3D3(RcBlob blob));
			bool IsLoaded() const { return !!_vertCount; }

			// Load extra:
			VERUS_P(void LoadPrimaryBones());
			VERUS_P(void LoadRig());
			VERUS_P(void LoadWarp());

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

			void ComputeTangentSpace();

			// GPU:
			virtual void CreateDeviceBuffers() {}
			virtual void UpdateVertexBuffer(const void* p, int binding) {}

			// Bounds:
			void GetBounds(RPoint3 mn, RPoint3 mx) const;
			Math::Bounds GetBounds() const;

			template<typename T>
			void ForEachVertex(const T& fn, bool matIndices = false)
			{
				VERUS_FOR(i, _vertCount)
				{
					Point3 pos, uv;
					DequantizeUsingDeq3D(_vBinding0[i]._pos, _posDeq, pos);
					DequantizeUsingDeq2D(_vBinding0[i]._tc0, _tc0Deq, uv);
					if (_vBinding1.empty() || !matIndices)
					{
						if (Continue::no == fn(pos, -1, uv))
							break;
					}
					else
					{
						bool done = false;
						VERUS_FOR(j, 4)
						{
							if (_vBinding1[i]._bw[j] > 0)
							{
								if (Continue::no == fn(pos, _vBinding1[i]._bi[j], uv))
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
}
