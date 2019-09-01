#pragma once

namespace verus
{
	namespace Scene
	{
		class MaterialManager : public Singleton<MaterialManager>, public Object
		{
		public:
			MaterialManager();
			virtual ~MaterialManager();

			void Init();
			void Done();
		};
		VERUS_TYPEDEFS(MaterialManager);
	}
}
