// Copyright Epic Games, Inc. All Rights Reserved.

#include "MMDCameraImporterStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FMmdCameraImporterStyle::StyleInstance = nullptr;

void FMmdCameraImporterStyle::Initialize()
{
    if (!StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

void FMmdCameraImporterStyle::Shutdown()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    ensure(StyleInstance.IsUnique());
    StyleInstance.Reset();
}

FName FMmdCameraImporterStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("MMDCameraImporterStyle"));
    return StyleSetName;
}


TSharedRef<FSlateStyleSet> FMmdCameraImporterStyle::Create()
{
    const FVector2D Icon20X20(20.0f, 20.0f);

    TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("MMDCameraImporterStyle"));
    Style->SetContentRoot(IPluginManager::Get().FindPlugin("MMDCameraImporter")->GetBaseDir() / TEXT("Resources"));

    Style->Set("MMDCameraImporter.ImportVmd", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20X20));

    return Style;
}

void FMmdCameraImporterStyle::ReloadTextures()
{
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
    }
}

const ISlateStyle& FMmdCameraImporterStyle::Get()
{
    return *StyleInstance;
}
