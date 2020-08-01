#include "verus.h"

using namespace verus;
using namespace verus::GUI;

Table::Table()
{
}

Table::~Table()
{
}

PWidget Table::Make()
{
	return new Table;
}

void Table::Update()
{
}

void Table::Draw()
{
	VERUS_QREF_VM;
	VERUS_QREF_RENDERER;

	DrawInputStyle();

	// Save:
	const float x = GetX();
	const float y = GetY();
	const float w = GetW();
	const float h = GetH();
	float yOffset = GetY();
	const float cell = GetW() / _cols;

	// Draw header:
	if (!_header._vCells.empty())
	{
		SetColor(Vector4(1, 1, 1, 0.75f));
		VERUS_FOR(i, _cols)
		{
			SetX(x + cell * i);
			SetY(yOffset);
			SetW(cell);
			SetH(_rowHeight);
			SetText(_C(_header._vCells[i]._text));
			Label::DrawInputStyle();
			Label::Draw();
		}
		yOffset += _rowHeight;
	}

	// Draw rows:
	for (int row = _offset; row < int(_vRows.size()); ++row)
	{
		if (row == _selectedRow)
		{
			auto cb = renderer.GetCommandBuffer();
			auto shader = vm.GetShader();

			vm.GetUbGui()._matW = Math::QuadMatrix(x, yOffset, w, _rowHeight).UniformBufferFormat();
			vm.GetUbGuiFS()._color = Vector4(1, 1, 1, 0.25f).GLM();

			vm.BindPipeline(ViewManager::PIPE_SOLID_COLOR, cb);
			shader->BeginBindDescriptors();
			cb->BindDescriptors(shader, 0);
			cb->BindDescriptors(shader, 1);
			shader->EndBindDescriptors();
			renderer.DrawQuad();
		}

		VERUS_FOR(i, _cols)
		{
			SetX(x + cell * i);
			SetY(yOffset);
			SetW(cell);
			SetH(_rowHeight);
			Vector4 color;
			color.FromColor(_vRows[row]._vCells[i]._color);
			SetColor(color);
			SetText(_C(_vRows[row]._vCells[i]._text));
			Label::Draw();
		}
		yOffset += _rowHeight;

		if (yOffset >= y + h)
			break;
	}

	// Restore:
	SetX(x);
	SetY(y);
	SetW(w);
	SetH(h);
}

void Table::Parse(pugi::xml_node node)
{
	Label::Parse(node);

	_cols = node.attribute("cols").as_int(_cols);
	_rowHeight = ViewManager::ParseCoordY(node.attribute("rowHeight").value());

	String textAttr("text");
	if (GetOwnerView())
		textAttr += _C(GetOwnerView()->GetLocale());

	int index = 0;
	for (auto thNode : node.children("th"))
	{
		if (_header._vCells.empty())
			_header._vCells.resize(_cols);

		if (auto attr = thNode.attribute(_C(textAttr)))
			_header._vCells[index++]._text = attr.value();
		else if (auto attr = thNode.attribute("text"))
			_header._vCells[index++]._text = attr.value();

		if (_cols == index)
			break;
	}
}

void Table::InputFocus_Key(int scancode)
{
	InvokeOnKey(scancode);
}

void Table::Clear()
{
	_selectedRow = -1;
	_vRows.clear();
}

void Table::SelectNextRow()
{
	if (_selectedRow >= 0 && _selectedRow < int(_vRows.size()) - 1)
		_selectedRow++;
}

void Table::SelectPreviousRow()
{
	if (_selectedRow > 0)
		_selectedRow--;
}

void Table::GetCellData(int index, int col, CSZ& txt, UINT32& color, const void** ppUser)
{
	if (index < 0)
		index = _selectedRow;
	if (index >= _vRows.size())
		return;
	if (col >= _cols)
		return;
	txt = _C(_vRows[index]._vCells[col]._text);
	color = _vRows[index]._vCells[col]._color;
	if (ppUser)
		*ppUser = _vRows[index]._vCells[col]._pUser;
}

void Table::UpdateRow(int index, int col, CSZ txt, UINT32 color, const void* pUser)
{
	if (index < 0)
		index = _selectedRow;
	if (index >= int(_vRows.size()))
		return;
	if (col >= _cols)
		return;
	_vRows[index]._vCells[col]._text = txt;
	_vRows[index]._vCells[col]._color = color;
	if (pUser)
		_vRows[index]._vCells[col]._pUser = pUser;
}

int Table::AppendRow()
{
	const int size = Utils::Cast32(_vRows.size());
	_vRows.resize(size + 1);
	_vRows[size]._vCells.resize(_cols);
	return size;
}

void Table::Sort(int col, bool asInt, bool descend)
{
	if (col >= _cols)
		return;
	_selectedRow = -1;
	std::sort(_vRows.begin(), _vRows.end(), [col, asInt, descend](RcRow a, RcRow b)
		{
			if (asInt)
			{
				if (descend)
					return atoi(_C(a._vCells[col]._text)) > atoi(_C(b._vCells[col]._text));
				else
					return atoi(_C(a._vCells[col]._text)) < atoi(_C(b._vCells[col]._text));
			}
			else
			{
				if (descend)
					return a._vCells[col]._text > b._vCells[col]._text;
				else
					return a._vCells[col]._text < b._vCells[col]._text;
			}
		});
}

bool Table::InvokeOnClick(float x, float y)
{
	const bool ret = Label::InvokeOnClick(x, y);
	const float xLocal = x - GetX();
	const float yLocal = y - GetY();
	const int row = int(yLocal / _rowHeight) + _offset - (_header._vCells.empty() ? 0 : 1);
	_selectedRow = -1;
	if (row >= 0 && row < GetRowCount())
		_selectedRow = row;
	return ret;
}

bool Table::InvokeOnKey(int scancode)
{
	const bool ret = Label::InvokeOnKey(scancode);
	if (SDL_SCANCODE_UP == scancode)
		SelectPreviousRow();
	else if (SDL_SCANCODE_DOWN == scancode)
		SelectNextRow();
	return ret;
}
