// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace GUI
	{
		class Table : public Label
		{
			struct Cell
			{
				String      _text;
				const void* _pUser = nullptr;
				UINT32      _color = VERUS_COLOR_WHITE;
			};
			VERUS_TYPEDEFS(Cell);

			struct Row
			{
				Vector<Cell> _vCells;
			};
			VERUS_TYPEDEFS(Row);

			Row         _header;
			Vector<Row> _vRows;
			int         _offset = 0;
			int         _selectedRow = -1;
			int         _cols = 0;
			float       _rowHeight = 0.1f;

		public:
			Table();
			virtual ~Table();

			static PWidget Make();

			virtual void Update() override;
			virtual void Draw() override;
			virtual void Parse(pugi::xml_node node) override;
			virtual PInputFocus AsInputFocus() override { return this; }

			virtual bool InputFocus_KeyOnly() override { return true; }
			virtual void InputFocus_Key(int scancode) override;

			void Clear();
			int GetRowCount() const { return Utils::Cast32(_vRows.size()); }
			int GetSelectedRow() const { return _selectedRow; }
			void SelectRow(int row) { _selectedRow = row; }
			void SelectNextRow();
			void SelectPreviousRow();
			void GetCellData(int index, int col, CSZ& txt, UINT32& color, const void** ppUser = nullptr);
			void UpdateRow(int index, int col, CSZ txt, UINT32 color = VERUS_COLOR_WHITE, const void* pUser = nullptr);
			int AppendRow();
			void Sort(int col, bool asInt, bool descend);

			virtual bool InvokeOnClick(float x, float y) override;
			virtual bool InvokeOnKey(int scancode) override;
		};
		VERUS_TYPEDEFS(Table);
	}
}
