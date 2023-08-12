// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Game
{
	class QuestSystem : public Singleton<QuestSystem>, public Object
	{
	public:
		class Quest
		{
		public:
			class Req
			{
			public:
				enum class Type : int
				{
					quest,
					xp,
					count
				};

			private:
				String _id;
				Type   _type = Type::quest;
			};
			VERUS_TYPEDEFS(Req);

			class Task
			{
			public:
				enum class Type : int
				{
					destroy,
					gather,
					protect,
					trigger,
					count
				};

				class Reward
				{
					String _id;
					int    _count = 0;
				};
				VERUS_TYPEDEFS(Reward);

			private:
				Vector<Reward> _vRewards;
				String         _desc;
				Type           _type = Type::destroy;
				int            _count = 0;
			};
			VERUS_TYPEDEFS(Task);

			class Reward
			{
				String _id;
				int    _count = 0;
			};
			VERUS_TYPEDEFS(Reward);

		private:
			Vector<Req>    _vReqs;
			Vector<Task>   _vTasks;
			Vector<Reward> _vRewards;
			String         _id;
			String         _desc;
		};
		VERUS_TYPEDEFS(Quest);

	private:
		Vector<Quest> _vQuests;

	public:
		QuestSystem();
		~QuestSystem();

		void Init();
		void Done();
	};
	VERUS_TYPEDEFS(QuestSystem);
}
