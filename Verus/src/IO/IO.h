// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#include "DDSHeader.h"
#include "Stream.h"
#include "StreamPtr.h"
#include "File.h"
#include "FileSystem.h"
#include "Async.h"
#include "Json.h"
#include "Xml.h"
#include "Dictionary.h"
#include "Vwx.h"

namespace verus
{
	void Make_IO();
	void Free_IO();
}
