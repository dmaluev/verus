#include "verus.h"

using namespace verus;
using namespace verus::Anim;

Skeleton::Skeleton()
{
}

Skeleton::~Skeleton()
{
	Done();
}

void Skeleton::operator=(RcSkeleton that)
{
	Init();
	for (const auto& kv : that._mapBones)
		_mapBones[kv.first] = kv.second;
	_numPrimaryBones = that._numPrimaryBones;
}

void Skeleton::Init()
{
	VERUS_INIT();
}

void Skeleton::Done()
{
	EndRagdoll();
	VERUS_DONE(Skeleton);
}

void Skeleton::Draw(bool bindPose, int selected)
{
#if 0
	VERUS_QREF_DR;
	dr.Begin(CGL::CDebugRender::T_LINES, nullptr, false);
	for (const auto& kv : _mapBones)
	{
		PcBone pBone = &kv.second;
		PcBone pParent = FindBone(_C(pBone->_parentName));
		if (pParent)
		{
			if (bindPose)
			{
				dr.AddLine(
					pParent->_matFromBoneSpace.getTranslation(),
					pBone->_matFromBoneSpace.getTranslation(),
					VERUS_COLOR_WHITE);
			}
			else
			{
				dr.AddLine(
					(pParent->_matFinal*pParent->_matFromBoneSpace).getTranslation(),
					(pBone->_matFinal*pBone->_matFromBoneSpace).getTranslation(),
					VERUS_COLOR_WHITE);
			}
		}
	}
	for (const auto& kv : _mapBones)
	{
		PcBone pBone = &kv.second;
		const Transform3 mat = bindPose ? pBone->_matFromBoneSpace : pBone->_matFinal*pBone->_matFromBoneSpace;

		const float scale = 0.04f;
		Point3 a(0);
		Point3 x(scale, 0, 0);
		Point3 y(0, scale, 0);
		Point3 z(0, 0, scale);
		a = mat * a;
		x = mat * x;
		y = mat * y;
		z = mat * z;

		dr.AddLine(a, x, VERUS_COLOR_RGBA(255, 0, 0, 255));
		dr.AddLine(a, y, VERUS_COLOR_RGBA(0, 255, 0, 255));
		dr.AddLine(a, z, VERUS_COLOR_RGBA(0, 0, 255, 255));

		if (pBone->_shaderIndex == selected)
		{
			const float scale2 = scale * 0.5f;
			Point3 x(scale2, 0, 0);
			Point3 y(0, scale2, 0);
			Point3 z(0, 0, scale2);
			x = mat * x;
			y = mat * y;
			z = mat * z;

			dr.AddLine(x, y, VERUS_COLOR_RGBA(255, 255, 0, 255));
			dr.AddLine(y, z, VERUS_COLOR_RGBA(255, 255, 0, 255));
			dr.AddLine(z, x, VERUS_COLOR_RGBA(255, 255, 0, 255));
		}
	}
	dr.End();
#endif
}

Skeleton::PBone Skeleton::InsertBone(RBone bone)
{
	PBone p = FindBone(_C(bone._name));
	if (p)
		return p;

	bone._shaderIndex = Utils::Cast32(_mapBones.size()); // Assume bones are added in same order as matrices in shader.
	_mapBones[bone._name] = bone;
	return FindBone(_C(bone._name));
}

Skeleton::PBone Skeleton::FindBone(CSZ name)
{
	VERUS_IF_FOUND_IN(TMapBones, _mapBones, name, it)
		return &it->second;
	return nullptr;
}

Skeleton::PcBone Skeleton::FindBone(CSZ name) const
{
	VERUS_IF_FOUND_IN(TMapBones, _mapBones, name, it)
		return &it->second;
	return nullptr;
}

Skeleton::PBone Skeleton::FindBoneByIndex(int index)
{
	for (auto& kv : _mapBones)
	{
		if (kv.second._shaderIndex == index)
			return &kv.second;
	}
	return nullptr;
}

void Skeleton::ApplyMotion(RMotion motion, float time, int numAlphaMotions, PAlphaMotion pAlphaMotions)
{
	if (_ragdollMode)
	{
		for (auto& kv : _mapBones)
		{
			RBone bone = kv.second;
			if (bone._pBody)
			{
				bone._matFinal = _matRagdollToWorldInv * Transform3(bone._pBody->getWorldTransform())*bone._matToActorSpace;
			}
			else
			{
				bone._matFinal = Transform3::identity();
				if (bone._shaderIndex >= 0)
				{
					PBone pParent = FindBone(_C(bone._parentName));
					while (pParent)
					{
						if (pParent->_pBody)
						{
							bone._matFinal = _matRagdollToWorldInv * Transform3(pParent->_pBody->getWorldTransform())*pParent->_matToActorSpace;
							break;
						}
						pParent = FindBone(_C(pParent->_parentName));
					}
				}
			}
			bone._matFinalInv = VMath::inverse(bone._matFinal);
		}
		return;
	}

	_pCurrentMotion = &motion;
	_currentTime = time * _pCurrentMotion->GetPlaybackSpeed(); // To native time.
	if (_pCurrentMotion->IsReversed())
		_currentTime = _pCurrentMotion->GetNativeDuration() - _currentTime;

	_numAlphaMotions = numAlphaMotions;
	_pAlphaMotions = pAlphaMotions;

	ResetBones();
	for (auto& kv : _mapBones)
	{
		RBone bone = kv.second;
		if (!bone._ready)
		{
			_pCurrentBone = &bone;
			_matParents = Transform3::identity();
			RecursiveBoneUpdate();
		}
	}

	// Reset blend motion!
	motion.BindBlendMotion(nullptr, 0);
	VERUS_FOR(i, _numAlphaMotions)
	{
		if (_pAlphaMotions[i]._pMotion)
			_pAlphaMotions[i]._pMotion->BindBlendMotion(nullptr, 0);
	}
}

void Skeleton::UpdateUniformBufferArray(mataff* p) const
{
	for (const auto& kv : _mapBones)
	{
		RcBone bone = kv.second;
		if (bone._shaderIndex >= 0 && bone._shaderIndex < VERUS_MAX_NUM_BONES)
			p[bone._shaderIndex] = bone._matFinal.UniformBufferFormat();
	}
}

void Skeleton::ResetFinalPose()
{
	for (auto& kv : _mapBones)
	{
		kv.second._matFinal = Transform3::identity();
		kv.second._matFinalInv = Transform3::identity();
	}
}

void Skeleton::ResetBones()
{
	for (auto& kv : _mapBones)
		kv.second._ready = false;
}

void Skeleton::RecursiveBoneUpdate()
{
	Transform3 mat;
	PBone pCurrentBone = _pCurrentBone;

	Motion::PBone pMotionBone = _pCurrentMotion->FindBone(_C(pCurrentBone->_name));

	if (pMotionBone)
	{
		Quat q;
		Vector3 scale, euler, pos;
		pMotionBone->ComputeRotationAt(_currentTime, euler, q);
		pMotionBone->ComputePositionAt(_currentTime, pos);
		pMotionBone->ComputeScaleAt(_currentTime, scale);

		// Blend with other motions:
		VERUS_FOR(i, _numAlphaMotions)
		{
			if (!_pAlphaMotions[i]._pMotion)
				continue;

			PMotion pAlphaMotion = _pAlphaMotions[i]._pMotion;
			Motion::PBone pAlphaBone = pAlphaMotion->FindBone(_C(pCurrentBone->_name));
			const float alpha = _pAlphaMotions[i]._alpha;
			float time = _pAlphaMotions[i]._time*pAlphaMotion->GetPlaybackSpeed(); // To native time.
			if (pAlphaMotion->IsReversed())
				time = pAlphaMotion->GetNativeDuration() - time;
			if (pAlphaBone && alpha > 0 &&
				(pCurrentBone->_name == _pAlphaMotions[i]._rootBone ||
					IsParentOf(_C(pCurrentBone->_name), _pAlphaMotions[i]._rootBone)))
			{
				// Alpha motion can also be in blend state.
				Quat qA;
				Vector3 scaleA, eulerA, posA;
				pAlphaBone->ComputeRotationAt(time, eulerA, qA);
				pAlphaBone->ComputePositionAt(time, posA);
				pAlphaBone->ComputeScaleAt(time, scaleA);

				// Mix with alpha motion:
				q = VMath::slerp(alpha, q, qA);
				pos = VMath::lerp(alpha, pos, posA);
				scale = VMath::lerp(alpha, scale, scaleA);
			}
		}

		const Transform3 matSRT = VMath::appendScale(Transform3(q, pos), scale);
		const Transform3 matBone = pCurrentBone->_matExternal*matSRT;
		mat = pCurrentBone->_matFromBoneSpace*matBone*pCurrentBone->_matToBoneSpace*pCurrentBone->_matAdapt;
	}
	else
		mat = Transform3::identity();

	_pCurrentBone = FindBone(_C(pCurrentBone->_parentName));
	if (_pCurrentBone)
	{
		if (_pCurrentBone->_ready)
			_matParents = _pCurrentBone->_matFinal;
		else
			RecursiveBoneUpdate();
	}
	else if (pMotionBone = _pCurrentMotion->FindBone(RootName()))
	{
		Quat q;
		Vector3 scale, euler, pos;
		pMotionBone->ComputeRotationAt(_currentTime, euler, q);
		pMotionBone->ComputePositionAt(_currentTime, pos);
		pMotionBone->ComputeScaleAt(_currentTime, scale);
		const Transform3 matSRT = VMath::appendScale(Transform3(q, pos), scale);
		mat = matSRT * mat;
	}

	_matParents = _matParents * mat;
	pCurrentBone->_matFinal = _matParents;
	pCurrentBone->_matFinalInv = VMath::inverse(_matParents);
	pCurrentBone->_ready = true;
}

void Skeleton::VisitBones(std::function<Continue(RBone)> fn)
{
	for (auto& kv : _mapBones)
		if (Continue::yes != fn(kv.second))
			return;
}

void Skeleton::VisitBones(std::function<Continue(RcBone)> fn) const
{
	for (const auto& kv : _mapBones)
		if (Continue::yes != fn(kv.second))
			return;
}

void Skeleton::InsertBonesIntoMotion(RMotion motion) const
{
	for (const auto& kv : _mapBones)
		motion.InsertBone(_C(kv.first));
}

void Skeleton::DeleteOutsiders(RMotion motion) const
{
	const int num = motion.GetNumBones();
	Vector<String> vNames;
	vNames.reserve(num);
	VERUS_FOR(i, num)
	{
		CSZ name = _C(motion.GetBoneByIndex(i)->GetName());
		if (_mapBones.find(name) == _mapBones.end() && name != RootName())
			vNames.push_back(name);
	}
	VERUS_FOREACH_CONST(Vector<String>, vNames, it)
		motion.DeleteBone(_C(*it));
}

void Skeleton::AdjustPrimaryBones(const Vector<String>& vPrimaryBones)
{
	typedef Map<int, String> TMapSort;
	TMapSort mapSort;

	static const int secondaryOffset = 1000000;

	const bool inverse = (vPrimaryBones.end() != std::find(vPrimaryBones.begin(), vPrimaryBones.end(), "-"));
	int addPri = 0;
	int addSec = secondaryOffset;
	if (inverse)
		std::swap(addPri, addSec);

	for (const auto& kv : _mapBones)
	{
		RcBone bone = kv.second;
		if (vPrimaryBones.end() != std::find(vPrimaryBones.begin(), vPrimaryBones.end(), bone._name))
			mapSort[bone._shaderIndex + addPri] = bone._name;
		else
			mapSort[bone._shaderIndex + addSec] = bone._name;
	}

	_numPrimaryBones = 0;

	for (const auto& kv : mapSort)
	{
		if (kv.first < secondaryOffset) // Primary bone:
		{
			PBone pBone = FindBone(_C(kv.second));
			const int newIndex = Utils::Cast32(_mapPrimary.size());
			_mapPrimary[pBone->_shaderIndex] = newIndex;
			pBone->_shaderIndex = newIndex;
			_numPrimaryBones++;
		}
		else // Secondary bone:
		{
			PBone pBone = FindBone(_C(kv.second));
			PcBone pParent = FindBone(_C(pBone->_parentName));
			bool isPrimary = false;
			while (!isPrimary)
			{
				isPrimary = (vPrimaryBones.end() != std::find(vPrimaryBones.begin(), vPrimaryBones.end(), pParent->_name));
				if (inverse)
					isPrimary = !isPrimary;
				if (!isPrimary)
					pParent = FindBone(_C(pParent->_parentName));
			}
			_mapPrimary[pBone->_shaderIndex] = pParent->_shaderIndex;
			pBone->_shaderIndex = -1; // Do not set any vertex shader's register.
		}
	}
}

int Skeleton::RemapBoneIndex(int index) const
{
	VERUS_IF_FOUND_IN(TMapPrimary, _mapPrimary, index, it)
		return it->second;
	return 0;
}

bool Skeleton::IsParentOf(CSZ bone, CSZ parent) const
{
	PcBone pBone = FindBone(bone);
	while (pBone)
	{
		if (pBone->_parentName == parent)
			return true;
		pBone = FindBone(_C(pBone->_parentName));
	}
	return false;
}

void Skeleton::LoadRigInfo(CSZ url)
{
	Vector<BYTE> v;
	IO::FileSystem::LoadResource(url, v, IO::FileSystem::LoadDesc(true));
	LoadRigInfoFromPtr(v.data());
}

void Skeleton::LoadRigInfoFromPtr(const BYTE* p)
{
	for (auto& kv : _mapBones)
	{
		RBone bone = kv.second;
		bone._rigRot = Vector3(0);
		bone._cRot = Vector3(0);
		bone._cLimits = Vector3(0);
		bone._boxSize = Vector3(0);
		bone._width = 0;
		bone._length = 0;
		bone._mass = 0.01f;
		bone._rigBone = true;
		bone._hinge = false;
		bone._noCollision = false;
	}

	float massCheck = 0;
	if (p)
	{
		for (auto& kv : _mapBones)
		{
			RBone bone = kv.second;
			bone._rigBone = false;
		}

		tinyxml2::XMLDocument doc;
		doc.Parse(reinterpret_cast<CSZ>(p));
		if (doc.Error())
			return;

		tinyxml2::XMLElement* pElem = nullptr;
		tinyxml2::XMLElement* pRoot = doc.FirstChildElement();
		if (!pRoot)
			return;

		pElem = pRoot->FirstChildElement("mass");
		if (pElem)
			_mass = static_cast<float>(atof(pElem->GetText()));

		for (pElem = pRoot->FirstChildElement("bone"); pElem; pElem = pElem->NextSiblingElement("bone"))
		{
			PBone pBone = FindBone(pElem->Attribute("name"));
			if (pBone)
			{
				pBone->_rigBone = true;
				pElem->QueryFloatAttribute("w", &pBone->_width);
				pElem->QueryFloatAttribute("l", &pBone->_length);
				CSZ rot = pElem->Attribute("rot");
				if (rot)
					pBone->_rigRot.FromString(rot);

				CSZ climit = pElem->Attribute("climit");
				if (climit)
					pBone->_cLimits.FromString(climit);

				CSZ ctype = pElem->Attribute("ctype");
				if (ctype)
				{
					if (!strcmp(ctype, "hinge"))
						pBone->_hinge = true;
					if (!strcmp(ctype, "free"))
						pBone->_cLimits.setZ(-100);
				}

				CSZ crot = pElem->Attribute("crot");
				if (crot)
					pBone->_cRot.FromString(crot);

				CSZ boxSize = pElem->Attribute("boxSize");
				if (boxSize)
					pBone->_boxSize.FromString(boxSize);

				float pmass = 0;
				pElem->QueryFloatAttribute("m", &pmass);
				if (pmass != 0)
					pBone->_mass = pmass * _mass;

				pElem->QueryBoolAttribute("noCollision", &pBone->_noCollision);

				massCheck += pBone->_mass;
			}
		}
	}
	if (massCheck > 1000)
	{
		VERUS_LOG_DEBUG("Ton");
	}
}

void Skeleton::BeginRagdoll(RcTransform3 matW, RcVector3 impulse, CSZ bone)
{
	VERUS_QREF_BULLET;

	EndRagdoll();

	_matRagdollToWorld = matW;
	_matRagdollToWorldInv = VMath::inverse(matW);

	// RagdollDemo values:
	const float dampL = 0.05f;
	const float dampA = 0.85f*1.15f;
	const float daTime = 0.8f;
	const float sleepL = 1.6f;
	const float sleepA = 2.5f;

	for (auto& kv : _mapBones)
		kv.second._ready = true;

	for (const auto& kv : _mapBones)
	{
		PcBone pBone = &kv.second;
		PBone pParent = FindBone(_C(pBone->_parentName));

		if (pParent && pParent->_rigBone && !pParent->_pShape) // Create a shape for parent bone:
		{
			const Point3 a = pParent->_matFromBoneSpace.getTranslation();
			const Point3 b = pBone->_matFromBoneSpace.getTranslation();

			if (VMath::distSqr(a, b) < 0.01f*0.01f) // Too short, unsuitable?
			{
				pParent->_ready = false; // Mark it.
				continue;
			}

			// Pick the correct child bone:
			if (0 == pParent->_length)
			{
				const Vector3 test = VMath::normalizeApprox(Vector3(pParent->_matToBoneSpace*b));
				if (test.getX() < 0.9f) // Bone should point to child bone.
					continue;
			}

			const float len = (pParent->_length != 0) ?
				pParent->_length : Math::Max(static_cast<float>(VMath::dist(a, b)), 0.01f);
			const float w = (pParent->_width != 0) ? pParent->_width : len * 0.3f;
			pParent->_length = len;
			pParent->_width = w;

			if (pParent->_boxSize.IsZero())
			{
				pParent->_pShape = new btCapsuleShape(w, len);
			}
			else
			{
				if (0 == pParent->_boxSize.getY())
					pParent->_boxSize.setY(len*0.5f);
				pParent->_pShape = new btBoxShape(pParent->_boxSize.Bullet());
			}

			Transform3 matBody;
			if (pParent->_rigRot.IsZero())
			{
				matBody = pParent->_matFromBoneSpace*
					Transform3(Matrix3::rotationZ(Math::ToRadians(90)), Vector3(len*0.5f, 0, 0));
			}
			else
			{
				const Transform3 matR = Transform3::rotationZYX(pParent->_rigRot);
				const Transform3 matT = Transform3(Matrix3::rotationZ(Math::ToRadians(90)), Vector3(len*0.5f, 0, 0));
				matBody = pParent->_matFromBoneSpace*matR*matT;
			}
			pParent->_matToActorSpace = VMath::inverse(matBody);
			matBody = _matRagdollToWorld * pParent->_matFinal*matBody;

			short group = 1, mask = -1;
			if (pParent->_noCollision)
				group = mask = 0;
			const btTransform t = matBody.Bullet();
			pParent->_pBody = bullet.AddNewRigidBody(pParent->_mass, t, pParent->_pShape, group, mask);
			pParent->_pBody->setFriction(Physics::Bullet::GetFriction(Physics::Material::leather));
			pParent->_pBody->setRestitution(Physics::Bullet::GetRestitution(Physics::Material::leather)*0.5f);
			pParent->_pBody->setDamping(dampL, dampA);
			pParent->_pBody->setDeactivationTime(daTime);
			pParent->_pBody->setSleepingThresholds(sleepL, sleepA);
		}
	}

	// Aligns constraint to good starting point:
	const Transform3 matInitC = Transform3::rotationZYX(Vector3(-VERUS_PI / 2, 0, -VERUS_PI / 2));

	// Create leaf actors and joints:
	for (auto& kv : _mapBones)
	{
		PBone pBone = &kv.second;
		PBone pParent = pBone;
		do
		{
			pParent = FindBone(_C(pParent->_parentName));
		} while (pParent && (!pParent->_rigBone || !pParent->_ready)); // Skip unsuitable bones.

		bool isLeaf = false;
		if (pBone->_rigBone && !pBone->_pShape && pBone->_ready)
		{
			const float len = (pBone->_length != 0) ? pBone->_length : 0.1f;
			const float w = (pBone->_width != 0) ? pBone->_width : len * 0.3f;
			pBone->_length = len;
			pBone->_width = w;

			pBone->_pShape = new btCapsuleShape(w, len); // Leaf is always a capsule.

			Transform3 matBody;
			if (pBone->_rigRot.IsZero())
			{
				matBody = pBone->_matFromBoneSpace*
					Transform3(Matrix3::rotationZ(Math::ToRadians(90)), Vector3(len*0.5f, 0, 0));
			}
			else
			{
				const Transform3 matR = Transform3::rotationZYX(pBone->_rigRot);
				const Transform3 matT = Transform3(Matrix3::rotationZ(Math::ToRadians(90)), Vector3(len*0.5f, 0, 0));
				matBody = pBone->_matFromBoneSpace*matR*matT;
			}
			pBone->_matToActorSpace = VMath::inverse(matBody);
			matBody = _matRagdollToWorld * pBone->_matFinal*matBody;

			short group = 1, mask = -1;
			if (pBone->_noCollision)
				group = mask = 0;
			const btTransform t = matBody.Bullet();
			pBone->_pBody = bullet.AddNewRigidBody(pBone->_mass, t, pBone->_pShape, group, mask);
			pBone->_pBody->setFriction(Physics::Bullet::GetFriction(Physics::Material::leather));
			pBone->_pBody->setRestitution(Physics::Bullet::GetRestitution(Physics::Material::leather)*0.5f);
			pBone->_pBody->setDamping(dampL, dampA);
			pBone->_pBody->setDeactivationTime(daTime);
			pBone->_pBody->setSleepingThresholds(sleepL, sleepA);

			isLeaf = true;
		}

		if (pBone->_pShape && pParent && pParent->_pShape && pBone->_cLimits.getZ() > -99) // Connect bones?
		{
			btTransform localA, localB;
			const Transform3 matToParentSpace = pParent->_matToActorSpace*VMath::inverse(pBone->_matToActorSpace);

			const Transform3 matR = Transform3::rotationZYX(pBone->_cRot);
			const Transform3 matT = Transform3::translation(Vector3(0, pBone->_length*0.5f, 0));
			localA = Transform3(matT*matInitC*matR).Bullet();
			localB = Transform3(matToParentSpace*matT*matInitC*matR).Bullet();

			if (pBone->_hinge)
			{
				btHingeConstraint* pHingeC = new btHingeConstraint(*pBone->_pBody, *pParent->_pBody, localA, localB);
				if (!pBone->_cLimits.IsZero())
					pHingeC->setLimit(pBone->_cLimits.getX(), pBone->_cLimits.getY());
				else
					pHingeC->setLimit(-VERUS_PI / 2, VERUS_PI / 2);
				pBone->_pConstraint = pHingeC;
				bullet.GetWorld()->addConstraint(pBone->_pConstraint, true);
			}
			else
			{
				btConeTwistConstraint* pConeC = new btConeTwistConstraint(*pBone->_pBody, *pParent->_pBody, localA, localB);
				if (!pBone->_cLimits.IsZero())
					pConeC->setLimit(pBone->_cLimits.getX(), pBone->_cLimits.getY(), pBone->_cLimits.getZ(), 0.9f);
				else
					pConeC->setLimit(VERUS_PI / 4, VERUS_PI / 4, VERUS_PI / 4, 0.9f);
				if (!isLeaf)
					pConeC->setFixThresh(1);
				pBone->_pConstraint = pConeC;
				bullet.GetWorld()->addConstraint(pBone->_pConstraint, true);
			}
		}

		if (pBone->_name == bone && pBone->_pBody)
			pBone->_pBody->applyCentralImpulse(impulse.Bullet());
	}

	_ragdollMode = true;
}

void Skeleton::EndRagdoll()
{
	VERUS_QREF_BULLET;

	for (auto& kv : _mapBones)
	{
		RBone bone = kv.second;
		if (bone._pConstraint)
		{
			bullet.GetWorld()->removeConstraint(bone._pConstraint);
			delete bone._pConstraint;
			bone._pConstraint = nullptr;
		}
	}

	for (auto& kv : _mapBones)
	{
		RBone bone = kv.second;
		if (bone._pBody)
		{
			bullet.GetWorld()->removeRigidBody(bone._pBody);
			delete bone._pBody->getMotionState();
			delete bone._pBody;
			bone._pBody = nullptr;
		}
		if (bone._pShape)
		{
			delete bone._pShape;
			bone._pShape = nullptr;
		}
	}

	_ragdollMode = false;
}

void Skeleton::BakeMotion(RMotion motion, int frame, bool kinect)
{
	for (const auto& kv : _mapBones)
	{
		RcBone bone = kv.second;
		if (kinect && !IsKinectBone(_C(bone._name)))
			continue;
		PcBone pParent = FindBone(_C(bone._parentName));

		Transform3 matParent, matFromParentSpace;
		if (pParent)
		{
			matParent = pParent->_matFinal*pParent->_matFromBoneSpace;
			matFromParentSpace = pParent->_matFromBoneSpace;
		}
		else
		{
			matParent = Transform3::identity();
			matFromParentSpace = Transform3::identity();
		}

		Motion::PBone pMotionBone = motion.FindBone(_C(bone._name));
		if (!pMotionBone)
			pMotionBone = motion.InsertBone(_C(bone._name));

		// Actor in parent actor's space is transformed to bind pose space
		// and then to bone space:
		const Transform3 mat =
			bone._matToBoneSpace*matFromParentSpace*VMath::inverse(matParent)*
			bone._matFinal*bone._matFromBoneSpace;

		Quat q(mat.getUpper3x3());
		Vector3 pos = mat.getTranslation();

		if (glm::epsilonEqual<float>(q.getX(), 0, VERUS_FLOAT_THRESHOLD)) q.setX(0);
		if (glm::epsilonEqual<float>(q.getY(), 0, VERUS_FLOAT_THRESHOLD)) q.setY(0);
		if (glm::epsilonEqual<float>(q.getZ(), 0, VERUS_FLOAT_THRESHOLD)) q.setZ(0);
		if (glm::epsilonEqual<float>(q.getW(), 1, VERUS_FLOAT_THRESHOLD)) q.setW(1);
		if (glm::epsilonEqual<float>(pos.getX(), 0, VERUS_FLOAT_THRESHOLD)) pos.setX(0);
		if (glm::epsilonEqual<float>(pos.getY(), 0, VERUS_FLOAT_THRESHOLD)) pos.setY(0);
		if (glm::epsilonEqual<float>(pos.getZ(), 0, VERUS_FLOAT_THRESHOLD)) pos.setZ(0);

		if (!Math::IsNaN(q.getX()) &&
			!Math::IsNaN(q.getY()) &&
			!Math::IsNaN(q.getZ()) &&
			!Math::IsNaN(q.getW()))
			pMotionBone->InsertKeyframeRotation(frame, q);
		if (!kinect)
			pMotionBone->InsertKeyframePosition(frame, pos);
	}

	// Set values for non-kinect bones:
	if (kinect)
	{
		Vector3 euler;
		Quat q, qI(0);
		Motion::PBone pSrcBone, pDstBone, pExtBone;

		pSrcBone = motion.FindBone("ShoulderLeft");
		pDstBone = motion.FindBone("ShoulderLeft0");
		if (pSrcBone && pDstBone)
		{
			pSrcBone->FindKeyframeRotation(frame, euler, q);
			pSrcBone->InsertKeyframeRotation(frame, VMath::slerp(0.8f, qI, q));
			pDstBone->InsertKeyframeRotation(frame, VMath::slerp(0.2f, qI, q));
		}

		pSrcBone = motion.FindBone("ShoulderRight");
		pDstBone = motion.FindBone("ShoulderRight0");
		if (pSrcBone && pDstBone)
		{
			pSrcBone->FindKeyframeRotation(frame, euler, q);
			pSrcBone->InsertKeyframeRotation(frame, VMath::slerp(0.8f, qI, q));
			pDstBone->InsertKeyframeRotation(frame, VMath::slerp(0.2f, qI, q));
		}

		pSrcBone = motion.FindBone("Spine");
		pDstBone = motion.FindBone("Spine1");
		pExtBone = motion.FindBone("Spine2");
		if (pSrcBone && pDstBone && pExtBone)
		{
			pSrcBone->FindKeyframeRotation(frame, euler, q);
			pSrcBone->InsertKeyframeRotation(frame, VMath::slerp(0.6f, qI, q));
			pDstBone->InsertKeyframeRotation(frame, VMath::slerp(0.2f, qI, q));
			pExtBone->InsertKeyframeRotation(frame, VMath::slerp(0.2f, qI, q));
		}
	}
}

void Skeleton::AdaptBindPoseOf(RcSkeleton that)
{
	// TODO: improve.

	const Point3 origin(0);
	for (auto& kv : _mapBones)
	{
		RBone boneDst = kv.second;
		PBone pParentDst = FindBone(_C(boneDst._parentName));
		PcBone pBoneSrc = that.FindBone(_C(boneDst._name));
		PcBone pParentSrc = pBoneSrc ? that.FindBone(_C(pBoneSrc->_parentName)) : nullptr;

		if (boneDst._parentName != "Shoulder.L" &&
			boneDst._parentName != "Shoulder.R")
			continue;

		if (pParentDst && pParentSrc)
		{
			const Point3 d0 = pParentDst->_matFromBoneSpace*origin;
			const Point3 d1 = boneDst._matFromBoneSpace*origin;
			const Point3 s0 = pParentSrc->_matFromBoneSpace*origin;
			const Point3 s1 = pBoneSrc->_matFromBoneSpace*origin;

			Vector3 vd = d1 - d0;
			Vector3 vs = s1 - s0;
			const float ld = VMath::length(vd);
			const float ls = VMath::length(vs);
			if (ld < 1e-6f || ls < 1e-6f)
				continue;
			vd /= ld;
			vs /= ls;
			const Matrix3 matWorldR = Matrix3::MakeRotateTo(vd, vs);

			const Point3 offset = pParentDst->_matFromBoneSpace*origin;
			const Transform3 matToBoneOrigin = Transform3::translation(-Vector3(offset));
			const Transform3 matFromBoneOrigin = Transform3::translation(Vector3(offset));

			pParentDst->_matAdapt = matFromBoneOrigin * Transform3(matWorldR, Vector3(0))*matToBoneOrigin;
		}
	}
}

void Skeleton::SimpleIK(CSZ boneDriven, CSZ boneDriver, RcVector3 dirDriverSpace, RcVector3 dirDesiredMeshSpace, float limitDot, float alpha)
{
	PBone pBoneDriven = FindBone(boneDriven);
	Vector3 dirActual = dirDriverSpace;
	if (boneDriver)
	{
		PBone pBoneDriver = FindBone(boneDriver);
		dirActual = pBoneDriver->_matFinal.getUpper3x3()*pBoneDriver->_matFromBoneSpace.getUpper3x3()*dirDriverSpace;
	}
	Vector3 dirDesired = dirDesiredMeshSpace;
	dirDesired.LimitDot(dirActual, limitDot);
	const Matrix3 matR = Matrix3::MakeRotateTo(
		dirActual,
		VMath::normalizeApprox(VMath::lerp(alpha, dirActual, dirDesired)));
	const Matrix3 matDrivenFrom = pBoneDriven->_matFinal.getUpper3x3()*pBoneDriven->_matFromBoneSpace.getUpper3x3();
	const Matrix3 matDrivenTo = VMath::inverse(matDrivenFrom);
	pBoneDriven->_matExternal.setUpper3x3(matDrivenTo*matR*matDrivenFrom);
}

void Skeleton::ProcessKinectData(const BYTE* p, RMotion motion, int frame)
{
	for (auto& kv : _mapBones)
	{
		kv.second._matFinal = Transform3::identity();
		kv.second._matExternal = Transform3::identity();
	}

	UINT64 timestamp;
	memcpy(&timestamp, p, 8);

	Vector3 skeletonPos;
	float temp;
	memcpy(&temp, p + 8, 4); skeletonPos.setX(temp);
	memcpy(&temp, p + 12, 4); skeletonPos.setY(temp);
	memcpy(&temp, p + 16, 4); skeletonPos.setZ(temp);

	ProcessKinectJoint(p + 20, "AnkleLeft", skeletonPos);
	ProcessKinectJoint(p + 36, "AnkleRight", skeletonPos);

	ProcessKinectJoint(p + 52, "ElbowLeft", skeletonPos);
	ProcessKinectJoint(p + 68, "ElbowRight", skeletonPos);

	ProcessKinectJoint(p + 84, "FootLeft", skeletonPos);
	ProcessKinectJoint(p + 100, "FootRight", skeletonPos);

	ProcessKinectJoint(p + 116, "HandLeft", skeletonPos);
	ProcessKinectJoint(p + 132, "HandRight", skeletonPos);

	ProcessKinectJoint(p + 148, "Head", skeletonPos);

	ProcessKinectJoint(p + 164, "HipCenter", skeletonPos);
	ProcessKinectJoint(p + 180, "HipLeft", skeletonPos);
	ProcessKinectJoint(p + 196, "HipRight", skeletonPos);

	ProcessKinectJoint(p + 212, "KneeLeft", skeletonPos);
	ProcessKinectJoint(p + 228, "KneeRight", skeletonPos);

	ProcessKinectJoint(p + 244, "ShoulderCenter", skeletonPos);
	ProcessKinectJoint(p + 260, "ShoulderLeft", skeletonPos);
	ProcessKinectJoint(p + 276, "ShoulderRight", skeletonPos);

	ProcessKinectJoint(p + 292, "Spine", skeletonPos);

	ProcessKinectJoint(p + 308, "WristLeft", skeletonPos);
	ProcessKinectJoint(p + 324, "WristRight", skeletonPos);

	// Twist:
	float hipTwist = 0;
	PBone pBoneL = FindBone("HipLeft");
	PBone pBoneR = FindBone("HipRight");
	if (pBoneL && pBoneR)
	{
		const Vector3 hipAxis = VMath::normalize(
			pBoneL->_matExternal.getTranslation() -
			pBoneR->_matExternal.getTranslation());
		const glm::vec3 a(1, 0, 0);
		const glm::vec3 b = hipAxis.GLM();
		const glm::vec3 r(0, 1, 0);
		//hipTwist = glm::orientedAngle(a, b, r);
		hipTwist *= 1 - abs(hipAxis.getY());
	}
	float shTwist = 0;
	pBoneL = FindBone("ShoulderLeft");
	pBoneR = FindBone("ShoulderRight");
	if (pBoneL && pBoneR)
	{
		const Vector3 shAxis = VMath::normalize(
			pBoneL->_matExternal.getTranslation() -
			pBoneR->_matExternal.getTranslation());
		const glm::vec3 a(1, 0, 0);
		const glm::vec3 b = shAxis.GLM();
		const glm::vec3 r(0, 1, 0);
		//shTwist = glm::orientedAngle(a, b, r);
		shTwist *= 1 - abs(shAxis.getY());
	}

	for (auto& kv : _mapBones)
	{
		RBone bone = kv.second;
		if (!IsKinectBone(_C(bone._name)))
			continue;
#if 0
		char xml[800];
		sprintf_s(xml, "<p n=\"%s\" v=\"%s\" />", _C(bone._name),
			_C(bone._matExternal.GetTranslationPart().ToString3()));
		VERUS_OUTPUT_DEBUG_STRING(xml);
#endif
		PBone pParent = FindBone(_C(bone._parentName));
		while (pParent && !IsKinectBone(_C(pParent->_name)))
			pParent = FindBone(_C(pParent->_parentName));
		if (pParent)
		{
			const Vector3 bindPosePos = pParent->_matFromBoneSpace.getTranslation();
			const Vector3 kinePosePos = pParent->_matExternal.getTranslation();
			const Vector3 bindPoseDir = VMath::normalize(bone._matFromBoneSpace.getTranslation() - bindPosePos);
			const Vector3 kinePoseDir = VMath::normalize(bone._matExternal.getTranslation() - kinePosePos);

			Matrix3 matR3;
			matR3.RotateTo(bindPoseDir, kinePoseDir);
			const Transform3 matR(matR3, Vector3(0));
			const Transform3 matT = Transform3::translation(kinePosePos - bindPosePos);
			const Transform3 matOffset = Transform3::translation(kinePosePos);

			// Twist:
			const float twist = IsParentOf(_C(bone._parentName), "Spine") ? shTwist : hipTwist;
			const float angle = kinePoseDir.getY()*twist;
			const Transform3 matTwist = Transform3(Quat::rotation(angle, kinePoseDir), Vector3(0));

			// Kinect's bind pose correction:
			const Transform3 m = matTwist * matR*pParent->_matToActorSpace;

			const bool secondary =
				strstr(_C(bone._name), "Left") ||
				strstr(_C(bone._name), "Right");
			if (!secondary || pParent->_matFinal.IsIdentity())
				pParent->_matFinal = matOffset * m*VMath::inverse(matOffset)*matT;
		}
	}

	VERUS_FOR(i, 4)
	{
		for (auto& kv : _mapBones)
		{
			RBone bone = kv.second;
			if (IsKinectBone(_C(bone._name)) && !IsKinectLeafBone(_C(bone._name)))
				continue;
			PBone pParent = FindBone(_C(bone._parentName));
			if (pParent)
				bone._matFinal = pParent->_matFinal;
			else
				bone._matFinal = Transform3::identity();
		}
	}

	BakeMotion(motion, frame, true);

	for (auto& kv : _mapBones)
	{
		kv.second._matFinal = Transform3::identity();
		kv.second._matExternal = Transform3::identity();
	}
}

void Skeleton::ProcessKinectJoint(const BYTE* p, CSZ name, RcVector3 skeletonPos)
{
	int id;
	memcpy(&id, p, 4);

	Vector3 pos;
	float temp;
	memcpy(&temp, p + 4, 4); pos.setX(temp);
	memcpy(&temp, p + 8, 4); pos.setY(temp);
	memcpy(&temp, p + 12, 4); pos.setZ(temp);
	pos -= skeletonPos;
	pos += Vector3(0, 1, 0);

	pos = Transform3::rotationY(VERUS_PI)*pos;

	PBone pBone = FindBone(name);
	if (pBone)
		pBone->_matExternal = Transform3::translation(pos);
}

void Skeleton::LoadKinectBindPose(CSZ xml)
{
#if 0
	tinyxml2::XMLDocument doc;
	doc.Parse(xml);
	if (doc.Error())
		return;

	tinyxml2::XMLElement* pElem = nullptr;
	tinyxml2::XMLElement* pRoot = doc.FirstChildElement();
	if (!pRoot)
		return;

	Map<String, Vector3> mapData;
	for (pElem = pRoot->FirstChildElement("p"); pElem; pElem = pElem->NextSiblingElement("p"))
	{
		Vector3 pos;
		pos.FromString(pElem->Attribute("v"));
		mapData[pElem->Attribute("n")] = pos;
	}

	for (const auto& kv : _mapBones)
	{
		RBone bone = kv.second;
		if (!IsKinectBone(_C(bone._name)))
			continue;
		PBone pParent = FindBone(_C(bone._parentName));
		while (pParent && !IsKinectBone(_C(pParent->_name)))
			pParent = FindBone(_C(pParent->_parentName));
		if (pParent)
		{
			const Vector3 bindPosePos = pParent->_matFromBoneSpace.getTranslation();
			const Vector3 kinePosePos = mapData[pParent->_name];
			const Vector3 bindPoseDir = VMath::normalize(bone._matFromBoneSpace.getTranslation() - bindPosePos);
			const Vector3 kinePoseDir = VMath::normalize(mapData[bone._name] - kinePosePos);

			Matrix3 matR3;
			matR3.RotateTo(bindPoseDir, kinePoseDir);
			matR3 = VMath::inverse(matR3);

			const bool secondary =
				strstr(_C(bone._name), "Left") ||
				strstr(_C(bone._name), "Right");
			if (!secondary || pParent->_matToActorSpace.IsIdentity())
				pParent->_matToActorSpace = Transform3(matR3, Vector3(0));
		}
	}
#endif
}

bool Skeleton::IsKinectBone(CSZ name)
{
	const CSZ names[] =
	{
		"AnkleLeft",
		"AnkleRight",
		"ElbowLeft",
		"ElbowRight",
		"FootLeft",
		"FootRight",
		"HandLeft",
		"HandRight",
		"Head",
		"HipCenter",
		"HipLeft",
		"HipRight",
		"KneeLeft",
		"KneeRight",
		"ShoulderCenter",
		"ShoulderLeft",
		"ShoulderRight",
		"Spine",
		"WristLeft",
		"WristRight"
	};
	return std::binary_search(names, names + VERUS_ARRAY_LENGTH(names),
		name, [](CSZ a, CSZ b) {return strcmp(a, b) < 0; });
}

bool Skeleton::IsKinectLeafBone(CSZ name)
{
	const CSZ names[] =
	{
		"FootLeft",
		"FootRight",
		"Head",
		"WristLeft",
		"WristRight"
	};
	return std::binary_search(names, names + VERUS_ARRAY_LENGTH(names),
		name, [](CSZ a, CSZ b) {return strcmp(a, b) < 0; });
}

void Skeleton::FixateFeet(RMotion motion)
{
	enum class Foot : int
	{
		none,
		left,
		right
	} foot = Foot::none;

	Point3 posFixed, posBase, posHeadPrev;
	int frameEvent = 0;

	PBone pAnkleL = FindBone("AnkleLeft");
	PBone pAnkleR = FindBone("AnkleRight");
	PBone pHead = FindBone("Head");

	if (!pAnkleL || !pAnkleR)
		return;

	Vector<Point3> vPos;
	vPos.resize(motion.GetNumFrames());
	VERUS_FOR(i, motion.GetNumFrames())
	{
		ApplyMotion(motion, i*motion.GetFpsInv());

		const Point3 posAnkleL = (pAnkleL->_matFinal*pAnkleL->_matFromBoneSpace).getTranslation();
		const Point3 posAnkleR = (pAnkleR->_matFinal*pAnkleR->_matFromBoneSpace).getTranslation();

		const Foot target = (posAnkleL.getY() < posAnkleR.getY()) ? Foot::left : Foot::right;
		Point3 pos;
		if (foot != target && (i - frameEvent > 5 || !i))
		{
			frameEvent = i;
			foot = target;
			posFixed = (foot == Foot::left) ? posAnkleL : posAnkleR;
			if (i)
				posFixed += Vector3(vPos[i - 1]);
			posFixed.setY(0.05f);
		}
		pos = (foot == Foot::left) ? posAnkleL : posAnkleR;

		vPos[i] = posFixed - pos;

		// Stabilize head position:
		const Point3 posHead = (pHead->_matFinal*pHead->_matFromBoneSpace).getTranslation() + vPos[i];
		if (i)
		{
			Vector3 d = posHeadPrev - posHead;
			d = VMath::mulPerElem(d, Vector3(0.5f, 0, 0.5f));
			vPos[i] += d;
		}
		posHeadPrev = posHead;
	}

	// Add significant keyframes:
	const float e = 0.01f;
	Motion::PBone pRoot = motion.InsertBone(RootName());
	VERUS_FOR(i, motion.GetNumFrames())
	{
		const Point3 p = vPos[i];
		const bool same = p.IsEqual(posBase, e);
		if (!same)
		{
			posBase = p;
			pRoot->InsertKeyframePosition(i, p);
		}
	}
}

Vector3 Skeleton::GetHighestSpeed(RMotion motion, CSZ name, RcVector3 scale, bool positive)
{
	const int numFrames = motion.GetNumFrames();
	const float dt = motion.GetFpsInv();
	const float dti = float(motion.GetFps());

	Vector<float> vDX;
	Vector<float> vDY;
	Vector<float> vDZ;
	vDX.reserve(numFrames);
	vDY.reserve(numFrames);
	vDZ.reserve(numFrames);

	Point3 prevPos(0);
	VERUS_FOR(i, numFrames)
	{
		ApplyMotion(motion, i*dt);
		PBone pBone = FindBone(name);
		Point3 pos = VMath::mulPerElem(pBone->_matFromBoneSpace.getTranslation(), scale);
		pos = pBone->_matFinal*pos;
		if (i)
		{
			vDX.push_back((pos.getX() - prevPos.getX())*dti);
			vDY.push_back((pos.getY() - prevPos.getY())*dti);
			vDZ.push_back((pos.getZ() - prevPos.getZ())*dti);
		}
		prevPos = pos;
	}
	if (positive)
	{
		std::sort(vDX.begin(), vDX.end(), std::greater<float>());
		std::sort(vDY.begin(), vDY.end(), std::greater<float>());
		std::sort(vDZ.begin(), vDZ.end(), std::greater<float>());
	}
	else
	{
		std::sort(vDX.begin(), vDX.end());
		std::sort(vDY.begin(), vDY.end());
		std::sort(vDZ.begin(), vDZ.end());
	}

	const int offA = motion.GetNumFrames() / 32;
	const int offB = motion.GetNumFrames() / 24;
	const int offC = motion.GetNumFrames() / 16;
	const int offD = motion.GetNumFrames() / 8;
	return Vector3(
		0.25f*(vDX[offA] + vDX[offB] + vDX[offC] + vDX[offD]),
		0.25f*(vDY[offA] + vDY[offB] + vDY[offC] + vDY[offD]),
		0.25f*(vDZ[offA] + vDZ[offB] + vDZ[offC] + vDZ[offD]));
}
