#include "verus.h"

using namespace verus;
using namespace verus::CGI;

int BaseGeometry::GetVertexInputAttrDescCount(PcVertexInputAttrDesc p)
{
	int i = 0;
	while (p[i]._offset >= 0)
		i++;
	return i;
}

int BaseGeometry::GetBindingCount(PcVertexInputAttrDesc p)
{
	int count = 0;
	int i = 0;
	while (p[i]._offset >= 0)
	{
		bool found = false;
		int j = 0;
		while (j < i) // Check all previous for this binding.
		{
			if (p[j]._binding == p[i]._binding)
			{
				found = true;
				break;
			}
			j++;
		}
		if (!found)
			count++;
		i++;
	}
	return count;
}

// GeometryPtr:

void GeometryPtr::Init(RcGeometryDesc desc)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(!_p);
	_p = renderer->InsertGeometry();
	_p->Init(desc);
}

void GeometryPwn::Done()
{
	if (_p)
	{
		VERUS_QREF_RENDERER;
		renderer->DeleteGeometry(_p);
		_p = nullptr;
	}
}
