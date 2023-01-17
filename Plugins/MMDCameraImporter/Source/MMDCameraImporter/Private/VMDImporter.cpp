// Copyright Epic Games, Inc. All Rights Reserved.

#include "VMDImporter.h"

#include "MMDCameraImporter.h"
#include "MMDImportHelper.h"
#include "Misc/ScopedSlowTask.h"

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

    if (ImportVmdTask.ShouldCancel())
    {
        return VmdParseResult;
    }
    ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFileMorphKeyFrames", "Reading Morph Key Frames"));
    uint32 MorphKeyFrameCount = 0;
    FileReader->Serialize(&MorphKeyFrameCount, sizeof(uint32));
    VmdParseResult.MorphKeyFrames.SetNum(MorphKeyFrameCount);
    FileReader->Serialize(VmdParseResult.MorphKeyFrames.GetData(), sizeof(FVmdObject::FMorphKeyFrame) * MorphKeyFrameCount);

    if (ImportVmdTask.ShouldCancel())
    {
        return VmdParseResult;
    }
    ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFileCameraKeyFrames", "Reading Camera Key Frames"));
    uint32 CameraKeyFrameCount = 0;
	FileReader->Serialize(&CameraKeyFrameCount, sizeof(uint32));
    VmdParseResult.CameraKeyFrames.SetNum(CameraKeyFrameCount);
    FileReader->Serialize(VmdParseResult.CameraKeyFrames.GetData(), sizeof(FVmdObject::FCameraKeyFrame) * CameraKeyFrameCount);

    if (ImportVmdTask.ShouldCancel())
    {
        return VmdParseResult;
    }
    ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFileLightKeyFrames", "Reading Light Key Frames"));
    uint32 LightKeyFrameCount = 0;
    FileReader->Serialize(&LightKeyFrameCount, sizeof(uint32));
    VmdParseResult.LightKeyFrames.SetNum(LightKeyFrameCount);
    FileReader->Serialize(VmdParseResult.LightKeyFrames.GetData(), sizeof(FVmdObject::FLightKeyFrame) * LightKeyFrameCount);

    if (ImportVmdTask.ShouldCancel())
    {
        return VmdParseResult;
    }
    ImportVmdTask.EnterProgressFrame(1, LOCTEXT("ReadingVMDFileSelfShadowKeyFrames", "Reading Self Shadow Key Frames"));
    uint32 SelfShadowKeyFrameCount = 0;
    FileReader->Serialize(&SelfShadowKeyFrameCount, sizeof(uint32));
    VmdParseResult.SelfShadowKeyFrames.SetNum(SelfShadowKeyFrameCount);
    FileReader->Serialize(VmdParseResult.SelfShadowKeyFrames.GetData(), sizeof(FVmdObject::FSelfShadowKeyFrame) * SelfShadowKeyFrameCount);

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

    VmdParseResult.bIsSuccess = true;

    return VmdParseResult;
}

FArchive* FVmdImporter::OpenFile(const FString FilePath)
{
	return IFileManager::Get().CreateFileReader(*FilePath);
}

#undef LOCTEXT_NAMESPACE
