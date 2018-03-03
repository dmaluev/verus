#include "verus.h"

namespace verus
{
	void Make_Input()
	{
		Input::KeyMapper::Make();
	}
	void Free_Input()
	{
		Input::KeyMapper::Free();
	}
}
