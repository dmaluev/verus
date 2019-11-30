#pragma once

#include "Groups.h"
#include "UserPtr.h"
#include "BulletDebugDraw.h"
#include "Bullet.h"
#include "KinematicCharacterController.h" // Improved btKinematicCharacterController.
#include "CharacterController.h"

namespace verus
{
	void Make_Physics();
	void Free_Physics();
}
