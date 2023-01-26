// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISequencer.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMMDCameraImporter, Log, All);

class FMmdCameraImporterModule final : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnSequencerCreated(const TSharedRef<ISequencer> Sequencer);
	void RegisterMenus();
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	void UnregisterMenus() const;
#endif
	void ImportVmd();
	bool ImportVmdWithDialog(UMovieSceneSequence* InSequence, ISequencer& InSequencer);

private:
	TSharedPtr<FUICommandList> PluginCommands;
	TWeakPtr<ISequencer> WeakSequencer;
	FDelegateHandle SequencerCreatedHandle;
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	TSharedPtr<FExtender> SequencerToolBarExtender;
#endif
};
