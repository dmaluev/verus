// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// Draft:

Draft::Draft()
{
}

Draft::~Draft()
{
	Done();
}

void Draft::Init(Site* p, RcDesc desc)
{
	_pParent = desc._pParent;
	CGI::LightType type = CGI::LightType::none;
	if (!strcmp(desc._url, "OMNI"))
		type = CGI::LightType::omni;
	if (!strcmp(desc._url, "SPOT"))
		type = CGI::LightType::spot;
	if (type != CGI::LightType::none)
	{
		Light::Desc lightDesc;
		lightDesc._node = desc._node;
		lightDesc._data._lightType = type;
		if (p->GetRadius())
			lightDesc._data._radius = p->GetRadius();
		_light.Init(lightDesc);
		_p = _light.Get();
	}
	else if (Str::EndsWith(desc._url, ".Prefab.xml"))
	{
		Prefab::Desc prefabDesc;
		prefabDesc._node = desc._node;
		prefabDesc._url = desc._url;
		prefabDesc._collide = desc._collide;
		_prefab.Init(prefabDesc);
		_p = _prefab.Get();
	}
	else
	{
		Block::Desc blockDesc;
		blockDesc._node = desc._node;
		blockDesc._model = desc._url;
		blockDesc._matIndex = desc._matIndex;
		blockDesc._collide = desc._collide;
		String mat;
		if (desc._matIndex)
		{
			mat = desc._url;
			Str::ReplaceExtension(mat, _C(std::to_string(desc._matIndex) + ".xmt"));
			blockDesc._blockMat = _C(mat);
		}
		_block.Init(blockDesc);
		_p = _block.Get();
	}
	_p->GetDictionary().Insert("Site", _C(p->GetName()));
}

void Draft::Done()
{
	_block.Done();
	_light.Done();
	_prefab.Done();
}

// DraftPtr:

void DraftPtr::Init(Site* p, Draft::RcDesc desc)
{
	VERUS_RT_ASSERT(!_p);
	_p = p->InsertDraft();
	_p->Init(p, desc);
}

void DraftPtr::Done(Site* p)
{
	if (_p)
	{
		p->DeleteDraft(_p);
		_p = nullptr;
	}
};

// Site:

Site::Site()
{
	_type = NodeType::site;
}

Site::~Site()
{
	Done();
}

void Site::Init(RcDesc desc)
{
	if (_refCount)
		return;

	_name = desc._name;
	_refCount = 1;
}

bool Site::Done()
{
	return true;
}

void Site::Update()
{
	if (_activeDraft)
	{
		if (_activeDraft->GetLight() && _params._radius)
			_activeDraft->GetLight()->SetRadius(_params._radius);
		const Transform3 mat = Transform3::translation(_pDelegate->Site_TransformOffset(_params._offset)) *
			_pDelegate->Site_GetTransform(_params);
		_activeDraft->Get()->SetTransform(mat);
	}
}

void Site::Draw()
{
}

void Site::DrawImmediate()
{
	if (!_pDelegate)
		return;

	VERUS_QREF_DD;
	VERUS_QREF_HELPERS;

	const Transform3 mat = _pDelegate->Site_GetTransform(_params);
	const Point3 at = mat.getTranslation();
	const Point3 to = at + _pDelegate->Site_TransformOffset(_params._offset);

	if (_activeDraft)
	{
		_activeDraft->Get()->DrawBounds();
		helpers.DrawBasis(&_activeDraft->Get()->GetTransform(), -1, true);
	}

	if (!_params._offset.IsZero())
	{
		//dr.Begin(CGI::CDebugRender::T_LINES, nullptr, false);
		//dr.AddLine(at, to, VERUS_COLOR_RGBA(0, 255, 0, 255));
		//dr.End();
	}

	if (_params._radius)
	{
		helpers.DrawCircle(at, _params._radius, VERUS_COLOR_WHITE, *_params._pTerrain);
	}

	if (_linkedDraft)
	{
		const Point3 from = _linkedDraft->Get()->GetPosition();
		//dr.Begin(CGI::CDebugRender::T_LINES, nullptr, false);
		//dr.AddLine(from, to, VERUS_COLOR_RGBA(255, 0, 255, 255));
		//dr.End();
	}

	for (auto& draft : TStoreDrafts::_list)
	{
		if (draft.GetLight())
		{
			LightPtr light = draft.GetLight();
			if (CGI::LightType::spot == light->GetLightType())
			{
				const Point3 target = light->GetPosition() + light->GetDirection() * light->GetRadius();
				helpers.DrawLight(light->GetPosition(), 0, &target);
			}
			else
				helpers.DrawLight(light->GetPosition());
		}
	}
}

PDraft Site::InsertDraft()
{
	return TStoreDrafts::Insert();
}

void Site::DeleteDraft(PDraft p)
{
	TStoreDrafts::Delete(p);
}

void Site::DeleteAllDrafts()
{
	TStoreDrafts::DeleteAll();
}

void Site::Save()
{
	IO::Xml xml;
	xml.SetFilename(_C("Site_" + _name + ".xml"));
	for (auto& draft : TStoreDrafts::_list)
	{
		pugi::xml_node node = xml.AddElement("draft");
		draft.Get()->SaveXML(node);
		if (draft.GetParent() && draft.GetLight())
		{
			const Transform3 tr = VMath::inverse(draft.GetParent()->Get()->GetTransform()) * draft.GetLight()->GetTransformNoScale();
			node.attribute("relative").set_value("1");
			node.attribute("m").set_value(_C(tr.ToString()));
		}
	}
	xml.Save();
}

void Site::Load(CSZ pathname)
{
	IO::Xml xml;
	if (pathname)
	{
		xml.SetFilename(pathname);
	}
	else if (String::npos != _name.find(':'))
	{
		const String path = IO::FileSystem::ConvertRelativePathToAbsolute(_name, true);
		xml.SetFilename(_C(path));
	}
	else
		xml.SetFilename(_C("Site_" + _name + ".xml"));
	StringStream ss;
	ss << "Site::Load(), url = " << _C(xml.GetFilename());
	xml.Load();

	int draftCount = 0;
	auto root = xml.GetRoot();
	for (auto node : root.children("draft"))
	{
		DraftPtr draft;
		Draft::Desc draftDesc;
		draftDesc._node = node;
		draftDesc._url = node.attribute("url").value();
		draftDesc._collide = true;
		draft.Init(this, draftDesc);
		draftCount++;
	}
	ss << ", draftCount = " << draftCount;
	VERUS_LOG_INFO(ss.str());
}

void Site::Add(CSZ url, int matIndex)
{
	if (!_activeDraft)
	{
		_prevUrl = url;
		_prevMatIndex = matIndex;
		Draft::Desc draftDesc;
		draftDesc._url = url;
		draftDesc._matIndex = matIndex;
		if (_linkedDraft)
			draftDesc._pParent = _linkedDraft.Get();
		_activeDraft.Init(this, draftDesc);
	}
}

void Site::Delete()
{
	if (_activeDraft)
	{
		if (_linkedDraft == _activeDraft)
			_linkedDraft.Detach();
		_prevUrl = _activeDraft->Get()->GetUrl();
		_prevMatIndex = 0;
		_activeDraft.Done(this);
		Save();
	}
}

void Site::Edit(bool link)
{
	RDraftPtr draft = link ? _linkedDraft : _activeDraft;
	if (!draft) // Find the nearest block?
	{
		FindNearest(&draft);
		if (draft && !link)
		{
			draft->Get()->SetCollisionGroup(Physics::Group::general);
			_prevUrl = draft->Get()->GetUrl();
			_prevMatIndex = 0;
		}
	}
	else // Reset block's matrix:
	{
		if (link)
			_linkedDraft.Detach();
	}
}

void Site::Duplicate()
{
	if (!_prevUrl.empty())
		Add(_C(_prevUrl), _prevMatIndex);
}

void Site::Put()
{
	if (_activeDraft)
	{
		_activeDraft->Get()->MoveRigidBody();
		_activeDraft->Get()->SetCollisionGroup(Physics::Group::immovable);
		_activeDraft.Detach();
		Save();
	}
}

PSceneNode Site::FindNearest(PDraftPtr pAttach)
{
	PSceneNode p = nullptr;
	const Transform3 mat = Transform3::translation(_pDelegate->Site_TransformOffset(_params._offset)) *
		_pDelegate->Site_GetTransform(_params);
	const Point3 at = mat.getTranslation();
	float minDist = FLT_MAX;
	for (auto& d : TStoreDrafts::_list)
	{
		const float dist = VMath::dist(at, d.Get()->GetPosition());
		if (dist < minDist)
		{
			minDist = dist;
			if (pAttach)
				pAttach->Attach(&d);
			p = d.Get();
		}
	}
	return p;
}

void Site::Dir(RcVector3 dir)
{
	if (_linkedDraft && _linkedDraft->GetLight())
	{
		_linkedDraft->GetLight()->SetDirection(dir);
		Save();
	}
}

void Site::DirFromPoint(float radiusScale)
{
	if (_linkedDraft && _linkedDraft->GetLight())
	{
		const Transform3 mat = Transform3::translation(_pDelegate->Site_TransformOffset(_params._offset)) *
			_pDelegate->Site_GetTransform(_params);
		const Point3 at = mat.getTranslation();
		_linkedDraft->GetLight()->DirFromPoint(at, radiusScale);
		Save();
	}
}

void Site::ConeFromPoint(bool coneIn)
{
	if (_linkedDraft && _linkedDraft->GetLight())
	{
		const Transform3 mat = Transform3::translation(_pDelegate->Site_TransformOffset(_params._offset)) *
			_pDelegate->Site_GetTransform(_params);
		const Point3 at = mat.getTranslation();
		_linkedDraft->GetLight()->ConeFromPoint(at, coneIn);
		Save();
	}
}

void Site::Commit()
{
}

PSiteDelegate Site::SetDelegate(PSiteDelegate p)
{
	return Utils::Swap(_pDelegate, p);
}

// SitePtr:

void SitePtr::Init(Site::RcDesc desc)
{
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(!_p);
	_p = sm.InsertSite(desc._name);
	_p->Init(desc);
}

void SitePwn::Done()
{
	if (_p)
	{
		SceneManager::I().DeleteSite(_C(_p->GetName()));
		_p = nullptr;
	}
}
