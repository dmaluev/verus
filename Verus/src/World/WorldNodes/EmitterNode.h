// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// EmitterNode is like an instance of a ParticlesNode.
		// Emitters define areas of the world where particles will appear.
		// * references particles node
		// * can adjust particles by changing color, offset, etc.
		class EmitterNode : public BaseNode
		{
			Vector4          _flowColor = Vector4::Replicate(1);
			Vector3          _flowOffset = Vector3(0);
			ParticlesNodePwn _particlesNode;
			float            _flow = 0;
			float            _flowScale = 1;

		public:
			struct Desc : BaseNode::Desc
			{
				CSZ _url = nullptr;

				Desc(CSZ url = nullptr) : _url(url) {}
			};
			VERUS_TYPEDEFS(Desc);

			EmitterNode();
			virtual ~EmitterNode();

			void Init(RcDesc desc);
			void Done();

			virtual void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication) override;

			virtual void Update() override;

			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;
			void Deserialize_LegacyXXX(IO::RStream stream);
			void DeserializeXML_LegacyXXX(pugi::xml_node node);

			// <Resources>
			Str GetURL() const { return _particlesNode->GetParticles().GetURL(); }

			ParticlesNodePtr GetParticlesNode() { return _particlesNode; }
			// </Resources>

			RcVector4 GetFlowColor() { return _flowColor; }
			void SetFlowColor(RcVector4 flowColor) { _flowColor = flowColor; }
			RcVector3 GetFlowOffset() { return _flowOffset; }
			void SetFlowOffset(RcVector3 flowOffset) { _flowOffset = flowOffset; }
			float GetFlowScale() const { return _flowScale; }
			void SetFlowScale(float flowScale) { _flowScale = flowScale; }
		};
		VERUS_TYPEDEFS(EmitterNode);

		class EmitterNodePtr : public Ptr<EmitterNode>
		{
		public:
			void Init(EmitterNode::RcDesc desc);
			void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication);
		};
		VERUS_TYPEDEFS(EmitterNodePtr);

		class EmitterNodePwn : public EmitterNodePtr
		{
		public:
			~EmitterNodePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(EmitterNodePwn);
	}
}
