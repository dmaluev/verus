// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace App
	{
		class UndoManager : public Object
		{
		public:
			class Command
			{
				bool _hasRedo = false;

			public:
				virtual ~Command() {}
				virtual void SaveState(bool forUndo = false) = 0;
				virtual void Undo() = 0;
				virtual void Redo() = 0;
				virtual Str GetName() const { return ""; }
				virtual bool IsValid() const { return true; }
				bool HasRedo() const { return _hasRedo; }
				void SetHasRedo(bool hasRedo) { _hasRedo = hasRedo; };
			};
			VERUS_TYPEDEFS(Command);

		private:
			Vector<PCommand> _vCommands;
			int              _maxCommands = 30;
			int              _nextUndo = 0;

		public:
			UndoManager();
			~UndoManager();

			void Init(int maxCommands = 30);
			void Done();

			int GetMaxCommands() const { return _maxCommands; }
			void SetMaxCommands(int maxCommands);

			int GetCommandCount() const { return Utils::Cast32(_vCommands.size()); }

			void Undo();
			void Redo();

			bool CanUndo() const;
			bool CanRedo() const;

			PCommand BeginChange(PCommand pCommand);
			PCommand EndChange(PCommand pCommand = nullptr);

			void DeleteInvalidCommands();

			template<typename T>
			void ForEachCommand(const T& fn)
			{
				int i = 0;
				for (auto& x : _vCommands)
				{
					if (Continue::no == fn(x, i < _nextUndo))
						return;
					i++;
				}
			}

			int GetNextUndo() const { return _nextUndo; }
		};
		VERUS_TYPEDEFS(UndoManager);
	}
}
