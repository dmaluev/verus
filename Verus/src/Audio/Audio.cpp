#include "verus.h"

namespace verus
{
	void Make_Audio()
	{
		Audio::AudioSystem::Make();
	}
	void Free_Audio()
	{
		Audio::AudioSystem::Free();
	}
}
