// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CineCameraComponent.h"
#include "ISequencer.h"
#include "MMDUserImportVMDSettings.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Tracks/MovieSceneCameraCutTrack.h"

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

	template<typename T>
	struct TComputedKey;

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
		const TArray<FGuid>& CameraGuids,
		const TArray<FGuid>& CameraCenterGuids,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

	static void CreateCameraCutTrack(
		const TArray<TRange<uint32>>& InCameraCuts,
		const TArray<FGuid>& ObjectBindings,
		const UMovieSceneSequence* InSequence
	);

	static bool ImportVmdCameraFocalLengthProperty(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
		const TArray<TRange<uint32>>& InCameraCuts,
		const TArray<FGuid>& ObjectBindings,
		const UMovieSceneSequence* InSequence,
		const TArray<UCineCameraComponent*>& InCineCameraComponents,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

	static bool CreateVmdCameraMotionBlurProperty(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
		const TArray<TRange<uint32>>& InCameraCuts,
		const TArray<FGuid>& ObjectBindings,
		const UMovieSceneSequence* InSequence,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

	static bool ImportVmdCameraTransform(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
		const TArray<TRange<uint32>>& InCameraCuts,
		const TArray<FGuid>& ObjectBindings,
		const UMovieSceneSequence* InSequence,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

	static bool ImportVmdCameraCenterTransform(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
		const TArray<TRange<uint32>>& InCameraCuts,
		const TArray<FGuid>& ObjectBindings,
		const UMovieSceneSequence* InSequence,
		const UMmdUserImportVmdSettings* ImportVmdSettings
	);

	static float ComputeFocalLength(const float FieldOfView, const float SensorWidth);

	static TArray<TRange<uint32>> ComputeCameraCuts(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames
	);

	static FGuid GetHandleToObject(
		UObject* InObject,
		UMovieSceneSequence* InSequence,
		IMovieScenePlayer* Player,
		FMovieSceneSequenceIDRef TemplateID,
		bool bCreateIfMissing
	);

	static UMovieSceneCameraCutTrack* GetCameraCutTrack(UMovieScene* InMovieScene);
	
	// MovieSceneChannel must be FMovieSceneDoubleChannel or FMovieSceneFloatChannel
	template<typename MovieSceneChannel>
	static void ImportCameraSingleChannel(
		const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
		const TArray<TRange<uint32>>& InCameraCuts,
		TArray<MovieSceneChannel*>& Channels,
		const FFrameRate SampleRate,
		const FFrameRate FrameRate,
		const ECameraCutImportType CameraCutImportType,
		const FTangentAccessIndices TangentAccessIndices,
		const TFunction<typename MovieSceneChannel::CurveValueType(const FVmdObject::FCameraKeyFrame&)> GetValueFunc,
		const TFunction<typename MovieSceneChannel::CurveValueType(const typename MovieSceneChannel::CurveValueType)> MapFunc
	)
	{
		using T = typename MovieSceneChannel::CurveValueType;

		if (CameraKeyFrames.Num() == 0)
		{
			return;
		}

		const FFrameNumber OneSampleFrame = (FrameRate / SampleRate).AsFrameNumber(1);
		const int32 FrameRatio = static_cast<int32>(FrameRate.AsDecimal() / 30.f);

		{
			const FVmdObject::FCameraKeyFrame& FirstCameraKeyFrame = CameraKeyFrames[0];
			for (MovieSceneChannel* Channel : Channels)
			{
				Channel->SetDefault(MapFunc(GetValueFunc(FirstCameraKeyFrame)));
			}
		}

		const TArray<FVmdObject::FCameraKeyFrame> ReducedKeys = ReduceKeys<T>(
			CameraKeyFrames,
			[&GetValueFunc](const TArray<FVmdObject::FCameraKeyFrame>& KeyFrames, const PTRINT Index)
			{
				return GetValueFunc(KeyFrames[Index]);
			});

		TArray<TComputedKey<T>> TimeComputedKeys;

		for (PTRINT i = 0; i < ReducedKeys.Num(); ++i)
		{
			// ReSharper disable once CppUseStructuredBinding
			const FVmdObject::FCameraKeyFrame& CurrentKeyFrame = ReducedKeys[i];

			// ReSharper disable once CppTooWideScopeInitStatement
			const FVmdObject::FCameraKeyFrame* NextKeyFrame = (i + 1) < ReducedKeys.Num()
				? &ReducedKeys[i + 1]
				: nullptr;

			const T Value = MapFunc(GetValueFunc(CurrentKeyFrame));

			const float ArriveTangentX = static_cast<float>(CurrentKeyFrame.Interpolation[TangentAccessIndices.ArriveTangentX]) / 127.0f;
			const float ArriveTangentY = static_cast<float>(CurrentKeyFrame.Interpolation[TangentAccessIndices.ArriveTangentY]) / 127.0f;
			const float LeaveTangentX = NextKeyFrame != nullptr
				? static_cast<float>(NextKeyFrame->Interpolation[TangentAccessIndices.LeaveTangentX]) / 127.0f
				: 0.0f;
			const float LeaveTangentY = NextKeyFrame != nullptr
				? static_cast<float>(NextKeyFrame->Interpolation[TangentAccessIndices.LeaveTangentY]) / 127.0f
				: 0.0f;

			TComputedKey<T> ComputedKey;
			{
				ComputedKey.Value = Value;
				ComputedKey.ArriveTangent = FVector2D(1.0f - ArriveTangentX, 1.0f - ArriveTangentY);
				ComputedKey.LeaveTangent = FVector2D(LeaveTangentX, LeaveTangentY);
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
					ComputedKey.Time = static_cast<int32>(CurrentKeyFrame.FrameNumber) * FrameRatio;

					if (CameraCutImportType == ECameraCutImportType::ConstantKey)
					{
						ComputedKey.InterpMode = RCIM_Constant;
					}
					else if (CameraCutImportType == ECameraCutImportType::OneFrameInterval ||
						CameraCutImportType == ECameraCutImportType::OneFrameIntervalWithConstantKey)
					{
						// if use multiple camera, ConstantKey is not required
						ComputedKey.InterpMode = Channels.Num() == 1
							? RCIM_Constant
							: RCIM_Cubic;
					}
				}
				else
				{
					if (CameraCutImportType == ECameraCutImportType::ConstantKey)
					{
						ComputedKey.Time = static_cast<int32>(CurrentKeyFrame.FrameNumber) * FrameRatio;
						ComputedKey.InterpMode = RCIM_Constant;
					}
					else if (CameraCutImportType == ECameraCutImportType::OneFrameInterval)
					{
						ComputedKey.Time = (static_cast<int32>(NextKeyFrame->FrameNumber) * FrameRatio) - OneSampleFrame;
						ComputedKey.InterpMode = RCIM_Cubic;
					}
					else if (CameraCutImportType == ECameraCutImportType::OneFrameIntervalWithConstantKey)
					{
						ComputedKey.Time = (static_cast<int32>(NextKeyFrame->FrameNumber) * FrameRatio) - OneSampleFrame;
						ComputedKey.InterpMode = RCIM_Constant;
					}
				}
			}
			else
			{
				ComputedKey.Time = static_cast<int32>(CurrentKeyFrame.FrameNumber) * FrameRatio;
				ComputedKey.InterpMode = RCIM_Cubic;
			}

			TimeComputedKeys.Push(ComputedKey);
		}

		ImportComputedKeysToChannel<MovieSceneChannel>(
			TimeComputedKeys,
			InCameraCuts,
			Channels,
			FrameRate);
	}

	template<typename MovieSceneChannel>
	static void ImportComputedKeysToChannel(
		TArray<TComputedKey<typename MovieSceneChannel::CurveValueType>>& TimeComputedKeys,
		const TArray<TRange<uint32>>& InCameraCuts,
		TArray<MovieSceneChannel*>& Channels,
		const FFrameRate FrameRate
	)
	{
		using T = typename MovieSceneChannel::CurveValueType;
		using FMovieSceneValue = typename MovieSceneChannel::ChannelValueType;

		TArray<TPair<FFrameNumber, FMovieSceneValue>> Keys;
		
		for (PTRINT i = 0; i < TimeComputedKeys.Num(); ++i)
		{
			const TComputedKey<T>& CurrentKey = TimeComputedKeys[i];

			const TComputedKey<T>* NextKey = (i + 1) < TimeComputedKeys.Num()
				? &TimeComputedKeys[i + 1]
				: nullptr;

			const TComputedKey<T>* PreviousKey = 1 <= i
				? &TimeComputedKeys[i - 1]
				: nullptr;

			FMovieSceneTangentData Tangent;
			Tangent.TangentWeightMode = RCTWM_WeightedBoth;
			{
				const double DecimalFrameRate = FrameRate.AsDecimal();

				FVector2D ArriveTangentUniform;
				if (PreviousKey != nullptr)
				{
					ArriveTangentUniform = FVector2D(
						(CurrentKey.Time.Value - PreviousKey->Time.Value) / DecimalFrameRate,
						CurrentKey.Value - PreviousKey->Value);
				}
				else
				{
					ArriveTangentUniform = FVector2D(1.0f / DecimalFrameRate, 0.0f);
				}

				FVector2D LeaveTangentUniform;
				if (NextKey != nullptr)
				{
					LeaveTangentUniform = FVector2D(
						(NextKey->Time.Value - CurrentKey.Time.Value) / DecimalFrameRate,
						NextKey->Value - CurrentKey.Value);
				}
				else
				{
					LeaveTangentUniform = FVector2D(1.0f / DecimalFrameRate, 0.0f);
				}

				const FVector2D ArriveTangent = CurrentKey.ArriveTangent * ArriveTangentUniform;
				const FVector2D LeaveTangent = CurrentKey.LeaveTangent * LeaveTangentUniform;

				if (ArriveTangent.X == 0.0f)
				{
				    Tangent.ArriveTangent = ArriveTangent.Y < 0 ? -1.0f : 1.0f;
				}
				else
				{
					Tangent.ArriveTangent = ArriveTangent.Y / (ArriveTangent.X * DecimalFrameRate);
				}

				if (LeaveTangent.X == 0.0f)
				{
					Tangent.LeaveTangent = LeaveTangent.Y < 0 ? -1.0f : 1.0f;
				}
				else
				{
					Tangent.LeaveTangent = LeaveTangent.Y / (LeaveTangent.X * DecimalFrameRate);
				}

				Tangent.ArriveTangentWeight = ArriveTangent.Length();
				Tangent.LeaveTangentWeight = LeaveTangent.Length();
			}

			FMovieSceneValue MovieSceneValueInstance;
			{
				MovieSceneValueInstance.Value = CurrentKey.Value;
				MovieSceneValueInstance.InterpMode = CurrentKey.InterpMode;
				MovieSceneValueInstance.TangentMode = RCTM_Break;
				MovieSceneValueInstance.Tangent = Tangent;
			}

			Keys.Add({ CurrentKey.Time, MovieSceneValueInstance });
		}

		const int32 FrameRatio = static_cast<int32>(FrameRate.AsDecimal() / 30.f);

		PTRINT CurrentCameraCutIndex = 0;
		for (PTRINT i = 0; i < Keys.Num(); ++i)
		{
			const TPair<FFrameNumber, FMovieSceneValue>& CurrentKey = Keys[i];

			bool bIsFirstFrame = false;

			while (static_cast<int32>(InCameraCuts[CurrentCameraCutIndex].GetUpperBoundValue() * FrameRatio) <= CurrentKey.Key)
			{
				CurrentCameraCutIndex += 1;

				bIsFirstFrame = true;

				TMovieSceneChannelData<FMovieSceneValue> PreviousChannelData = Channels[(CurrentCameraCutIndex - 1) % Channels.Num()]->GetData();
				TMovieSceneChannelData<FMovieSceneValue> CurrentChannelData = Channels[CurrentCameraCutIndex % Channels.Num()]->GetData();
				const TRange<uint32>& PreviousCameraCut = InCameraCuts[CurrentCameraCutIndex - 1];

				const TPair<FFrameNumber, FMovieSceneValue>& PreviousKey = 0 <= i - 1
					? Keys[i - 1]
					: Keys[0];

				const FFrameNumber CurrentCameraCutStartFrameNumber = static_cast<int32>(PreviousCameraCut.GetUpperBoundValue() * FrameRatio);

				FMovieSceneValue PreviousZeroTangentValue = PreviousKey.Value;
				{
					PreviousZeroTangentValue.InterpMode = RCIM_Cubic;

					FMovieSceneTangentData Tangent;
					Tangent.TangentWeightMode = RCTWM_WeightedNone;
					Tangent.ArriveTangent = 0.0f;
					Tangent.LeaveTangent = 0.0f;
					PreviousZeroTangentValue.Tangent = Tangent;
				}

				if (CurrentCameraCutIndex != InCameraCuts.Num())
				{
					if (CurrentKey.Key != CurrentCameraCutStartFrameNumber)
					{
						CurrentChannelData.AddKey(CurrentCameraCutStartFrameNumber, PreviousZeroTangentValue);
					}
				}

				PreviousChannelData.AddKey(CurrentCameraCutStartFrameNumber, PreviousZeroTangentValue);

				if (CurrentCameraCutIndex == InCameraCuts.Num())
				{
					break;
				}
			}

			if (CurrentCameraCutIndex == InCameraCuts.Num())
			{
				break;
			}

			bool bIsLastFrame = false;
			{
                if (Keys.Num() <= i + 1)
                {
                    bIsLastFrame = true;
                }
                else
                {
                    // ReSharper disable once CppTooWideScopeInitStatement
                    const TPair<FFrameNumber, FMovieSceneValue>& NextKey = Keys[i + 1];

                    if (static_cast<int32>(InCameraCuts[CurrentCameraCutIndex].GetUpperBoundValue() * FrameRatio) <= NextKey.Key)
                    {
                        bIsLastFrame = true;
                    }
                }
			}

			FMovieSceneValue TangentValue = CurrentKey.Value;
			{
				if (bIsFirstFrame)
				{
					TangentValue.Tangent.ArriveTangent = 0.0f;
				}
				if (bIsLastFrame)
				{
					TangentValue.Tangent.LeaveTangent = 0.0f;
				}

				if (bIsFirstFrame && bIsLastFrame)
				{
					TangentValue.Tangent.TangentWeightMode = RCTWM_WeightedNone;
				}
				else if (bIsFirstFrame)
				{
					TangentValue.Tangent.TangentWeightMode = RCTWM_WeightedLeave;
				}
				else if (bIsLastFrame)
				{
					TangentValue.Tangent.TangentWeightMode = RCTWM_WeightedArrive;
				}
			}

			TMovieSceneChannelData<FMovieSceneValue> ChannelData = Channels[CurrentCameraCutIndex % Channels.Num()]->GetData();
			ChannelData.AddKey(CurrentKey.Key, TangentValue);
		}

		const TPair<FFrameNumber, FMovieSceneValue>& LastKey = Keys.Last();

		for (PTRINT i = 0; i < Channels.Num() - 1; ++i)
		{
			CurrentCameraCutIndex += 1;
			TMovieSceneChannelData<FMovieSceneValue> ChannelData = Channels[CurrentCameraCutIndex % Channels.Num()]->GetData();
			ChannelData.AddKey(LastKey.Key, LastKey.Value);
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

		Result.Push(InCameraKeyFrames[0]);

		for (PTRINT i = 1; i < InCameraKeyFrames.Num() - 1; ++i)
		{
			const T PreviousValue = InGetValueFunc(InCameraKeyFrames, i - 1);
			const T CurrentValue = InGetValueFunc(InCameraKeyFrames, i);
			const T NextValue = InGetValueFunc(InCameraKeyFrames, i + 1);

			if (PreviousValue == CurrentValue && CurrentValue == NextValue)
			{
				continue;
			}

			const FVmdObject::FCameraKeyFrame& Current = InCameraKeyFrames[i];
			Result.Push(Current);
		}

		if (1 < InCameraKeyFrames.Num())
		{
			Result.Push(InCameraKeyFrames.Last());
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

	template<typename T>
	struct TComputedKey
	{
		FFrameNumber Time;
		T Value;
		ERichCurveInterpMode InterpMode;
		FVector2D ArriveTangent;
		FVector2D LeaveTangent;
	};
};
