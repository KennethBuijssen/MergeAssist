#include "MergeAssist.h"

#include "SBlueprintMergeAssist.h"
#include "BlueprintMergeData.h"

#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FMergeAssistModule"

static const FName MergeAssistTabId = FName(TEXT("MergeAssist"));

class FMergeAssistModule : public IMergeAssistModule
{
public:
	/** IModuleInterface implementation */
	void StartupModule() override;
	void ShutdownModule() override;
};

void FMergeAssistModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Define our tab spawner, which opens our merge UI with the test blueprints preselected
	// this is done to speed up iteration time when testing and developing
	const auto TabSpawner = FOnSpawnTab::CreateStatic([](const FSpawnTabArgs&)
	{
		auto* BaseBP = Cast<UBlueprint>(FStringAssetReference("/MergeAssist/BaseBP").TryLoad());
		auto* LocalBP = Cast<UBlueprint>(FStringAssetReference("/MergeAssist/LocalBP").TryLoad());
		auto* RemoteBP = Cast<UBlueprint>(FStringAssetReference("/MergeAssist/RemoteBP").TryLoad());
		auto* TargetBP = Cast<UBlueprint>(FStringAssetReference("/MergeAssist/TargetBP").TryLoad());

		FBlueprintMergeData Data(
			LocalBP,
			BaseBP,   FRevisionInfo::InvalidRevision(),
			RemoteBP, FRevisionInfo::InvalidRevision(),
			TargetBP
		);

		// Create a dock tab and fill it with the merge assist UI
		return SNew(SDockTab)
		[
			SNew(SBlueprintMergeAssist, Data)
		];
	});

	// Register our tab spawner
	FTabSpawnerEntry& TabSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MergeAssistTabId, TabSpawner);

	// Configure our tab spawner by adding a name and tooltip
	TabSpawnerEntry.SetDisplayName(LOCTEXT("TabTitle", "Merge Assist"));
	TabSpawnerEntry.SetTooltipText(LOCTEXT("TooltipText", "Open the Merge assist tool"));

	// @TODO: Stop the tab from opening by default
	FGlobalTabmanager::Get()->TryInvokeTab(MergeAssistTabId);
}

void FMergeAssistModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MergeAssistTabId);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMergeAssistModule, MergeAssist)