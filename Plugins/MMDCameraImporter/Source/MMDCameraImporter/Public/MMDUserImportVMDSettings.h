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

	/** Whether to match vmd node names to sequencer node names. */
	UPROPERTY(EditAnywhere, config, Category = Import, meta = (ToolTip = "Match vmd node names to sequencer node names"))
	bool bMatchByNameOnly;

	/** Whether to force the front axis to be align with X instead of -Y. */
	UPROPERTY(EditAnywhere, config, Category = Import, meta = (ToolTip = "Convert the scene from VMD coordinate system to UE coordinate system with front X axis instead of -Y"))
	bool bForceFrontXAxis;

	/** Convert the scene from VMD unit to UE unit(centimeter)*/
	UPROPERTY(EditAnywhere, config, Category = Import, meta = (ToolTip = "Convert the scene from VMD unit to UE unit(centimeter)"))
	bool bConvertSceneUnit;

	/** Import Uniform Scale*/
	UPROPERTY(EditAnywhere, config, Category = Import, meta = (ToolTip = "Import Uniform Scale"))
	float ImportUniformScale;

	/** Whether to create cameras if they don't already exist in the level. */
	UPROPERTY(EditAnywhere, config, Category = Import)
	bool bCreateCameras;

	/** Whether to replace the existing transform track or create a new track/section */
	UPROPERTY(EditAnywhere, config, Category = Import)
	bool bReplaceTransformTrack;

	/** Whether to remove keyframes within a tolerance from the imported tracks */
	UPROPERTY(EditAnywhere, config, Category = Import)
	bool bReduceKeys;

	/** The tolerance for reduce keys */
	UPROPERTY(EditAnywhere, config, Category = Import, meta = (EditCondition = bReduceKeys))
	float ReduceKeysTolerance;
};
