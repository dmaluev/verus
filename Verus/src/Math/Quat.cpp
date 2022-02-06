// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;

bool Quat::IsEqual(RcQuat that, float e) const
{
	return
		glm::epsilonEqual(static_cast<float>(getX()), static_cast<float>(that.getX()), e) &&
		glm::epsilonEqual(static_cast<float>(getY()), static_cast<float>(that.getY()), e) &&
		glm::epsilonEqual(static_cast<float>(getZ()), static_cast<float>(that.getZ()), e) &&
		glm::epsilonEqual(static_cast<float>(getW()), static_cast<float>(that.getW()), e);
}

bool Quat::IsIdentity() const
{
	const float epsilon = 0.001f;
	const bool posUnit =
		abs(getW() - 1) < epsilon &&
		abs(getX() - 0) < epsilon &&
		abs(getY() - 0) < epsilon &&
		abs(getZ() - 0) < epsilon;
	const bool negUnit =
		abs(getW() + 1) < epsilon &&
		abs(getX() + 0) < epsilon &&
		abs(getY() + 0) < epsilon &&
		abs(getZ() + 0) < epsilon;
	return posUnit || negUnit;
}
