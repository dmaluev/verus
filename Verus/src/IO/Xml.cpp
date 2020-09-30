#include "verus.h"

using namespace verus;
using namespace verus::IO;

Xml::Xml()
{
}

Xml::Xml(CSZ pathname) : _pathname(pathname)
{
}

Xml::~Xml()
{
}

void Xml::SetFilename(CSZ name)
{
	String pathname;
	CSZ pSlash = strchr(name, '/');
	if (!pSlash)
	{
		pathname = String(_C(Utils::I().GetWritablePath())) + "/" + name;
		name = _C(pathname);
	}
	_pathname = name;
}

void Xml::Load(bool fromCache)
{
	_vData.clear();
	if (fromCache)
	{
		FileSystem::I().LoadResourceFromCache(_C(_pathname), _vData);
		const pugi::xml_parse_result result = _doc.load_buffer_inplace(_vData.data(), _vData.size());
		if (!result)
			throw VERUS_RECOVERABLE << "load_buffer_inplace(), " << result.description();
	}
	else
	{
		StringStream ss;
		ss << "Load() url=" << _pathname;
		VERUS_LOG_INFO(_C(ss.str()));

		const size_t pakPos = FileSystem::FindPosForPAK(_C(_pathname));
		if (pakPos != String::npos)
		{
			FileSystem::LoadResource(_C(_pathname), _vData, FileSystem::LoadDesc(true));
			const pugi::xml_parse_result result = _doc.load_buffer_inplace(_vData.data(), _vData.size());
			if (!result)
				throw VERUS_RECOVERABLE << "load_buffer_inplace(), " << result.description();
		}
		else
		{
			File file;
			if (file.Open(_C(_pathname)))
			{
				file.ReadAll(_vData, true);
				const pugi::xml_parse_result result = _doc.load_buffer_inplace(_vData.data(), _vData.size());
				if (!result)
					throw VERUS_RECOVERABLE << "load_buffer_inplace(), " << result.description();
			}
		}
	}
}

void Xml::Save()
{
	StringStream ss;
	ss << "Save() url=" << _pathname;
	File file;
	if (file.Open(_C(_pathname), "w"))
	{
		pugi::xml_writer_file writer(file.GetFile());
		_doc.save(writer);
		ss << ", OK";
	}
	VERUS_LOG_INFO(_C(ss.str()));
}

pugi::xml_node Xml::GetRoot()
{
	pugi::xml_node ret = _doc.first_child();
	if (!ret)
	{
		_doc.append_child("root");
		ret = _doc.first_child();
	}
	return ret;
}

pugi::xml_node Xml::AddElement(CSZ name)
{
	return GetRoot().append_child(name);
}

void Xml::Set(CSZ name, CSZ v, bool ifNull)
{
	pugi::xml_node node = GetRoot().child(name);
	if (!node)
		node = GetRoot().append_child(name);
	else if (ifNull)
		return;
	node.append_attribute("v") = v;
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
	pugi::xml_node node = GetRoot().child(name);
	return node ? node.attribute("v").value() : def;
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
