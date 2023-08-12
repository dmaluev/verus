// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	class AmbientNode : public BaseNode
	{
	public:
		enum class Priority : int
		{
			mulSky,
			add,
			mul
		};

	private:
		Transform3 _trToBoxSpace = Transform3::identity();
		Vector4    _color = Vector4(0, 0, 0, 1);
		float      _wall = 0.5f;
		float      _cylindrical = 0;
		float      _spherical = 0;
		Priority   _priority = Priority::mulSky;

	public:
		struct Desc : BaseNode::Desc
		{
			Desc(CSZ name = nullptr) : BaseNode::Desc(name) {}
		};
		VERUS_TYPEDEFS(Desc);

		AmbientNode();
		virtual ~AmbientNode();

		void Init(RcDesc desc);
		void Done();

		virtual void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication) override;

		virtual void DrawEditorOverlays(DrawEditorOverlaysFlags flags) override;

		// <Editor>
		virtual void GetEditorCommands(Vector<EditorCommand>& v) override;
		virtual void ExecuteEditorCommand(RcEditorCommand command) override;
		// </Editor>

		virtual void UpdateBounds() override;

		virtual void OnNodeTransformed(PBaseNode pNode, bool afterEvent) override;

		virtual void Serialize(IO::RSeekableStream stream) override;
		virtual void Deserialize(IO::RStream stream) override;

		Priority GetPriority() const;
		void SetPriority(Priority priority);
		Vector3 GetColor() const;
		void SetColor(RcVector3 color);
		float GetIntensity() const;
		void SetIntensity(float i);
		float GetWall() const;
		void SetWall(float wall);
		float GetCylindrical() const;
		void SetCylindrical(float x);
		float GetSpherical() const;
		void SetSpherical(float x);
		Vector4 GetInstData() const;

		RcTransform3 GetToBoxSpaceTransform() const { return _trToBoxSpace; }

		void FitParentBlock();
		void FillTheRoom();
	};
	VERUS_TYPEDEFS(AmbientNode);

	class AmbientNodePtr : public Ptr<AmbientNode>
	{
	public:
		void Init(AmbientNode::RcDesc desc);
		void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication);
	};
	VERUS_TYPEDEFS(AmbientNodePtr);

	class AmbientNodePwn : public AmbientNodePtr
	{
	public:
		~AmbientNodePwn() { Done(); }
		void Done();
	};
	VERUS_TYPEDEFS(AmbientNodePwn);
}
