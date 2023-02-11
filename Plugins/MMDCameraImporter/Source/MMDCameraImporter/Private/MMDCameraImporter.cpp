// Copyright Epic Games, Inc. All Rights Reserved.

#include "MMDCameraImporter.h"

#include "EditorDirectories.h"
#include "ISequencerModule.h"
#include "MMDCameraImporterCommands.h"
#include "MMDCameraImporterStyle.h"
#include "MMDUserImportVMDSettings.h"
#include "MovieSceneSequence.h"
#include "MovieSceneToolHelpers.h"
#include "ToolMenus.h"
#include "VMDImporter.h"
#include "DesktopPlatform/Public/DesktopPlatformModule.h"
#include "DesktopPlatform/Public/IDesktopPlatform.h"
#include "Runtime/Launch/Resources/Version.h"

DEFINE_LOG_CATEGORY(LogMMDCameraImporter);

#define LOCTEXT_NAMESPACE "FMmdCameraImporterModule"

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
	
private:

	FReply OnImportVmdClicked()
	{
		const UMmdUserImportVmdSettings* ImportVmdSettings = GetMutableDefault<UMmdUserImportVmdSettings>();
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_IMPORT, FPaths::GetPath(ImportFilename)); // Save path as default for next time.

		if (!Sequence || !Sequence->GetMovieScene() || Sequence->GetMovieScene()->IsReadOnly())
		{
			return FReply::Unhandled();
		}

		FVmdImporter VmdImporter;
		VmdImporter.SetFilePath(ImportFilename);

		if (!VmdImporter.IsValidVmdFile())
		{
			return FReply::Unhandled();
		}

		const FScopedTransaction Transaction(LOCTEXT("ImportVMDTransaction", "Import VMD"));
		
		const FVmdParseResult ParseResult = VmdImporter.ParseVmdFile();

		if (!ParseResult.bIsSuccess)
		{
			return FReply::Unhandled();
		}

		FVmdImporter::ImportVmdCamera(
			ParseResult,
			Sequence,
			*Sequencer,
			ImportVmdSettings);

		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);

		if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()); Window.IsValid())
		{
			Window->RequestDestroyWindow();
		}

		return FReply::Handled();
	}

	TSharedPtr<IDetailsView> DetailView;
	FString ImportFilename;
	// ReSharper disable once CppUninitializedNonStaticDataMember
	UMovieSceneSequence* Sequence;
	// ReSharper disable once CppUninitializedNonStaticDataMember
	ISequencer* Sequencer;
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
#else
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
#else 
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
		UE_LOG(LogMMDCameraImporter, Error, TEXT("Import VMD: Sequencer is not valid"));
		return;
	}
	const TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	ImportVmdWithDialog(Sequencer->GetFocusedMovieSceneSequence(), *Sequencer.Get());
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool FMmdCameraImporterModule::ImportVmdWithDialog(UMovieSceneSequence* InSequence, ISequencer& InSequencer)
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
		.ClientSize(FVector2D(480.0f, 310.0f))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false);

	const TSharedRef<SMovieSceneImportVmdSettings> DialogWidget = SNew(SMovieSceneImportVmdSettings)
		.ImportFilename(OpenFileNames[0])
		.Sequence(InSequence)
		.Sequencer(&InSequencer);
	Window->SetContent(DialogWidget);

	FSlateApplication::Get().AddWindow(Window);

	return true;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMmdCameraImporterModule, MMDCameraImporter)
