// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// PathNode defines a curve or multiple curves, which can be used for different purposes.
		// * can have child ControlPointNodes, which define the shape of the curve.
		class PathNode : public BaseNode
		{
		public:
			struct Desc : BaseNode::Desc
			{
				Desc(CSZ name = nullptr) : BaseNode::Desc(name) {}
			};
			VERUS_TYPEDEFS(Desc);

			PathNode();
			virtual ~PathNode();

			void Init(RcDesc desc);
			void Done();

			virtual void Duplicate(RBaseNode node) override;

			virtual void GetEditorCommands(Vector<EditorCommand>& v) override;

			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;
		};
		VERUS_TYPEDEFS(PathNode);

		class PathNodePtr : public Ptr<PathNode>
		{
		public:
			void Init(PathNode::RcDesc desc);
			void Duplicate(RBaseNode node);
		};
		VERUS_TYPEDEFS(PathNodePtr);

		class PathNodePwn : public PathNodePtr
		{
		public:
			~PathNodePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(PathNodePwn);
	}
}
