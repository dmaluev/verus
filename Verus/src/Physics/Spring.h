#pragma once

namespace verus
{
	namespace Physics
	{
		class Spring
		{
			Vector3 _offset = Vector3(0);
			Vector3 _velocity = Vector3(0);
			float   _stiffness = 50;
			float   _damping = 2;

		public:
			Spring(float stiffness = 50, float damping = 2);

			void Update(RcVector3 appliedForce);

			RcVector3 GetOffset() const { return _offset; }
		};
		VERUS_TYPEDEFS(Spring);
	}
}
