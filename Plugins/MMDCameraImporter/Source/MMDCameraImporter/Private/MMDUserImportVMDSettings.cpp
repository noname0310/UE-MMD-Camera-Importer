// Copyright Epic Games, Inc. All Rights Reserved.

#include "MmdUserImportVmdSettings.h"
#include "UObject/UnrealType.h"

UMmdUserImportVmdSettings::UMmdUserImportVmdSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	ImportUniformScale = 10.0f;
	CameraCutImportType = ECameraCutImportType::OneFrameInterval;
	bAddMotionBlurKey = true;
	MotionBlurAmount = 0.5f;
}
