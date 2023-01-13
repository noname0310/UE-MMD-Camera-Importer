// Copyright Epic Games, Inc. All Rights Reserved.

#include "MMDCameraImporter.h"

#include "EditorDirectories.h"
#include "ISequencerModule.h"
#include "MMDCameraImporterCommands.h"
#include "MMDCameraImporterStyle.h"
#include "MMDUserImportVMDSettings.h"
#include "ToolMenus.h"
#include "DesktopPlatform/Public/DesktopPlatformModule.h"
#include "DesktopPlatform/Public/IDesktopPlatform.h"
#include "LevelSequence/Public/LevelSequence.h"
#include "Runtime/Launch/Resources/Version.h"

#define LOCTEXT_NAMESPACE "FMmdCameraImporterModule"

//void ImportFBXCamera(UnFbx::FFbxImporter* FbxImporter, UMovieSceneSequence* InSequence, ISequencer& InSequencer, TMap<FGuid, FString>& InObjectBindingMap, bool bMatchByNameOnly, bool bCreateCameras)
//{
//    bool bNotifySlate = !FApp::IsUnattended() && !GIsRunningUnattendedScript;
//
//    UMovieScene* MovieScene = InSequence->GetMovieScene();
//
//    TArray<fbxsdk::FbxCamera*> AllCameras;
//    MovieSceneToolHelpers::GetCameras(FbxImporter->Scene->GetRootNode(), AllCameras);
//
//    if (AllCameras.Num() == 0)
//    {
//        return;
//    }
//
//    if (bCreateCameras)
//    {
//        UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
//
//        // Find unmatched cameras
//        TArray<fbxsdk::FbxCamera*> UnmatchedCameras;
//        for (auto Camera : AllCameras)
//        {
//            FString NodeName = MovieSceneToolHelpers::GetCameraName(Camera);
//
//            // ReSharper disable once CppTooWideScopeInitStatement
//            bool bMatched = false;
//            for (auto InObjectBinding : InObjectBindingMap)
//            {
//                // ReSharper disable once CppTooWideScopeInitStatement
//                FString ObjectName = InObjectBinding.Value;
//                if (ObjectName == NodeName)
//                {
//                    // Look for a valid bound object, otherwise need to create a new camera and assign this binding to it
//                    bool bFoundBoundObject = false;
//                    TArrayView<TWeakObjectPtr<>> BoundObjects = InSequencer.FindBoundObjects(InObjectBinding.Key, InSequencer.GetFocusedTemplateID());
//                    for (auto BoundObject : BoundObjects)
//                    {
//                        if (BoundObject.IsValid())
//                        {
//                            bFoundBoundObject = true;
//                            break;
//                        }
//                    }
//
//                    if (!bFoundBoundObject)
//                    {
//                        if (bNotifySlate)
//                        {
//                            FNotificationInfo Info(FText::Format(NSLOCTEXT("MovieSceneTools", "NoBoundObjectsError", "Existing binding has no objects. Creating a new camera and binding for {0}"), FText::FromString(ObjectName)));
//                            Info.ExpireDuration = 5.0f;
//                            FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
//                        }
//                    }
//                }
//            }
//
//            if (!bMatched)
//            {
//                UnmatchedCameras.Add(Camera);
//            }
//        }
//
//        // If there are new cameras, clear the object binding map so that we're only assigning values to the newly created cameras
//        if (UnmatchedCameras.Num() != 0)
//        {
//            InObjectBindingMap.Reset();
//            bMatchByNameOnly = true;
//        }
//
//        // Add any unmatched cameras
//        for (auto UnmatchedCamera : UnmatchedCameras)
//        {
//            FString CameraName = MovieSceneToolHelpers::GetCameraName(UnmatchedCamera);
//
//            AActor* NewCamera = nullptr;
//            if (UnmatchedCamera->GetApertureMode() == fbxsdk::FbxCamera::eFocalLength)
//            {
//                FActorSpawnParameters SpawnParams;
//                NewCamera = World->SpawnActor<ACineCameraActor>(SpawnParams);
//                NewCamera->SetActorLabel(*CameraName);
//            }
//            else
//            {
//                FActorSpawnParameters SpawnParams;
//                NewCamera = World->SpawnActor<ACameraActor>(SpawnParams);
//                NewCamera->SetActorLabel(*CameraName);
//            }
//
//            // Copy camera properties before adding default tracks so that initial camera properties match and can be restored after sequencer finishes
//            MovieSceneToolHelpers::CopyCameraProperties(UnmatchedCamera, NewCamera);
//
//            TArray<TWeakObjectPtr<AActor> > NewCameras;
//            NewCameras.Add(NewCamera);
//            // ReSharper disable once CppTooWideScopeInitStatement
//            TArray<FGuid> NewCameraGuids = InSequencer.AddActors(NewCameras);
//
//            if (NewCameraGuids.Num())
//            {
//                InObjectBindingMap.Add(NewCameraGuids[0]);
//                InObjectBindingMap[NewCameraGuids[0]] = CameraName;
//            }
//        }
//    }
//
//    MovieSceneToolHelpers::ImportFBXCameraToExisting(FbxImporter, InSequence, &InSequencer, InSequencer.GetFocusedTemplateID(), InObjectBindingMap, bMatchByNameOnly, true);
//}

// ReSharper disable once CppClassNeedsConstructorBecauseOfUninitializedMember
class SMovieSceneImportVmdSettings final : public SCompoundWidget, public FGCObject
{
	SLATE_BEGIN_ARGS(SMovieSceneImportVmdSettings) { }
		SLATE_ARGUMENT(FString, ImportFilename)
		SLATE_ARGUMENT(UMovieSceneSequence*, Sequence)
		SLATE_ARGUMENT(ISequencer*, Sequencer)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bShowPropertyMatrixButton = false;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.ViewIdentifier = "Import VMD Settings";

		DetailView = PropertyEditor.CreateDetailView(DetailsViewArgs);

		ChildSlot
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
			[
				DetailView.ToSharedRef()
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(5.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(10, 5))
			.Text(LOCTEXT("ImportVMDButtonText", "Import"))
			.OnClicked(this, &SMovieSceneImportVmdSettings::OnImportVmdClicked)
			]

			];

		ImportFilename = InArgs._ImportFilename;
		Sequence = InArgs._Sequence;
		Sequencer = InArgs._Sequencer;

		UMmdUserImportVmdSettings* ImportVmdSettings = GetMutableDefault<UMmdUserImportVmdSettings>();
		DetailView->SetObject(ImportVmdSettings);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(Sequence);
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("SMovieSceneImportVmdSettings");
	}

	void SetObjectBindingMap(const TMap<FGuid, FString>& InObjectBindingMap)
	{
		ObjectBindingMap = InObjectBindingMap;
	}

	void SetCreateCameras(const TOptional<bool> bInCreateCameras)
	{
		bCreateCameras = bInCreateCameras;
	}

private:

	FReply OnImportVmdClicked()
	{
		const UMmdUserImportVmdSettings* ImportVmdSettings = GetMutableDefault<UMmdUserImportVmdSettings>();
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_IMPORT, FPaths::GetPath(ImportFilename)); // Save path as default for next time.

		if (!Sequence || !Sequence->GetMovieScene() || Sequence->GetMovieScene()->IsReadOnly())
		{
			return FReply::Unhandled();
		}

		//FFBXInOutParameters InOutParams;
		//if (!MovieSceneToolHelpers::ReadyFBXForImport(ImportFilename, ImportVmdSettings, InOutParams))
		//{
		//	return FReply::Unhandled();
		//}

		const FScopedTransaction Transaction(LOCTEXT("ImportVMDTransaction", "Import VMD"));
		//UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();

		// ReSharper disable once CppTooWideScopeInitStatement
		bool bMatchByNameOnly = ImportVmdSettings->bMatchByNameOnly;
		if (ObjectBindingMap.Num() == 1 && bMatchByNameOnly)
		{
			UE_LOG(LogMovieScene, Display, TEXT("Fbx Import: Importing onto one selected binding, disabling match by name only."));
			bMatchByNameOnly = false;
		}

		//Import static cameras first
		// ImportFBXCamera(FbxImporter, Sequence, *Sequencer, ObjectBindingMap, bMatchByNameOnly, bCreateCameras.IsSet() ? bCreateCameras.GetValue() : ImportVmdSettings->bCreateCameras);

		UWorld* World = Cast<UWorld>(Sequencer->GetPlaybackContext());
		const bool bValid = true; // MovieSceneToolHelpers::ImportFBXIfReady(World, Sequence, Sequencer, Sequencer->GetFocusedTemplateID(), ObjectBindingMap, ImportVmdSettings, InOutParams);

		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);

		// ReSharper disable once CppTooWideScopeInitStatement
		const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());

		if (Window.IsValid())
		{
			Window->RequestDestroyWindow();
		}

		return bValid ? FReply::Handled() : FReply::Unhandled();
	}

	TSharedPtr<IDetailsView> DetailView;
	FString ImportFilename;
	// ReSharper disable once CppUninitializedNonStaticDataMember
	UMovieSceneSequence* Sequence;
	// ReSharper disable once CppUninitializedNonStaticDataMember
	ISequencer* Sequencer;
	TMap<FGuid, FString> ObjectBindingMap;
	TOptional<bool> bCreateCameras;
};

void FMmdCameraImporterModule::StartupModule()
{
	FMmdCameraImporterStyle::Initialize();
	FMmdCameraImporterStyle::ReloadTextures();

	FMmdCameraImporterCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FMmdCameraImporterCommands::Get().ImportVmd,
		FExecuteAction::CreateRaw(this, &FMmdCameraImporterModule::ImportVmd),
		FCanExecuteAction::CreateLambda([] { return true; })
	);

	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	const FOnSequencerCreated::FDelegate OnSequencerCreated =
		FOnSequencerCreated::FDelegate::CreateRaw(this, &FMmdCameraImporterModule::OnSequencerCreated);
	SequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(OnSequencerCreated);

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	RegisterMenus();
#elif
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMmdCameraImporterModule::RegisterMenus));
#endif
}

void FMmdCameraImporterModule::ShutdownModule()
{
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	UnregisterMenus();
#endif
	
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	SequencerModule.UnregisterOnSequencerCreated(SequencerCreatedHandle);

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FMmdCameraImporterStyle::Shutdown();

	FMmdCameraImporterCommands::Unregister();
}

void FMmdCameraImporterModule::OnSequencerCreated(const TSharedRef<ISequencer> Sequencer)
{
	WeakSequencer = Sequencer;
}

void FMmdCameraImporterModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	{
		SequencerToolBarExtender = MakeShareable(new FExtender);
		SequencerToolBarExtender->AddToolBarExtension(
			"Curve Editor",
			EExtensionHook::After,
			PluginCommands,
			FToolBarExtensionDelegate::CreateStatic([](FToolBarBuilder& ToolBarBuilder) {
				ToolBarBuilder.AddToolBarButton(FMmdCameraImporterCommands::Get().ImportVmd);
				}));

		const ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
		const TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager = SequencerModule.GetToolBarExtensibilityManager();
		ToolBarExtensibilityManager->AddExtender(SequencerToolBarExtender);
	}
#elif 
	{
		const FName SequencerToolbarStyleName = "SequencerToolbar";

		UToolMenu* SequencerMenu = UToolMenus::Get()->ExtendMenu("Sequencer.MainToolBar");
		FToolMenuSection& Section = SequencerMenu->FindOrAddSection("MmdCameraImporterExtensions");
		FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(FMmdCameraImporterCommands::Get().ImportVmd);
		Entry.StyleNameOverride = SequencerToolbarStyleName;
		Entry.SetCommandList(PluginCommands);
		Section.AddEntry(Entry);
	}
#endif
}

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
void FMmdCameraImporterModule::UnregisterMenus() const
{
	const ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	const TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager = SequencerModule.GetToolBarExtensibilityManager();
	ToolBarExtensibilityManager->RemoveExtender(SequencerToolBarExtender);
}
#endif

void FMmdCameraImporterModule::ImportVmd()
{
	if (!WeakSequencer.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Import VMD: Sequencer is not valid"));
		return;
	}
	const TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	const TMap<FGuid, FString> ObjectBindingNameMap;

	ImportVmdWIthDialog(Sequencer->GetFocusedMovieSceneSequence(), *Sequencer.Get(), ObjectBindingNameMap, TOptional(false));
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool FMmdCameraImporterModule::ImportVmdWIthDialog(UMovieSceneSequence* InSequence, ISequencer& InSequencer, const TMap<FGuid, FString>& InObjectBindingMap, TOptional<bool> bCreateCameras)
{
	TArray<FString> OpenFileNames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		FString ExtensionStr;
		ExtensionStr += TEXT("VMD (*.vmd)|*.vmd|");

		bOpen = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("ImportVMD", "Import VMD from...").ToString(),
			FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT),
			TEXT(""),
			*ExtensionStr,
			EFileDialogFlags::None,
			OpenFileNames
		);
	}
	if (!bOpen)
	{
		return false;
	}

	if (OpenFileNames.Num() == 0)
	{
		return false;
	}

	const FText TitleText = LOCTEXT("ImportVMDTitle", "Import VMD");

	// Create the window to choose our options
	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(TitleText)
		.HasCloseButton(true)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(450.0f, 300.0f))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false);

	const TSharedRef<SMovieSceneImportVmdSettings> DialogWidget = SNew(SMovieSceneImportVmdSettings)
		.ImportFilename(OpenFileNames[0])
		.Sequence(InSequence)
		.Sequencer(&InSequencer);
	DialogWidget->SetObjectBindingMap(InObjectBindingMap);
	DialogWidget->SetCreateCameras(bCreateCameras);
	Window->SetContent(DialogWidget);

	FSlateApplication::Get().AddWindow(Window);

	return true;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMmdCameraImporterModule, MMDCameraImporter)
