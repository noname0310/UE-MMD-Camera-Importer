// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MMDUserImportVMDSettings.generated.h"

UENUM()
enum class ECameraCutImportType
{
	OneFrameInterval UMETA(DisplayName = "One Frame Interval (Best for Sequencer)"),
	ConstantKey UMETA(DisplayName = "Constant Key (MMD 60 frame animation method)"),
	OneFrameIntervalWithConstantKey UMETA(DisplayNAme = "One Frame Interval with Constant Key (For fps scalable environment e.g. game)"),
	ImportAsIs UMETA(DisplayName = "Import As Is (For 30 frame animation)"),
};

USTRUCT()
struct FFilmbackImportSettings
{
	GENERATED_BODY()

	FFilmbackImportSettings();

	/** Horizontal size of filmback or digital sensor, in mm. */
	UPROPERTY(EditAnywhere, config, meta = (ClampMin = "0.001", ForceUnits = mm))
	float SensorWidth;

	/** Vertical size of filmback or digital sensor, in mm. */
	UPROPERTY(EditAnywhere, config, meta = (ClampMin = "0.001", ForceUnits = mm))
	float SensorHeight;
};

UCLASS(config = EditorSettings, BlueprintType)
class UMmdUserImportVmdSettings final : public UObject
{
public:
	explicit UMmdUserImportVmdSettings(const FObjectInitializer& Initializer);

	GENERATED_BODY()

	/** Import Uniform Scale */
	UPROPERTY(EditAnywhere, config, Category = Transform, meta = (ClampMin = "0.0"))
	float ImportUniformScale;

	/** Camera Cut Import Type */
	UPROPERTY(EditAnywhere, config, Category = KeyFrame)
	ECameraCutImportType CameraCutImportType;

	/** Camera Count */
	UPROPERTY(EditAnywhere, config, Category = KeyFrame, meta = (ClampMin = "1", ClampMax = "4"))
	int CameraCount;

	/** Add Motion Blur Key */
	UPROPERTY(EditAnywhere, config, Category = KeyFrame)
	bool bAddMotionBlurKey;

	/** Motion Blur Amount */
	UPROPERTY(EditAnywhere, config, Category = KeyFrame, meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bAddMotionBlurKey"))
	float MotionBlurAmount;

	/** Filmback */
	UPROPERTY(EditAnywhere, config, Category = Camera, meta = (ShowOnlyInnerProperties))
	FFilmbackImportSettings CameraFilmback;
};
