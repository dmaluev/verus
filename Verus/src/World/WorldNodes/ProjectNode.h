// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	class ProjectNode : public BaseNode
	{
		Transform3    _trToBoxSpace = Transform3::identity();
		Vector4       _levels = Vector4::Replicate(1000);
		String        _url;
		TexturePwn    _tex;
		CGI::CSHandle _csh;

	public:
		struct Desc : BaseNode::Desc
		{
			CSZ _url = nullptr;

			Desc(CSZ url = nullptr) : _url(url) {}
		};
		VERUS_TYPEDEFS(Desc);

		ProjectNode();
		virtual ~ProjectNode();

		void Init(RcDesc desc);
		void Done();

		virtual void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication) override;

		virtual void Update() override;
		virtual void DrawEditorOverlays(DrawEditorOverlaysFlags flags) override;

		virtual void UpdateBounds() override;

		virtual void OnNodeTransformed(PBaseNode pNode, bool afterEvent) override;

		virtual void Serialize(IO::RSeekableStream stream) override;
		virtual void Deserialize(IO::RStream stream) override;

		// <Resources>
		void LoadTexture(CSZ url);
		void RemoveTexture();
		Str GetURL() const { return _C(_url); }
		CGI::CSHandle GetComplexSetHandle() const { return _csh; }
		// </Resources>

		RcVector4 GetLevels() const { return _levels; }
		void SetLevels(RcVector4 levels) { _levels = levels; }
		RcTransform3 GetToBoxSpaceTransform() const { return _trToBoxSpace; }
	};
	VERUS_TYPEDEFS(ProjectNode);

	class ProjectNodePtr : public Ptr<ProjectNode>
	{
	public:
		void Init(ProjectNode::RcDesc desc);
		void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication);
	};
	VERUS_TYPEDEFS(ProjectNodePtr);

	class ProjectNodePwn : public ProjectNodePtr
	{
	public:
		~ProjectNodePwn() { Done(); }
		void Done();
	};
	VERUS_TYPEDEFS(ProjectNodePwn);
}
