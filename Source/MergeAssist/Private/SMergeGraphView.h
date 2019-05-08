// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"
#include "BlueprintMergeData.h"
#include "SBlueprintDiff.h"


class FSpawnTabArgs;
class FTabManager;

enum struct EMergeState;
struct MergeGraphChange;
class GraphMergeHelper;

/**
 * 
 */
class SMergeGraphView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMergeGraphView)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const FBlueprintSelection& Data, TSharedPtr<SBox> SideContainer);

	void FocusGraph(FName GraphName);

	void Highlight(MergeGraphChange& Change);
	void HighlightClear();

	void NotifyStatus(bool IsSuccessful, const FText ErrorMessage);

	void OnToolBarPrev();
	void OnToolBarNext();
	void OnToolBarNextConflict();
	void OnToolBarPrevConflict();

	void OnToolbarApplyRemote();
	void OnToolbarApplyLocal();
	void OnToolbarRevert();

private:
	TSharedRef<ITableRow> DiffListWidgetGenerateListItems(
		TSharedPtr<MergeGraphChange> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	void DiffListWidgetOnSelectionChanged(
		TSharedPtr<MergeGraphChange> SelectedItem, 
		ESelectInfo::Type SelectInfo, 
		TSharedPtr<SBox> DetailContainer);

	void OnMergeGraphChangeRadioChanged(
		ECheckBoxState NewRadioState, 
		TSharedPtr<MergeGraphChange> Row, 
		EMergeState ButtonId);

	FBlueprintSelection Data;

	TSharedPtr<FTabManager> TabManager;
	TSharedRef<SDockTab> CreateMergeGraphTab(const FSpawnTabArgs& Args);

	TArray<TSharedPtr<GraphMergeHelper>> GraphMergeHelpers;
	TSharedPtr<GraphMergeHelper> CurrentGraphMergeHelper;

	TMap<UEdGraph*, TSharedPtr<SGraphEditor>> TargetGraphEditorMap;
	TSharedPtr<SBox> TargetGraphEditorContainer;
	TSharedPtr<SGraphEditor> CurrentTargetGraphEditor;

	TSharedPtr<SListView<TSharedPtr<GraphMergeHelper>>> GraphListWidget;
	TSharedPtr<SListView<TSharedPtr<MergeGraphChange>>> DiffListWidget;
	TSharedPtr<SBox> DetailWidget;

	TSharedPtr<STextBlock> StatusWidget;
		
	TArray<FDiffPanel> DiffPanels;

	TSharedPtr<MergeGraphChange> SelectedChange;
};
