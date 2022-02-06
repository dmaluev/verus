// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::IO;

void Xxx::Serialize(CSZ path)
{
	VERUS_QREF_SM;
	VERUS_QREF_MM;

	File file;
	if (!file.Open(path, "wb"))
		return;
	const UINT32 magic = '2XXX';
	file << magic;
	const UINT16 version = 0x0105;
	file << version;

	mm.Serialize(file);
	sm.Serialize(file);
}

void Xxx::Deserialize(CSZ path, RString error, bool keepExisting)
{
#if 0
	VERUS_QREF_SM;
	VERUS_QREF_MM;

	if (!IO::CFileSys::FileExist(path))
	{
		error = "File not found.";
		return;
	}
	CVector<BYTE> vData;
	IO::CFileSys::LoadResource(path, vData);
	if (vData.size() < 4)
	{
		error = "Invalid file size.";
		return;
	}

	if (CStr::EndsWith(path, ".xml", false))
	{
		tinyxml2::XMLDocument doc;
		doc.Parse(reinterpret_cast<CSZ>(&vData.front()));
		if (doc.Error())
		{
			error = "Invalid XML.";
			return;
		}

		tinyxml2::XMLHandle hDoc(&doc);
		tinyxml2::XMLHandle hRoot = hDoc.FirstChildElement();

		// Restart material manager:
		if (!keepExisting)
		{
			mm.Done();
			mm.Init();
		}

		CString levelName = strrchr(path, '\\') + 1;
		levelName = levelName.substr(0, levelName.length() - 4);

		tinyxml2::XMLElement* pElemPath = hRoot.FirstChildElement("path").ToElement();
		if (pElemPath && pElemPath->GetText())
			levelName = pElemPath->GetText();

		CStringStream ssMissingTextures;
		for (tinyxml2::XMLElement* pElem = hRoot.FirstChildElement("material").ToElement(); pElem; pElem = pElem->NextSiblingElement("material"))
		{
			CSZ name = pElem->GetText();
			if (!name)
				continue;

			CString nameEx = strchr(name, '\\') ? CStr::FixCyrillicX(name, false) : name;
			CString nameEx2 = strchr(name, '\\') ? CStr::FixCyrillicX(name, true) : name;

			CString textureUrl = CString("Textures:") + levelName + "/" + nameEx;
			textureUrl = textureUrl.substr(0, textureUrl.length() - 4) + ".dds";

			if (!IO::CFileSys::FileExist(textureUrl.c_str()))
			{
				ssMissingTextures << textureUrl << VERUS_CRNL;
				textureUrl = CString("Textures:") + levelName + "/" + nameEx2;
				textureUrl = textureUrl.substr(0, textureUrl.length() - 4) + ".dds";
			}

			if (IO::CFileSys::FileExist(textureUrl.c_str()))
			{
				Scene::PMaterial pMat = new Scene::CMaterial(pElem->Attribute("name"));
				pMat->m_texAlbedoID = mm.Texture_Add(textureUrl.c_str());
				pMat->m_colorDiffuse.FromString4(pElem->Attribute("faceColor"));
				pMat->m_colorSpec.FromString3(pElem->Attribute("specularColor"));
				pMat->m_colorEmission.FromString3(pElem->Attribute("emissiveColor"));
				pElem->QueryFloatAttribute("power", &pMat->m_specPower);
				mm.Material_Add(pMat, false);
			}
			else
			{
				ssMissingTextures << textureUrl << VERUS_CRNL;
			}
		}
		if (!ssMissingTextures.str().empty())
		{
			IO::CFileSys::SaveString(IO::CFileSys::ReplaceFilename(path, "MissingTextures.txt").c_str(), ssMissingTextures.str().c_str());
		}

		if (!keepExisting)
			sm.DeleteAll();

		CStringStream ssMissingObjects;
		for (tinyxml2::XMLElement* pElem = hRoot.FirstChildElement("object").ToElement(); pElem; pElem = pElem->NextSiblingElement("object"))
		{
			CSZ name = pElem->GetText();
			if (!name)
				continue;
			CSZ copyOf = pElem->Attribute("copyOf");
			float4x4 initialMatrix;
			initialMatrix.FromString(pElem->Attribute("matrix"));
			CSZ material = pElem->Attribute("mat");
			if (copyOf && strlen(copyOf) > 0)
			{
				Scene::PUnit pu = static_cast<Scene::PUnit>(sm.GetObjectByID(sm.FindUnitID(copyOf)));
				Scene::PModel pModel = sm.FindModel(pu->GetModelID());

				Math::CBounds modelBounds;
				modelBounds.FromOrientedBox(pModel->GetMesh().GetBounds(), initialMatrix);
				const float size = modelBounds.GetSmartSize();

				Scene::CUnit::CInitParams ip;
				ip.m_initialMatrix = initialMatrix;
				ip.m_url = name;
				ip.m_urlModel = pu->GetModelName().c_str();
				ip.m_material = material;
				sm.AddUnit(ip);

				pu = static_cast<Scene::PUnit>(sm.GetObjectByID(sm.FindUnitID(name)));

				if (size > 20)
				{
					char scale[16];
					sprintf_s(scale, "%g", 20 / size);
					pu->GetParamList().Add("$LM_SCALE", scale);
				}
			}
			else
			{
				CString modelUrl = CString("Geometry:") + levelName + "/" + name + ".x3d";
				if (IO::CFileSys::FileExist(modelUrl.c_str()))
				{
					sm.InsertModel(modelUrl.c_str(), false, material);
					Scene::PModel pModel = sm.FindModel(sm.FindModelID(modelUrl.c_str()));

					Math::CBounds modelBounds;
					modelBounds.FromOrientedBox(pModel->GetMesh().GetBounds(), initialMatrix);
					const float size = modelBounds.GetSmartSize();

					Scene::CUnit::CInitParams ip;
					ip.m_initialMatrix = initialMatrix;
					ip.m_url = name;
					ip.m_urlModel = modelUrl.c_str();
					ip.m_material = material;
					sm.AddUnit(ip);

					Scene::PUnit pu = static_cast<Scene::PUnit>(sm.GetObjectByID(sm.FindUnitID(name)));

					if (size > 20)
					{
						char scale[16];
						sprintf_s(scale, "%g", 20 / size);
						pu->GetParamList().Add("$LM_SCALE", scale);
					}
				}
				else
				{
					ssMissingObjects << modelUrl << VERUS_CRNL;
				}
			}
		}
		if (!ssMissingObjects.str().empty())
		{
			IO::CFileSys::SaveString(IO::CFileSys::ReplaceFilename(path, "MissingObjects.txt").c_str(), ssMissingObjects.str().c_str());
		}
	}
	else
	{
		IO::StreamPtr sp(&vData.front(), vData.size());
		UINT32 magic = 0;
		sp >> magic;
		if (magic != '2XXX')
		{
			error = "File format is not XXX2.";
			return;
		}
		UINT16 version = 0;
		sp >> version;
		if (version != 0x0105)
		{
			CStringStream ss;
			ss << "Invalid file version (0x" << std::hex << version << ").";
			error = ss.str();
			return;
		}
		UINT16 chunkID = 0;
		int blockSize, blockBegin, blockEnd;
		while (sp.GetOffset() < sp.GetSize())
		{
			sp >> chunkID;
			sp >> blockSize;
			blockBegin = sp.GetOffset();
			switch (chunkID)
			{
			case VERUS_XXX_MATERIALMANAGER:
				mm.Deserialize(sp);
				break;
			case VERUS_XXX_LANDSCAPE:
				break;
			case VERUS_XXX_SCENEMANAGER:
				sm.Deserialize(sp);
				break;
			default:
			{
				sp.Advance(blockSize);
			}
			}
			blockEnd = sp.GetOffset();
			if (blockEnd - blockBegin != blockSize)
			{
				error = "File is corrupted.";
				return;
			}
		}
	}
#endif
}

Xxx::Xxx(Scene::Terrain* pTerrain) :
	_pTerrain(pTerrain)
{
}

Xxx::~Xxx()
{
	Async::I().Cancel(this);
}

void Xxx::Async_WhenLoaded(CSZ url, RcBlob blob)
{
	VERUS_QREF_SM;

	IO::StreamPtr sp(blob);

	UINT32 temp;
	UINT32 blockType = 0;
	INT64 blockSize = 0;
	char buffer[IO::Stream::s_bufferSize] = {};
	sp.Read(buffer, 5);
	sp >> temp;
	sp >> temp;
	sp.ReadString(buffer);
	VERUS_RT_ASSERT(!strcmp(buffer, "3.0"));
	UINT32 verMajor = 0, verMinor = 0;
	sscanf(buffer, "%d.%d", &verMajor, &verMinor);
	sp.SetVersion(MakeVersion(verMajor, verMinor));

	while (!sp.IsEnd())
	{
		sp >> temp;
		sp >> blockType;
		sp >> blockSize;
		switch (blockType)
		{
		case '>RT<':
		{
			if (_pTerrain)
				_pTerrain->Deserialize(sp);
			else
				sp.Advance(blockSize);
		}
		break;
		case '>MS<':
		{
			sm.Deserialize(sp);
		}
		break;
		default:
		{
			sp.Advance(blockSize);
		}
		}
	}
}

void Xxx::SerializeXXX3(CSZ url)
{
	File file;
	if (!file.Open(url, "wb"))
		return;

	VERUS_QREF_SM;

	file.WriteText("<XXX>");

	file.WriteText(VERUS_CRNL VERUS_CRNL "<VN>");
	file.WriteString("3.0");

	if (_pTerrain)
		_pTerrain->Serialize(file);

	sm.Serialize(file);
}

void Xxx::DeserializeXXX3(CSZ url, bool sync)
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

UINT32 Xxx::MakeVersion(UINT32 verMajor, UINT32 verMinor)
{
	return (verMajor << 16) | (verMinor & 0xFFFF);
}
