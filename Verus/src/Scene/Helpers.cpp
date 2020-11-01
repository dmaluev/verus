// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

Helpers::Helpers()
{
}

Helpers::~Helpers()
{
	Done();
}

void Helpers::Init()
{
	VERUS_INIT();

	VERUS_QREF_RENDERER;

	// Grid:
	_vGrid.reserve(92);
	const UINT32 colorMain = VERUS_COLOR_RGBA(32, 32, 32, 255);
	const UINT32 colorExtra = VERUS_COLOR_RGBA(64, 64, 64, 255);
	const int edgeMin = 5, edgeMax = 55;
	for (int i = -edgeMin; i <= edgeMin; ++i)
	{
		const UINT32 color = i ? colorExtra : colorMain;
		_vGrid.push_back(Vertex(Point3(static_cast<float>(i), 0, static_cast<float>(edgeMin)), color));
		_vGrid.push_back(Vertex(Point3(static_cast<float>(i), 0, static_cast<float>(-edgeMin)), color));
		_vGrid.push_back(Vertex(Point3(static_cast<float>(edgeMin), 0, static_cast<float>(i)), color));
		_vGrid.push_back(Vertex(Point3(static_cast<float>(-edgeMin), 0, static_cast<float>(i)), color));
	}
	for (int i = -edgeMax; i <= edgeMax; i += 10)
	{
		_vGrid.push_back(Vertex(Point3(static_cast<float>(i), 0, static_cast<float>(edgeMax)), colorExtra));
		_vGrid.push_back(Vertex(Point3(static_cast<float>(i), 0, static_cast<float>(-edgeMax)), colorExtra));
		_vGrid.push_back(Vertex(Point3(static_cast<float>(edgeMax), 0, static_cast<float>(i)), colorExtra));
		_vGrid.push_back(Vertex(Point3(static_cast<float>(-edgeMax), 0, static_cast<float>(i)), colorExtra));
	}

	// Basis:
	_vBasis.reserve(30);
	const UINT32 r = GetBasisColorX();
	const UINT32 g = GetBasisColorY();
	const UINT32 b = GetBasisColorZ();
	const float h = 0.8f;
	const float w = 0.05f;
	// X:
	_vBasis.push_back(Vertex(Point3(1, 0, 0), r));
	_vBasis.push_back(Vertex(Point3(0, 0, 0), r));
	_vBasis.push_back(Vertex(Point3(1, 0, 0), r));
	_vBasis.push_back(Vertex(Point3(h, w, 0), r));
	_vBasis.push_back(Vertex(Point3(1, 0, 0), r));
	_vBasis.push_back(Vertex(Point3(h, -w, 0), r));
	_vBasis.push_back(Vertex(Point3(1, 0, 0), r));
	_vBasis.push_back(Vertex(Point3(h, 0, w), r));
	_vBasis.push_back(Vertex(Point3(1, 0, 0), r));
	_vBasis.push_back(Vertex(Point3(h, 0, -w), r));
	// Z:
	_vBasis.push_back(Vertex(Point3(0, 0, 1), b));
	_vBasis.push_back(Vertex(Point3(0, 0, 0), b));
	_vBasis.push_back(Vertex(Point3(0, 0, 1), b));
	_vBasis.push_back(Vertex(Point3(w, 0, h), b));
	_vBasis.push_back(Vertex(Point3(0, 0, 1), b));
	_vBasis.push_back(Vertex(Point3(-w, 0, h), b));
	_vBasis.push_back(Vertex(Point3(0, 0, 1), b));
	_vBasis.push_back(Vertex(Point3(0, w, h), b));
	_vBasis.push_back(Vertex(Point3(0, 0, 1), b));
	_vBasis.push_back(Vertex(Point3(0, -w, h), b));
	// Y:
	_vBasis.push_back(Vertex(Point3(0, 1, 0), g));
	_vBasis.push_back(Vertex(Point3(0, 0, 0), g));
	_vBasis.push_back(Vertex(Point3(0, 1, 0), g));
	_vBasis.push_back(Vertex(Point3(w, h, 0), g));
	_vBasis.push_back(Vertex(Point3(0, 1, 0), g));
	_vBasis.push_back(Vertex(Point3(-w, h, 0), g));
	_vBasis.push_back(Vertex(Point3(0, 1, 0), g));
	_vBasis.push_back(Vertex(Point3(0, h, w), g));
	_vBasis.push_back(Vertex(Point3(0, 1, 0), g));
	_vBasis.push_back(Vertex(Point3(0, h, -w), g));

	// Circle:
	_vCircle.reserve(48);
	const int angle = 15;
	const int vertCount = 360 / angle;
	_vCircle.reserve(vertCount * 2);
	for (int i = angle; i <= 360; i += angle)
	{
		const float s = sin(Math::ToRadians(float(i)));
		const float c = cos(Math::ToRadians(float(i)));
		Point3 pos(s, 0, c);
		if (_vCircle.empty())
			_vCircle.push_back(Vertex(Point3(0, 0, 1), VERUS_COLOR_WHITE));
		else
			_vCircle.push_back(_vCircle.back());
		_vCircle.push_back(Vertex(pos, VERUS_COLOR_WHITE));
	}

	// Box:
	_vBox.reserve(24);
	const UINT32 colorBox = VERUS_COLOR_WHITE;
	Point3 _min(-0.5f, 0, -0.5f);
	Point3 _max(0.5f, 1, 0.5f);

	_vBox.push_back(Vertex(Point3(_min.getX(), _min.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _min.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_min.getX(), _max.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _max.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_min.getX(), _min.getY(), _max.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _min.getY(), _max.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_min.getX(), _max.getY(), _max.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _max.getY(), _max.getZ()), colorBox));

	_vBox.push_back(Vertex(Point3(_min.getX(), _min.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_min.getX(), _max.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _min.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _max.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_min.getX(), _min.getY(), _max.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_min.getX(), _max.getY(), _max.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _min.getY(), _max.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _max.getY(), _max.getZ()), colorBox));

	_vBox.push_back(Vertex(Point3(_min.getX(), _min.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_min.getX(), _min.getY(), _max.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _min.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _min.getY(), _max.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_min.getX(), _max.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_min.getX(), _max.getY(), _max.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _max.getY(), _min.getZ()), colorBox));
	_vBox.push_back(Vertex(Point3(_max.getX(), _max.getY(), _max.getZ()), colorBox));

	// Light:
	_vLight.reserve(10);
	const UINT32 colorLight = VERUS_COLOR_RGBA(255, 229, 0, 255);
	const float lightSize = 0.25f;
	_vLight.push_back(Vertex(Point3(lightSize, 0, 0), colorLight));
	_vLight.push_back(Vertex(Point3(0, 0, lightSize), colorLight));
	_vLight.push_back(Vertex(Point3(0, 0, lightSize), colorLight));
	_vLight.push_back(Vertex(Point3(-lightSize, 0, 0), colorLight));
	_vLight.push_back(Vertex(Point3(-lightSize, 0, 0), colorLight));
	_vLight.push_back(Vertex(Point3(0, 0, -lightSize), colorLight));
	_vLight.push_back(Vertex(Point3(0, 0, -lightSize), colorLight));
	_vLight.push_back(Vertex(Point3(lightSize, 0, 0), colorLight));

	_vLight.push_back(Vertex(Point3(0, -lightSize, 0), colorLight));
	_vLight.push_back(Vertex(Point3(0, lightSize, 0), colorLight));

	renderer.GetDS().Load();
}

void Helpers::Done()
{
	VERUS_DONE(Helpers);
}

void Helpers::DrawGrid()
{
	CGI::DebugDraw::I().Begin(CGI::DebugDraw::Type::lines);
	const int lineCount = Utils::Cast32(_vGrid.size() >> 1);
	VERUS_FOR(i, lineCount)
	{
		CGI::DebugDraw::I().AddLine(
			_vGrid[(i << 1)]._pos,
			_vGrid[(i << 1) + 1]._pos,
			_vGrid[(i << 1)]._color);
	}
	CGI::DebugDraw::I().End();
}

void Helpers::DrawBasis(PcTransform3 pMat, int axis, bool overlay)
{
	// Draw order is XZY:
	if (axis == 2)
		axis = 1;
	else if (axis == 1)
		axis = 2;
	const UINT32 hover = VERUS_COLOR_RGBA(200, 200, 0, 255);
	CGI::DebugDraw::I().Begin(CGI::DebugDraw::Type::lines, pMat, !overlay);
	const int lineCount = Utils::Cast32(_vBasis.size() >> 1);
	VERUS_FOR(i, lineCount)
	{
		const bool isHover = (i / 5 == axis);
		CGI::DebugDraw::I().AddLine(
			_vBasis[(i << 1) + 0]._pos,
			_vBasis[(i << 1) + 1]._pos,
			isHover ? hover : _vBasis[(i << 1) + 0]._color);
	}
	CGI::DebugDraw::I().End();
}

void Helpers::DrawCircle(RcPoint3 pos, float radius, UINT32 color, RTerrain terrain)
{
	CGI::DebugDraw::I().Begin(CGI::DebugDraw::Type::lines, nullptr, false);
	const int lineCount = Utils::Cast32(_vCircle.size() >> 1);
	Point3 a = VMath::scale(_vCircle[0]._pos, radius) + Vector3(pos);
	float xz[2] = { a.getX(), a.getZ() };
	a.setY(terrain.GetHeightAt(xz));
	VERUS_FOR(i, lineCount)
	{
		Point3 b = VMath::scale(_vCircle[(i << 1) + 1]._pos, radius) + Vector3(pos);
		xz[0] = b.getX();
		xz[1] = b.getZ();
		b.setY(terrain.GetHeightAt(xz));
		CGI::DebugDraw::I().AddLine(a, b, color);
		a = b;
	}
	CGI::DebugDraw::I().End();
}

void Helpers::DrawBox(PcTransform3 pMat, UINT32 color)
{
	CGI::DebugDraw::I().Begin(CGI::DebugDraw::Type::lines, pMat);
	const int lineCount = Utils::Cast32(_vBox.size() >> 1);
	VERUS_FOR(i, lineCount)
	{
		CGI::DebugDraw::I().AddLine(
			_vBox[(i << 1)]._pos,
			_vBox[(i << 1) + 1]._pos,
			color ? color : _vBox[(i << 1)]._color);
	}
	CGI::DebugDraw::I().End();
}

void Helpers::DrawLight(RcPoint3 pos, UINT32 color, PcPoint3 pTarget)
{
	Transform3 matW = Transform3::translation(Vector3(pos));
	CGI::DebugDraw::I().Begin(CGI::DebugDraw::Type::lines, &matW);
	const int lineCount = Utils::Cast32(_vLight.size() >> 1);
	VERUS_FOR(i, lineCount)
	{
		CGI::DebugDraw::I().AddLine(
			_vLight[(i << 1)]._pos,
			_vLight[(i << 1) + 1]._pos,
			color ? color : _vLight[(i << 1)]._color);
	}
	if (pTarget)
	{
		CGI::DebugDraw::I().AddLine(Point3(0), *pTarget - pos, color ? color : _vLight[0]._color);
	}
	CGI::DebugDraw::I().End();
}

void Helpers::DrawSphere(RcPoint3 pos, float r, UINT32 color, CGI::CommandBufferPtr cb)
{
	if (!_sphere.IsInitialized())
		_sphere.Init("[Models]:Sphere.x3d");
	if (!_sphere.IsLoaded())
		return;

	float floats[4];
	Convert::ColorInt32ToFloat(color, floats);
	const Vector4 userColor = Vector4::MakeFromPointer(floats);

	const float d = r * 2;
	const Transform3 tr = VMath::appendScale(Transform3::translation(Vector3(pos)), Vector3(d, d, d));

	Mesh::DrawDesc drawDesc;
	drawDesc._matW = tr;
	drawDesc._userColor = userColor;
	drawDesc._pipe = Mesh::PIPE_WIREFRAME;
	drawDesc._bindMaterial = false;
	drawDesc._bindSkeleton = false;
	_sphere.Draw(drawDesc, cb);
}

UINT32 Helpers::GetBasisColorX(bool linear, int alpha)
{
	return linear ? VERUS_COLOR_RGBA(255, 1, 1, alpha) : VERUS_COLOR_RGBA(255, 10, 10, alpha);
}

UINT32 Helpers::GetBasisColorY(bool linear, int alpha)
{
	return linear ? VERUS_COLOR_RGBA(1, 118, 1, alpha) : VERUS_COLOR_RGBA(10, 180, 10, alpha);
}

UINT32 Helpers::GetBasisColorZ(bool linear, int alpha)
{
	return linear ? VERUS_COLOR_RGBA(31, 31, 255, alpha) : VERUS_COLOR_RGBA(99, 99, 255, alpha);
}
