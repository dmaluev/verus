// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// ShakerNode can add dynamic noise to some properties of parent node.
		// * property must be accessible with GetPropertyByName of BaseNode
		class ShakerNode : public BaseNode
		{
			Anim::Shaker _shaker;
			String       _propertyName;
			float        _initialValue = 0;

		public:
			struct Desc : BaseNode::Desc
			{
				CSZ _url = nullptr;

				Desc(CSZ url = nullptr) : _url(url) {}
			};
			VERUS_TYPEDEFS(Desc);

			ShakerNode();
			virtual ~ShakerNode();

			void Init(RcDesc desc);
			void Done();

			virtual void Duplicate(RBaseNode node) override;

			virtual void Update() override;

			virtual void GetEditorCommands(Vector<EditorCommand>& v) override;
			virtual void ExecuteEditorCommand(RcEditorCommand command) override;

			virtual void Disable(bool disable) override;

			virtual void OnNodeParentChanged(PBaseNode pNode, bool afterEvent) override;

			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;

			Anim::RShaker GetShaker() { return _shaker; }

			Str GetPropertyName() const { return _C(_propertyName); }
			void SetPropertyName(CSZ name);

			void UpdateInitialValue();
			void ApplyInitialValue();
		};
		VERUS_TYPEDEFS(ShakerNode);

		class ShakerNodePtr : public Ptr<ShakerNode>
		{
		public:
			void Init(ShakerNode::RcDesc desc);
			void Duplicate(RBaseNode node);
		};
		VERUS_TYPEDEFS(ShakerNodePtr);

		class ShakerNodePwn : public ShakerNodePtr
		{
		public:
			~ShakerNodePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(ShakerNodePwn);
	}
}
