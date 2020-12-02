// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

Container::Container()
{
}

Container::~Container()
{
	_mapCreators.clear();
	for (const auto& p : _vWidgets)
		delete p;
	_vWidgets.clear();
}

void Container::RegisterAll()
{
	RegisterWidget("button", Button::Make);
	RegisterWidget("image", Image::Make);
	RegisterWidget("label", Label::Make);
	RegisterWidget("sizer", Sizer::Make);
	RegisterWidget("table", Table::Make);
	RegisterWidget("textBox", TextBox::Make);
}

void Container::RegisterWidget(CSZ type, PFNCREATOR pCreator)
{
	_mapCreators[type] = pCreator;
}

PWidget Container::CreateWidget(CSZ type)
{
	if (_mapCreators.empty())
		RegisterAll();
	VERUS_IF_FOUND_IN(TMapCreators, _mapCreators, type, it)
		return it->second();
	return nullptr;
}

void Container::UpdateWidgets()
{
	for (const auto& p : _vWidgets)
		p->Update();
}

void Container::DrawWidgets()
{
	VERUS_FOREACH_REVERSE_CONST(Vector<PWidget>, _vWidgets, it)
	{
		PWidget p = *it;
		if (!p->IsHidden())
			p->Draw();
	}
}

void Container::ParseWidgets(pugi::xml_node node, CSZ sizerID)
{
	VERUS_QREF_VM;
	for (auto widgetNode : node.children())
	{
		CSZ type = widgetNode.name();
		PWidget pWidget = CreateWidget(type);
		if (pWidget)
		{
			if (sizerID)
				pWidget->SetSizer(sizerID);
			pWidget->Parse(widgetNode);
			_vWidgets.push_back(pWidget);

			if (pWidget->AsInputFocus())
				vm.GetCurrentParseView()->AddControlToTabList(pWidget);
		}
		else if (strcmp(type, "fx") && strcmp(type, "string"))
		{
			VERUS_RT_FAIL("Unknown widget type");
		}
	}
	// Z-order:
	std::reverse(_vWidgets.begin(), _vWidgets.end());
}

void Container::ResetAnimators(float reverseTime)
{
	for (const auto& p : _vWidgets)
	{
		if (auto pC = p->AsContainer())
			pC->ResetAnimators(reverseTime);
		else
			p->GetAnimator().Reset(reverseTime);
	}
}

PWidget Container::GetHovered(float x, float y)
{
	for (const auto& p : _vWidgets)
	{
		if (p->IsHidden() || p->IsDisabled())
			continue;

		PContainer pContainer = p->AsContainer();
		if (pContainer) // Container inside a container?
		{
			PWidget pChild = pContainer->GetHovered(x, y);
			if (pChild)
				return pChild;
		}

		if ((!pContainer || !pContainer->IsSizer()) && p->IsHovered(x, y))
			return p;
	}
	return nullptr;
}

PWidget Container::GetWidgetById(CSZ id)
{
	for (const auto& p : _vWidgets)
	{
		if (p->GetID() == id)
			return p;

		PContainer pContainer = p->AsContainer();
		if (pContainer)
		{
			PWidget p = pContainer->GetWidgetById(id);
			if (p)
				return p;
		}
	}
	return nullptr;
}

int Container::GetWidgetIndex(PWidget p)
{
	const int size = Utils::Cast32(_vWidgets.size());
	VERUS_FOR(i, size)
	{
		if (_vWidgets[i] == p)
			return i;
	}
	return -1;
}
