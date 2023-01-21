// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MMDUserImportVMDSettings.generated.h"

UCLASS(config = EditorSettings, BlueprintType)
class UMmdUserImportVmdSettings final : public UObject
{
public:
	explicit UMmdUserImportVmdSettings(const FObjectInitializer& Initializer);

	GENERATED_BODY()

	/** Whether to create cameras if they don't already exist in the level. */
	UPROPERTY(EditAnywhere, config, Category = Import)
	bool bCreateCameras;

	/** Whether to replace the existing transform track or create a new track/section */
	UPROPERTY(EditAnywhere, config, Category = Import)
    bool bReplaceTransformTrack;

    /** Import Uniform Scale*/
	UPROPERTY(EditAnywhere, config, Category = Import, meta = (ToolTip = "Import Uniform Scale"))
    float ImportUniformScale;
};
