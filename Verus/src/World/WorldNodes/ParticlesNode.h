// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	// ParticlesNode stores particles, which can be used by EmitterNode.
	// * has particles
	class ParticlesNode : public BaseNode
	{
		Effects::Particles _particles;
		int                _refCount = 0;

	public:
		struct Desc : BaseNode::Desc
		{
			CSZ _url = nullptr;

			Desc(CSZ url = nullptr) : _url(url) {}
		};
		VERUS_TYPEDEFS(Desc);

		ParticlesNode();
		virtual ~ParticlesNode();

		void AddRef() { _refCount++; }
		int GetRefCount() const { return _refCount; }

		void Init(RcDesc desc);
		bool Done();

		virtual void Update() override;
		virtual void Draw() override;

		virtual void GetEditorCommands(Vector<EditorCommand>& v) override;

		virtual bool CanSetParent(PBaseNode pNode) const override;

		virtual void AddDefaultPickingBody() override {}

		virtual void Serialize(IO::RSeekableStream stream) override;
		virtual void Deserialize(IO::RStream stream) override;

		// <Resources>
		Str GetURL() const { return _particles.GetURL(); }

		Effects::RParticles GetParticles() { return _particles; }
		// </Resources>
	};
	VERUS_TYPEDEFS(ParticlesNode);

	class ParticlesNodePtr : public Ptr<ParticlesNode>
	{
	public:
		void Init(ParticlesNode::RcDesc desc);
	};
	VERUS_TYPEDEFS(ParticlesNodePtr);

	class ParticlesNodePwn : public ParticlesNodePtr
	{
	public:
		~ParticlesNodePwn() { Done(); }
		void Done();
	};
	VERUS_TYPEDEFS(ParticlesNodePwn);
}
