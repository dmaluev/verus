#include "verus.h"

namespace verus
{
	void Make_GUI()
	{
		GUI::ViewManager::Make();
	}
	void Free_GUI()
	{
		GUI::ViewManager::Free();
	}
}
