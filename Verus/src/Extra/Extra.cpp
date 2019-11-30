#include "verus.h"

namespace verus
{
	void Make_Extra()
	{
		Extra::FileParser::Make();
	}
	void Free_Extra()
	{
		Extra::FileParser::Free();
	}
}
