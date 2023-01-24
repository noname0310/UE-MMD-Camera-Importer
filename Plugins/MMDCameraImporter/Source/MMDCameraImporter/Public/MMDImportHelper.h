// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FMmdImportHelper
{
public:
	static FString ShiftJisToFString(const uint8* InBuffer, int32 InSize);
};
