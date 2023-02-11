// Copyright Epic Games, Inc. All Rights Reserved.

#include "MmdUserImportVmdSettings.h"
#include "UObject/UnrealType.h"

UMmdUserImportVmdSettings::UMmdUserImportVmdSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	ImportUniformScale = 10.0f;
	CameraCutImportType = ECameraCutImportType::OneFrameInterval;
	CameraCount = 2;
	bAddMotionBlurKey = false;
	MotionBlurAmount = 0.5f;
	CameraFilmback.SensorWidth = 24.0f;
	CameraFilmback.SensorHeight = 13.5f;
}
