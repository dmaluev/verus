#include "verus.h"

using namespace verus;
using namespace verus::IO;

Xml::Xml()
{
}

Xml::Xml(CSZ pathName) : _pathName(pathName)
{
}

Xml::~Xml()
{
}

void Xml::SetFilename(CSZ name)
{
	String pathName;
	CSZ pSlash = strchr(name, '/');
	if (!pSlash)
	{
		//pathName = CUtils::I().GetWritablePath() + "/" + name;
		name = _C(pathName);
	}
	_pathName = name;
}

void Xml::Load(bool fromCache)
{
	Vector<BYTE> vData;
	if (fromCache)
	{
		IO::FileSystem::I().LoadResourceFromCache(_C(_pathName), vData);
		_doc.Parse(reinterpret_cast<CSZ>(vData.data()));
	}
	else
	{
		StringStream ss;
		ss << "Load() url=" << _pathName;
		VERUS_LOG_INFO(_C(ss.str()));

		const size_t colon = _pathName.find(':');
		if (colon != String::npos && colon > 1)
		{
			IO::FileSystem::LoadResource(_C(_pathName), vData, IO::FileSystem::LoadDesc(true));
			_doc.Parse(reinterpret_cast<CSZ>(vData.data()));
		}
		else
		{
			IO::File file;
			if (file.Open(_C(_pathName)))
				_doc.LoadFile(file.GetFile());
		}
	}
}

void Xml::Save()
{
	StringStream ss;
	ss << "Save() url=" << _pathName;
	IO::File file;
	if (file.Open(_C(_pathName), "w"))
	{
		_doc.SaveFile(file.GetFile());
		ss << ", OK";
	}
	VERUS_LOG_INFO(_C(ss.str()));
}

tinyxml2::XMLElement* Xml::GetRoot()
{
	tinyxml2::XMLElement* pElem = _doc.FirstChildElement();
	if (!pElem)
	{
		_doc.Clear();
		_doc.LinkEndChild(_doc.NewDeclaration());
		_doc.LinkEndChild(_doc.NewElement("root"));
		pElem = _doc.FirstChildElement();
	}
	return pElem;
}

tinyxml2::XMLElement* Xml::AddElement(CSZ name)
{
	tinyxml2::XMLElement* pElem = _doc.NewElement(name);
	GetRoot()->LinkEndChild(pElem);
	return pElem;
}

void Xml::Set(CSZ name, CSZ v, bool ifNull)
{
	tinyxml2::XMLElement* pElem = GetRoot()->FirstChildElement(name);
	if (!pElem)
	{
		GetRoot()->LinkEndChild(_doc.NewElement(name));
		pElem = GetRoot()->FirstChildElement(name);
	}
	else if (ifNull)
		return;
	pElem->SetAttribute("v", v);
}

void Xml::Set(CSZ name, int v, bool ifNull)
{
	char txt[16];
	sprintf_s(txt, "%d", v);
	Set(name, txt, ifNull);
}

void Xml::Set(CSZ name, float v, bool ifNull)
{
	char txt[16];
	sprintf_s(txt, "%g", v);
	Set(name, txt, ifNull);
}

void Xml::Set(CSZ name, bool v, bool ifNull)
{
	char txt[16];
	sprintf_s(txt, "%d", v ? 1 : 0);
	Set(name, txt, ifNull);
}

CSZ Xml::GetS(CSZ name, CSZ def)
{
	tinyxml2::XMLElement* pElem = GetRoot()->FirstChildElement(name);
	return pElem ? pElem->Attribute("v") : def;
}

int Xml::GetI(CSZ name, int def)
{
	CSZ v = GetS(name);
	return v ? atoi(v) : def;
}

float Xml::GetF(CSZ name, float def)
{
	CSZ v = GetS(name);
	return v ? static_cast<float>(atof(v)) : def;
}

bool Xml::GetB(CSZ name, bool def)
{
	CSZ v = GetS(name);
	return v ? !!atoi(v) : def;
}
