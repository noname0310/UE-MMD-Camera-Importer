// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CineCameraComponent.h"
#include "MMDUserImportVMDSettings.h"
#include "MovieSceneSequence.h"
#include "Channels/MovieSceneDoubleChannel.h"

// TODO: little endian check

#pragma pack (push, 1)
/**
 * Representation of raw VMD data
 */
struct FVmdObject
{
	struct FHeader
	{
		uint8 Magic[30]; // this value must be 'Vocaloid Motion Data 0002'
		uint8 ModelName[20];
	};

	// uint32 BoneKeyFrameCount; then FBoneKeyFrame[BoneKeyFrameCount]

	struct FBoneKeyFrame
	{
		uint8 BoneName[15];
		uint32 FrameNumber;
		float Position[3];
		float Rotation[4];
		int8 Interpolation[64];
	};

	// uint32 MorphKeyFrameCount; then FMorphKeyFrame[MorphKeyFrameCount]

	struct FMorphKeyFrame
	{
		uint8 MorphName[15];
		uint32 FrameNumber;
		float Weight;
	};

	// uint32 CameraKeyFrameCount; then FCameraKeyFrame[CameraKeyFrameCount]

	struct FCameraKeyFrame
	{
		uint32 FrameNumber;
		float Distance;
		float Position[3];
		float Rotation[3];
		int8 Interpolation[24];
		uint32 ViewAngle;
		uint8 Perspective; // this value can be reinterpret_cast to bool
	};

	// uint32 LightKeyFrameCount; then FLightKeyFrame[LightKeyFrameCount]

	struct FLightKeyFrame
	{
		uint32 FrameNumber;
		float Color[3];
		float Direction[3];
	};

	// uint32 SelfShadowKeyFrameCount; then FSelfShadowKeyFrame[SelfShadowKeyFrameCount]

	struct FSelfShadowKeyFrame
	{
		uint32 FrameNumber;
		uint8 Mode;
		float Distance;
	};

	// uint32 PropertyKeyFrameCount; then FPropertyKeyFrame[PropertyKeyFrameCount]

	struct FPropertyKeyFrame
	{
		uint32 FrameNumber;
		uint8 Visible; // this value can be reinterpret_cast to bool

		// uint32 IkStateCount; then FIkState[IkStateCount]

		struct FIkState
		{
			uint8 IkName[20];
			uint8 Enabled; // this value can be reinterpret_cast to bool
		};
	};
};
#pragma pack (pop)

struct FVmdParseResult
{
	bool bIsSuccess;

	FVmdObject::FHeader Header;


	TArray<FVmdObject::FBoneKeyFrame> BoneKeyFrames;

	TArray<FVmdObject::FMorphKeyFrame> MorphKeyFrames;

	TArray<FVmdObject::FCameraKeyFrame> CameraKeyFrames;

	TArray<FVmdObject::FLightKeyFrame> LightKeyFrames;

	TArray<FVmdObject::FSelfShadowKeyFrame> SelfShadowKeyFrames;

	struct FPropertyKeyFrameWithIkState
	{
		uint32 FrameNumber;
		bool Visible;
		TArray<FVmdObject::FPropertyKeyFrame::FIkState> IkStates;
	};
	TArray<FPropertyKeyFrameWithIkState> PropertyKeyFrames;
};

class FVmdImporter
{
private:
	struct FTangentAccessIndices;

public:
	void SetFilePath(const FString& InFilePath);
	bool IsValidVmdFile();
	FVmdParseResult ParseVmdFile();

	static void ImportVmdCamera(
		const FVmdParseResult& InVmdParseResult,
		UMovieSceneSequence* InSequence,
		ISequencer& InSequencer,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

private:
	static FArchive* OpenFile(FString FilePath);
	
	static void ImportVmdCameraToExisting(
		const FVmdParseResult& InVmdParseResult,
		UMovieSceneSequence* InSequence,
		IMovieScenePlayer* Player,
		FMovieSceneSequenceIDRef TemplateID,
		const FGuid MmdCameraGuid,
		const FGuid MmdCameraCenterGuid,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

	static bool ImportVmdCameraFocalLengthProperty(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
		const FGuid ObjectBinding,
		const UMovieSceneSequence* InSequence,
		const UCineCameraComponent* InCineCameraComponent,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

	static bool CreateVmdCameraMotionBlurProperty(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
		const FGuid ObjectBinding,
		const UMovieSceneSequence* InSequence,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

	static bool ImportVmdCameraTransform(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
		const FGuid ObjectBinding,
		const UMovieSceneSequence* InSequence,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

	static bool ImportVmdCameraCenterTransform(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
		const FGuid ObjectBinding,
		const UMovieSceneSequence* InSequence,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

	static float ComputeFocalLength(const float FieldOfView, const float SensorWidth);

	static FGuid GetHandleToObject(
		UObject* InObject,
		UMovieSceneSequence* InSequence,
		IMovieScenePlayer* Player,
		FMovieSceneSequenceIDRef TemplateID,
		bool bCreateIfMissing
	);

	// T must be double or float
	// MovieSceneChannel must be FMovieSceneDoubleChannel or FMovieSceneFloatChannel
	// MovieSceneValue must be FMovieSceneDoubleValue or FMovieSceneFloatValue
	template<typename T, typename MovieSceneChannel, typename MovieSceneValue>
	static void ImportCameraSingleChannel(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
		MovieSceneChannel* Channel,
		const FFrameRate SampleRate,
		const FFrameRate FrameRate,
		const ECameraCutImportType CameraCutImportType,
		const FTangentAccessIndices TangentAccessIndices,
		const TFunction<T(const FVmdObject::FCameraKeyFrame&)> GetValueFunc,
		const TFunction<T(const T)> MapFunc
	)
	{
		if (CameraKeyFrames.Num() == 0)
		{
			return;
		}

		const FFrameNumber OneSampleFrame = (FrameRate / SampleRate).AsFrameNumber(1);
		const int32 FrameRatio = static_cast<int32>(FrameRate.AsDecimal() / 30.f);

		{
			const FVmdObject::FCameraKeyFrame& FirstCameraKeyFrame = CameraKeyFrames[0];
			Channel->SetDefault(MapFunc(GetValueFunc(FirstCameraKeyFrame)));
		}

		const TArray<FVmdObject::FCameraKeyFrame> ReducedKeys = ReduceKeys<T>(
			CameraKeyFrames,
			[&GetValueFunc](const TArray<FVmdObject::FCameraKeyFrame>& KeyFrames, const PTRINT Index)
			{
				return GetValueFunc(KeyFrames[Index]);
			});

		for (PTRINT i = 0; i < ReducedKeys.Num(); ++i)
		{
			// ReSharper disable once CppUseStructuredBinding
			const FVmdObject::FCameraKeyFrame& CurrentKeyFrame = ReducedKeys[i];

			// ReSharper disable once CppTooWideScopeInitStatement
			const FVmdObject::FCameraKeyFrame* NextKeyFrame = (i + 1) < ReducedKeys.Num()
				? &ReducedKeys[i + 1]
				: nullptr;

			const T Value = MapFunc(GetValueFunc(CurrentKeyFrame));

			FMovieSceneTangentData Tangent;
			Tangent.TangentWeightMode = RCTWM_WeightedBoth;
			{
				const float ArriveTangentX = static_cast<float>(CurrentKeyFrame.Interpolation[TangentAccessIndices.ArriveTangentX]) / 127.0f;
				const float ArriveTangentY = static_cast<float>(CurrentKeyFrame.Interpolation[TangentAccessIndices.ArriveTangentY]) / 127.0f;
				const float LeaveTangentX = NextKeyFrame != nullptr
					? static_cast<float>(NextKeyFrame->Interpolation[TangentAccessIndices.LeaveTangentX]) / 127.0f
					: 0.0f;
				const float LeaveTangentY = NextKeyFrame != nullptr
					? static_cast<float>(NextKeyFrame->Interpolation[TangentAccessIndices.LeaveTangentY]) / 127.0f
					: 0.0f;

				FVector2D ArriveTangent(ArriveTangentX, ArriveTangentY);
				FVector2D LeaveTangent(LeaveTangentX, LeaveTangentY);
			}

			if (CameraCutImportType != ECameraCutImportType::ImportAsIs &&
				NextKeyFrame != nullptr && NextKeyFrame->FrameNumber - CurrentKeyFrame.FrameNumber <= 1 && GetValueFunc(*NextKeyFrame) != GetValueFunc(CurrentKeyFrame)
			)
			{
				// ReSharper disable once CppTooWideScopeInitStatement
				const FVmdObject::FCameraKeyFrame* PreviousKeyFrame = 1 <= i
					? &ReducedKeys[i - 1]
					: nullptr;

				if (PreviousKeyFrame != nullptr && CurrentKeyFrame.FrameNumber - PreviousKeyFrame->FrameNumber <= 1 && GetValueFunc(CurrentKeyFrame) != GetValueFunc(*PreviousKeyFrame))
				{
					TArray<FFrameNumber> Times;
					Times.Push(static_cast<int32>(CurrentKeyFrame.FrameNumber) * FrameRatio);

					TArray<MovieSceneValue> MovieSceneValues;
					MovieSceneValue MovieSceneValueInstance;
					{
						MovieSceneValueInstance.Value = Value;
						MovieSceneValueInstance.InterpMode = RCIM_Constant;
						MovieSceneValueInstance.TangentMode = RCTM_Break;
						MovieSceneValueInstance.Tangent = Tangent;
					}
					MovieSceneValues.Push(MovieSceneValueInstance);

					Channel->AddKeys(Times, MovieSceneValues);
				}
				else
				{
					if (CameraCutImportType == ECameraCutImportType::ConstantKey)
					{
						TArray<FFrameNumber> Times;
						Times.Push(static_cast<int32>(CurrentKeyFrame.FrameNumber) * FrameRatio);

						TArray<MovieSceneValue> MovieSceneValues;
						MovieSceneValue MovieSceneValueInstance;
						{
							MovieSceneValueInstance.Value = Value;
							MovieSceneValueInstance.InterpMode = RCIM_Constant;
							MovieSceneValueInstance.TangentMode = RCTM_Break;
							MovieSceneValueInstance.Tangent = Tangent;
						}
						MovieSceneValues.Push(MovieSceneValueInstance);

						Channel->AddKeys(Times, MovieSceneValues);
					}
					else if (CameraCutImportType == ECameraCutImportType::OneFrameInterval)
					{
						const FFrameNumber Time = (static_cast<int32>(NextKeyFrame->FrameNumber) * FrameRatio) - OneSampleFrame;
						Channel->AddCubicKey(Time, Value, RCTM_Break, Tangent);
					}
				}
			}
			else
			{
				const FFrameNumber Time = static_cast<int32>(CurrentKeyFrame.FrameNumber) * FrameRatio;
				Channel->AddCubicKey(Time, Value, RCTM_Break, Tangent);
			}
		}
	}

	// T must be number
	template<typename T>
	static TArray<FVmdObject::FCameraKeyFrame> ReduceKeys(
		const TArray<FVmdObject::FCameraKeyFrame>& InCameraKeyFrames,
		const TFunction<T(const TArray<FVmdObject::FCameraKeyFrame>&, PTRINT)>& InGetValueFunc
	)
	{
		TArray<FVmdObject::FCameraKeyFrame> Result;

		T LastValue = 0;

		for (PTRINT i = 0; i < InCameraKeyFrames.Num() - 1; ++i)
		{
			const T CurrentValue = InGetValueFunc(InCameraKeyFrames, i);
			const T NextValue = InGetValueFunc(InCameraKeyFrames, i + 1);

			if (
				LastValue == CurrentValue &&
				CurrentValue == NextValue
			)
			{
				continue;
			}

			const FVmdObject::FCameraKeyFrame& Current = InCameraKeyFrames[i];
			Result.Push(Current);

			LastValue = CurrentValue;
		}

		return Result;
	}

private:
	FString FilePath;
	TUniquePtr<FArchive> FileReader;

	struct FTangentAccessIndices
	{
		PTRINT ArriveTangentX;
		PTRINT ArriveTangentY;
		PTRINT LeaveTangentX;
		PTRINT LeaveTangentY;
	};
};
