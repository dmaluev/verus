// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// ModelNode stores mesh and material (aka model), which can be used by a BlockNode.
		// * has a mesh
		// * has a material
		class ModelNode : public BaseNode
		{
			Mesh        _mesh;
			MaterialPwn _material;
			int         _refCount = 0;

		public:
			struct Desc : BaseNode::Desc
			{
				CSZ _url = nullptr;
				CSZ _materialURL = nullptr;

				Desc(CSZ url = nullptr) : _url(url) {}
			};
			VERUS_TYPEDEFS(Desc);

			ModelNode();
			virtual ~ModelNode();

			void AddRef() { _refCount++; }
			int GetRefCount() const { return _refCount; }

			void Init(RcDesc desc);
			bool Done();

			virtual void GetEditorCommands(Vector<EditorCommand>& v) override;

			virtual bool CanSetParent(PBaseNode pNode) const override;

			virtual void AddDefaultPickingBody() override {}

			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;
			void Deserialize_LegacyXXX(IO::RStream stream, CSZ url);

			// <Resources>
			bool IsLoaded() const { return _mesh.IsLoaded(); }
			void LoadMaterial(CSZ url);
			Str GetURL() const { return _mesh.GetURL(); }

			RMesh GetMesh() { return _mesh; }
			MaterialPtr GetMaterial() { return _material; }
			// </Resources>

			void BindPipeline(CGI::CommandBufferPtr cb);
			void BindPipelineSimple(DrawSimpleMode mode, CGI::CommandBufferPtr cb);
			void BindGeo(CGI::CommandBufferPtr cb);
			void MarkInstance();
			void PushInstance(RcTransform3 matW, RcVector4 instData);
			void Draw(CGI::CommandBufferPtr cb);
		};
		VERUS_TYPEDEFS(ModelNode);

		class ModelNodePtr : public Ptr<ModelNode>
		{
		public:
			void Init(ModelNode::RcDesc desc);
		};
		VERUS_TYPEDEFS(ModelNodePtr);

		class ModelNodePwn : public ModelNodePtr
		{
		public:
			~ModelNodePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(ModelNodePwn);
	}
}
