#include "verus.h"

using namespace verus;
using namespace verus::CGI;

void BaseShader::Load(CSZ url)
{
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(url, vData, IO::FileSystem::LoadDesc(true));

	Vector<String> v;
	CSZ pText = reinterpret_cast<CSZ>(vData.data());
	CSZ p = strstr(pText, "//:");
	while (p)
	{
		const size_t span = strcspn(p, VERUS_CRNL);
		const String value(p + 3, span - 3);
		v.push_back(value);
		p = strstr(p + span, "//:");
	}

	Vector<CSZ> vp;
	vp.reserve(v.size() + 1);
	VERUS_FOREACH_CONST(Vector<String>, v, it)
		vp.push_back(_C(*it));
	vp.push_back(0);

	Init(reinterpret_cast<CSZ>(vData.data()), vp.data());
}
