// Copyright Epic Games, Inc. All Rights Reserved.

#include "MMDCameraImporterCommands.h"

#define LOCTEXT_NAMESPACE "FMMDCameraImporterModule"

void FMmdCameraImporterCommands::RegisterCommands()
{
	UI_COMMAND(ImportVmd, "Import VMD", "Import VMD file", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
