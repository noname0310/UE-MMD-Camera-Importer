// Copyright Epic Games, Inc. All Rights Reserved.

#include "VMDImporter.h"

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
			return false;
		}
	}

    // ReSharper disable once CppTooWideScopeInitStatement
    const int64 FileSize = FileReader->TotalSize();

	if (FileSize < sizeof(FVmdObject::FHeader))
	{
		return false;
	}

    const TUniquePtr<uint8[]> Magic = TUniquePtr<uint8[]>(new uint8[30]);

	FileReader->Serialize(Magic.Get(), 30);

    const TUniquePtr<uint8[]> ModelName = TUniquePtr<uint8[]>(new uint8[20]);

	FileReader->Serialize(ModelName.Get(), 20);

	UE_LOG(LogTemp, Warning, TEXT("%s"), *FMmdImportHelper::ShiftJisToFString(Magic.Get(), 30));
	UE_LOG(LogTemp, Warning, TEXT("%s"), *FMmdImportHelper::ShiftJisToFString(ModelName.Get(), 20));

	return true;
}

FVmdParseResult FVmdImporter::ParseVmdFile()
{
	if (!FileReader.IsValid())
	{
		FileReader = TUniquePtr<FArchive>(OpenFile(FilePath));

		if (!FileReader.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to open file: %s"), *FilePath);

			FVmdParseResult FailedResult;
			FailedResult.bIsSuccess = false;

			return FailedResult;
		}
	}

	FScopedSlowTask ImportVmdTask(20, LOCTEXT("OpeningFile", "Reading File"));
	ImportVmdTask.MakeDialog(false, true);

	return {};
}

FArchive* FVmdImporter::OpenFile(const FString FilePath)
{
	return IFileManager::Get().CreateFileReader(*FilePath);
}
