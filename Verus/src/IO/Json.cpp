#include "verus.h"

using namespace verus;
using namespace verus::IO;

Json::Json()
{
}

Json::Json(CSZ pathName) : _pathName(pathName)
{
}

Json::~Json()
{
}

void Json::SetFilename(CSZ name)
{
	String pathName;
	CSZ pSlash = strchr(name, '/');
	if (!pSlash)
	{
		//pathName = CUtils::I().GetWritablePath() + "/" + name;
		//name = _C(pathName);
	}
	_pathName = name;
}

void Json::Load(bool fromCache)
{
	Vector<BYTE> vData;
	if (fromCache)
	{
		IO::FileSystem::I().LoadResourceFromCache(_C(_pathName), vData);
		_json = nlohmann::json::parse(reinterpret_cast<CSZ>(vData.data()));
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
			_json = nlohmann::json::parse(reinterpret_cast<CSZ>(vData.data()));
		}
		else
		{
			IO::File file;
			if (file.Open(_C(_pathName)))
			{
				file.ReadAll(vData, true);
				_json = nlohmann::json::parse(reinterpret_cast<CSZ>(vData.data()));
			}
		}
	}
}

void Json::Save()
{
	StringStream ss;
	ss << "Save() url=" << _pathName;
	IO::File file;
	if (file.Open(_C(_pathName), "w"))
	{
		const String s = _json.dump(1, '\t');
		file.WriteText(_C(s));
		ss << ", OK";
	}
	VERUS_LOG_INFO(_C(ss.str()));
}

void Json::Set(CSZ name, CSZ v, bool ifNull)
{
	if (ifNull && _json.find(name) != _json.end())
		return;
	_json[name] = v;
}

void Json::Set(CSZ name, int v, bool ifNull)
{
	if (ifNull && _json.find(name) != _json.end())
		return;
	_json[name] = v;
}

void Json::Set(CSZ name, float v, bool ifNull)
{
	if (ifNull && _json.find(name) != _json.end())
		return;
	_json[name] = v;
}

void Json::Set(CSZ name, bool v, bool ifNull)
{
	if (ifNull && _json.find(name) != _json.end())
		return;
	_json[name] = v;
}

CSZ Json::GetS(CSZ name, CSZ def)
{
	if (_json.find(name) == _json.end())
		return def;
	return _json[name].get_ref<const nlohmann::json::string_t&>().c_str();
}

int Json::GetI(CSZ name, int def)
{
	return _json.value(name, def);
}

float Json::GetF(CSZ name, float def)
{
	return _json.value(name, def);
}

bool Json::GetB(CSZ name, bool def)
{
	return _json.value(name, def);
}
