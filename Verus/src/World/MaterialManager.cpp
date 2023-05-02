// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// Texture:

Texture::Texture()
{
}

Texture::~Texture()
{
	Done();
}

void Texture::Init(CSZ url, bool streamParts, bool sync, CGI::PcSamplerDesc pSamplerDesc)
{
	if (_refCount)
		return;

	_streamParts = streamParts;
	if (_streamParts)
		_requestedPart = FLT_MAX; // Start with tiny texture.

	CGI::TextureDesc texDesc;
	texDesc._pSamplerDesc = pSamplerDesc;
	texDesc._url = url;
	texDesc._texturePart = _streamParts ? 512 : 0;
	if (sync)
		texDesc._flags = CGI::TextureDesc::Flags::sync;
	_texTiny.Init(texDesc);
	_refCount = 1;
}

bool Texture::Done()
{
	_refCount--;
	if (_refCount <= 0)
	{
		_texTiny.Done();
		return true;
	}
	return false;
}

void Texture::Update()
{
	UpdateStreaming();
}

void Texture::UpdateStreaming()
{
	if (!_streamParts)
		return;

	VERUS_QREF_RENDERER;

	// Check if the next huge texture is loaded:
	const int nextHuge = (_currentHuge + 1) & 0x1;
	if (_loading && _texHuge[nextHuge] && _texHuge[nextHuge]->IsLoaded())
	{
		_loading = false;
		_currentHuge = nextHuge; // Use it.
		_doneFrame = renderer.GetFrameCount() + CGI::BaseRenderer::s_ringBufferSize + 1;
	}

	if (!_loading && renderer.GetFrameCount() >= _doneFrame)
	{
		const int prevHuge = (_currentHuge + 1) & 0x1;
		if (_texHuge[prevHuge]) // Garbage collection (next huge texture must be empty).
			_texHuge[prevHuge].Done();

		const int maxPart = _texTiny->GetPart();
		if (maxPart > 0) // Must have at least two parts: 0 and 1.
		{
			const float reqPart = Math::Min<float>(_requestedPart, SHRT_MAX);

			const bool hugeReady = (_texHuge[_currentHuge] && _texHuge[_currentHuge]->IsLoaded());
			const int currentPart = hugeReady ? _texHuge[_currentHuge]->GetPart() : maxPart;
			int newPart = currentPart;

			if (reqPart < currentPart) // Increase texture?
				newPart = static_cast<int>(reqPart);
			if (reqPart > currentPart + 1.5f) // Decrease texture?
				newPart = static_cast<int>(reqPart);

			newPart = Math::Clamp(newPart, 0, maxPart);

			if (_texHuge[_currentHuge] && (newPart == maxPart)) // Unload huge texture, use tiny?
			{
				_currentHuge = (_currentHuge + 1) & 0x1; // Go to empty huge texture.
				_doneFrame = renderer.GetFrameCount() + CGI::BaseRenderer::s_ringBufferSize + 1;
			}
			else if (newPart != currentPart) // Change part?
			{
				_loading = true;
				const int nextHuge = (_currentHuge + 1) & 0x1;

				CGI::TextureDesc texDesc;
				texDesc._url = _C(_texTiny->GetName());
				texDesc._texturePart = newPart;
				_texHuge[nextHuge].Init(texDesc);
			}
		}
	}
}

CGI::TexturePtr Texture::GetTex() const
{
	return _texHuge[_currentHuge] ? _texHuge[_currentHuge] : _texTiny;
}

CGI::TexturePtr Texture::GetTinyTex() const
{
	return _texTiny;
}

CGI::TexturePtr Texture::GetHugeTex() const
{
	return _texHuge[_currentHuge];
}

int Texture::GetPart() const
{
	return _texHuge[_currentHuge] ? _texHuge[_currentHuge]->GetPart() : _texTiny->GetPart();
}

// TexturePtr:

void TexturePtr::Init(CSZ url, bool streamParts, bool sync, CGI::PcSamplerDesc pSamplerDesc)
{
	VERUS_QREF_MM;
	VERUS_RT_ASSERT(!_p);
	_p = mm.InsertTexture(url);
	_p->Init(url, streamParts, sync, pSamplerDesc);
}

void TexturePwn::Done()
{
	if (_p)
	{
		VERUS_QREF_MM;
		mm.DeleteTexture(_C(_p->GetTex()->GetName()));
		_p = nullptr;
	}
}

// Material::Pick:

Material::Pick::Pick()
{
}

Material::Pick::Pick(const glm::vec4& v) : glm::vec4(v)
{
}

void Material::Pick::SetColor(float r, float g, float b)
{
	x = r;
	y = g;
	z = b;
}

void Material::Pick::SetAlpha(float x)
{
	w = x;
}

void Material::Pick::FromString(CSZ sz)
{
	float rgba[4];
	Convert::ColorTextToFloat4(sz, rgba);
	glm::vec4::operator=(glm::make_vec4(rgba));
}

String Material::Pick::ToString()
{
	const UINT32 color = Convert::ColorFloatToInt32(glm::value_ptr(static_cast<glm::vec4&>(*this)));
	char txt[20];
	sprintf_s(txt, "0x%02X%02X%02X%02X",
		(color >> 0) & 0xFF,
		(color >> 8) & 0xFF,
		(color >> 16) & 0xFF,
		(color >> 24) & 0xFF);
	return txt;
}

// Material:

Material::Material()
{
}

Material::~Material()
{
	Done();
}

void Material::Init(RcDesc desc)
{
	if (_refCount)
		return;

	VERUS_INIT();

	_name = desc._name;
	_refCount = 1;

	if (desc._load)
	{
		VERUS_RT_ASSERT(
			Str::EndsWith(_C(_name), ".x3d") ||
			Str::EndsWith(_C(_name), ".vml"));
		String url(desc._name);
		Str::ReplaceExtension(url, ".vml");
		_name = url;

		Vector<BYTE> vData;
		IO::FileSystem::I().LoadResourceFromCache(_C(url), vData, desc._mandatory);
		if (!vData.empty())
		{
			FromString(reinterpret_cast<CSZ>(vData.data()));
			LoadTextures(desc._streamParts);
		}
	}
}

bool Material::Done()
{
	_refCount--;
	if (_refCount <= 0)
	{
		Mesh::GetSimpleShader()->FreeDescriptorSet(_cshSimple);
		Mesh::GetShader()->FreeDescriptorSet(_cshTemp);
		Mesh::GetShader()->FreeDescriptorSet(_cshTiny);
		Mesh::GetShader()->FreeDescriptorSet(_csh);
		_texX.Done();
		_texN.Done();
		_texA.Done();
		VERUS_DONE(Material);
		return true;
	}
	return false;
}

bool Material::operator<(RcMaterial that) const
{
	if (_blending != that._blending)
		return _blending < that._blending;

	if (_texA != that._texA)
		return _texA < that._texA;

	if (_texN != that._texN)
		return _texN < that._texN;

	if (_texX != that._texX)
		return _texX < that._texX;

	return _name < that._name;
}

void Material::Update()
{
	VERUS_UPDATE_ONCE_CHECK;
	VERUS_QREF_RENDERER;

	// Garbage collection:
	if (_cshTemp.IsSet() && static_cast<INT64>(renderer.GetFrameCount()) >= _cshFreeFrame)
		Mesh::GetShader()->FreeDescriptorSet(_cshTemp);

	// Request BindDescriptorSetTextures call by clearing _csh:
	const bool partChanged =
		_texA && (_aPart != _texA->GetPart()) ||
		_texN && (_nPart != _texN->GetPart()) ||
		_texX && (_xPart != _texX->GetPart());
	if (partChanged && !_cshTemp.IsSet())
	{
		_aPart = _texA->GetPart();
		_nPart = _texN->GetPart();
		_xPart = _texX->GetPart();
		_cshTemp = _csh;
		_csh = CGI::CSHandle();
		_cshFreeFrame = renderer.GetFrameCount() + CGI::BaseRenderer::s_ringBufferSize;
	}

	// Update _csh:
	if (!_csh.IsSet() && IsLoaded())
		BindDescriptorSetTextures();
}

bool Material::IsLoaded() const
{
	if (!_texA || !_texN || !_texX)
		return false;
	return
		_texA->GetTex()->IsLoaded() &&
		_texN->GetTex()->IsLoaded() &&
		_texX->GetTex()->IsLoaded();
}

void Material::LoadTextures(bool streamParts)
{
	_texX.Done();
	_texN.Done();
	_texA.Done();

	String newUrl;
	auto ExpandPath = [this, &newUrl](CSZ url)
	{
		if (!strncmp(url, "./", 2))
		{
			newUrl = Str::GetPath(_C(_name));
			newUrl += url + 1;
			return _C(newUrl);
		}
		return url;
	};

	_texA.Init(ExpandPath(_dict.Find("texA")), streamParts);
	_texN.Init(ExpandPath(_dict.Find("texN")), streamParts);
	_texX.Init(ExpandPath(_dict.Find("texX")), streamParts);
}

String Material::ToString(bool cleanDict)
{
	if (cleanDict)
		_dict.DeleteAll();

	CSZ empty = "";

	CGI::PBaseTexture pTexA = _texA->GetTex().Get();
	CGI::PBaseTexture pTexN = _texN->GetTex().Get();
	CGI::PBaseTexture pTexX = _texX->GetTex().Get();

	_dict.Insert("anisoSpecDir",          /**/_C(Str::ToString(_anisoSpecDir, true)));
	_dict.Insert("detail",                /**/_C(Str::ToString(_detail)));
	_dict.Insert("emission",              /**/_C(Str::ToString(_emission)));
	_dict.Insert("nmContrast",            /**/_C(Str::ToString(_nmContrast)));
	_dict.Insert("roughDiffuse",          /**/_C(Str::ToString(_roughDiffuse)));
	_dict.Insert("solidA",                /**/_C(Str::ToColorString(_solidA, true, true)));
	_dict.Insert("solidN",                /**/_C(Str::ToColorString(_solidN, false, true)));
	_dict.Insert("solidX",                /**/_C(Str::ToColorString(_solidX, false, true)));
	_dict.Insert("sssHue",                /**/_C(Str::ToString(_sssHue)));
	_dict.Insert("sssPick",               /**/_C(_sssPick.ToString()));
	_dict.Insert("tc0ScaleBias",          /**/_C(Str::ToString(_tc0ScaleBias, true)));
	_dict.Insert("texA",                  /**/pTexA ? _C(pTexA->GetName()) : empty);
	_dict.Insert("texN",                  /**/pTexN ? _C(pTexN->GetName()) : empty);
	_dict.Insert("texX",                  /**/pTexX ? _C(pTexX->GetName()) : empty);
	_dict.Insert("userColor",             /**/_C(Str::ToColorString(_userColor)));
	_dict.Insert("userPick",              /**/_C(_userPick.ToString()));
	_dict.Insert("xAnisoSpecScaleBias",   /**/_C(Str::ToString(_xAnisoSpecScaleBias, true)));
	_dict.Insert("xMetallicScaleBias",    /**/_C(Str::ToString(_xMetallicScaleBias, true)));
	_dict.Insert("xRoughnessScaleBias",   /**/_C(Str::ToString(_xRoughnessScaleBias, true)));
	_dict.Insert("xWrapDiffuseScaleBias", /**/_C(Str::ToString(_xWrapDiffuseScaleBias, true)));

	return _dict.ToString();
}

void Material::FromString(CSZ txt)
{
	_dict.FromString(txt);

	_anisoSpecDir = Str::FromStringVec2(_dict.Find("anisoSpecDir", "0 1"));
	_detail = _dict.FindFloat("detail", 0);
	_emission = _dict.FindFloat("emission", 0);
	_nmContrast = _dict.FindFloat("nmContrast", -2);
	_roughDiffuse = _dict.FindFloat("roughDiffuse", 0);
	_solidA = Str::FromColorString(_dict.Find("solidA", "80808000"));
	_solidN = Str::FromColorString(_dict.Find("solidN", "8080FF00"), false);
	_solidX = Str::FromColorString(_dict.Find("solidX", "FF808000"), false);
	_sssHue = _dict.FindFloat("sssHue", 0);
	_sssPick.FromString(_dict.Find("sssPick", "00000000"));
	_tc0ScaleBias = Str::FromStringVec4(_dict.Find("tc0ScaleBias", "1 1 0 0"));
	_userColor = Str::FromColorString(_dict.Find("userColor", "00000000"));
	_userPick.FromString(_dict.Find("userPick", "00000000"));
	_xAnisoSpecScaleBias = Str::FromStringVec2(_dict.Find("xAnisoSpecScaleBias", "1 0"));
	_xMetallicScaleBias = Str::FromStringVec2(_dict.Find("xMetallicScaleBias", "1 0"));
	_xRoughnessScaleBias = Str::FromStringVec2(_dict.Find("xRoughnessScaleBias", "1 0"));
	_xWrapDiffuseScaleBias = Str::FromStringVec2(_dict.Find("xWrapDiffuseScaleBias", "1 0"));
}

CGI::CSHandle Material::GetComplexSetHandle() const
{
	if (_csh.IsSet())
		return _csh;
	return _cshTiny;
}

CGI::CSHandle Material::GetComplexSetHandleSimple() const
{
	return _cshSimple;
}

void Material::BindDescriptorSetTextures()
{
	VERUS_RT_ASSERT(!_csh.IsSet());
	VERUS_RT_ASSERT(IsLoaded());
	VERUS_QREF_CSMB;
	VERUS_QREF_MM;
	if (!_cshTiny.IsSet())
	{
		_cshTiny = Mesh::GetShader()->BindDescriptorSetTextures(1,
			{
				_texA->GetTinyTex(),
				_texN->GetTinyTex(),
				_texX->GetTinyTex(),
				mm.GetDetailTexture(),
				mm.GetDetailNTexture(),
				mm.GetStrassTexture()
			});
	}
	if (!_cshSimple.IsSet())
	{
		_cshSimple = Mesh::GetSimpleShader()->BindDescriptorSetTextures(1,
			{
				_texA->GetTinyTex(),
				_texX->GetTinyTex(),
				csmb.GetTexture()
			});
	}
	_csh = Mesh::GetShader()->BindDescriptorSetTextures(1,
		{
			_texA->GetTex(),
			_texN->GetTex(),
			_texX->GetTex(),
			mm.GetDetailTexture(),
			mm.GetDetailNTexture(),
			mm.GetStrassTexture()
		});
}

bool Material::UpdateMeshUniformBuffer(float motionBlur, bool resolveDitheringMaskEnabled)
{
	Mesh::UB_PerMaterialFS& ub = Mesh::GetUbPerMaterialFS();
	Mesh::UB_PerMaterialFS ubPrev;
	memcpy(&ubPrev, &ub, sizeof(Mesh::UB_PerMaterialFS));

	ub._anisoSpecDir_detail_emission = float4(_anisoSpecDir, _detail, _emission);
	ub._motionBlur_nmContrast_roughDiffuse_sssHue.x = motionBlur;
	ub._motionBlur_nmContrast_roughDiffuse_sssHue.y = _nmContrast;
	ub._motionBlur_nmContrast_roughDiffuse_sssHue.z = _roughDiffuse + (resolveDitheringMaskEnabled ? 0.f : 1.f);
	ub._motionBlur_nmContrast_roughDiffuse_sssHue.w = _sssHue;
	ub._detailScale_strassScale = float4(1, 1, 1, 1);
	ub._solidA = _solidA;
	ub._solidN = _solidN;
	ub._solidX = _solidX;
	ub._sssPick = _sssPick;
	ub._tc0ScaleBias = _tc0ScaleBias;
	ub._userColor = _userColor;
	ub._userPick = _userPick;
	ub._xAnisoSpecScaleBias_xMetallicScaleBias = float4(_xAnisoSpecScaleBias, _xMetallicScaleBias);
	ub._xRoughnessScaleBias_xWrapDiffuseScaleBias = float4(_xRoughnessScaleBias, _xWrapDiffuseScaleBias);
	if (_texA)
	{
		ub._detailScale_strassScale = float4(
			_texA->GetTex()->GetSize().getX() / 256 * 4,
			_texA->GetTex()->GetSize().getY() / 256 * 4,
			_texA->GetTex()->GetSize().getX() / 256 * 8,
			_texA->GetTex()->GetSize().getY() / 256 * 8);
	}

	return 0 != memcmp(&ubPrev, &ub, sizeof(Mesh::UB_PerMaterialFS));
}

bool Material::UpdateMeshUniformBufferSimple()
{
	Mesh::UB_SimplePerMaterialFS& ub = Mesh::GetUbSimplePerMaterialFS();
	Mesh::UB_SimplePerMaterialFS ubPrev;
	memcpy(&ubPrev, &ub, sizeof(Mesh::UB_SimplePerMaterialFS));

	ub._anisoSpecDir_detail_emission = float4(_anisoSpecDir, _detail, _emission);
	ub._motionBlur_nmContrast_roughDiffuse_sssHue.x = 0;
	ub._motionBlur_nmContrast_roughDiffuse_sssHue.y = _nmContrast;
	ub._motionBlur_nmContrast_roughDiffuse_sssHue.z = _roughDiffuse;
	ub._motionBlur_nmContrast_roughDiffuse_sssHue.w = _sssHue;
	ub._detailScale_strassScale = float4(1, 1, 1, 1);
	ub._solidA = _solidA;
	ub._solidN = _solidN;
	ub._solidX = _solidX;
	ub._sssPick = _sssPick;
	ub._tc0ScaleBias = _tc0ScaleBias;
	ub._userColor = _userColor;
	ub._userPick = _userPick;
	ub._xAnisoSpecScaleBias_xMetallicScaleBias = float4(_xAnisoSpecScaleBias, _xMetallicScaleBias);
	ub._xRoughnessScaleBias_xWrapDiffuseScaleBias = float4(_xRoughnessScaleBias, _xWrapDiffuseScaleBias);
	if (_texA)
	{
		ub._detailScale_strassScale = float4(
			_texA->GetTex()->GetSize().getX() / 256 * 4,
			_texA->GetTex()->GetSize().getY() / 256 * 4,
			_texA->GetTex()->GetSize().getX() / 256 * 8,
			_texA->GetTex()->GetSize().getY() / 256 * 8);
	}

	return 0 != memcmp(&ubPrev, &ub, sizeof(Mesh::UB_SimplePerMaterialFS));
}

void Material::IncludePart(float part)
{
	_texA->IncludePart(part);
	_texN->IncludePart(part);
	_texX->IncludePart(part);
}

// MaterialPtr:

void MaterialPtr::Init(Material::RcDesc desc)
{
	VERUS_QREF_MM;
	VERUS_RT_ASSERT(!_p);
	_p = mm.InsertMaterial(desc._name);
	_p->Init(desc);
}

void MaterialPwn::Done()
{
	if (_p)
	{
		MaterialManager::I().DeleteMaterial(_C(_p->_name));
		_p = nullptr;
	}
}

// MaterialManager:

MaterialManager::MaterialManager()
{
}

MaterialManager::~MaterialManager()
{
	Done();
}

void MaterialManager::Init()
{
	VERUS_INIT();
}

void MaterialManager::InitCmd()
{
	_texDefaultA.Init("[Textures]:Default.dds", false, true);
	_texDefaultN.Init("[Textures]:Default.N.dds", false, true);
	_texDefaultX.Init("[Textures]:Default.X.dds", false, true);
	_texDetail.Init("[Textures]:Detail.X.dds", false, true);
	_texDetailN.Init("[Textures]:Detail.N.dds", false, true);
	_texStrass.Init("[Textures]:Strass.X.dds", false, true);

	CGI::TextureDesc texDesc;
	texDesc._clearValue = Vector4(1);
	texDesc._name = "MaterialManager.DummyShadow";
	texDesc._format = CGI::Format::unormD24uintS8;
	texDesc._width = 16;
	texDesc._height = 16;
	texDesc._flags = CGI::TextureDesc::Flags::depthSampledR;
	_texDummyShadow.Init(texDesc);

	_cshDefault = Mesh::GetShader()->BindDescriptorSetTextures(1,
		{
			_texDefaultA->GetTex(),
			_texDefaultN->GetTex(),
			_texDefaultX->GetTex(),
			_texDetail->GetTex(),
			_texDetailN->GetTex(),
			_texStrass->GetTex()
		});
	_cshDefaultSimple = Mesh::GetSimpleShader()->BindDescriptorSetTextures(1,
		{
			_texDefaultA->GetTinyTex(),
			_texDefaultX->GetTinyTex(),
			_texDummyShadow
		});
}

void MaterialManager::Done()
{
	if (Mesh::GetSimpleShader())
		Mesh::GetSimpleShader()->FreeDescriptorSet(_cshDefaultSimple);
	if (Mesh::GetShader())
		Mesh::GetShader()->FreeDescriptorSet(_cshDefault);
	_texDummyShadow.Done();
	_texStrass.Done();
	_texDetailN.Done();
	_texDetail.Done();
	_texDefaultX.Done();
	_texDefaultN.Done();
	_texDefaultA.Done();
	DeleteAll();
	VERUS_DONE(MaterialManager);
}

void MaterialManager::DeleteAll()
{
	DeleteAllMaterials();
	DeleteAllTextures();
}

void MaterialManager::Update()
{
	VERUS_UPDATE_ONCE_CHECK;

	for (auto& x : TStoreTextures::_map)
		x.second.Update();

	for (auto& x : TStoreMaterials::_map)
		x.second.Update();
}

PTexture MaterialManager::InsertTexture(CSZ url)
{
	return TStoreTextures::Insert(url);
}

PTexture MaterialManager::FindTexture(CSZ url)
{
	return TStoreTextures::Find(url);
}

void MaterialManager::DeleteTexture(CSZ url)
{
	TStoreTextures::Delete(url);
}

void MaterialManager::DeleteAllTextures()
{
	TStoreTextures::DeleteAll();
}

PMaterial MaterialManager::InsertMaterial(CSZ name)
{
	return TStoreMaterials::Insert(name);
}

PMaterial MaterialManager::FindMaterial(CSZ name)
{
	return TStoreMaterials::Find(name);
}

void MaterialManager::DeleteMaterial(CSZ name)
{
	TStoreMaterials::Delete(name);
}

void MaterialManager::DeleteAllMaterials()
{
	TStoreMaterials::DeleteAll();
}

void MaterialManager::Serialize(IO::RSeekableStream stream)
{
}

void MaterialManager::Deserialize(IO::RStream stream)
{
}

Vector<String> MaterialManager::GetTextureNames() const
{
	Vector<String> v;
	VERUS_FOREACH_CONST(TStoreTextures::TMap, TStoreTextures::_map, it)
		v.push_back(_C(it->second.GetTex()->GetName()));
	return v;
}

Vector<String> MaterialManager::GetMaterialNames() const
{
	Vector<String> v;
	VERUS_FOREACH_CONST(TStoreMaterials::TMap, TStoreMaterials::_map, it)
		v.push_back(it->second._name);
	return v;
}

void MaterialManager::ResetPart()
{
	for (auto& x : TStoreTextures::_map)
		x.second.ResetPart();
}

float MaterialManager::ComputePart(float distSq, float objectRadius)
{
	// PART | DIST
	// 0    | 0
	// 1    | 20
	// 2    | 40
	// 3    | 80
	// 4    | 160

	if (distSq < 160 * 160)
	{
		const float dist = Math::Max<float>(0, sqrt(distSq) - objectRadius);
		return glm::log2(Math::Clamp<float>(dist * (2 / 20.f), 1, 16));
	}
	return 4;
}
