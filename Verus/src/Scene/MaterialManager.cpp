#include "verus.h"

using namespace verus;
using namespace verus::Scene;

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
		_requestedPart = FLT_MAX;

	CGI::TextureDesc texDesc;
	texDesc._url = url;
	texDesc._texturePart = _streamParts ? 512 : 0;
	texDesc._pSamplerDesc = pSamplerDesc;
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
	const bool hugeReady = (_texHuge && _texHuge->IsLoaded());
	if (_streamParts && (!_texHuge || hugeReady))
	{
		const int maxPart = _texTiny->GetPart();
		if (maxPart > 0) // Must have at least two parts: 0 and 1.
		{
			const float reqPart = Math::Min<float>(_requestedPart, SHRT_MAX);

			const int part = hugeReady ? _texHuge->GetPart() : maxPart;
			int newPart = part;

			if (part > reqPart)
				newPart = static_cast<int>(reqPart);
			if (part + 1.5f < reqPart)
				newPart = static_cast<int>(reqPart);

			newPart = Math::Clamp(newPart, 0, maxPart);

			if (_texHuge && (newPart == maxPart)) // Unload texture?
			{
				_texHuge.Done();
			}
			else if (newPart != part) // Change part?
			{
				_texHuge.Done();
				CGI::TextureDesc texDesc;
				texDesc._url = _C(_texTiny->GetName());
				texDesc._texturePart = newPart;
				_texHuge.Init(texDesc);
			}
		}
	}
}

int Texture::GetPart() const
{
	if (_texHuge && _texHuge->IsLoaded())
		return _texHuge->GetPart();
	return _texTiny->GetPart();
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
	char txt[16];
	sprintf_s(txt, "%02X%02X%02X%02X",
		(color >> 0) & 0xFF,
		(color >> 8) & 0xFF,
		(color >> 16) & 0xFF,
		(color >> 24) & 0xFF);
	return txt;
}

// Material::PickRound:

Material::PickRound::PickRound()
{
}

Material::PickRound::PickRound(const glm::vec4& v) : glm::vec4(v)
{
}

void Material::PickRound::SetUV(float u, float v)
{
	x = u;
	y = v;
}

void Material::PickRound::SetRadius(float r)
{
	z = r;
}

void Material::PickRound::SetAlpha(float x)
{
	w = x;
}

glm::vec4 Material::PickRound::ToPixels() const
{
	if (w >= 2)
	{
		const float r = sqrt(z);
		return glm::vec4(
			glm::round(x * w),
			glm::round(y * w),
			glm::round(r * w),
			w);
	}
	else
		return *this;
}

void Material::PickRound::FromPixels(const glm::vec4& pix)
{
	if (w >= 2)
	{
		glm::vec4::operator=(glm::vec4(
			pix.x / pix.w,
			pix.y / pix.w,
			pow(pix.z / pix.w, 2),
			w));
	}
	else
		glm::vec4::operator=(pix);
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
			Str::EndsWith(_C(_name), ".xmt"));
		String url(desc._name);
		Str::ReplaceExtension(url, ".xmt");
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
		Mesh::GetShader()->FreeDescriptorSet(_cshTemp);
		Mesh::GetShader()->FreeDescriptorSet(_cshTiny);
		Mesh::GetShader()->FreeDescriptorSet(_csh);
		_texAlbedo.Done();
		_texNormal.Done();
		VERUS_DONE(Material);
		return true;
	}
	return false;
}

bool Material::operator<(RcMaterial that) const
{
	if (_blending != that._blending)
		return _blending < that._blending;

	if (_texAlbedo != that._texAlbedo)
		return _texAlbedo < that._texAlbedo;

	if (_texNormal != that._texNormal)
		return _texNormal < that._texNormal;

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
		_albedoPart != _texAlbedo->GetPart() ||
		_normalPart != _texNormal->GetPart();
	if (partChanged && !_cshTemp.IsSet())
	{
		_albedoPart = _texAlbedo->GetPart();
		_normalPart = _texNormal->GetPart();
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
	if (IsMissing())
		return false;
	return
		_texAlbedo->GetTex()->IsLoaded() &&
		_texNormal->GetTex()->IsLoaded();
}

bool Material::IsMissing() const
{
	return !_texAlbedo || !_texNormal;
}

void Material::LoadTextures(bool streamParts)
{
	VERUS_QREF_SM;

	_texAlbedo.Done();
	_texNormal.Done();
	_texAlbedo.Init(_dict.Find("texAlbedo"), streamParts);
	_texNormal.Init(_dict.Find("texNormal"), streamParts);
	_texEnableAlbedo.w = 1;
	_texEnableNormal.w = 1;
}

String Material::ToString(bool cleanDict)
{
	VERUS_QREF_MM;

	if (cleanDict)
		_dict.DeleteAll();

	CSZ empty = "";

	CGI::PBaseTexture pTexAlbedo = _texAlbedo->GetTex().Get();
	CGI::PBaseTexture pTexNormal = _texNormal->GetTex().Get();

	_dict.Insert("alphaSwitch",     /**/_C(Str::ToString(_alphaSwitch)));
	_dict.Insert("anisoSpecDir",    /**/_C(Str::ToString(_anisoSpecDir)));
	_dict.Insert("detail",          /**/_C(Str::ToString(_detail)));
	_dict.Insert("emission",        /**/_C(Str::ToString(_emission)));
	_dict.Insert("emissionPick",    /**/_C(_emissionPick.ToString()));
	_dict.Insert("eyePick",         /**/_C(Str::ToString(_eyePick.ToPixels())));
	_dict.Insert("gloss",           /**/_C(Str::ToString(_gloss)));
	_dict.Insert("glossPick",       /**/_C(_glossPick.ToString()));
	_dict.Insert("glossScaleBias",  /**/_C(Str::ToString(_glossScaleBias)));
	_dict.Insert("hairDesat",       /**/_C(Str::ToString(_hairDesat)));
	_dict.Insert("hairPick",        /**/_C(_hairPick.ToString()));
	_dict.Insert("lamScaleBias",    /**/_C(Str::ToString(_lamScaleBias)));
	_dict.Insert("lightPass",       /**/_C(Str::ToString(_lightPass)));
	_dict.Insert("metalPick",       /**/_C(_metalPick.ToString()));
	_dict.Insert("skinPick",        /**/_C(_skinPick.ToString()));
	_dict.Insert("specScaleBias",   /**/_C(Str::ToString(_specScaleBias)));
	_dict.Insert("tc0ScaleBias",    /**/_C(Str::ToString(_tc0ScaleBias)));
	_dict.Insert("texAlbedo",       /**/pTexAlbedo ? _C(pTexAlbedo->GetName()) : empty);
	_dict.Insert("texEnableAlbedo", /**/_C(Str::ToColorString(_texEnableAlbedo)));
	_dict.Insert("texNormal",       /**/pTexNormal ? _C(pTexNormal->GetName()) : empty);
	_dict.Insert("texEnableNormal", /**/_C(Str::ToColorString(_texEnableNormal, false)));
	_dict.Insert("userColor",       /**/_C(Str::ToColorString(_userColor)));
	_dict.Insert("userPick",        /**/_C(_userPick.ToString()));

	return _dict.ToString();
}

void Material::FromString(CSZ txt)
{
	_dict.FromString(txt);

	_alphaSwitch = Str::FromStringVec2(_dict.Find("alphaSwitch", "1 1"));
	_anisoSpecDir = Str::FromStringVec2(_dict.Find("anisoSpecDir", "0 1"));
	_detail = _dict.FindFloat("detail", 0);
	_emission = _dict.FindFloat("emission", 0);
	_emissionPick.FromString(_dict.Find("emissionPick", "00000000"));
	_eyePick = Str::FromStringVec4(_dict.Find("eyePick", "0 0 0 0"));
	_gloss = _dict.FindFloat("gloss", 10);
	_glossPick.FromString(_dict.Find("glossPick", "00000000"));
	_glossScaleBias = Str::FromStringVec2(_dict.Find("glossScaleBias", "1 0"));
	_hairDesat = _dict.FindFloat("hairDesat", 0);
	_hairPick.FromString(_dict.Find("hairPick", "00000000"));
	_lamScaleBias = Str::FromStringVec2(_dict.Find("lamScaleBias", "1 0"));
	_lightPass = _dict.FindFloat("lightPass", 1);
	_metalPick.FromString(_dict.Find("metalPick", "00000000"));
	_skinPick.FromString(_dict.Find("skinPick", "00000000"));
	_specScaleBias = Str::FromStringVec2(_dict.Find("specScaleBias", "1 0"));
	_tc0ScaleBias = Str::FromStringVec4(_dict.Find("tc0ScaleBias", "1 1 0 0"));
	_texEnableAlbedo = Str::FromColorString(_dict.Find("texEnableAlbedo", "80808000"));
	_texEnableNormal = Str::FromColorString(_dict.Find("texEnableNormal", "8080FF00"), false);
	_userColor = Str::FromColorString(_dict.Find("userColor", "00000000"));
	_userPick.FromString(_dict.Find("userPick", "0 0 0 0"));

	_eyePick.FromPixels(_eyePick);
}

CGI::CSHandle Material::GetComplexSetHandle() const
{
	if (_csh.IsSet())
		return _csh;
	return _cshTiny;
}

void Material::BindDescriptorSetTextures()
{
	VERUS_RT_ASSERT(!_csh.IsSet());
	VERUS_RT_ASSERT(IsLoaded());
	VERUS_QREF_MM;
	if (!_cshTiny.IsSet())
		_cshTiny = Mesh::GetShader()->BindDescriptorSetTextures(1, { _texAlbedo->GetTinyTex(), _texNormal->GetTinyTex(), mm.GetDetailTexture(), mm.GetStrassTexture() });
	_csh = Mesh::GetShader()->BindDescriptorSetTextures(1, { _texAlbedo->GetTex(), _texNormal->GetTex(), mm.GetDetailTexture(), mm.GetStrassTexture() });
}

bool Material::UpdateMeshUniformBuffer(float motionBlur)
{
	Mesh::UB_PerMaterialFS& ub = Mesh::GetUbPerMaterialFS();
	Mesh::UB_PerMaterialFS ubPrev;
	memcpy(&ubPrev, &ub, sizeof(Mesh::UB_PerMaterialFS));

	ub._alphaSwitch_anisoSpecDir = float4(_alphaSwitch, _anisoSpecDir);
	ub._detail_emission_gloss_hairDesat.x = _detail;
	ub._detail_emission_gloss_hairDesat.y = _emission;
	ub._detail_emission_gloss_hairDesat.z = _gloss;
	ub._detail_emission_gloss_hairDesat.w = _hairDesat;
	ub._detailScale_strassScale = float4(1, 1, 1, 1);
	ub._emissionPick = _emissionPick;
	ub._eyePick = _eyePick;
	ub._glossPick = _glossPick;
	ub._glossScaleBias_specScaleBias = float4(_glossScaleBias, _specScaleBias);
	ub._hairPick = _hairPick;
	ub._lamScaleBias_lightPass_motionBlur = float4(_lamScaleBias, _lightPass, motionBlur);
	ub._metalPick = _metalPick;
	ub._skinPick = _skinPick;
	ub._tc0ScaleBias = _tc0ScaleBias;
	ub._texEnableAlbedo = _texEnableAlbedo;
	ub._texEnableNormal = _texEnableNormal;
	ub._userColor = _userColor;
	ub._userPick = _userPick;
	if (_texAlbedo)
	{
		ub._detailScale_strassScale = float4(
			_texAlbedo->GetTex()->GetSize().getX() / 256 * 4,
			_texAlbedo->GetTex()->GetSize().getY() / 256 * 4,
			_texAlbedo->GetTex()->GetSize().getX() / 256 * 8,
			_texAlbedo->GetTex()->GetSize().getY() / 256 * 8);
	}

	return 0 != memcmp(&ubPrev, &ub, sizeof(Mesh::UB_PerMaterialFS));
}

void Material::IncludePart(float part)
{
	_texAlbedo->IncludePart(part);
	_texNormal->IncludePart(part);
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
	CGI::SamplerDesc strassSamplerDesc;
	strassSamplerDesc.SetFilter("a");
	strassSamplerDesc.SetAddressMode("rr");
	strassSamplerDesc._mipLodBias = -2;

	_texDefaultAlbedo.Init("[Textures]:Default.dds", false, true);
	_texDefaultNormal.Init("[Textures]:Default.NM.dds", false, true);
	_texDetail.Init("[Textures]:Detail.FX.dds", false, true);
	_texStrass.Init("[Textures]:Strass.dds", false, true, &strassSamplerDesc);

	_cshDefault = Mesh::GetShader()->BindDescriptorSetTextures(1,
		{
			_texDefaultAlbedo->GetTex(),
			_texDefaultNormal->GetTex(),
			_texDetail->GetTex(),
			_texStrass->GetTex()
		});
}

void MaterialManager::Done()
{
	Mesh::GetShader()->FreeDescriptorSet(_cshDefault);
	_texStrass.Done();
	_texDetail.Done();
	_texDefaultNormal.Done();
	_texDefaultAlbedo.Done();
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
	// 1    | 25
	// 2    | 50
	// 3    | 100
	// 4    | 200

	if (distSq < 200 * 200)
	{
		const float dist = Math::Max<float>(0, sqrt(distSq) - objectRadius);
		return glm::log2(Math::Clamp<float>(dist * (2 / 25.f), 1, 18));
	}
	return 4;
}
