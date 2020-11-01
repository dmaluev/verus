// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

void EditorTerrain::ConvertToBufferCoords(const float xz[2], int ij[2]) const
{
	const int edge = _mapSide - 1;
	const float half = float(_mapSide >> 1);
	ij[0] = Math::Clamp(int(xz[1] + half + 0.5f), 0, edge);
	ij[1] = Math::Clamp(int(xz[0] + half + 0.5f), 0, edge);
}

glm::int4 EditorTerrain::ComputeBrushRect(const int ij[2], int radius) const
{
	return glm::int4(
		Math::Clamp(ij[1] - radius, 0, _mapSide),
		Math::Clamp(ij[0] - radius, 0, _mapSide),
		Math::Clamp(ij[1] + radius + 1, 0, _mapSide),
		Math::Clamp(ij[0] + radius + 1, 0, _mapSide));
}

void EditorTerrain::SmoothenHeight(short stepSize)
{
	Vector<short> vCache;
	vCache.resize(_mapSide * _mapSide);

	// Horizontal, result goes to cache:
	VERUS_P_FOR(i, _mapSide)
	{
		short prevH = 0;
		int prevJ = 0;
		VERUS_FOR(j, _mapSide)
		{
			short h;
			const int ij[] = { i, j };
			GetHeightAt(ij, 0, &h);
			vCache[(i << _mapShift) + j] = h;
			if (h != prevH)
			{
				if (abs(h - prevH) < stepSize)
				{
					const int tileCount = j - prevJ;
					if (tileCount)
					{
						const short part = (h - prevH) / tileCount;
						VERUS_FOR(k, tileCount)
						{
							vCache[(i << _mapShift) + prevJ + k] = prevH + part * k;
						}
					}
				}
				prevH = h;
				prevJ = j;
			}
		}
	});

	// Vertical, calc control points from source, but use values from cache:
	VERUS_P_FOR(j, _mapSide)
	{
		short prevS = 0;
		short prevH = 0;
		int prevI = 0;
		VERUS_FOR(i, _mapSide)
		{
			short h, s;
			const int ij[] = { i, j };
			GetHeightAt(ij, 0, &h);
			s = vCache[(i << _mapShift) + j];
			SetHeightAt(ij, s);
			if (h != prevH)
			{
				if (abs(h - prevH) < stepSize)
				{
					const int tileCount = i - prevI;
					if (tileCount)
					{
						const short part = (s - prevS) / tileCount;
						VERUS_FOR(k, tileCount)
						{
							const int ij[] = { prevI + k, j };
							SetHeightAt(ij, prevS + part * k);
						}
					}
				}
				prevS = s;
				prevH = h;
				prevI = i;
			}
		}
	});

	// Blur:
	VERUS_P_FOR(i, _mapSide)
	{
		VERUS_FOR(j, _mapSide)
		{
			const int ij[] = { i, j };
			const int ij0[] = { i, j - 1 };
			const int ij1[] = { i, j + 1 };
			const int ij2[] = { i - 1, j };
			const int ij3[] = { i + 1, j };
			const int ij4[] = { i - 1, j - 1 };
			const int ij5[] = { i - 1, j + 1 };
			const int ij6[] = { i + 1, j + 1 };
			const int ij7[] = { i + 1, j - 1 };

			int acc = 0;
			short h = 0;

			GetHeightAt(ij, 0, &h); acc += h;
			GetHeightAt(ij0, 0, &h); acc += h;
			GetHeightAt(ij1, 0, &h); acc += h;
			GetHeightAt(ij2, 0, &h); acc += h;
			GetHeightAt(ij3, 0, &h); acc += h;
			GetHeightAt(ij4, 0, &h); acc += h;
			GetHeightAt(ij5, 0, &h); acc += h;
			GetHeightAt(ij6, 0, &h); acc += h;
			GetHeightAt(ij7, 0, &h); acc += h;

			vCache[(i << _mapShift) + j] = acc / 9;
		}
	});

	VERUS_P_FOR(i, _mapSide)
	{
		VERUS_FOR(j, _mapSide)
		{
			const int ij[] = { i, j };
			SetHeightAt(ij, vCache[(i << _mapShift) + j]);
		}
	});
}

void EditorTerrain::ApplyBrushHeight(const float xz[2], int radius, int strength)
{
	int ijCenter[2];
	ConvertToBufferCoords(xz, ijCenter);
	const glm::int4 rc = ComputeBrushRect(ijCenter, radius);
	const int r2 = radius * radius;
	for (int i = rc.y; i < rc.w; ++i)
	{
		for (int j = rc.x; j < rc.z; ++j)
		{
			const int di = ijCenter[0] - i;
			const int dj = ijCenter[1] - j;
			const int rr = di * di + dj * dj;
			if (rr <= r2)
			{
				const int delta = strength + 3 * strength * (r2 - rr) / r2;
				const int ij[] = { i, j };
				short h;
				GetHeightAt(ij, 0, &h);
				SetHeightAt(ij, Math::Clamp(h + delta, -SHRT_MAX, SHRT_MAX));
			}
		}
	}

	UpdateNormalsForArea(rc);
}

void EditorTerrain::ApplyBrushFlatten(const float xz[2], int radius, int strength, bool useNormal, PcVector3 pNormal)
{
	int ijCenter[2];
	ConvertToBufferCoords(xz, ijCenter);
	const glm::int4 rc = ComputeBrushRect(ijCenter, radius);
	const int r2 = radius * radius;
	short centerHeight;
	const float height = GetHeightAt(ijCenter, 0, &centerHeight);

	float slopeX = 0;
	float slopeZ = 0;
	if (useNormal)
	{
		if (pNormal)
		{
			slopeX = pNormal->getX() / pNormal->getY();
			slopeZ = pNormal->getZ() / pNormal->getY();
		}
		else
		{
			float temp[3];
			Convert::Sint8ToSnorm(GetNormalAt(ijCenter), temp, 3);
			slopeX = temp[0] / temp[1];
			slopeZ = temp[2] / temp[1];
		}
	}

	for (int i = rc.y; i < rc.w; ++i)
	{
		for (int j = rc.x; j < rc.z; ++j)
		{
			const int di = ijCenter[0] - i;
			const int dj = ijCenter[1] - j;
			const int rr = di * di + dj * dj;
			if (rr <= r2)
			{
				const int ij[] = { i, j };
				short h;
				GetHeightAt(ij, 0, &h);
				short targetHeight = centerHeight;
				if (useNormal)
				{
					const float h = height + slopeX * dj + slopeZ * di;
					targetHeight = Math::Clamp<int>(ConvertHeight(h), -SHRT_MAX, SHRT_MAX);
				}

				const int maxDelta = std::abs(targetHeight - h);

				int delta = 0;
				if (h > targetHeight)
					delta = -std::abs(Math::Min(strength, maxDelta));
				else if (h < targetHeight)
					delta = std::abs(Math::Min(strength, maxDelta));

				SetHeightAt(ij, Math::Clamp(h + delta, -SHRT_MAX, SHRT_MAX));
			}
		}
	}

	UpdateNormalsForArea(rc);
}

void EditorTerrain::ApplyBrushSmoothen(const float xz[2], int radius, int strength)
{
	int ijCenter[2];
	ConvertToBufferCoords(xz, ijCenter);
	const glm::int4 rc = ComputeBrushRect(ijCenter, radius);
	const int r2 = radius * radius;

	if (!strength)
		strength = radius;

	auto Filter = [this, strength](const int ij[2]) -> short
	{
		const int r2 = strength * strength;
		const int iMin = Math::Clamp(ij[0] - strength, 0, _mapSide);
		const int jMin = Math::Clamp(ij[1] - strength, 0, _mapSide);
		const int iMax = Math::Clamp(ij[0] + strength, 0, _mapSide);
		const int jMax = Math::Clamp(ij[1] + strength, 0, _mapSide);
		int hSum = 0;
		int count = 0;
		for (int i = iMin; i < iMax; ++i)
		{
			for (int j = jMin; j < jMax; ++j)
			{
				const int di = ij[0] - i;
				const int dj = ij[1] - j;
				const int rr = di * di + dj * dj;
				if (rr <= r2)
				{
					const int ij[] = { i, j };
					short h;
					GetHeightAt(ij, 0, &h);
					hSum += h;
					count++;
				}
			}
		}
		return Math::Clamp(hSum / count, -SHRT_MAX, SHRT_MAX);
	};

	for (int i = rc.y; i < rc.w; ++i)
	{
		for (int j = rc.x; j < rc.z; ++j)
		{
			const int di = ijCenter[0] - i;
			const int dj = ijCenter[1] - j;
			const int rr = di * di + dj * dj;
			if (rr <= r2)
			{
				const int ij[] = { i, j };
				short h;
				GetHeightAt(ij, 0, &h);
				const short hFiltered = Filter(ij);
				SetHeightAt(ij, h + (hFiltered - h) * (r2 - rr) / r2);
			}
		}
	}

	UpdateNormalsForArea(rc);
}

void EditorTerrain::UpdateNormalsForArea(const glm::int4& rc)
{
	const int patchShift = _mapShift - 4;
	const int patchEdge = (_mapSide >> 4) - 1;
	const int iPatchMin = Math::Clamp((rc.y - 1) / 16 - 1, 0, patchEdge);
	const int iPatchMax = Math::Clamp((rc.w + 1) / 16 + 1, 0, patchEdge);
	const int jPatchMin = Math::Clamp((rc.x - 1) / 16 - 1, 0, patchEdge);
	const int jPatchMax = Math::Clamp((rc.z + 1) / 16 + 1, 0, patchEdge);
	VERUS_FOR(lod, 5)
	{
		for (int i = iPatchMin; i <= iPatchMax; ++i)
			for (int j = jPatchMin; j <= jPatchMax; ++j)
				_vPatches[(i << patchShift) + j].UpdateNormals(this, lod);
	}
}

void EditorTerrain::SplatTileAt(const int ij[2], int channel, int strength)
{
	const int mapEdge = _mapSide - 1;
	const int i = Math::Clamp(ij[0], 0, mapEdge);
	const int j = Math::Clamp(ij[1], 0, mapEdge);

	// Subtract strength from all channels:
	const int offset = (i << _mapShift) + j;
	BYTE* rgba = reinterpret_cast<BYTE*>(&_vBlendBuffer[offset]);
	int rgba32[4];
	Utils::CopyByteToInt4(rgba, rgba32);
	int rgbaTemp[4];
	memcpy(rgbaTemp, rgba32, sizeof(rgba32));
	rgbaTemp[3] = 255 - rgbaTemp[0] - rgbaTemp[1] - rgbaTemp[2];
	VERUS_FOR(i, 4)
		rgbaTemp[i] = Math::Max(0, rgbaTemp[i] - strength);

	// Set correct weight for target channel:
	int leftover = 255;
	VERUS_FOR(i, 4)
	{
		if (i != channel)
			leftover -= rgbaTemp[i];
	}
	rgbaTemp[channel] = leftover;

	memcpy(rgba32, rgbaTemp, sizeof(int) * 3);

	Utils::CopyIntToByte4(rgba32, rgba);
}

void EditorTerrain::SplatTileAtEx(const int ij[2], int layer, float maskValue, bool extended)
{
	if (!CanSplatTileAt(ij, layer))
		return;

	const int mapEdge = _mapSide - 1;
	const int i = Math::Clamp(ij[0], 0, mapEdge);
	const int j = Math::Clamp(ij[1], 0, mapEdge);

	// Not allowed to change these two edges. Only extended splat can change them.
	if (!extended && (!(i & 0xF) || !(j & 0xF)))
		return;

	const bool iExtend = !extended && ((15 == (i & 0xF)) && (i != mapEdge));
	const bool jExtend = !extended && ((15 == (j & 0xF)) && (j != mapEdge));

	// Test if we can proceed:
	if (iExtend)
	{
		const int ij2[] = { i + 1, j };
		if (!CanSplatTileAt(ij2, layer))
			return;
	}
	if (jExtend)
	{
		const int ij2[] = { i, j + 1 };
		if (!CanSplatTileAt(ij2, layer))
			return;
	}
	if (iExtend && jExtend)
	{
		const int ij2[] = { i + 1, j + 1 };
		if (!CanSplatTileAt(ij2, layer))
			return;
	}

	// Extend this splat if required:
	if (iExtend)
	{
		const int ij2[] = { i + 1, j };
		SplatTileAtEx(ij2, layer, maskValue, true);
	}
	if (jExtend)
	{
		const int ij2[] = { i, j + 1 };
		SplatTileAtEx(ij2, layer, maskValue, true);
	}
	if (iExtend && jExtend)
	{
		const int ij2[] = { i + 1, j + 1 };
		SplatTileAtEx(ij2, layer, maskValue, true);
	}

	const int ijPatch[2] = { i >> 4, j >> 4 };
	const int offsetPatch = (ijPatch[0] << (_mapShift - 4)) + ijPatch[1];
	RTerrainPatch patch = _vPatches[offsetPatch];

	int channel = patch.GetSplatChannelForLayer(layer);
	if (channel >= 0) // Use existing splat channel for patch:
	{
		SplatTileAt(ij, channel, static_cast<int>(glm::round(255 * maskValue)));
	}
	else if (-1 == channel && patch._usedChannelCount < 4) // Add new splat channel, if there is space for it:
	{
		channel = patch._usedChannelCount;
		patch._layerForChannel[channel] = layer;
		patch._usedChannelCount++;
		SplatTileAt(ij, channel, static_cast<int>(glm::round(255 * maskValue)));
	}

	// Defragment channels:
	if (patch._usedChannelCount > 0 && IsSplatChannelBlankAt(ijPatch, 0))
	{
		for (int k = 0; k < 3; ++k)
		{
			SwapSplatLayersAt(ijPatch, k, k + 1);
			std::swap(patch._layerForChannel[k], patch._layerForChannel[k + 1]);
		}
		patch._usedChannelCount--;
	}
	if (patch._usedChannelCount > 1 && IsSplatChannelBlankAt(ijPatch, 1))
	{
		for (int k = 1; k < 3; ++k)
		{
			SwapSplatLayersAt(ijPatch, k, k + 1);
			std::swap(patch._layerForChannel[k], patch._layerForChannel[k + 1]);
		}
		patch._usedChannelCount--;
	}
	if (patch._usedChannelCount > 2 && IsSplatChannelBlankAt(ijPatch, 2))
	{
		for (int k = 2; k < 3; ++k)
		{
			SwapSplatLayersAt(ijPatch, k, k + 1);
			std::swap(patch._layerForChannel[k], patch._layerForChannel[k + 1]);
		}
		patch._usedChannelCount--;
	}
	if (patch._usedChannelCount > 3 && IsSplatChannelBlankAt(ijPatch, 3))
	{
		patch._usedChannelCount--;
	}

	UpdateMainLayerAt(ij);
}

void EditorTerrain::SwapSplatLayersAt(const int ij[2], int a, int b)
{
	const int iMin = ij[0] << 4;
	const int jMin = ij[1] << 4;
	const int iMax = iMin + 16;
	const int jMax = jMin + 16;
	for (int i = iMin; i < iMax; ++i)
	{
		const int offset = i << _mapShift;
		for (int j = jMin; j < jMax; ++j)
		{
			BYTE* rgba = reinterpret_cast<BYTE*>(&_vBlendBuffer[offset + j]);
			int rgba32[4];
			Utils::CopyByteToInt4(rgba, rgba32);
			int rgbaTemp[4];
			memcpy(rgbaTemp, rgba32, sizeof(rgba32));
			rgbaTemp[3] = 255 - rgbaTemp[0] - rgbaTemp[1] - rgbaTemp[2];
			std::swap(rgbaTemp[a], rgbaTemp[b]);
			memcpy(rgba32, rgbaTemp, sizeof(int) * 3);
			Utils::CopyIntToByte4(rgba32, rgba);
		}
	}
}

bool EditorTerrain::IsSplatChannelBlankAt(const int ij[2], int channel) const
{
	const int iMin = ij[0] << 4;
	const int jMin = ij[1] << 4;
	const int iMax = iMin + 16;
	const int jMax = jMin + 16;
	for (int i = iMin; i < iMax; ++i)
	{
		const int offset = i << _mapShift;
		for (int j = jMin; j < jMax; ++j)
		{
			const BYTE* rgba = reinterpret_cast<const BYTE*>(&_vBlendBuffer[offset + j]);
			if (channel != 3)
			{
				if (rgba[channel])
					return false;
			}
			else
			{
				if (255 - rgba[0] - rgba[1] - rgba[2])
					return false;
			}
		}
	}
	return true;
}

bool EditorTerrain::CanSplatTileAt(const int ij[2], int layer) const
{
	const int mapEdge = _mapSide - 1;
	const int i = Math::Clamp(ij[0], 0, mapEdge);
	const int j = Math::Clamp(ij[1], 0, mapEdge);

	const int ijPatch[2] = { i >> 4, j >> 4 };
	const int offsetPatch = (ijPatch[0] << (_mapShift - 4)) + ijPatch[1];
	RcTerrainPatch patch = _vPatches[offsetPatch];

	int channel = patch.GetSplatChannelForLayer(layer);
	if (channel >= 0)
		return true;
	else
		return patch._usedChannelCount < 4;
}

void EditorTerrain::ApplyBrushSplat(const float xz[2], int layer, int radius, float strength,
	TerrainSplatMode mode, const float* pMask, bool updateTexture)
{
	int ijCenter[2];
	ConvertToBufferCoords(xz, ijCenter);
	const glm::int4 rc = ComputeBrushRect(ijCenter, radius);
	const int r2 = radius * radius;
	const int diameter = radius * 2;
	int iZero = 0, jZero = 0;

	if (TerrainSplatMode::rand == mode)
	{
		std::random_device rd;
		std::mt19937 gen(rd());

		for (int i = rc.y; i < rc.w; ++i)
		{
			for (int j = rc.x; j < rc.z; ++j)
			{
				const int di = ijCenter[0] - i;
				const int dj = ijCenter[1] - j;
				const int rr = di * di + dj * dj;
				if (rr <= r2)
				{
					const int ij[2] = { i, j };
					const float value = float(gen() % 3) * 0.5f;
					SplatTileAtEx(ij, layer, strength * value);
				}
				jZero++;
			}
			iZero++;
			jZero = 0;
		}
	}
	else
	{
		for (int i = rc.y; i < rc.w; ++i)
		{
			for (int j = rc.x; j < rc.z; ++j)
			{
				const int di = ijCenter[0] - i;
				const int dj = ijCenter[1] - j;
				const int rr = di * di + dj * dj;
				if (rr <= r2)
				{
					const float alpha = 1 - static_cast<float>(rr) / r2;
					float maskValue = Math::Min(1.f, alpha * 4);
					if (pMask)
					{
						// Coords for mask lookup:
						const int iMask = Math::Clamp(iZero * 16 / diameter, 0, 15);
						const int jMask = Math::Clamp(jZero * 16 / diameter, 0, 15);
						maskValue = pMask[(iMask << 4) + jMask];
					}
					const float falloff = (TerrainSplatMode::grad == mode) ? alpha : 1;
					const int ij[2] = { i, j };
					SplatTileAtEx(ij, layer, strength * maskValue * falloff);
				}
				jZero++;
			}
			iZero++;
			jZero = 0;
		}
	}

	if (updateTexture)
	{
		UpdateBlendTexture();
		UpdateMainLayerTexture();
	}
}

void EditorTerrain::SplatFromFile(CSZ url, int layer)
{
	if (!IO::FileSystem::FileExist(url))
		return;

	IO::Image image;
	image.Init(url);

	if (image._width != image._height || image._width != _mapSide)
		return;

	const int half = _mapSide >> 1;
	VERUS_P_FOR(i, _mapSide)
	{
		VERUS_FOR(j, _mapSide)
		{
			const BYTE value = image._p[((i << _mapShift) + j) * image._bytesPerPixel];
			const int ij[] = { i, j };
			SplatTileAtEx(ij, layer, value / 255.f);
		}
	});

	UpdateBlendTexture();
	UpdateMainLayerTexture();
}
