#pragma once

namespace verus
{
	namespace Scene
	{
		class BaseMesh : public Object, public IO::AsyncCallback, public AllocatorAware
		{
		protected:
			struct VertexStream0 // 16 bytes, common.
			{
				short _pos[4];
				short _tc0[2];
				char  _nrm[4];
			};

			struct VertexStream1 // 16 bytes, skinned mesh.
			{
				short _bw[4];
				short _bi[4];
			};

			struct VertexStream2 // 16 bytes, per-pixel lighting / normal maps.
			{
				short _tan[4];
				short _bin[4];
			};

			struct VertexStream3 // 8 bytes, static light maps / extra color.
			{
				short _tc1[2];
				BYTE  _clr[4];
			};

			Vector<UINT16>          _vIndices;
			Vector<UINT32>          _vIndices32;
			Vector<VertexStream0>   _vStream0;
			Vector<VertexStream1>   _vStream1;
			Vector<VertexStream2>   _vStream2;
			Vector<VertexStream3>   _vStream3;
			String                  _url;
			btBvhTriangleMeshShape* _pShape = nullptr;
			int                     _numVerts = 0;
			int                     _numFaces = 0;
			int                     _numBones = 0;
			float                   _posDeq[6];
			float                   _tc0Deq[4];
			float                   _tc1Deq[4];
			bool                    _loadKinectBindPose = false;
			bool                    _loadOnly = false;
			bool                    _rigidSkeleton = false;
			bool                    _async_initShape = false;

		public:
			BaseMesh();
			virtual ~BaseMesh();

			void Init(CSZ url);
			void Done();

			Str GetUrl() const { return _C(_url); }

			int GetNumVerts() const { return _numVerts; }
			int GetNumFaces() const { return _numFaces; }
			int GetNumBones() const { return _numBones; }

			virtual void Async_Run(CSZ url, RcBlob blob) override;

			VERUS_P(void Load(RcBlob blob));
			VERUS_P(void LoadX3D3(RcBlob blob));
			bool IsLoaded() const { return !!_numVerts; }

			// Load extra:
			VERUS_P(void LoadPrimaryBones());
			VERUS_P(void LoadRig());
			VERUS_P(void LoadMimic());
			VERUS_P(void LoadKinectBindPose());

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

			void ComputeTangentSpace();

			// GPU:
			virtual void CreateDeviceBuffers() {}
			virtual void BufferDataVB(const void* p, int streamID) {}

			// Physics:
			btBvhTriangleMeshShape* GetShape() const { return _pShape; }
			btBvhTriangleMeshShape* InitShape(RcTransform3 tr, CSZ url = nullptr);
			void DoneShape();

			// Bounds:
			void GetBounds(RPoint3 mn, RPoint3 mx) const;
			Math::Bounds GetBounds() const;

			void VisitVertices(std::function<Continue(RcPoint3, int)> fn);

			String ToXmlString() const;
			String ToObjString() const;
		};
	}
}