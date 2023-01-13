// Copyright Epic Games, Inc. All Rights Reserved.

#include "MmdUserImportVmdSettings.h"
#include "UObject/UnrealType.h"

UMmdUserImportVmdSettings::UMmdUserImportVmdSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	bMatchByNameOnly = true;
	bForceFrontXAxis = false;
	bCreateCameras = true;
	bReplaceTransformTrack = true;
	bReduceKeys = true;
	ReduceKeysTolerance = 0.001f;
	bConvertSceneUnit = true;
	ImportUniformScale = 1.0f;
}
