// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Physics;

void BulletDebugDraw::drawLine(const btVector3& from, const btVector3& to,
	const btVector3& color)
{
	VERUS_QREF_WM;
	const float maxDistSq = s_maxDrawDist * s_maxDrawDist;
	const Point3 headPos = wm.GetHeadCamera()->GetEyePosition();
	if (VMath::distSqr(headPos, Point3(from)) >= maxDistSq &&
		VMath::distSqr(headPos, Point3(to)) >= maxDistSq)
		return;

	VERUS_QREF_DD;
	dd.AddLine(from, to, Convert::ColorFloatToInt32(color) | VERUS_COLOR_BLACK);
}

void BulletDebugDraw::drawLine(const btVector3& from, const btVector3& to,
	const btVector3& fromColor, const btVector3& toColor)
{
	VERUS_QREF_WM;
	const float maxDistSq = s_maxDrawDist * s_maxDrawDist;
	const Point3 headPos = wm.GetHeadCamera()->GetEyePosition();
	if (VMath::distSqr(headPos, Point3(from)) >= maxDistSq &&
		VMath::distSqr(headPos, Point3(to)) >= maxDistSq)
		return;

	VERUS_QREF_DD;
	dd.AddLine(from, to, Convert::ColorFloatToInt32(fromColor) | VERUS_COLOR_BLACK);
}

void BulletDebugDraw::drawSphere(const btVector3& p, btScalar radius, const btVector3& color)
{
}

void BulletDebugDraw::drawBox(const btVector3& boxMin, const btVector3& boxMax,
	const btVector3& color)
{
}

void BulletDebugDraw::drawTriangle(const btVector3& a, const btVector3& b, const btVector3& c,
	const btVector3& color, btScalar alpha)
{
	VERUS_QREF_DD;
	Vector4 colorEx = color;
	colorEx.setW(alpha);
	const UINT32 color32 = Convert::ColorFloatToInt32(colorEx.ToPointer());
	dd.AddLine(a, b, color32);
	dd.AddLine(b, c, color32);
	dd.AddLine(c, a, color32);
}

void BulletDebugDraw::drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB,
	btScalar distance, int lifeTime, const btVector3& color)
{
}

void BulletDebugDraw::reportErrorWarning(const char* warningString)
{
}

void BulletDebugDraw::draw3dText(const btVector3& location, const char* textString)
{
}

void BulletDebugDraw::drawTransform(const btTransform& transform, btScalar orthoLen)
{
	btIDebugDraw::drawTransform(transform, 0.1f);
}
