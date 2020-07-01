#include "verus.h"

using namespace verus;
using namespace verus::GUI;

Label::Label()
{
	SetColor(Vector4(0, 0, 0, 1));
}

Label::~Label()
{
}

PWidget Label::Make()
{
	return new Label;
}

void Label::Update()
{
	Widget::Update();
}

void Label::Draw()
{
	VERUS_QREF_VM;

	Widget::DrawDebug();

	if (!_vChoices.empty())
		Widget::DrawInputStyle();

	float x, y;
	GetAbsolutePosition(x, y);

	PFont pFont = vm.FindFont(_C(_font));

	if (_center)
	{
		const float textWidth = Font::ToFloatX(pFont->GetTextWidth(_C(GetText())), _fontScale);
		x = x + (GetW() - textWidth) * 0.5f;
	}

	Font::DrawDesc dd;
	dd._text = _C(GetText());
	dd._x = x;
	dd._y = y;
	dd._w = GetW();
	dd._h = GetH();
	dd._scale = _fontScale;
	dd._colorFont = GetColor().ToColor();
	pFont->Draw(dd);
}

void Label::Parse(pugi::xml_node node)
{
	Widget::Parse(node);

	if (auto attr = node.attribute("shadowColor"))
		_shadowColor = Convert::ColorTextToInt32(attr.value());

	_font = node.attribute("font").value();

	CSZ fontScale = node.attribute("fontScale").value();
	_fontScale = ViewManager::ParseCoordY(fontScale, 1);
	if (fontScale && Str::EndsWith(fontScale, "px"))
	{
		PFont pFont = ViewManager::I().FindFont(_C(_font));
		const float lineHeight = Font::ToFloatY(pFont->GetLineHeight(), 1);
		_fontScale = _fontScale / lineHeight;
	}

	String choiceAttr("choice");
	if (GetOwnerView())
		choiceAttr += _C(GetOwnerView()->GetLocale());
	if (auto attr = node.attribute(_C(choiceAttr)))
		SetChoice(attr.value());
	else if (auto attr = node.attribute("choice"))
		SetChoice(attr.value());

	bool center = !_vChoices.empty();
	if (auto attr = node.attribute("center"))
		center = attr.as_bool();
	_center = center;
}

float Label::GetFontH() const
{
	PFont pFont = ViewManager::I().FindFont(_C(_font));
	return Font::ToFloatY(pFont->GetLineHeight(), _fontScale);
}

void Label::SetChoice(CSZ s)
{
	if (s)
	{
		Str::Explode(s, "|", _vChoices);
		Str::TrimVector(_vChoices);
		if (!_vChoices.empty())
			SetText(_C(_vChoices[0]));
	}
	else
	{
		_vChoices.clear();
	}
}

void Label::SetChoiceValue(CWSZ s)
{
	const int index = GetChoiceIndex(s);
	if (index >= 0)
		SetChoiceIndex(index);
}

void Label::NextChoiceValue()
{
	if (_vChoices.empty())
		return;

	int index = GetChoiceIndex(nullptr);
	index++;
	if (index >= _vChoices.size())
		index = 0;
	SetChoiceValue(_C(Str::Utf8ToWide(_vChoices[index])));
}

int Label::GetChoiceIndex(CWSZ s)
{
	if (!s)
		s = _C(GetText());

	int i = 0;
	for (const auto& choice : _vChoices)
	{
		if (Str::Utf8ToWide(choice) == s)
			return i;
		i++;
	}
	return -1;
}

void Label::SetChoiceIndex(int index)
{
	if (index >= 0 && index < _vChoices.size())
		SetText(_C(_vChoices[index]));
}

bool Label::InvokeOnClick(float x, float y)
{
	NextChoiceValue();
	return Widget::InvokeOnClick(x, y);
}
