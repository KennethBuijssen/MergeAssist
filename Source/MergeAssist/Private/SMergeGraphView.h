// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "BlueprintMergeData.h"
#include "SBlueprintDiff.h"

class FSpawnTabArgs;
class FTabManager;

enum struct EMergeState;
struct MergeGraphChange;
class GraphMergeHelper;
class SMergeTreeView;

class SMergeGraphView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMergeGraphView)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const FBlueprintMergeData& Data, TSharedPtr<SMergeTreeView> MergeTreeWidget);

	void FocusGraph(FName GraphName);

	void Highlight(MergeGraphChange& Change);
	void HighlightClear();

private:
	FBlueprintMergeData Data;

	TSharedPtr<FTabManager> TabManager;
	TSharedRef<SDockTab> CreateMergeGraphTab(const FSpawnTabArgs& Args);

	TArray<TSharedPtr<GraphMergeHelper>> GraphMergeHelpers;
	TSharedPtr<GraphMergeHelper> CurrentGraphMergeHelper;

	TMap<UEdGraph*, TSharedPtr<SGraphEditor>> TargetGraphEditorMap;
	TSharedPtr<SBox> TargetGraphEditorContainer;
	TSharedPtr<SGraphEditor> CurrentTargetGraphEditor;

	TArray<FDiffPanel> DiffPanels;
};
