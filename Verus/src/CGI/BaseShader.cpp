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
		const String value(p + branchCommentMarkerLen, span - branchCommentMarkerLen);
		vBranches.push_back(value);
		p = strstr(p + span, s_branchCommentMarker);
	}

	Vector<CSZ> vBranchPtrs;
	vBranchPtrs.reserve(vBranches.size() + 1);
	VERUS_FOREACH_CONST(Vector<String>, vBranches, it)
		vBranchPtrs.push_back(_C(*it));
	vBranchPtrs.push_back(nullptr);

	Init(reinterpret_cast<CSZ>(vData.data()), vBranchPtrs.data());
}

String BaseShader::Parse(
	CSZ branchDesc,
	RString entryVS,
	RString entryHS,
	RString entryDS,
	RString entryGS,
	RString entryFS,
	Vector<String>& vMacroName,
	Vector<String>& vMacroValue,
	CSZ prefix)
{
	VERUS_RT_ASSERT(branchDesc);
	entryVS.clear();
	entryHS.clear();
	entryDS.clear();
	entryGS.clear();
	entryFS.clear();
	vMacroName.clear();
	vMacroValue.clear();

	CSZ space = strchr(branchDesc, ' ');

	String entry;
	space ? entry.assign(branchDesc, space) : entry.assign(branchDesc);

	if (space)
	{
		branchDesc = space;
		do
		{
			branchDesc += strspn(branchDesc, " ");
			space = strchr(branchDesc, ' ');

			String macro;
			space ? macro.assign(branchDesc, space) : macro.assign(branchDesc);

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

			branchDesc = space;
		} while (branchDesc);
	}

	const size_t colon = entry.find(":");
	if (String::npos == colon)
	{
		entryVS = entry + "VS";
		entryHS = entry + "HS";
		entryDS = entry + "DS";
		entryGS = entry + "GS";
		entryFS = entry + "FS";
		return entry;
	}
	else
	{
		const String func = entry.substr(0, colon);
		entryVS = func + "VS";
		entryHS = func + "HS";
		entryDS = func + "DS";
		entryGS = func + "GS";
		entryFS = func + "FS";
		return entry.substr(colon + 1);
	}
}

void BaseShader::TestParse()
{
	String entryVS, entryHS, entryDS, entryGS, entryFS;
	Vector<String> vMacroName;
	Vector<String> vMacroValue;

	Parse("",
		entryVS, entryHS, entryDS, entryGS, entryFS, vMacroName, vMacroValue, "DEF_");
	Parse("Foo",
		entryVS, entryHS, entryDS, entryGS, entryFS, vMacroName, vMacroValue, "DEF_");
	Parse("Bar FOO BAR XYZ",
		entryVS, entryHS, entryDS, entryGS, entryFS, vMacroName, vMacroValue, "DEF_");
	Parse("FooBar FOO=1 BAR=xyz XYZ= XYZ2=",
		entryVS, entryHS, entryDS, entryGS, entryFS, vMacroName, vMacroValue, "DEF_");
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
	_p->SetSaveCompiled(desc._saveCompiled);
	if (desc._url)
		_p->Load(desc._url);
	else
		_p->Init(desc._source, desc._branches);
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
