// Copyright Epic Games, Inc. All Rights Reserved.

#include "VMDImporter.h"

#include "CineCameraActor.h"
#include "ISequencerModule.h"
#include "LevelEditorViewport.h"
#include "MMDCameraImporter.h"
#include "MMDImportHelper.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SequencerSelection.h"

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
    const UMovieSceneSequence* InSequence,
	ISequencer& InSequencer,
    const bool bCreateCameras
)
{
	if (InVmdParseResult.CameraKeyFrames.Num() == 0)
	{
		return;
	}

    const bool bNotifySlate = !FApp::IsUnattended() && !GIsRunningUnattendedScript;


	TMap<FGuid, FString> ObjectBindingMap;

	InSequencer.GetSelection().GetSelectedOutlinerNodes();

	//for (const TSharedRef<FSequencerDisplayNode>& Node : InSequencer.GetSelection().GetSelectedOutlinerNodes())
	//{
	//	/*if (Node->GetType() == ESequencerNode::Object)
	//	{
	//		auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

	//		FGuid ObjectBinding = ObjectBindingNode.Get().GetObjectBinding();

	//		ObjectBindingMap.Add(ObjectBinding, ObjectBindingNode.Get().GetDisplayName().ToString());
	//	}*/
	//}

    if (bCreateCameras)
	{
        UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;

		// Check camera binding
		
		for (auto InObjectBinding : ObjectBindingMap)
		{
            // ReSharper disable once CppTooWideScopeInitStatement
            FString ObjectName = InObjectBinding.Value;
			if (ObjectName == "NodeName")
			{
				// Look for a valid bound object, otherwise need to create a new camera and assign this binding to it
				bool bFoundBoundObject = false;
				TArrayView<TWeakObjectPtr<>> BoundObjects = InSequencer.FindBoundObjects(InObjectBinding.Key, InSequencer.GetFocusedTemplateID());
				for (auto BoundObject : BoundObjects)
				{
					if (BoundObject.IsValid())
					{
						bFoundBoundObject = true;
						break;
					}
				}

				if (!bFoundBoundObject)
				{
					if (bNotifySlate)
					{
						FNotificationInfo Info(FText::Format(NSLOCTEXT("MovieSceneTools", "NoBoundObjectsError", "Existing binding has no objects. Creating a new camera and binding for {0}"), FText::FromString(ObjectName)));
						Info.ExpireDuration = 5.0f;
						FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
					}
				}
			}
		}

        const FActorSpawnParameters SpawnParams;
		AActor* NewCamera = World->SpawnActor<ACineCameraActor>(SpawnParams);
		NewCamera->SetActorLabel("TestCamera1");

		{
			
		}

		TArray<TWeakObjectPtr<AActor> > NewCameras;
		NewCameras.Add(NewCamera);
        // ReSharper disable once CppTooWideScopeInitStatement
        TArray<FGuid> NewCameraGuids = InSequencer.AddActors(NewCameras);

		if (NewCameraGuids.Num() != 0)
		{
			ObjectBindingMap.Add(NewCameraGuids[0]);
			ObjectBindingMap[NewCameraGuids[0]] = "MmdCamera1";
		}
	}
}

FArchive* FVmdImporter::OpenFile(const FString FilePath)
{
	return IFileManager::Get().CreateFileReader(*FilePath);
}

#undef LOCTEXT_NAMESPACE
