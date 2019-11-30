#pragma once

namespace verus
{
	namespace Physics
	{
		class BulletDebugDraw : public btIDebugDraw
		{
			static const int s_maxDrawDist = 30;

			int _debugMode = 0;

		public:
			BulletDebugDraw() : _debugMode(btIDebugDraw::DBG_DrawWireframe) {}
			~BulletDebugDraw() {}

			virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
			virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& fromColor, const btVector3& toColor) override;

			virtual void drawSphere(const btVector3& p, btScalar radius, const btVector3& color) override;
			virtual void drawBox(const btVector3& boxMin, const btVector3& boxMax, const btVector3& color) override;

			virtual void drawTriangle(const btVector3& a, const btVector3& b, const btVector3& c, const btVector3& color, btScalar alpha) override;

			virtual void drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override;

			virtual void reportErrorWarning(const char* warningString) override;

			virtual void draw3dText(const btVector3& location, const char* textString) override;

			virtual void drawTransform(const btTransform& transform, btScalar orthoLen) override;

			virtual void setDebugMode(int debugMode) override { _debugMode = debugMode; }
			virtual int getDebugMode() const override { return _debugMode; }
		};
		VERUS_TYPEDEFS(BulletDebugDraw);
	}
}
