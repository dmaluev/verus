#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// Prefab:

Prefab::Prefab()
{
	_type = NodeType::prefab;
}

Prefab::~Prefab()
{
	Done();
}

void Prefab::Init(RcDesc desc)
{
	VERUS_QREF_SM;
	_name = sm.EnsureUniqueName(desc._url);
	_url = desc._url;
	_collide = desc._collide;

	Vector<BYTE> vData;
	IO::FileSystem::I().LoadResourceFromCache(_C(_url), vData);
	LoadPrefab(reinterpret_cast<SZ>(vData.data()));
}

void Prefab::Done()
{
	for (auto& f : _vFragments)
		f._block.Done();
}

void Prefab::LoadPrefab(SZ xml)
{
	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(xml, strlen(xml));
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(), " << result.description();
	pugi::xml_node root = doc.first_child();

	int fragCount = 0;
	for (auto node : root.children("frag"))
		fragCount++;
	_vFragments.resize(fragCount);

	int i = 0;
	for (auto node : root.children("frag"))
	{
		RFragment f = _vFragments[i];

		CSZ txt = node.text().as_string();
		CSZ mat = node.attribute("mat").value();

		String urlFrag = _url;
		String matFrag;
		Str::ReplaceFilename(urlFrag, txt);
		urlFrag += ".x3d";
		if (strchr(mat, ':'))
		{
			matFrag = mat;
		}
		else
		{
			matFrag = _url;
			Str::ReplaceFilename(matFrag, mat);
			matFrag += ".mat";
		}

		Block::Desc descBlock;
		descBlock._model = _C(urlFrag);
		descBlock._modelMat = _C(matFrag);
		descBlock._collide = _collide;
		f._block.Init(descBlock);
		CSZ m = node.attribute("m").value();
		f._matrix.FromString(m);

		i++;
	}

	UpdateBounds();
}

void Prefab::Update()
{
	if (!_async_loadedAllBlocks)
	{
		bool all = true;
		for (auto& f : _vFragments)
		{
			if (!f._block->IsLoadedModel())
			{
				all = false;
				break;
			}
		}
		if (all)
		{
			_async_loadedAllBlocks = true;
			UpdateBounds();
		}
	}
}

void Prefab::Draw()
{
}

Vector4 Prefab::GetColor()
{
	if (_vFragments[0]._block)
		return _vFragments[0]._block->GetColor();
	return Vector4(0);
}

void Prefab::SetColor(RcVector4 color)
{
	for (auto& f : _vFragments)
	{
		if (f._block) // Async loaded?
			f._block->SetColor(color);
	}
}

void Prefab::UpdateBounds()
{
	for (auto& f : _vFragments)
	{
		if (f._block) // Async loaded?
		{
			const Transform3 mat = GetTransform() * f._matrix;
			f._block->SetTransform(mat);
		}
	}
	if (_async_loadedAllBlocks)
	{
		_bounds.Reset();
		for (auto& f : _vFragments)
			_bounds.CombineWith(f._block->GetBounds());
	}
}

void Prefab::SetCollisionGroup(Physics::Group g)
{
	for (auto& f : _vFragments)
		f._block->SetCollisionGroup(g);
}

void Prefab::MoveRigidBody()
{
	for (auto& f : _vFragments)
		f._block->MoveRigidBody();
}

void Prefab::SaveXML(pugi::xml_node node)
{
	SceneNode::SaveXML(node);

	node.attribute("url").set_value(_C(GetUrl()));
}

void Prefab::LoadXML(pugi::xml_node node)
{
	SceneNode::LoadXML(node);
	PreventNameCollision();

	Desc desc;
	desc._url = node.attribute("url").value();
	Init(desc);
}

// PrefabPtr:

void PrefabPtr::Init(Prefab::RcDesc desc)
{
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(!_p);
	_p = sm.InsertPrefab();
	if (desc._node)
		_p->LoadXML(desc._node);
	else
		_p->Init(desc);
}

void PrefabPwn::Done()
{
	if (_p)
	{
		SceneManager::I().DeletePrefab(_p);
		_p = nullptr;
	}
}
