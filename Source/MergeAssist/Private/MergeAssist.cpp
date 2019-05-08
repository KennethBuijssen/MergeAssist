#include "MergeAssist.h"

#include "SBlueprintMergeAssist.h"
#include "BlueprintMergeData.h"

#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "FMergeAssistModule"

static const FName MergeAssistTabId = FName(TEXT("MergeAssist"));

class FMergeAssistModule : public IMergeAssistModule
{
public:
	/** IModuleInterface implementation */
	void StartupModule() override;
	void ShutdownModule() override;

	void GenerateMergeAssistWidget(UBlueprint* BaseBlueprint, UBlueprint* LocalBlueprint, UBlueprint* RemoteBlueprint, UBlueprint* TargetBlueprint) override;
};

void FMergeAssistModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Define a tab spawner that spawns an empty dock tab on purpose
	// This allows us to later call InvokeTab() using our TabId to set the content.
	const FOnSpawnTab TabSpawner = FOnSpawnTab::CreateStatic([](const FSpawnTabArgs&) { return SNew(SDockTab); });

	// Register our tab spawner
	FTabSpawnerEntry& TabSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MergeAssistTabId, TabSpawner);

	// Configer our tab spawner by adding a name and tooltip
	TabSpawnerEntry.SetDisplayName(LOCTEXT("TabTitle", "Merge Assist"));
	TabSpawnerEntry.SetTooltipText(LOCTEXT("TooltipText", "Merge assistant main window"));

	// Preload the picking with some test values
	UBlueprint* BaseBP = Cast<UBlueprint>(FStringAssetReference("/MergeAssist/BaseBP").TryLoad());
	UBlueprint* LocalBP = Cast<UBlueprint>(FStringAssetReference("/MergeAssist/LocalBP").TryLoad());
	UBlueprint* RemoteBP = Cast<UBlueprint>(FStringAssetReference("/MergeAssist/RemoteBP").TryLoad());
	UBlueprint* TargetBP = Cast<UBlueprint>(FStringAssetReference("/MergeAssist/TargetBP").TryLoad());

	// @TODO: Stop the window from opening by default, bind it to an menu item instead
	GenerateMergeAssistWidget(BaseBP, LocalBP, RemoteBP, TargetBP);
}

void FMergeAssistModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MergeAssistTabId);
}

void FMergeAssistModule::GenerateMergeAssistWidget(UBlueprint* BaseBlueprint, UBlueprint* LocalBlueprint, UBlueprint* RemoteBlueprint, UBlueprint* TargetBlueprint)
{
	//@TODO: Only open the widget if the content has already been generated before

	FBlueprintSelection Data(
		LocalBlueprint,
		BaseBlueprint,
		FRevisionInfo::InvalidRevision(),
		RemoteBlueprint,
		FRevisionInfo::InvalidRevision(),
		TargetBlueprint
	);

	// Ensure that the widget is open
	TSharedRef<SDockTab> Tab = FGlobalTabmanager::Get()->InvokeTab(MergeAssistTabId);

	TSharedPtr<SWidget> TabContents = SNew(SBlueprintMergeAssist, Data);	
	Tab->SetContent(TabContents.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMergeAssistModule, MergeAssist)