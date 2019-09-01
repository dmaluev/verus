#include "verus.h"

namespace verus
{
	void Make_Physics()
	{
		Physics::Bullet::Make();
	}
	void Free_Physics()
	{
		Physics::Bullet::Free();
	}
}
