#include "verus.h"

using namespace verus;
using namespace verus::Scene;

void CameraOrbit::Update()
{
	const Vector3 toEye = Matrix3::rotationZYX(Vector3(_pitch, _yaw, 0)) * Vector3(0, 0, _radius);
	MoveEyeTo(_posAt + toEye);
	MainCamera::Update();
}

void CameraOrbit::DragController_GetParams(float& x, float& y)
{
	x = _yaw;
	y = _pitch;
}

void CameraOrbit::DragController_SetParams(float x, float y)
{
	_yaw = fmod(x, VERUS_2PI);
	_pitch = Math::Clamp(y, -VERUS_PI * 0.49f, VERUS_PI * 0.49f);
	Update();
}

void CameraOrbit::DragController_GetRatio(float& x, float& y)
{
	x = -VERUS_2PI / 1440.f;
	y = -VERUS_2PI / 1440.f;
}

void CameraOrbit::MulRadiusBy(float a)
{
	_radius *= a;
	Update();
}

void CameraOrbit::SaveState(int slot)
{
	StringStream ss;
	ss << _C(Utils::I().GetWritablePath()) << "/CameraState.xml";
	IO::Xml xml;
	xml.SetFilename(_C(ss.str()));
	xml.Load();
	char txt[16];
	sprintf_s(txt, "slot%d", slot);
	char params[40];
	sprintf_s(params, "|%f %f %f", _pitch, _yaw, _radius);
	xml.Set(txt, _C(_posAt.ToString() + params));
}

void CameraOrbit::LoadState(int slot)
{
	StringStream ss;
	ss << _C(Utils::I().GetWritablePath()) << "/CameraState.xml";
	IO::Xml xml;
	xml.SetFilename(_C(ss.str()));
	xml.Load();
	char txt[16];
	sprintf_s(txt, "slot%d", slot);
	CSZ v = xml.GetS(txt);
	if (v)
	{
		CSZ p = strchr(v, '|');
		_posAt.FromString(v);
		sscanf(p + 1, "%f %f %f", &_pitch, &_yaw, &_radius);
		_update |= Update::v;
		Update();
	}
}
