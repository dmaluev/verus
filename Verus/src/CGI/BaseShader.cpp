// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::CGI;

CSZ BaseShader::s_branchCommentMarker = "//@";

void BaseShader::Load(CSZ url)
{
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(url, vData, IO::FileSystem::LoadDesc(true));

	Vector<String> vBranches;
	CSZ pSource = reinterpret_cast<CSZ>(vData.data());
	CSZ p = strstr(pSource, s_branchCommentMarker);
	const size_t branchCommentMarkerLen = strlen(s_branchCommentMarker);
	while (p)
	{
		const size_t span = strcspn(p, VERUS_CRNL);
		String value(p + branchCommentMarkerLen, span - branchCommentMarkerLen);
		if (_userDefines)
		{
			if (Str::EndsWith(_C(value), ")"))
			{
				const size_t pos = value.rfind("(");
				value.insert(pos, " ");
				value.insert(pos, _userDefines);
			}
			else
			{
				value += " ";
				value += _userDefines;
			}
		}
		vBranches.push_back(value);
		p = strstr(p + span, s_branchCommentMarker);
	}

	Vector<CSZ> vBranchPtrs;
	vBranchPtrs.reserve(vBranches.size() + 1);
	VERUS_FOREACH_CONST(Vector<String>, vBranches, it)
		vBranchPtrs.push_back(_C(*it));
	vBranchPtrs.push_back(nullptr);

	const size_t pakPos = IO::FileSystem::FindPosForPAK(url);
	if (pakPos != String::npos)
		url = url + pakPos + 2;
	Init(reinterpret_cast<CSZ>(vData.data()), url, vBranchPtrs.data());
}

String BaseShader::Parse(
	CSZ branchDesc,
	RString entry,
	String stageEntries[],
	RString stages,
	Vector<String>& vMacroName,
	Vector<String>& vMacroValue,
	CSZ prefix)
{
	VERUS_RT_ASSERT(branchDesc);
	VERUS_FOR(i, +Stage::count)
		stageEntries[i].clear();
	stages = "(VF)";
	vMacroName.clear();
	vMacroValue.clear();

	CSZ space = strchr(branchDesc, ' ');

	String entryBranch;
	space ? entryBranch.assign(branchDesc, space) : entryBranch.assign(branchDesc);

	if (space)
	{
		branchDesc = space;
		do
		{
			branchDesc += strspn(branchDesc, " ");
			space = strchr(branchDesc, ' ');

			String macro;
			space ? macro.assign(branchDesc, space) : macro.assign(branchDesc);

			if (Str::StartsWith(_C(macro), "("))
			{
				stages = macro;
			}
			else
			{
				const size_t eq = macro.find('=');
				if (eq != String::npos)
				{
					vMacroName.push_back(prefix + macro.substr(0, eq));
					vMacroValue.push_back(macro.substr(eq + 1));
				}
				else
				{
					vMacroName.push_back(prefix + macro);
					vMacroValue.push_back("1");
				}
			}

			branchDesc = space;
		} while (branchDesc);
	}

	const size_t colon = entryBranch.find(":");
	if (String::npos == colon)
	{
		entry = entryBranch;
		stageEntries[+Stage::vs] = entryBranch + "VS";
		stageEntries[+Stage::hs] = entryBranch + "HS";
		stageEntries[+Stage::ds] = entryBranch + "DS";
		stageEntries[+Stage::gs] = entryBranch + "GS";
		stageEntries[+Stage::fs] = entryBranch + "FS";
		stageEntries[+Stage::cs] = entryBranch + "CS";
		return entryBranch;
	}
	else
	{
		const String func = entryBranch.substr(0, colon);
		entry = func;
		stageEntries[+Stage::vs] = func + "VS";
		stageEntries[+Stage::hs] = func + "HS";
		stageEntries[+Stage::ds] = func + "DS";
		stageEntries[+Stage::gs] = func + "GS";
		stageEntries[+Stage::fs] = func + "FS";
		stageEntries[+Stage::cs] = func + "CS";
		return entryBranch.substr(colon + 1);
	}
}

void BaseShader::TestParse()
{
	String entry, stageEntries[+Stage::count], stages;
	Vector<String> vMacroName;
	Vector<String> vMacroValue;

	Parse("",
		entry, stageEntries, stages, vMacroName, vMacroValue, "DEF_");
	Parse("Foo",
		entry, stageEntries, stages, vMacroName, vMacroValue, "DEF_");
	Parse("Bar FOO BAR XYZ (VGF)",
		entry, stageEntries, stages, vMacroName, vMacroValue, "DEF_");
	Parse("FooBar FOO=1 BAR=xyz XYZ= XYZ2=",
		entry, stageEntries, stages, vMacroName, vMacroValue, "DEF_");
}

bool BaseShader::IsInIgnoreList(CSZ name) const
{
	if (!_ignoreList)
		return false;
	CSZ* pp = _ignoreList;
	while (*pp)
	{
		if (!strcmp(*pp, name))
			return true;
		pp++;
	}
	return false;
}

void BaseShader::SaveCompiled(CSZ code, CSZ filename)
{
	IO::File file;
	if (file.Open(filename, "w"))
		file.Write(code, strlen(code));
}

// ShaderPtr:

void ShaderPtr::Init(RcShaderDesc desc)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(!_p);
	_p = renderer->InsertShader();
	_p->SetIgnoreList(desc._ignoreList);
	_p->SetUserDefines(desc._userDefines);
	_p->SetSaveCompiled(desc._saveCompiled);
	if (desc._url)
		_p->Load(desc._url);
	else
		_p->Init(desc._source, "", desc._branches);
}

void ShaderPwn::Done()
{
	if (_p)
	{
		VERUS_QREF_RENDERER;
		renderer->DeleteShader(_p);
		_p = nullptr;
	}
}
