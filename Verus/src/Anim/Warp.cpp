// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Anim;

Warp::Warp()
{
}

Warp::~Warp()
{
	Done();
}

void Warp::Init()
{
	VERUS_INIT();
}

void Warp::Done()
{
	VERUS_DONE(Warp);
}

void Warp::Update(RSkeleton skeleton)
{
	VERUS_QREF_TIMER;
	const float affect = 1 - Math::Clamp<float>((dt - 0.1f) * 10, 0, 1);
	for (auto& zone : _vZones)
	{
		if (zone._spring > 0)
		{
			Skeleton::PBone pBone = skeleton.FindBone(_C(zone._bone));
			if (pBone)
			{
				Point3 posWorldSpace = pBone->_matFinal * zone._pos;
				if (zone._posW.IsZero())
					zone._posW = posWorldSpace;

				Point3 prevPosBindSpace = pBone->_matFinalInv * zone._posW;

				// Bind-pose space:
				const Vector3 dirMove = zone._pos - prevPosBindSpace;
				zone._off -= dirMove * affect;
				const float len = VMath::length(zone._off);
				if (len > zone._maxOffset)
				{
					zone._off /= len;
					zone._off *= zone._maxOffset;
				}

				// Damped harmonic oscillator:
				const float k = zone._spring * 100; // Stiffness.
				const Vector3 force = -zone._off * k - zone._vel * zone._damping; // Hooke's law.
				zone._vel += force * dt;
				zone._off += zone._vel * dt;

				zone._posW = posWorldSpace;
			}
		}
	}
}

void Warp::DrawLines() const
{
	VERUS_QREF_DD;

	dd.Begin(CGI::DebugDraw::Type::lines, nullptr, false);
	for (const auto& zone : _vZones)
	{
		VERUS_IF_FOUND_IN(TMapOffsets, zone._mapOffsets, _preview, it)
		{
			dd.AddLine(
				zone._pos,
				zone._pos + it->second,
				VERUS_COLOR_RGBA(0, 255, 255, 255));
		}
	}
	dd.End();
}

void Warp::DrawZones() const
{
	VERUS_QREF_HELPERS;
	VERUS_QREF_RENDERER;

	for (const auto& zone : _vZones)
	{
		UINT32 color = VERUS_COLOR_WHITE;
		switch (zone._type)
		{
		case 'r': color = VERUS_COLOR_RGBA(255, 0, 0, 255); break;
		case 'g': color = VERUS_COLOR_RGBA(0, 255, 0, 255); break;
		case 'b': color = VERUS_COLOR_RGBA(0, 0, 255, 255); break;
		}
		helpers.DrawSphere(zone._pos, zone._radius, color, renderer.GetCommandBuffer());
	}
}

void Warp::Load(CSZ url)
{
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(url, vData, IO::FileSystem::LoadDesc(true));
	LoadFromPtr(reinterpret_cast<SZ>(vData.data()));
}

void Warp::LoadFromPtr(SZ p)
{
	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(p, strlen(p));
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(); " << result.description();
	pugi::xml_node root = doc.first_child();

	if (auto node = root.child("preview"))
		_preview = node.text().as_string();
	_jawScale = root.child("jaw").text().as_float(1);

	_vZones.clear();
	_vZones.reserve(16);
	_yMin = FLT_MAX;

	for (auto node : root.children("z"))
	{
		Zone zone;
		zone._name = node.attribute("name").value();
		zone._bone = node.attribute("bone").value();
		zone._pos.FromString(node.attribute("pos").value());
		zone._radius = node.attribute("r").as_float();
		zone._type = node.attribute("type").value()[0];
		zone._spring = node.attribute("spring").as_float();
		zone._damping = node.attribute("damping").as_float(5);
		zone._maxOffset = node.attribute("maxOffset").as_float(0.03f);
		_yMin = Math::Min(_yMin, zone._pos.getY() - zone._radius - 0.01f);
		_vZones.push_back(std::move(zone));
		RZone zref = _vZones.back();
		for (auto offsetNode : node.children("o"))
		{
			CSZ name = offsetNode.attribute("name").value();
			Vector3 d;
			d.FromString(offsetNode.attribute("d").value());
			zref._mapOffsets[name] = d;
		}
	}
}

void Warp::Fill(PVector4 p)
{
	int r = 0, g = 0, b = 0, a = 0;
	for (const auto& zone : _vZones)
	{
		int offset = 0;
		int* pType = &a;

		switch (zone._type)
		{
		case 'r':
			offset = 8;
			pType = &r;
			break;
		case 'g':
			offset = 16;
			pType = &g;
			break;
		case 'b':
			offset = 24;
			pType = &b;
			break;
		}

		if (*pType < 4)
		{
			const int pos = offset + (*pType) * 2;
			p[pos] = Vector4(Vector3(zone._pos), 1 / zone._radius);
			p[pos].setW(p[pos].getW() * p[pos].getW());
			p[pos + 1] = Vector4(zone._off, 0);
			VERUS_IF_FOUND_IN(TMapOffsets, zone._mapOffsets, _preview, it)
			{
				p[pos + 1] = Vector4(it->second, 0);
			}
			(*pType)++;
		}
	}
	p[1].setW(_yMin);
}

void Warp::ApplyMotion(RMotion motion, float time)
{
	for (auto& zone : _vZones)
	{
		if (zone._spring == 0)
		{
			zone._off = Vector3(0);
			Motion::PBone pBone = motion.FindBone(_C(zone._name));
			if (pBone)
				pBone->ComputePositionAt(time, zone._off);
			VERUS_FOREACH_CONST(TMapOffsets, zone._mapOffsets, it)
			{
				pBone = motion.FindBone(_C(it->first));
				if (pBone)
				{
					Vector3 pos;
					pBone->ComputePositionAt(time, pos);
					zone._off += it->second * pos.getX();
				}
			}
		}
	}
}

void Warp::ComputeLipSyncFromAudio(RcBlob blob, RMotion motion, float jawScale)
{
	const int sampleRate = 44100;
	const int byteDepth = sizeof(short);
	const float length = static_cast<float>(blob._size) / (sampleRate * byteDepth);
	const int frameCount = static_cast<int>(length * motion.GetFps());
	const int blockSize = static_cast<int>(motion.GetFpsInv() * sampleRate);

	// Allocate:
	Vector<short> vSamplesJaw(blob._size / byteDepth);
	Vector<short> vSamplesLetterO(vSamplesJaw.size());
	Vector<short> vSamplesLetterS(vSamplesJaw.size());

	// Use source for jaw:
	memcpy(vSamplesJaw.data(), blob._p, vSamplesJaw.size() * sizeof(short));

	// Letter S (high-pass):
	VERUS_P_FOR(i, static_cast<int>(vSamplesLetterO.size()))
	{
		const int strength = 4;
		const int from = Math::Clamp<int>(i - strength, 0, static_cast<int>(vSamplesJaw.size()) - 1);
		const int to = Math::Clamp<int>(i + strength + 1, 0, static_cast<int>(vSamplesJaw.size()) - 1);
		INT64 acc = 0;
		for (int j = from; j < to; j++)
		{
			const int d = abs(j - i);
			const int sample = vSamplesJaw[j];
			acc += sample - sample * d / strength;
		}
		acc /= to - from;
		vSamplesLetterO[i] = static_cast<short>(Math::Clamp<INT64>(acc * 22 / 10, SHRT_MIN, SHRT_MAX));
	});
	VERUS_P_FOR(i, static_cast<int>(vSamplesLetterS.size()))
	{
		vSamplesLetterS[i] = Math::Clamp<int>(vSamplesJaw[i] - vSamplesLetterO[i], SHRT_MIN, SHRT_MAX);
	});

	// Letter O (low-pass):
	VERUS_P_FOR(i, static_cast<int>(vSamplesLetterO.size()))
	{
		const int strength = 175;
		const int from = Math::Clamp<int>(i - strength, 0, static_cast<int>(vSamplesJaw.size()) - 1);
		const int to = Math::Clamp<int>(i + strength + 1, 0, static_cast<int>(vSamplesJaw.size()) - 1);
		INT64 acc = 0;
		for (int j = from; j < to; j++)
		{
			const int d = abs(j - i);
			const int sample = vSamplesJaw[j];
			acc += sample - sample * d / strength;
		}
		acc /= to - from;
		vSamplesLetterO[i] = static_cast<short>(Math::Clamp<INT64>(acc * 3, SHRT_MIN, SHRT_MAX));
	});

	const short* pJaw = vSamplesJaw.data();
	const short* pLetterO = vSamplesLetterO.data();
	const short* pLetterS = vSamplesLetterS.data();

	motion.DeleteBone("Jaw");
	motion.DeleteBone("WarpLetterO");
	motion.DeleteBone("WarpLetterS");
	Motion::PBone pBoneJaw = motion.InsertBone("Jaw");
	Motion::PBone pBoneLetterO = motion.InsertBone("WarpLetterO");
	Motion::PBone pBoneLetterS = motion.InsertBone("WarpLetterS");

	const float e = 1e-4f;
	const float scale = (1.f / blockSize) * e;
	INT64 acc = 0, accO = 0, accS = 0;
	Point3 prevPosJaw(0), prevPosLetterO(0), prevPosLetterS(0);
	VERUS_FOR(i, frameCount)
	{
		const int offset = i * blockSize;

		acc = 0, accO = 0, accS = 0;
		VERUS_FOR(j, blockSize)
		{
			const int index = offset + j;
			acc += abs(pJaw[index]);
			accO += abs(pLetterO[index]);
			accS += abs(pLetterS[index]);
		}
		float jaw = acc * scale * -0.1f * jawScale;
		float letterO = Math::Min(accO * scale, 1.f);
		float letterS = Math::Min(accS * scale * 5.5f, 1.f);
		if (abs(jaw) < 0.05f)
			jaw = 0;
		if (abs(letterO) < 0.3f)
			letterO = 0;
		if (abs(letterS) < 0.3f)
			letterS = 0;
		jaw = Math::Min(jaw + letterS * 0.05f, 0.f);

		const Point3 posJaw(0, jaw, 0);
		if (VMath::distSqr(posJaw, prevPosJaw) > e)
		{
			if (VMath::distSqr(prevPosJaw, Point3(0)) < e && i > 1)
				pBoneJaw->InsertKeyframeRotation(i - 1, prevPosJaw);
			pBoneJaw->InsertKeyframeRotation(i, posJaw);
			prevPosJaw = posJaw;
		}

		const Point3 posLetterO(letterO);
		if (VMath::distSqr(posLetterO, prevPosLetterO) > e)
		{
			if (VMath::distSqr(prevPosLetterO, Point3(0)) < e && i > 1)
				pBoneLetterO->InsertKeyframePosition(i - 1, prevPosLetterO);
			pBoneLetterO->InsertKeyframePosition(i, posLetterO);
			prevPosLetterO = posLetterO;
		}

		const Point3 posLetterS(letterS);
		if (VMath::distSqr(posLetterS, prevPosLetterS) > e)
		{
			if (VMath::distSqr(prevPosLetterS, Point3(0)) < e && i > 1)
				pBoneLetterS->InsertKeyframePosition(i - 1, prevPosLetterS);
			pBoneLetterS->InsertKeyframePosition(i, posLetterS);
			prevPosLetterS = posLetterS;
		}
	}
}
