#pragma once

namespace verus
{
	namespace Scene
	{
		//! Model is an element of the scene manager container
		//! * has a mesh
		//! * has a material
		//! * has generic parameters
		class Model : public Object
		{
			Mesh           _mesh;
			MaterialPwn    _material;
			IO::Dictionary _dict;
			int            _refCount = 0;

		public:
			struct Desc
			{
				CSZ _url = nullptr;
				CSZ _mat = nullptr;

				Desc(CSZ url = nullptr) : _url(url) {}
			};
			VERUS_TYPEDEFS(Desc);

			Model();
			~Model();

			void Init(RcDesc desc);
			bool Done();

			bool IsLoaded() const { return _mesh.IsLoaded(); }

			void AddRef() { _refCount++; }
			int GetRefCount() const { return _refCount; }

			void MarkFirstInstance();
			void Draw(CGI::CommandBufferPtr cb);
			void BindPipeline(CGI::CommandBufferPtr cb);
			void BindGeo(CGI::CommandBufferPtr cb);
			void PushInstance(RcTransform3 matW, RcVector4 instData);

			RMesh GetMesh() { return _mesh; }
			MaterialPtr GetMaterial() { return _material; }
		};
		VERUS_TYPEDEFS(Model);

		class ModelPtr : public Ptr<Model>
		{
		public:
			void Init(Model::RcDesc desc);
		};
		VERUS_TYPEDEFS(ModelPtr);

		class ModelPwn : public ModelPtr
		{
		public:
			~ModelPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(ModelPwn);
	}
}
