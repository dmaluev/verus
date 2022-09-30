// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::IO;

Vwx::Vwx()
{
}

Vwx::~Vwx()
{
	Async::I().Cancel(this);
}

void Vwx::Async_WhenLoaded(CSZ url, RcBlob blob)
{
	if (Str::EndsWith(url, ".xxx", false))
	{
		Deserialize_LegacyXXX(url, blob);
		return;
	}

	VERUS_QREF_WM;

	IO::StreamPtr sp(blob);

	UINT32 temp;
	UINT32 blockType = 0;
	INT64 blockSize = 0;
	char buffer[IO::Stream::s_bufferSize] = {};

	sp.Read(buffer, 5);
	VERUS_RT_ASSERT(!strcmp(buffer, "<VWX>"));
	sp >> temp;
	sp >> temp;
	sp.ReadString(buffer);
	VERUS_RT_ASSERT(!strcmp(buffer, "1.0.0"));
	UINT32 verMajor = 0, verMinor = 0, verPatch = 0;
	sscanf(buffer, "%d.%d.%d", &verMajor, &verMinor, &verPatch);
	sp.SetVersion(VERUS_MAKE_VERSION(verMajor, verMinor, verPatch));

	while (!sp.IsEnd())
	{
		sp >> temp;
		sp >> blockType;
		sp >> blockSize;
		switch (blockType)
		{
		case '>MW<':
		{
			wm.Deserialize(sp);
		}
		break;
		default:
		{
			sp.Advance(blockSize);
		}
		}
	}
}

void Vwx::Serialize(CSZ url)
{
	File file;
	if (!file.Open(url, "wb"))
		return;

	VERUS_QREF_WM;

	file.WriteText("<VWX>");

	file.WriteText(VERUS_CRNL VERUS_CRNL "<VN>");
	file.WriteString("1.0.0");

	wm.Serialize(file);
}

void Vwx::Deserialize(CSZ url, bool sync)
{
	if (sync)
	{
		Vector<BYTE> vData;
		IO::FileSystem::LoadResource(url, vData);
		Async_WhenLoaded(url, Blob(vData.data(), vData.size()));
	}
	else
		Async::I().Load(url, this);
}

void Vwx::Deserialize_LegacyXXX(CSZ url, RcBlob blob)
{
	VERUS_QREF_WM;

	IO::StreamPtr sp(blob);

	UINT32 temp;
	UINT32 blockType = 0;
	INT64 blockSize = 0;
	char buffer[IO::Stream::s_bufferSize] = {};

	sp.Read(buffer, 5);
	VERUS_RT_ASSERT(!strcmp(buffer, "<XXX>"));
	sp >> temp;
	sp >> temp;
	sp.ReadString(buffer);
	VERUS_RT_ASSERT(!strcmp(buffer, "3.0"));
	UINT32 verMajor = 0, verMinor = 0;
	sscanf(buffer, "%d.%d", &verMajor, &verMinor);
	sp.SetVersion(VERUS_MAKE_VERSION(1, 0, 0));

	Vector<BYTE> vTerrain;
	while (!sp.IsEnd())
	{
		sp >> temp;
		sp >> blockType;
		sp >> blockSize;
		switch (blockType)
		{
		case '>RT<':
		{
			vTerrain.resize(blockSize);
			sp.Read(vTerrain.data(), blockSize);
		}
		break;
		case '>MS<':
		{
			wm.Deserialize_LegacyXXX(sp);
		}
		break;
		default:
		{
			sp.Advance(blockSize);
		}
		}
	}

	if (!vTerrain.empty() && wm.IsInitialized())
	{
		IO::StreamPtr sp(Blob(vTerrain.data(), vTerrain.size()));
		World::TerrainNode::Desc desc;
		desc._terrainDesc._mapSide = 0;
		World::TerrainNodePtr terrainNode;
		terrainNode.Init(desc);
		terrainNode->GetTerrain().Deserialize(sp);
	}
}
