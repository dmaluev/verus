// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#include "Groups.h"
#include "UserPtr.h"
#include "Spring.h"
#include "BulletDebugDraw.h"
#include "Bullet.h"
#include "KinematicCharacterController.h" // Improved btKinematicCharacterController.
#include "CharacterController.h"
#include "Vehicle.h"

namespace verus
{
	void Make_Physics();
	void Free_Physics();
}
