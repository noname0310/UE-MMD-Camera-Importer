// Copyright Epic Games, Inc. All Rights Reserved.

#include "VMDImporter.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "ISequencerModule.h"
#include "LevelEditorViewport.h"
#include "MMDCameraImporter.h"
#include "MMDImportHelper.h"
#include "MovieSceneToolHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FMmdCameraImporterModule"

void FVmdImporter::SetFilePath(const FString& InFilePath)
{
	FilePath = InFilePath;
}

bool FVmdImporter::IsValidVmdFile()
{
	if (!FileReader.IsValid())
	{
		FileReader = TUniquePtr<FArchive>(OpenFile(FilePath));

		if (!FileReader.IsValid())
		{
			UE_LOG(LogMMDCameraImporter, Error, TEXT("Can't open file(%s)"), *FilePath);
			return false;
		}
	}
	
	const int64 FileSize = FileReader->TotalSize();

	if (FileSize < sizeof(FVmdObject::FHeader))
	{
		UE_LOG(LogMMDCameraImporter, Error, TEXT("File seems to be corrupt(FileSize < sizeof(FVmdObject::FHeader))"));
		return false;
	}

	FileReader->Seek(0);
	uint8 Magic[30];
	FileReader->Serialize(Magic, sizeof Magic);
	if (FMmdImportHelper::ShiftJisToFString(Magic, sizeof Magic) != "Vocaloid Motion Data 0002")
	{
		UE_LOG(LogMMDCameraImporter, Error, TEXT("File is not vmd format"));
		return false;
	}
	
	int64 Offset = sizeof(FVmdObject::FHeader);

	if (FileSize < Offset + static_cast<int64>(sizeof(uint32)))
	{
		UE_LOG(LogMMDCameraImporter, Error, TEXT("File seems to be corrupt(Failed to read number of bone keyframes)"));
		return false;
	}
	FileReader->Seek(Offset);
	uint32 BoneKeyFrameCount = 0;
	FileReader->Serialize(&BoneKeyFrameCount, sizeof(uint32));
	Offset += sizeof(uint32) + (sizeof(FVmdObject::FBoneKeyFrame) * BoneKeyFrameCount);

	if (FileSize < Offset + static_cast<int64>(sizeof(uint32)))
	{
		UE_LOG(LogMMDCameraImporter, Error, TEXT("File seems to be corrupt(Failed to read number of morph keyframes)"));
		return false;
	}
	FileReader->Seek(Offset);
	uint32 MorphKeyFrameCount = 0;
	FileReader->Serialize(&MorphKeyFrameCount, sizeof(uint32));
	Offset += sizeof(uint32) + (sizeof(FVmdObject::FMorphKeyFrame) * MorphKeyFrameCount);

	if (FileSize < Offset + static_cast<int64>(sizeof(uint32)))
	{
		UE_LOG(LogMMDCameraImporter, Error, TEXT("File seems to be corrupt(Failed to read number of camera keyframes)"));
		return false;
	}
	FileReader->Seek(Offset);
	uint32 CameraKeyFrameCount = 0;
	FileReader->Serialize(&CameraKeyFrameCount, sizeof(uint32));
	Offset += sizeof(uint32) + (sizeof(FVmdObject::FCameraKeyFrame) * CameraKeyFrameCount);

	if (FileSize < Offset + static_cast<int64>(sizeof(uint32)))
	{
		UE_LOG(LogMMDCameraImporter, Error, TEXT("File seems to be corrupt(Failed to read number of light keyframes)"));
		return false;
	}
	FileReader->Seek(Offset);
	uint32 LightKeyFrameCount = 0;
	FileReader->Serialize(&LightKeyFrameCount, sizeof(uint32));
	Offset += sizeof(uint32) + (sizeof(FVmdObject::FLightKeyFrame) * LightKeyFrameCount);

	if (FileSize < Offset + static_cast<int64>(sizeof(uint32)))
	{
		UE_LOG(LogMMDCameraImporter, Error, TEXT("File seems to be corrupt(Failed to read number of self shadow keyframes)"));
		return false;
	}
	FileReader->Seek(Offset);
	uint32 SelfShadowKeyFrameCount = 0;
	FileReader->Serialize(&SelfShadowKeyFrameCount, sizeof(uint32));
	Offset += sizeof(uint32) + (sizeof(FVmdObject::FSelfShadowKeyFrame) * SelfShadowKeyFrameCount);

	if (FileSize < Offset + static_cast<int64>(sizeof(uint32)))
	{
		UE_LOG(LogMMDCameraImporter, Error, TEXT("File seems to be corrupt(Failed to read number of properties keyframes)"));
		return false;
	}
	FileReader->Seek(Offset);
	uint32 PropertyKeyFrameCount = 0;
	FileReader->Serialize(&PropertyKeyFrameCount, sizeof(uint32));
	Offset += sizeof(uint32);

	for (PTRINT i = 0; i < PropertyKeyFrameCount; ++i)
	{
		Offset += sizeof(FVmdObject::FPropertyKeyFrame);

		if (FileSize < Offset + static_cast<int64>(sizeof(uint32)))
		{
			UE_LOG(LogMMDCameraImporter, Error, TEXT("File seems to be corrupt(Failed to read number of IK state keyframes)"));
			return false;
		}
		FileReader->Seek(Offset);
		uint32 IkStateCount = 0;
		FileReader->Serialize(&IkStateCount, sizeof(uint32));
		Offset += sizeof(uint32) + (sizeof(FVmdObject::FPropertyKeyFrame::FIkState) * IkStateCount);
	}

	if (FileSize < Offset)
	{
		UE_LOG(LogMMDCameraImporter, Error, TEXT("File seems to be corrupt(FileSize < Offset)"));
		return false;
	}

	if (FileSize != Offset)
	{
		UE_LOG(LogMMDCameraImporter, Warning, TEXT("File seems to be corrupt or additional data exists"));
	}
	
	return true;
}

FVmdParseResult FVmdImporter::ParseVmdFile()
{
	if (!FileReader.IsValid())
	{
		FileReader = TUniquePtr<FArchive>(OpenFile(FilePath));

		if (!FileReader.IsValid())
		{
			UE_LOG(LogMMDCameraImporter, Error, TEXT("Can't open file(%s)"), *FilePath);

			FVmdParseResult FailedResult;
			FailedResult.bIsSuccess = false;

			return FailedResult;
		}
	}
	
	FScopedSlowTask ImportVmdTask(7, LOCTEXT("ReadingVMDFile", "Reading VMD File"));
	ImportVmdTask.MakeDialog(true, true);

	FVmdParseResult VmdParseResult;
	VmdParseResult.bIsSuccess = false;
	
	if (ImportVmdTask.ShouldCancel())
	{
		return VmdParseResult;
	}
	ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFileHeader", "Reading Header"));
	FileReader->Seek(0);
	FileReader->Serialize(&VmdParseResult.Header, sizeof(FVmdObject::FHeader));

	if (ImportVmdTask.ShouldCancel())
	{
		return VmdParseResult;
	}
	ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFileBoneKeyFrames", "Reading Bone Key Frames"));
	uint32 BoneKeyFrameCount = 0;
	FileReader->Serialize(&BoneKeyFrameCount, sizeof(uint32));
	VmdParseResult.BoneKeyFrames.SetNum(BoneKeyFrameCount);
	FileReader->Serialize(VmdParseResult.BoneKeyFrames.GetData(), sizeof(FVmdObject::FBoneKeyFrame) * BoneKeyFrameCount);
	VmdParseResult.BoneKeyFrames.Sort([](const FVmdObject::FBoneKeyFrame& A, const FVmdObject::FBoneKeyFrame& B) { return A.FrameNumber < B.FrameNumber; });

	if (ImportVmdTask.ShouldCancel())
	{
		return VmdParseResult;
	}
	ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFileMorphKeyFrames", "Reading Morph Key Frames"));
	uint32 MorphKeyFrameCount = 0;
	FileReader->Serialize(&MorphKeyFrameCount, sizeof(uint32));
	VmdParseResult.MorphKeyFrames.SetNum(MorphKeyFrameCount);
	FileReader->Serialize(VmdParseResult.MorphKeyFrames.GetData(), sizeof(FVmdObject::FMorphKeyFrame) * MorphKeyFrameCount);
	VmdParseResult.MorphKeyFrames.Sort([](const FVmdObject::FMorphKeyFrame& A, const FVmdObject::FMorphKeyFrame& B) { return A.FrameNumber < B.FrameNumber; });

	if (ImportVmdTask.ShouldCancel())
	{
		return VmdParseResult;
	}
	ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFileCameraKeyFrames", "Reading Camera Key Frames"));
	uint32 CameraKeyFrameCount = 0;
	FileReader->Serialize(&CameraKeyFrameCount, sizeof(uint32));
	VmdParseResult.CameraKeyFrames.SetNum(CameraKeyFrameCount);
	FileReader->Serialize(VmdParseResult.CameraKeyFrames.GetData(), sizeof(FVmdObject::FCameraKeyFrame) * CameraKeyFrameCount);
	VmdParseResult.CameraKeyFrames.Sort([](const FVmdObject::FCameraKeyFrame& A, const FVmdObject::FCameraKeyFrame& B) { return A.FrameNumber < B.FrameNumber; });

	if (ImportVmdTask.ShouldCancel())
	{
		return VmdParseResult;
	}
	ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFileLightKeyFrames", "Reading Light Key Frames"));
	uint32 LightKeyFrameCount = 0;
	FileReader->Serialize(&LightKeyFrameCount, sizeof(uint32));
	VmdParseResult.LightKeyFrames.SetNum(LightKeyFrameCount);
	FileReader->Serialize(VmdParseResult.LightKeyFrames.GetData(), sizeof(FVmdObject::FLightKeyFrame) * LightKeyFrameCount);
	VmdParseResult.LightKeyFrames.Sort([](const FVmdObject::FLightKeyFrame& A, const FVmdObject::FLightKeyFrame& B) { return A.FrameNumber < B.FrameNumber; });

	if (ImportVmdTask.ShouldCancel())
	{
		return VmdParseResult;
	}
	ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFileSelfShadowKeyFrames", "Reading Self Shadow Key Frames"));
	uint32 SelfShadowKeyFrameCount = 0;
	FileReader->Serialize(&SelfShadowKeyFrameCount, sizeof(uint32));
	VmdParseResult.SelfShadowKeyFrames.SetNum(SelfShadowKeyFrameCount);
	FileReader->Serialize(VmdParseResult.SelfShadowKeyFrames.GetData(), sizeof(FVmdObject::FSelfShadowKeyFrame) * SelfShadowKeyFrameCount);
	VmdParseResult.SelfShadowKeyFrames.Sort([](const FVmdObject::FSelfShadowKeyFrame& A, const FVmdObject::FSelfShadowKeyFrame& B) { return A.FrameNumber < B.FrameNumber; });

	if (ImportVmdTask.ShouldCancel())
	{
		return VmdParseResult;
	}
	ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFilePropertyKeyFrames", "Reading Property Key Frames"));
	uint32 PropertyKeyFrameCount = 0;
	FileReader->Serialize(&PropertyKeyFrameCount, sizeof(uint32));
	VmdParseResult.PropertyKeyFrames.SetNum(PropertyKeyFrameCount);
	{
		FScopedSlowTask ImportPropertyKeyFramesTask(PropertyKeyFrameCount, LOCTEXT("ReadingVMDFilePropertyKeyFrames", "Reading Property Key Frames"));

		for (PTRINT i = 0; i < PropertyKeyFrameCount; ++i)
		{
			if (ImportPropertyKeyFramesTask.ShouldCancel())
			{
				return VmdParseResult;
			}
			ImportPropertyKeyFramesTask.EnterProgressFrame();

			FVmdObject::FPropertyKeyFrame PropertyKeyFrame;
			FileReader->Serialize(&PropertyKeyFrame, sizeof(FVmdObject::FPropertyKeyFrame));
			VmdParseResult.PropertyKeyFrames[i].FrameNumber = PropertyKeyFrame.FrameNumber;
			VmdParseResult.PropertyKeyFrames[i].Visible = static_cast<bool>(PropertyKeyFrame.Visible);

			uint32 IkStateCount = 0;
			FileReader->Serialize(&IkStateCount, sizeof(uint32));
			VmdParseResult.PropertyKeyFrames[i].IkStates.SetNum(IkStateCount);
			FileReader->Serialize(VmdParseResult.PropertyKeyFrames[i].IkStates.GetData(), sizeof(FVmdObject::FPropertyKeyFrame::FIkState) * IkStateCount);
		}
	}
	VmdParseResult.PropertyKeyFrames.Sort([](const FVmdParseResult::FPropertyKeyFrameWithIkState& A, const FVmdParseResult::FPropertyKeyFrameWithIkState& B) { return A.FrameNumber < B.FrameNumber; });

	VmdParseResult.bIsSuccess = true;

	return VmdParseResult;
}

void FVmdImporter::ImportVmdCamera(
	const FVmdParseResult& InVmdParseResult,
	UMovieSceneSequence* InSequence,
	ISequencer& InSequencer,
	const UMmdUserImportVmdSettings* ImportVmdSettings
)
{
	const bool bNotifySlate = !FApp::IsUnattended() && !GIsRunningUnattendedScript;

	if (InVmdParseResult.CameraKeyFrames.Num() == 0)
	{
		UE_LOG(LogMMDCameraImporter, Warning, TEXT("This VMD file is not camera motion"));

		if (bNotifySlate)
		{
			FNotificationInfo Info(LOCTEXT("NoCameraMotionError", "This VMD file is not camera motion"));
			Info.ExpireDuration = 5.0f;
			FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
		}

		return;
	}

	FGuid MmdCameraGuid;
	FGuid MmdCameraCenterGuid;
	{
		UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
		check(World != nullptr && "World is null");

		FActorSpawnParameters CameraCenterSpawnParams;
		CameraCenterSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* NewCameraCenter = World->SpawnActor<AActor>(CameraCenterSpawnParams);
		NewCameraCenter->SetActorLabel("MmdCameraCenter");
		USceneComponent* RootSceneComponent = NewObject<USceneComponent>(NewCameraCenter, TEXT("SceneComponent"));
		NewCameraCenter->SetRootComponent(RootSceneComponent);
		NewCameraCenter->AddInstanceComponent(RootSceneComponent);

		FActorSpawnParameters CameraSpawnParams;
		CameraSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACineCameraActor* NewCamera = World->SpawnActor<ACineCameraActor>(CameraSpawnParams);
		NewCamera->SetActorLabel("MmdCamera");

		NewCamera->AttachToActor(NewCameraCenter, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true));

		{
			// ReSharper disable once CppUseStructuredBinding
			const FVmdObject::FCameraKeyFrame FirstFrame = InVmdParseResult.CameraKeyFrames[0];
			const float UniformScale = ImportVmdSettings->ImportUniformScale;
			
			NewCamera->SetActorRelativeLocation(FVector(FirstFrame.Distance * UniformScale, 0, 0));

			// Position:
			// X -> Y
			// Y -> Z
			// Z -> X
			NewCameraCenter->SetActorRelativeLocation(
				FVector(
					FirstFrame.Position[2] * UniformScale,
					FirstFrame.Position[0] * UniformScale,
					FirstFrame.Position[1] * UniformScale));

			// Rotation:
			// X -> Y
			// Y -> Z
			// Z -> X
			NewCameraCenter->SetActorRelativeRotation(
				FRotator(
					FMath::RadiansToDegrees(FirstFrame.Rotation[2]),
					FMath::RadiansToDegrees(FirstFrame.Rotation[0]),
					-FMath::RadiansToDegrees(FirstFrame.Rotation[1])));

			UCineCameraComponent* CineCameraComponent = NewCamera->GetCineCameraComponent();

			CineCameraComponent->Filmback.SensorWidth = ImportVmdSettings->CameraFilmback.SensorWidth;
			CineCameraComponent->Filmback.SensorHeight = ImportVmdSettings->CameraFilmback.SensorHeight;

			CineCameraComponent->CurrentFocalLength =
				ComputeFocalLength(FirstFrame.ViewAngle, CineCameraComponent->Filmback.SensorWidth) / 2;

			CineCameraComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;
		}

		TArray<TWeakObjectPtr<AActor>> NewActors;
		NewActors.Add(NewCameraCenter);
		NewActors.Add(NewCamera);
		TArray<FGuid> NewActorGuids = InSequencer.AddActors(NewActors);

		MmdCameraCenterGuid = NewActorGuids[0];
		MmdCameraGuid = NewActorGuids[1];
	}

	ImportVmdCameraToExisting(
		InVmdParseResult,
		InSequence,
		&InSequencer,
		InSequencer.GetFocusedTemplateID(),
		MmdCameraGuid,
		MmdCameraCenterGuid,
		ImportVmdSettings);
}

FArchive* FVmdImporter::OpenFile(const FString FilePath)
{
	return IFileManager::Get().CreateFileReader(*FilePath);
}

void FVmdImporter::ImportVmdCameraToExisting(
	const FVmdParseResult& InVmdParseResult,
	UMovieSceneSequence* InSequence,
	IMovieScenePlayer* Player,
	FMovieSceneSequenceIDRef TemplateID,
	const FGuid MmdCameraGuid,
	const FGuid MmdCameraCenterGuid,
	const UMmdUserImportVmdSettings* ImportVmdSettings
)
{
	UMovieScene* MovieScene = InSequence->GetMovieScene();
	
	const TArrayView<TWeakObjectPtr<>> BoundObjects = Player->FindBoundObjects(MmdCameraGuid, TemplateID);
	for (TWeakObjectPtr<>& WeakObject : BoundObjects)
	{
		// ReSharper disable once CppTooWideScopeInitStatement
		UObject* FoundObject = WeakObject.Get();

		if (FoundObject && FoundObject->GetClass()->IsChildOf(ACineCameraActor::StaticClass()))
		{
			const ACineCameraActor* CineCameraActor = Cast<ACineCameraActor>(FoundObject);
			UCineCameraComponent* CameraComponent = CineCameraActor->GetCineCameraComponent();


			// Set the default value of the current focal length or field of view section
			//FGuid PropertyOwnerGuid = Player->GetHandleToObject(CameraComponent);
			FGuid PropertyOwnerGuid = GetHandleToObject(CameraComponent, InSequence, Player, TemplateID, true);

			if (!PropertyOwnerGuid.IsValid())
			{
				continue;
			}

			// If copying properties to a spawnable object, the template object must be updated
			// ReSharper disable once CppTooWideScope
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(MmdCameraGuid);
			if (Spawnable)
			{
				Spawnable->CopyObjectTemplate(*FoundObject, *InSequence);
			}

			ImportVmdCameraFocalLengthProperty(
				InVmdParseResult.CameraKeyFrames,
				PropertyOwnerGuid,
				InSequence,
				CameraComponent,
				ImportVmdSettings);

			if (ImportVmdSettings->bAddMotionBlurKey)
			{
				CreateVmdCameraMotionBlurProperty(
					InVmdParseResult.CameraKeyFrames,
					PropertyOwnerGuid,
					InSequence,
					ImportVmdSettings);
			}
		}
	}

	ImportVmdCameraTransform(
		InVmdParseResult.CameraKeyFrames,
		MmdCameraGuid,
		InSequence,
		ImportVmdSettings);

	ImportVmdCameraCenterTransform(
		InVmdParseResult.CameraKeyFrames,
		MmdCameraCenterGuid,
		InSequence,
		ImportVmdSettings);
}

bool FVmdImporter::ImportVmdCameraFocalLengthProperty(
	const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
	const FGuid ObjectBinding,
	const UMovieSceneSequence* InSequence,
	const UCineCameraComponent* InCineCameraComponent,
	const UMmdUserImportVmdSettings* ImportVmdSettings
)
{
	const UMovieScene* MovieScene = InSequence->GetMovieScene();

	const FName TrackName = TEXT("CurrentFocalLength");
	
	UMovieSceneFloatTrack* FloatTrack = MovieScene->FindTrack<UMovieSceneFloatTrack>(ObjectBinding, TrackName);
	if (FloatTrack == nullptr)
	{
		return false;
	}

	FloatTrack->Modify();
	FloatTrack->RemoveAllAnimationData();

	bool bSectionAdded = false;
	UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(FloatTrack->FindOrAddSection(0, bSectionAdded));
	if (!FloatSection)
	{
		return false;
	}

	FloatSection->Modify();

	if (bSectionAdded)
	{
		FloatSection->SetRange(TRange<FFrameNumber>::All());
	}

	FMovieSceneFloatChannel* Channel = FloatSection->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
	const FFrameRate SampleRate = MovieScene->GetDisplayRate();
	const FFrameRate FrameRate = FloatSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
	const float SensorWidth = InCineCameraComponent->Filmback.SensorWidth;
	FTangentAccessIndices TangentAccessIndices;
	{
		TangentAccessIndices.ArriveTangentX = 21;
		TangentAccessIndices.ArriveTangentY = 23;
		TangentAccessIndices.LeaveTangentX = 20;
		TangentAccessIndices.LeaveTangentY = 22;
	}

	ImportCameraSingleChannel<float, FMovieSceneFloatChannel>(
		CameraKeyFrames,
		Channel,
		SampleRate,
		FrameRate,
		ImportVmdSettings->CameraCutImportType,
		TangentAccessIndices,
		[](const FVmdObject::FCameraKeyFrame& KeyFrames)
		{
			return KeyFrames.ViewAngle;
		},
		[SensorWidth](const float Value)
		{
			return ComputeFocalLength(Value, SensorWidth) / 2;
		});
	
	return true;
}

bool FVmdImporter::CreateVmdCameraMotionBlurProperty(
	const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
	const FGuid ObjectBinding,
	const UMovieSceneSequence* InSequence,
	const UMmdUserImportVmdSettings* ImportVmdSettings
)
{
	UMovieScene* MovieScene = InSequence->GetMovieScene();

	const FName TrackName = TEXT("PostProcessSettings.MotionBlurAmount");
	
	UMovieSceneFloatTrack* FloatTrack = MovieScene->FindTrack<UMovieSceneFloatTrack>(ObjectBinding, TrackName);
	if (FloatTrack == nullptr)
	{
		FloatTrack = MovieScene->AddTrack<UMovieSceneFloatTrack>(ObjectBinding);
		FloatTrack->SetPropertyNameAndPath("MotionBlurAmount", "PostProcessSettings.MotionBlurAmount");
	}

	FloatTrack->Modify();
	FloatTrack->RemoveAllAnimationData();

	bool bSectionAdded = false;
	UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(FloatTrack->FindOrAddSection(0, bSectionAdded));
	if (!FloatSection)
	{
		return false;
	}

	FloatSection->Modify();

	if (bSectionAdded)
	{
		FloatSection->SetRange(TRange<FFrameNumber>::All());
	}

	if (CameraKeyFrames.Num() == 0)
	{
		return true;
	}

	FMovieSceneFloatChannel* Channel = FloatSection->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
	TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = Channel->GetData();
	const FFrameRate SampleRate = MovieScene->GetDisplayRate();
	const FFrameRate FrameRate = FloatSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
	const FFrameNumber OneSampleFrame = (FrameRate / SampleRate).AsFrameNumber(1);
	const int32 FrameRatio = static_cast<int32>(FrameRate.AsDecimal() / 30.f);
	const float MotionBlurAmount = ImportVmdSettings->MotionBlurAmount;
	const ECameraCutImportType CameraCutImportType = ImportVmdSettings->CameraCutImportType;

	Channel->SetDefault(MotionBlurAmount);

	TArray<TRange<uint32>> CameraCutRanges;
	{
		uint32 RangeStart = CameraKeyFrames[0].FrameNumber;

		for (PTRINT i = 1; i < CameraKeyFrames.Num(); ++i)
		{
			// ReSharper disable once CppUseStructuredBinding
			const FVmdObject::FCameraKeyFrame& PreviousFrame = CameraKeyFrames[i - 1];
			// ReSharper disable once CppUseStructuredBinding
			const FVmdObject::FCameraKeyFrame& CurrentFrame = CameraKeyFrames[i];

			if (
				(CurrentFrame.FrameNumber - PreviousFrame.FrameNumber) <= 1 &&

				(CurrentFrame.ViewAngle != PreviousFrame.ViewAngle ||
				CurrentFrame.Distance != PreviousFrame.Distance ||
				CurrentFrame.Position[0] != PreviousFrame.Position[0] ||
				CurrentFrame.Position[1] != PreviousFrame.Position[1] ||
				CurrentFrame.Position[2] != PreviousFrame.Position[2] ||
				CurrentFrame.Rotation[0] != PreviousFrame.Rotation[0] ||
				CurrentFrame.Rotation[1] != PreviousFrame.Rotation[1] ||
				CurrentFrame.Rotation[2] != PreviousFrame.Rotation[2]))
			{
				continue;
			}

			if (PreviousFrame.FrameNumber != RangeStart)
			{
				CameraCutRanges.Push(TRange<uint32>(RangeStart, PreviousFrame.FrameNumber));
			}
			RangeStart = CurrentFrame.FrameNumber;
		}
	}

	if (0 < CameraCutRanges.Num() && CameraCutRanges[0].GetLowerBoundValue() != 0)
	{
		FMovieSceneFloatValue MovieSceneFloatValue;
		MovieSceneFloatValue.Value = MotionBlurAmount;
		MovieSceneFloatValue.InterpMode = RCIM_Constant;
		ChannelData.AddKey(0, MovieSceneFloatValue);
	}

	for (TRange<uint32>& CameraCutRange : CameraCutRanges)
	{
		const uint32 LowerBound = CameraCutRange.GetLowerBoundValue();
		const uint32 UpperBound = CameraCutRange.GetUpperBoundValue();


		if (CameraCutImportType == ECameraCutImportType::ImportAsIs)
		{
			FMovieSceneFloatValue MovieSceneFloatValue;
			MovieSceneFloatValue.Value = 0.0f;
			MovieSceneFloatValue.InterpMode = RCIM_Constant;
			ChannelData.AddKey(static_cast<int32>(LowerBound) * FrameRatio, MovieSceneFloatValue);
		}
		else
		{
			FMovieSceneFloatValue MovieSceneFloatValue;
			MovieSceneFloatValue.Value = 0.0f;
			MovieSceneFloatValue.InterpMode = RCIM_Constant;
			ChannelData.AddKey((static_cast<int32>(LowerBound + 1) * FrameRatio) - OneSampleFrame, MovieSceneFloatValue);
		}

		{
			FMovieSceneFloatValue MovieSceneFloatValue;
			MovieSceneFloatValue.Value = MotionBlurAmount;
			MovieSceneFloatValue.InterpMode = RCIM_Constant;
			ChannelData.AddKey((static_cast<int32>(UpperBound) * FrameRatio) + OneSampleFrame, MovieSceneFloatValue);
		}
	}

	return true;
}

bool FVmdImporter::ImportVmdCameraTransform(
	const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
	const FGuid ObjectBinding,
	const UMovieSceneSequence* InSequence,
	const UMmdUserImportVmdSettings* ImportVmdSettings
)
{
	UMovieScene* MovieScene = InSequence->GetMovieScene();

	UMovieScene3DTransformTrack* TransformTrack = MovieScene->FindTrack<UMovieScene3DTransformTrack>(ObjectBinding);
	if (!TransformTrack)
	{
		MovieScene->Modify();
		TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(ObjectBinding);
	}

	TransformTrack->Modify();

	bool bSectionAdded = false;
	UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->FindOrAddSection(0, bSectionAdded));
	if (!TransformSection)
	{
		return false;
	}

	TransformSection->Modify();

	if (bSectionAdded)
	{
		TransformSection->SetRange(TRange<FFrameNumber>::All());
	}
	
	FMovieSceneDoubleChannel* LocationXChannel = TransformSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(0);
	const FFrameRate SampleRate = MovieScene->GetDisplayRate();
	const FFrameRate FrameRate = TransformSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
	const float UniformScale = ImportVmdSettings->ImportUniformScale;
	FTangentAccessIndices TangentAccessIndices;
	{
		TangentAccessIndices.ArriveTangentX = 17;
		TangentAccessIndices.ArriveTangentY = 19;
		TangentAccessIndices.LeaveTangentX = 16;
		TangentAccessIndices.LeaveTangentY = 18;
	}

	ImportCameraSingleChannel<double, FMovieSceneDoubleChannel>(
		CameraKeyFrames,
		LocationXChannel,
		SampleRate,
		FrameRate,
		ImportVmdSettings->CameraCutImportType,
		TangentAccessIndices,
		[](const FVmdObject::FCameraKeyFrame& KeyFrames)
		{
			return KeyFrames.Distance;
		},
		[UniformScale](const double Value)
		{
			return Value * UniformScale;
		});

	return true;
}

bool FVmdImporter::ImportVmdCameraCenterTransform(
	const TArray<FVmdObject::FCameraKeyFrame>& CameraKeyFrames,
	const FGuid ObjectBinding,
	const UMovieSceneSequence* InSequence,
	const UMmdUserImportVmdSettings* ImportVmdSettings
)
{
	UMovieScene* MovieScene = InSequence->GetMovieScene();

	UMovieScene3DTransformTrack* TransformTrack = MovieScene->FindTrack<UMovieScene3DTransformTrack>(ObjectBinding);
	if (!TransformTrack)
	{
		MovieScene->Modify();
		TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(ObjectBinding);
	}
	TransformTrack->Modify();

	bool bSectionAdded = false;
	UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->FindOrAddSection(0, bSectionAdded));
	if (!TransformSection)
	{
		return false;
	}

	TransformSection->Modify();

	if (bSectionAdded)
	{
		TransformSection->SetRange(TRange<FFrameNumber>::All());
	}

	const TArrayView<FMovieSceneDoubleChannel*> Channels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
	const FFrameRate SampleRate = MovieScene->GetDisplayRate();
	const FFrameRate FrameRate = TransformSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
	const float UniformScale = ImportVmdSettings->ImportUniformScale;
	const ECameraCutImportType CameraCutImportType = ImportVmdSettings->CameraCutImportType;

	{
		FTangentAccessIndices LocationXTangentAccessIndices;
		{
			LocationXTangentAccessIndices.ArriveTangentX = 1;
			LocationXTangentAccessIndices.ArriveTangentY = 3;
			LocationXTangentAccessIndices.LeaveTangentX = 0;
			LocationXTangentAccessIndices.LeaveTangentY = 2;
		}

		ImportCameraSingleChannel<double, FMovieSceneDoubleChannel>(
			CameraKeyFrames,
			Channels[0], // Location X
			SampleRate,
			FrameRate,
			CameraCutImportType,
			LocationXTangentAccessIndices,
			[](const FVmdObject::FCameraKeyFrame& KeyFrames)
			{
				return KeyFrames.Position[2];
			},
			[UniformScale](const double Value)
			{
				return Value * UniformScale;
			});
	}

	{
		FTangentAccessIndices LocationYTangentAccessIndices;
		{
			LocationYTangentAccessIndices.ArriveTangentX = 5;
			LocationYTangentAccessIndices.ArriveTangentY = 7;
			LocationYTangentAccessIndices.LeaveTangentX = 4;
			LocationYTangentAccessIndices.LeaveTangentY = 6;
		}

		ImportCameraSingleChannel<double, FMovieSceneDoubleChannel>(
			CameraKeyFrames,
			Channels[1], // Location Y
			SampleRate,
			FrameRate,
			CameraCutImportType,
			LocationYTangentAccessIndices,
			[](const FVmdObject::FCameraKeyFrame& KeyFrames)
			{
				return KeyFrames.Position[0];
			},
			[UniformScale](const double Value)
			{
				return Value * UniformScale;
			});
	}

	{
		FTangentAccessIndices LocationZTangentAccessIndices;
		{
			LocationZTangentAccessIndices.ArriveTangentX = 9;
			LocationZTangentAccessIndices.ArriveTangentY = 11;
			LocationZTangentAccessIndices.LeaveTangentX = 8;
			LocationZTangentAccessIndices.LeaveTangentY = 10;
		}

		ImportCameraSingleChannel<double, FMovieSceneDoubleChannel>(
			CameraKeyFrames,
			Channels[2], // Location Z
			SampleRate,
			FrameRate,
			CameraCutImportType,
			LocationZTangentAccessIndices,
			[](const FVmdObject::FCameraKeyFrame& KeyFrames)
			{
				return KeyFrames.Position[1];
			},
			[UniformScale](const double Value)
			{
				return Value * UniformScale;
			});
	}

	{
		FTangentAccessIndices RotationTangentAccessIndices;
		{
			RotationTangentAccessIndices.ArriveTangentX = 13;
			RotationTangentAccessIndices.ArriveTangentY = 15;
			RotationTangentAccessIndices.LeaveTangentX = 12;
			RotationTangentAccessIndices.LeaveTangentY = 14;
		}

		ImportCameraSingleChannel<double, FMovieSceneDoubleChannel>(
			CameraKeyFrames,
			Channels[3], // Rotation X
			SampleRate,
			FrameRate,
			CameraCutImportType,
			RotationTangentAccessIndices,
			[](const FVmdObject::FCameraKeyFrame& KeyFrames)
			{
				return KeyFrames.Rotation[2];
			},
			[](const double Value)
			{
				return FMath::RadiansToDegrees(Value);
			});

		ImportCameraSingleChannel<double, FMovieSceneDoubleChannel>(
			CameraKeyFrames,
			Channels[4], // Rotation Y
			SampleRate,
			FrameRate,
			CameraCutImportType,
			RotationTangentAccessIndices,
			[](const FVmdObject::FCameraKeyFrame& KeyFrames)
			{
				return KeyFrames.Rotation[0];
			},
			[](const double Value)
			{
				return FMath::RadiansToDegrees(Value);
			});

		ImportCameraSingleChannel<double, FMovieSceneDoubleChannel>(
			CameraKeyFrames,
			Channels[5], // Rotation Z
			SampleRate,
			FrameRate,
			CameraCutImportType,
			RotationTangentAccessIndices,
			[](const FVmdObject::FCameraKeyFrame& KeyFrames)
			{
				return KeyFrames.Rotation[1];
			},
			[](const double Value)
			{
				return -FMath::RadiansToDegrees(Value);
			});
	}

	return true;
}

float FVmdImporter::ComputeFocalLength(const float FieldOfView, const float SensorWidth)
{
	// Focal Length = (Film or sensor width) / (2 * tan(FOV / 2))
	return (SensorWidth / 2.f) / FMath::Tan(FMath::DegreesToRadians(FieldOfView / 2.f));
}

FGuid FVmdImporter::GetHandleToObject(
	UObject* InObject,
	UMovieSceneSequence* InSequence,
	IMovieScenePlayer* Player,
	FMovieSceneSequenceIDRef TemplateID,
	const bool bCreateIfMissing
)
{
	UMovieScene* MovieScene = InSequence->GetMovieScene();

	// Attempt to resolve the object through the movie scene instance first, 
	FGuid PropertyOwnerGuid = FGuid();
	if (InObject != nullptr && !MovieScene->IsReadOnly())
	{
		// ReSharper disable once CppTooWideScopeInitStatement
		const FGuid ObjectGuid = Player->FindObjectId(*InObject, TemplateID);
		if (ObjectGuid.IsValid())
		{
			// Check here for spawnable otherwise spawnables get recreated as possessables, which doesn't make sense
			// ReSharper disable once CppTooWideScope
			const FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectGuid);
			if (Spawnable)
			{
				PropertyOwnerGuid = ObjectGuid;
			}
			else
			{
				// ReSharper disable once CppTooWideScope
				const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectGuid);
				if (Possessable)
				{
					PropertyOwnerGuid = ObjectGuid;
				}
			}
		}
	}

	if (PropertyOwnerGuid.IsValid())
	{
		return PropertyOwnerGuid;
	}

	if (bCreateIfMissing)
	{
		// Otherwise, create a possessable for this object. Note this will handle creating the parent possessables if this is a component.
		PropertyOwnerGuid = InSequence->CreatePossessable(InObject);
	}

	return PropertyOwnerGuid;
}

#undef LOCTEXT_NAMESPACE
