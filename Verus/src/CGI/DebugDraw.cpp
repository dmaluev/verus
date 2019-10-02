#include "verus.h"

using namespace verus;
using namespace verus::CGI;

//DebugDraw::CB_PerObject DebugDraw::ms_cbPerObject;

DebugDraw::DebugDraw()
{
	VERUS_CT_ASSERT(16 == sizeof(Vertex));
}

DebugDraw::~DebugDraw()
{
	Done();
}

void DebugDraw::Init()
{
	VERUS_INIT();

	VERUS_QREF_RENDERER;

#if 0
	CShaderDesc sd;
	sd.m_url = "Shaders:DR.cg";
	_shader.Init(sd);
	_shader->BindBufferSource(&ms_cbPerObject, sizeof(ms_cbPerObject), 0, "PerObject");

	CVertexElement ve[] =
	{
		{0, offsetof(Vertex, pos),		/**/VeType::_float, 3, VeUsage::position, 0},
		{0, offsetof(Vertex, color),	/**/VeType::_ubyte, 4, VeUsage::color, 0},
		VERUS_END_DECL
	};
	CGeometryDesc gd;
	gd.m_pVertDecl = ve;
	gd.m_pShader = &(*_shader);
	_geo.Init(gd);
	_geo->DefineVertexStream(0, sizeof(Vertex), _maxNumVert * sizeof(Vertex), USAGE_DYNAMIC_DRAW);

	CStateBlockDesc sbd;
	sbd.B().rtBlendEqs[0] = VERUS_BLEND_NORMAL;
	sbd.R().antialiasedLineEnable = true;
	sbd.Z().depthEnable = true;
	sbd.Z().depthWriteEnable = false;
	m_sb[SB_MASTER].Init(sbd);

	sbd.Reset();
	sbd.B().rtBlendEqs[0] = VERUS_BLEND_NORMAL;
	sbd.R().antialiasedLineEnable = true;
	sbd.Z().depthEnable = false;
	sbd.Z().depthWriteEnable = false;
	m_sb[SB_NO_Z].Init(sbd);
#endif
}

void DebugDraw::Done()
{
	VERUS_DONE(DebugDraw);
}

void DebugDraw::Begin(Type type, PcTransform3 pMat, bool zEnable)
{
#if 0
	VERUS_QREF_SM;
	VERUS_QREF_RENDER;

	_type = type;
	_numVert = 0;

	if (zEnable)
		m_sb[SB_MASTER]->Apply();
	else
		m_sb[SB_NO_Z]->Apply();

	Matrix4 matWVP;
	if (sm.GetCamera())
		matWVP = sm.GetCamera()->GetMatrixVP();
	else
		matWVP = Matrix4::identity();

	ms_cbPerObject.matWVP = pMat ? Matrix4(matWVP**pMat).ConstBufferFormat() : matWVP.ConstBufferFormat();

	_pVB = static_cast<Vertex*>(_geo->MapVB(true));

	_shader->Bind("T");
	_shader->UpdateBuffer(0);

	_geo->BeginDraw(0x1);
#endif
}

void DebugDraw::End()
{
#if 0
	_geo->UnmapVB(0);
	if (_numVert)
	{
		VERUS_QREF_RENDER;
		switch (_type)
		{
		case T_POINTS:	render->DrawPrimitive(PT_POINTLIST,		/**/0, _numVert); break;
		case T_LINES:	render->DrawPrimitive(PT_LINELIST,		/**/0, _numVert); break;
		case T_POLY:	render->DrawPrimitive(PT_TRIANGLELIST,	/**/0, _numVert); break;
		}
	}
	_geo->EndDraw(0x1);
#endif
}

void DebugDraw::AddPoint(
	RcPoint3 pos,
	UINT32 color)
{
}

void DebugDraw::AddLine(
	RcPoint3 posA,
	RcPoint3 posB,
	UINT32 color)
{
#if 0
	if (_numVert + 1 >= _maxNumVert)
	{
		VERUS_QREF_RENDERER;
		_geo->UnmapVB(0);
		render->DrawPrimitive(PT_LINELIST, 0, _numVert);
		_pVB = static_cast<Vertex*>(_geo->MapVB(true));
		_numVert = 0;
	}
	posA.ToArray3(_pVB[_numVert]._pos);
	Utils::CopyColor(_pVB[_numVert]._color, Convert::ToDeviceColor(color));
	_numVert++;
	posB.ToArray3(_pVB[_numVert]._pos);
	Utils::CopyColor(_pVB[_numVert]._color, Convert::ToDeviceColor(color));
	_numVert++;
#endif
}

void DebugDraw::AddTriangle(
	RcPoint3 posA,
	RcPoint3 posB,
	RcPoint3 posC,
	UINT32 color)
{
}
