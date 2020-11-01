// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace AI
	{
		struct TaskDriverDelegate
		{
			virtual void TaskDriver_OnTask(CSZ task, CSZ mode) = 0;
		};
		VERUS_TYPEDEFS(TaskDriverDelegate);

		class TaskDriver
		{
			struct Task
			{
				String _name;
				float  _chance = 100;
				float  _time = 0;
				float  _deadline = 0;
				float  _intervalMin = 0;
				float  _intervalSize = 0;
			};
			VERUS_TYPEDEFS(Task);

			typedef Map<String, Task> TMapTasks;

			struct Mode
			{
				String    _name;
				TMapTasks _mapTasks;
			};
			VERUS_TYPEDEFS(Mode);

			typedef Map<String, Mode> TMapModes;

			TMapModes           _mapModes;
			String              _currentMode;
			PTaskDriverDelegate _pDelegate = nullptr;
			float               _cooldown = 1;
			float               _cooldownTimer = 0;

		public:
			TaskDriver();
			~TaskDriver();

			void Update();

			Str GetCurrentMode() const { return _C(_currentMode); }
			void SetCurrentMode(CSZ name) { _currentMode = name; }
			void SetDelegate(PTaskDriverDelegate p) { _pDelegate = p; }

			void AddMode(CSZ name);
			void AddTask(CSZ name, float chance, float intervalMin, float intervalMax, CSZ mode = nullptr);
		};
		VERUS_TYPEDEFS(TaskDriver);
	}
}
