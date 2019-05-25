// Fill out your copyright notice in the Description page of Project Settings.

#include "SBlueprintMergeAssist.h"
#include "SlateOptMacros.h"

//#include "EditorStyle.h"
#include "MultiBoxBuilder.h"
#include "VerticalBox.h"
#include "SSplitter.h"
#include "EditorStyle.h"

#include "Unreal/SMergeAssetPickerView.h"
#include "BlueprintMergeData.h"
#include "SMergeGraphView.h"
#include "SMergeTreeView.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

#define LOCTEXT_NAMESPACE "SBlueprintMergeAssist"

void SBlueprintMergeAssist::Construct(const FArguments& InArgs, const FBlueprintMergeData& InData)
{
	Data = InData;

	FToolBarBuilder ToolBarBuilder(nullptr, FMultiBoxCustomization::None);

	// Diff navigation
	ToolBarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::OnToolbarPrev),
			FCanExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging))
		, NAME_None
		, LOCTEXT("ToolbarPrevLabel", "Prev")
		, LOCTEXT("ToolbarPrevTooltip", "Prev")
		, FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintMerge.PrevDiff")
	);

	ToolBarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::OnToolbarNext),
			FCanExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging))
		, NAME_None
		, LOCTEXT("ToolbarNextLabel", "Next")
		, LOCTEXT("ToolbarNextTooltip", "Next")
		, FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintMerge.NextDiff")
	);

	// Conflict navigation
	ToolBarBuilder.AddSeparator();
	ToolBarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::OnToolbarPrevConflict),
			FCanExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging))
		, NAME_None
		, LOCTEXT("ToolbarPrevConflictLabel", "Prev conflict")
		, LOCTEXT("ToolbarPrevConflictTooltip", "Prev conflict")
		, FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintMerge.PrevDiff")
	);

	ToolBarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::OnToolbarNextConflict),
			FCanExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging))
		, NAME_None
		, LOCTEXT("ToolbarNextConflictLabel", "Next conflict")
		, LOCTEXT("ToolbarNextConflictTooltip", "Next conflict")
		, FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintMerge.NextDiff")
	);

	// Buttons for during the merge
	ToolBarBuilder.AddSeparator();

	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::OnToolbarApplyRemote),
			FCanExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging))
		, NAME_None
		, LOCTEXT("ToolbarApplyRemoteLabel", "Remote")
		, LOCTEXT("ToolbarApplyRemoteTooltip", "Apply remote"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintMerge.AcceptTarget")
	);

	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::OnToolbarRevert),
			FCanExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging))
		, NAME_None
		, LOCTEXT("ToolbarRevertLabel", "Base")
		, LOCTEXT("ToolbarRevertTooltip", "Revert to base"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintMerge.AcceptTarget")
	);

	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::OnToolbarApplyLocal),
			FCanExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging))
		, NAME_None
		, LOCTEXT("ToolbarApplyLocalLabel", "Local")
		, LOCTEXT("ToolbarApplyLocalTooltip", "Apply Local"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintMerge.AcceptTarget")
	);

	// Buttons for starting and finishing the merge
	ToolBarBuilder.AddSeparator();
	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::OnStartMerge),
			FCanExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::IsSelectingAssets),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateRaw(this, &SBlueprintMergeAssist::IsSelectingAssets)
			)
		, NAME_None
		, LOCTEXT("StartMergeLabel", "Start Merge")
		, LOCTEXT("StartMergeTooltip", "Starts the merge")
		, FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintMerge.StartMerge")
	);

	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::OnFinishMerge),
			FCanExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging)
			)
		, NAME_None
		, LOCTEXT("FinishMergeLabel", "Finish Merge")
		, LOCTEXT("FinishMergeTooltip", "Finish the merge")
		, FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintMerge.Finish")
	);

	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::OnCancelMerge),
			FCanExecuteAction::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateRaw(this, &SBlueprintMergeAssist::IsActivelyMerging)
			)
		, NAME_None
		, LOCTEXT("CancelMergeLabel", "Cancel Merge")
		, LOCTEXT("CancelMergeTooltip", "Cancel the merge")
		, FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintMerge.Cancel")
	);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				ToolBarBuilder.MakeWidget()
			]
		]
		+ SVerticalBox::Slot().Padding(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(.2f)
			[
				SAssignNew(SideContainer, SBox)
			]
			+ SSplitter::Slot()
			.Value(.8f)
			[
				SAssignNew(MainContainer, SBox)
			]
		]
		+SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(StatusWidget, STextBlock).Justification(ETextJustify::Right)
		]
	];

	if (IsActivelyMerging())
	{
		OnStartMerge();
	}
	else
	{
		bIsPickingAssets = true;

		// Setup the asset picker
		AssetPickerControl = SNew(SMergeAssetPickerView, InData).OnAssetChanged(this, &SBlueprintMergeAssist::OnMergeAssetSelected);
	}

	// Change the mode to initialize the UI to the state for the current mode
	OnModeChanged();
}

void SBlueprintMergeAssist::OnToolbarNext()
{
	if (MergeTreeWidget) MergeTreeWidget->OnToolBarNext();
}

void SBlueprintMergeAssist::OnToolbarPrev()
{
	if (MergeTreeWidget) MergeTreeWidget->OnToolBarPrev();
}

void SBlueprintMergeAssist::OnToolbarNextConflict()
{
	if (MergeTreeWidget) MergeTreeWidget->OnToolBarNextConflict();
}

void SBlueprintMergeAssist::OnToolbarPrevConflict()
{
	if (MergeTreeWidget) MergeTreeWidget->OnToolBarPrevConflict();
}

void SBlueprintMergeAssist::OnToolbarApplyRemote()
{
	if (MergeTreeWidget) MergeTreeWidget->OnToolbarApplyRemote();
}

void SBlueprintMergeAssist::OnToolbarApplyLocal()
{
	if (MergeTreeWidget) MergeTreeWidget->OnToolbarApplyLocal();
}

void SBlueprintMergeAssist::OnToolbarRevert()
{
	if (MergeTreeWidget) MergeTreeWidget->OnToolbarRevert();
}

void SBlueprintMergeAssist::OnToolbarFinishMerge()
{
	
}

bool SBlueprintMergeAssist::IsSelectingAssets() const
{
	return bIsPickingAssets;
}

void SBlueprintMergeAssist::OnStartMerge()
{
	// Load the correct versions of the blueprint assets
	if (Data.BlueprintRemote == nullptr)
	{
		Data.BlueprintRemote = Cast<UBlueprint>(FMergeToolUtils::LoadRevision(RemotePath, Data.RevisionRemote));
	}

	if (Data.BlueprintBase == nullptr)
	{
		Data.BlueprintBase = Cast<UBlueprint>(FMergeToolUtils::LoadRevision(BasePath, Data.RevisionBase));
	}

	if (Data.BlueprintLocal == nullptr)
	{
		Data.BlueprintLocal = Cast<UBlueprint>(FMergeToolUtils::LoadRevision(LocalPath, Data.RevisionLocal));
	}

	// We cannot start the merge if one of the assets are not set
	if (Data.BlueprintRemote == nullptr
		&& Data.BlueprintBase == nullptr
		&& Data.BlueprintLocal == nullptr) return;

	// @TODO: Create a backup (and cancel functionality in case the user does not merge into a target BP)

	MergeTreeWidget = SNew(SMergeTreeView);
	GraphViewWidget = SNew(SMergeGraphView, Data, MergeTreeWidget);

	bIsPickingAssets = false;
	OnModeChanged();
}

void SBlueprintMergeAssist::OnFinishMerge()
{
	// For now finishing the merge just closes the UI
	// Later this will save all changes to the target
	bIsPickingAssets = true;
	OnModeChanged();

}

void SBlueprintMergeAssist::OnCancelMerge()
{
	// For now canceling the merge just closes the UI
	// Later this will revert all changes to the target
	bIsPickingAssets = true;
	OnModeChanged();
}

void SBlueprintMergeAssist::OnMergeAssetSelected(EMergeAssetId::Type AssetId, const FAssetRevisionInfo& AssetInfo)
{
	switch (AssetId)
	{
	case EMergeAssetId::MergeRemote:
		{
			RemotePath = AssetInfo.AssetName;
			Data.RevisionRemote  = AssetInfo.Revision;
			Data.BlueprintRemote = nullptr;
			break;
		}
	case EMergeAssetId::MergeBase:
		{
			BasePath = AssetInfo.AssetName;
			Data.RevisionBase  = AssetInfo.Revision;
			Data.BlueprintBase = nullptr;
			break;
		}
	case EMergeAssetId::MergeLocal:
		{
			LocalPath = AssetInfo.AssetName;
			Data.RevisionLocal  = AssetInfo.Revision;
			Data.BlueprintLocal = nullptr;
			break;
		}
	}
}

bool SBlueprintMergeAssist::IsActivelyMerging() const
{
	return !bIsPickingAssets
		&& Data.BlueprintRemote != nullptr
		&& Data.BlueprintBase != nullptr
		&& Data.BlueprintLocal != nullptr;
}

void SBlueprintMergeAssist::OnModeChanged()
{
	if (!IsActivelyMerging())
	{
		MainContainer->SetContent(AssetPickerControl.ToSharedRef());
		SideContainer->SetContent(SNew(STextBlock));
	}
	else
	{
		MainContainer->SetContent(GraphViewWidget.ToSharedRef());
		SideContainer->SetContent(MergeTreeWidget.ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE

END_SLATE_FUNCTION_BUILD_OPTIMIZATION