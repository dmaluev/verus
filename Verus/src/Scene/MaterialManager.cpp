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

void Texture::Init(CSZ url, bool streamParts)
{
	if (_refCount)
		return;

	_streamParts = streamParts;
	if (_streamParts)
		_requestedPart = FLT_MAX;

	CGI::TextureDesc texDesc;
	texDesc._url = url;
	texDesc._texturePart = _streamParts ? 512 : 0;
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

// TexturePtr:

void TexturePtr::Init(CSZ url, bool streamParts)
{
	VERUS_QREF_MM;
	VERUS_RT_ASSERT(!_p);
	_p = mm.InsertTexture(url);
	_p->Init(url, streamParts);
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

void Material::Pick::SetAmount(float x)
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

void Material::PickRound::SetAmount(float x)
{
	w = x;
}

glm::vec4 Material::PickRound::ToPixels() const
{
	if (w >= 2)
	{
		const float r = sqrt(z);
		return glm::vec4(
			glm::round(x*w),
			glm::round(y*w),
			glm::round(r*w),
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

	_texAlbedo.Init("[Textures]:Default.dds");
	_texNormal.Init("[Textures]:Default.NM.dds");

	if (desc._load)
	{
		VERUS_RT_ASSERT(
			Str::EndsWith(_C(_name), ".x3d") ||
			Str::EndsWith(_C(_name), ".xmt"));
		String url(desc._name);
		Str::ReplaceExtension(url, ".xmt");
		_name = url;

		Vector<BYTE> vData;
		IO::FileSystem::I().LoadResourceFromCache(_C(url), vData);
		FromString(reinterpret_cast<CSZ>(vData.data()));
		LoadTextures(desc._streamParts);
	}
}

bool Material::Done()
{
	_refCount--;
	if (_refCount <= 0)
	{
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
}

bool Material::IsLoaded(bool allowDefaultTextures) const
{
	const bool loaded =
		_texAlbedo->GetTex()->IsLoaded() &&
		_texNormal->GetTex()->IsLoaded();
	if (loaded && !allowDefaultTextures) // Final textures must be loaded (for editor)?
	{
		if (_texAlbedo->GetTex()->GetName() == "Textures:Default.dds" ||
			_texNormal->GetTex()->GetName() == "Textures:Default.NM.dds")
			return false;
	}
	return loaded;
}

void Material::LoadTextures(bool streamParts)
{
	VERUS_QREF_SM;

	_texAlbedo.Done();
	_texNormal.Done();
	_texAlbedo.Init(_dict.Find("texAlbedo"), streamParts);
	_texNormal.Init(_dict.Find("texNormal"), streamParts);
	_texAlbedoEnable.w = 1;
	_texNormalEnable.w = 1;
}

String Material::ToString()
{
	VERUS_QREF_MM;

	CSZ empty = "";

	CGI::PBaseTexture pTexAlbedo = &(*_texAlbedo->GetTex());
	CGI::PBaseTexture pTexNormal = &(*_texNormal->GetTex());

	_dict.Insert("alphaSwitch",     /**/_C(Str::ToString(_alphaSwitch)));
	_dict.Insert("anisoSpecDir",    /**/_C(Str::ToString(_anisoSpecDir)));
	_dict.Insert("bushEffect",      /**/_C(Str::ToString(_bushEffect)));
	_dict.Insert("detail",          /**/_C(Str::ToString(_detail)));
	_dict.Insert("emitPick",        /**/_C(_emitPick.ToString()));
	_dict.Insert("emitXPick",       /**/_C(_emitXPick.ToString()));
	_dict.Insert("eyePick",         /**/_C(Str::ToString(_eyePick.ToPixels())));
	_dict.Insert("gloss",           /**/_C(Str::ToString(_gloss)));
	_dict.Insert("glossX",          /**/_C(Str::ToString(_glossX)));
	_dict.Insert("glossXPick",      /**/_C(_glossXPick.ToString()));
	_dict.Insert("hairDesat",       /**/_C(Str::ToString(_hairDesat)));
	_dict.Insert("hairPick",        /**/_C(_hairPick.ToString()));
	_dict.Insert("lamBias",         /**/_C(Str::ToString(_lamBias)));
	_dict.Insert("lamScale",        /**/_C(Str::ToString(_lamScale)));
	_dict.Insert("lightPass",       /**/_C(Str::ToString(_lightPass)));
	_dict.Insert("metalPick",       /**/_C(_metalPick.ToString()));
	_dict.Insert("parallaxDepth",   /**/_C(Str::ToString(_parallaxDepth)));
	_dict.Insert("skinPick",        /**/_C(_skinPick.ToString()));
	_dict.Insert("specBias",        /**/_C(Str::ToString(_specBias)));
	_dict.Insert("specScale",       /**/_C(Str::ToString(_specScale)));
	_dict.Insert("tc0ScaleBias",    /**/_C(Str::ToString(_tc0ScaleBias)));
	_dict.Insert("texAlbedo",       /**/pTexAlbedo ? _C(pTexAlbedo->GetName()) : empty);
	_dict.Insert("texAlbedoEnable", /**/_C(Str::ToColorString(_texAlbedoEnable)));
	_dict.Insert("texNormal",       /**/pTexNormal ? _C(pTexNormal->GetName()) : empty);
	_dict.Insert("texNormalEnable", /**/_C(Str::ToColorString(_texNormalEnable, false)));
	_dict.Insert("userColor",       /**/_C(Str::ToColorString(_userColor)));
	_dict.Insert("userPick",        /**/_C(_userPick.ToString()));

	return _dict.ToString();
}

void Material::FromString(CSZ txt)
{
	_dict.FromString(txt);

	_alphaSwitch = Str::FromStringVec2(_dict.Find("alphaSwitch", "1 1"));
	_anisoSpecDir = Str::FromStringVec2(_dict.Find("anisoSpecDir", "0 1"));
	_bushEffect = Str::FromStringVec4(_dict.Find("bushEffect", "0 0 0 0"));
	_detail = _dict.FindFloat("detail", 0);
	_emitPick.FromString(_dict.Find("emitPick", "00000000"));
	_emitXPick.FromString(_dict.Find("emitXPick", "00000000"));
	_eyePick = Str::FromStringVec4(_dict.Find("eyePick", "0 0 0 0"));
	_gloss = _dict.FindFloat("gloss", 10);
	_glossX = _dict.FindFloat("glossX", 10);
	_glossXPick.FromString(_dict.Find("glossXPick", "00000000"));
	_hairDesat = _dict.FindFloat("hairDesat", 0);
	_hairPick.FromString(_dict.Find("hairPick", "00000000"));
	_lamBias = _dict.FindFloat("lamBias", 0);
	_lamScale = _dict.FindFloat("lamScale", 1);
	_lightPass = _dict.FindFloat("lightPass", 1);
	_metalPick.FromString(_dict.Find("metalPick", "00000000"));
	_parallaxDepth = _dict.FindFloat("parallaxDepth", 0);
	_skinPick.FromString(_dict.Find("skinPick", "00000000"));
	_specBias = _dict.FindFloat("specBias", 0);
	_specScale = _dict.FindFloat("specScale", 1);
	_tc0ScaleBias = Str::FromStringVec4(_dict.Find("tc0ScaleBias", "1 1 0 0"));
	_texAlbedoEnable = Str::FromColorString(_dict.Find("texAlbedoEnable", "80808000"));
	_texNormalEnable = Str::FromColorString(_dict.Find("texNormalEnable", "8080FF00"), false);
	_userColor = Str::FromColorString(_dict.Find("userColor", "00000000"));
	_userPick.FromString(_dict.Find("userPick", "0 0 0 0"));

	_eyePick.FromPixels(_eyePick);
}

void Material::BindDescriptorSetTextures()
{
	VERUS_RT_ASSERT(-1 == _csid);
	VERUS_RT_ASSERT(IsLoaded(false));
	_csid = Scene::Mesh::GetShader()->BindDescriptorSetTextures(1, { _texAlbedo->GetTex(), _texNormal->GetTex() });
}

bool Material::UpdateMeshUniformBuffer(float motionBlur)
{
	Mesh::UB_PerMaterial& ub = Mesh::GetUbPerMaterial();
	Mesh::UB_PerMaterial ubPrev;
	memcpy(&ubPrev, &ub, sizeof(Mesh::UB_PerMaterial));

	ub._texEnableAlbedo = _texAlbedoEnable;
	ub._texEnableNormal = _texNormalEnable;
	ub._skinPick = _skinPick;
	ub._hairPick = _hairPick;
	ub._emitPick = _emitPick;
	ub._emitXPick = _emitXPick;
	ub._metalPick = _metalPick;
	ub._glossXPick = _glossXPick;
	ub._eyePick = _eyePick;
	ub._hairParams = glm::vec4(_anisoSpecDir, 0, _hairDesat);
	ub._lsb_gloss_lp = glm::vec4(_lamScale, _lamBias, _gloss, _lightPass);
	ub._ssb_as = glm::vec4(_specScale, _specBias, _alphaSwitch.x, _alphaSwitch.y);
	ub._userPick = _userPick;
	ub._ds_scale = glm::vec4(_detail, 1, 1, 1);
	if (_texAlbedo)
	{
		ub._ds_scale = glm::vec4(_detail, 1,
			_texAlbedo->GetTex()->GetSize().getX() / 256 * 4,
			_texAlbedo->GetTex()->GetSize().getY() / 256 * 4);
	}
	ub._motionBlur_glossX.x = motionBlur;
	ub._motionBlur_glossX.y = _glossX;
	ub._bushEffect = _bushEffect;

	return 0 != memcmp(&ubPrev, &ub, sizeof(Mesh::UB_PerMaterial));
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

	_texDefaultAlbedo.Init("[Textures]:Default.dds");
	_texDefaultNormal.Init("[Textures]:Default.NM.dds");
	_texDetail.Init("[Textures]:Detail.FX.dds");
	_texStrass.Init("[Textures]:Strass.dds");
}

void MaterialManager::Done()
{
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
		return glm::log2(Math::Clamp<float>(dist*(2 / 25.f), 1, 18));
	}
	return 4;
}
