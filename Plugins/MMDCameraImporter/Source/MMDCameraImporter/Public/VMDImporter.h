// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

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
public:
	void SetFilePath(const FString& InFilePath);
	bool IsValidVmdFile();
	FVmdParseResult ParseVmdFile();

private:
	static FArchive* OpenFile(FString FilePath);

private:
	FString FilePath;
	TUniquePtr<FArchive> FileReader;
};
