// Fill out your copyright notice in the Description page of Project Settings.

#include "SMergeGraphView.h"
#include "SlateOptMacros.h"
#include "SSplitter.h"
#include "SDockTab.h"
#include "EdGraph/EdGraph.h"
#include "SBlueprintDiff.h"
#include "Engine/Blueprint.h"
#include "SCheckBox.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorUtils.h"
#include "GraphMergeHelper.h"
#include "SMergeTreeView.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

#define LOCTEXT_NAMESPACE "SMergeAssistGraphView"

static const FName MergeGraphTabId = FName(TEXT("MergeGraphTab"));

struct ChangeTreeEntryGraph : IMergeTreeEntry
{
	ChangeTreeEntryGraph(TSharedPtr<GraphMergeHelper> MergeHelper) : MergeHelper(MergeHelper) {}

	TSharedRef<SWidget> OnGenerateRow() override;
	void OnSelected(SMergeGraphView* GraphView) override;

	TSharedPtr<GraphMergeHelper> MergeHelper;
};

struct ChangeTreeEntryChange : IMergeTreeEntry
{
	ChangeTreeEntryChange(
		TSharedPtr<GraphMergeHelper> MergeHelper, 
		TSharedPtr<MergeGraphChange> Change) 
	: MergeHelper(MergeHelper)
	, Change(Change) {}

	TSharedRef<SWidget> OnGenerateRow() override;
	void OnSelected(SMergeGraphView* GraphView) override;

	bool ApplyRemote() override
	{
		return MergeHelper->ApplyRemoteChange(*Change);
	}

	bool ApplyLocal() override
	{
		return MergeHelper->ApplyLocalChange(*Change);
	}

	bool Revert() override
	{
		return MergeHelper->RevertChange(*Change);
	}

	TSharedPtr<GraphMergeHelper> MergeHelper;
	TSharedPtr<MergeGraphChange> Change;
};

struct FBlueprintRevPair
{
	const UBlueprint* Blueprint;
	const FRevisionInfo& RevData;
};

static UEdGraph* FindGraphByName(UBlueprint const& FromBlueprint, const FName& GraphName)
{
	TArray<UEdGraph*> Graphs;
	FromBlueprint.GetAllGraphs(Graphs);

	UEdGraph* Ret = nullptr;
	if (UEdGraph** Result = Graphs.FindByPredicate(FMatchFName(GraphName)))
	{
		Ret = *Result;
	}
	return Ret;
}

TSharedRef<ITableRow> GraphListWidgetGenerateListItems(TSharedPtr<GraphMergeHelper> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SHorizontalBox> RowContent = SNew(SHorizontalBox)
	+SHorizontalBox::Slot()
	[
		SNew(STextBlock)
		.Text(FText::FromName(Item->GraphName))
	];

	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
	[
		RowContent
	];
}

void GraphListWidgetOnSelectionChanged(TSharedPtr<GraphMergeHelper> SelectedItem, 
	ESelectInfo::Type SelectInfo, SMergeGraphView* MergeGraphView)
{
	if(!SelectedItem) return;

	MergeGraphView->FocusGraph(SelectedItem->GraphName);
}

static FDiffPanel* GetDiffPanelForNode(const UEdGraphNode& Node, TArray<FDiffPanel>& Panels)
{
	for (auto& Panel : Panels)
	{
		auto GraphEditor = Panel.GraphEditor.Pin();
		if (GraphEditor.IsValid())
		{
			if (Node.GetGraph() == GraphEditor->GetCurrentGraph())
			{
				return &Panel;
			}
		}
	}

	return nullptr;
}

void SMergeGraphView::Highlight(MergeGraphChange& Change)
{
	// Always clear the old highlight before setting the new one
	HighlightClear();

	// Highlight the pin if it exists, otherwise highlight the node
	const auto HighlightPinOrNode = [this](UEdGraphPin* Pin, UEdGraphNode* Node)
	{
		if (Pin)
		{
			// then look for the diff panel and focus on the change:
			auto* Panel = GetDiffPanelForNode(*Pin->GetOwningNode(), DiffPanels);
			if (Panel) Panel->FocusDiff(*Pin);
		}
		else if (Node)
		{
			auto* Panel = GetDiffPanelForNode(*Node, DiffPanels);
			if (Panel) Panel->FocusDiff(*Node);
		}
	};

	// Highlight the remote diff
	HighlightPinOrNode(Change.RemoteDiff.PinOld, Change.RemoteDiff.NodeOld);
	HighlightPinOrNode(Change.RemoteDiff.PinNew, Change.RemoteDiff.NodeNew);

	// Highlight the local diff
	HighlightPinOrNode(Change.LocalDiff.PinOld, Change.LocalDiff.NodeOld);
	HighlightPinOrNode(Change.LocalDiff.PinNew, Change.LocalDiff.NodeNew);

	// Highlight the related pins and nodes in the target graph
	const auto HighlightInTargetGraph = [this](UEdGraphPin* Pin, UEdGraphNode* Node)
	{
		// Translate the pin and node to the target graph
		UEdGraphNode* TargetNode = CurrentGraphMergeHelper->FindNodeInTargetGraph(Pin ? Pin->GetOwningNode() : Node);
		if (!TargetNode) return;

		// In the target graph we can only select the corresponding node
		// highlighting a pin is not possible
		CurrentTargetGraphEditor->SetNodeSelection(TargetNode, true);
		CurrentTargetGraphEditor->JumpToNode(TargetNode);
	};

	// Highlight the remote diff
	HighlightInTargetGraph(Change.RemoteDiff.PinOld, Change.RemoteDiff.NodeOld);
	HighlightInTargetGraph(Change.RemoteDiff.PinNew, Change.RemoteDiff.NodeNew);

	// Highlight the local diff
	HighlightInTargetGraph(Change.LocalDiff.PinOld, Change.LocalDiff.NodeOld);
	HighlightInTargetGraph(Change.LocalDiff.PinNew, Change.LocalDiff.NodeNew);
}

void SMergeGraphView::HighlightClear()
{
	const auto ClearSelection = [](FDiffPanel& Panel) {
		// Clear the node selection
		if (!Panel.GraphEditor.IsValid()) return;

		Panel.GraphEditor.Pin()->ClearSelectionSet();

		// Clear any markers indicating pins are being diffed
		if (Panel.LastFocusedPin) Panel.LastFocusedPin->bIsDiffing = false;
	};
		
	for (auto Panel : DiffPanels)
	{
		ClearSelection(Panel);
	}

	// Also clear the selection on the target editor
	if (CurrentTargetGraphEditor) CurrentTargetGraphEditor->ClearSelectionSet();
}

void SMergeGraphView::NotifyStatus(bool IsSuccessful, const FText ErrorMessage)
{
	if (IsSuccessful)
	{
		StatusWidget->SetColorAndOpacity(SoftGreen);
		StatusWidget->SetText(FText::FromString("Success"));
	}
	else
	{
		StatusWidget->SetColorAndOpacity(SoftRed);
		StatusWidget->SetText(ErrorMessage);
	}
}

void SMergeGraphView::Construct(const FArguments& InArgs, const FBlueprintMergeData& InData, TSharedPtr<SBox> SideContainer)
{
	Data = InData;

	check(Data.BlueprintRemote != nullptr);
	check(Data.BlueprintBase != nullptr);
	check(Data.BlueprintLocal != nullptr);
	check(Data.BlueprintTarget != nullptr);

	// Enumerate all the EVENT graphs in the blueprint
	// for now we ignore function, delete, and macro graphs
	// since these could have additional requirements which 
	// we do not support for now.
	// !Note: EventGraphs are stored in the UbergraphPages array
	//        and not in the EventGraphs array.
	TSet<FName> AllGraphNames;
	TMap<FName, UEdGraph*> RemoteGraphMap, BaseGraphMap, LocalGraphMap;
	{
		const auto RemoteGraphs = Data.BlueprintRemote->UbergraphPages;
		const auto BaseGraphs =  Data.BlueprintBase->UbergraphPages;
		const auto LocalGraphs = Data.BlueprintLocal->UbergraphPages;

		const auto Enumerate = [&AllGraphNames](const TArray<UEdGraph*>& GraphsToEnumerate, TMap<FName, UEdGraph*>& OutMap)
		{
			for (auto Graph : GraphsToEnumerate)
			{
				OutMap.Add(Graph->GetFName(), Graph);
				AllGraphNames.Add(Graph->GetFName());
			}
		};

		Enumerate(RemoteGraphs, RemoteGraphMap);
		Enumerate(BaseGraphs, BaseGraphMap);
		Enumerate(LocalGraphs, LocalGraphMap);
	}

	// Create editors for each of the graphs
	for (auto GraphName : AllGraphNames)
	{
		UEdGraph* TargetGraph = FindGraphByName(*Data.BlueprintTarget, GraphName);

		if (!TargetGraph)
		{
			// Create an event graph with the GraphName in case we could not find one with the matching name
			//FName DocumentName = FBlueprintEditorUtils::FindUniqueKismetName(Data.BlueprintTarget, GraphName.ToString());
			TargetGraph = FBlueprintEditorUtils::CreateNewGraph(Data.BlueprintTarget, GraphName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			FBlueprintEditorUtils::AddUbergraphPage(Data.BlueprintTarget, TargetGraph);
		}

		// We found the target graph, or successfully made one
		if (TargetGraph)
		{
			const TSharedPtr<SGraphEditor> Editor = SNew(SGraphEditor)
					.GraphToEdit(TargetGraph)
					.IsEditable(true);

			TargetGraphEditorMap.Add(TargetGraph, Editor);
		}
	}

	// Create merge helpers for each of the graphs
	for (auto GraphName : AllGraphNames)
	{
		GraphMergeHelpers.Push(TSharedPtr<GraphMergeHelper>(new GraphMergeHelper(
			FindGraphByName(*Data.BlueprintRemote, GraphName),
			FindGraphByName(*Data.BlueprintBase, GraphName),
			FindGraphByName(*Data.BlueprintLocal, GraphName),
			FindGraphByName(*Data.BlueprintTarget, GraphName)
		)));
	}

	// Set up a tab view so we can split the content into different views
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab).TabRole(ETabRole::MajorTab);
	TabManager = FGlobalTabmanager::Get()->NewTabManager(MajorTab);

	TabManager->RegisterTabSpawner(MergeGraphTabId,
		FOnSpawnTab::CreateRaw(this, &SMergeGraphView::CreateMergeGraphTab))
		.SetDisplayName(LOCTEXT("MergeGraphTabTitle", "Graphs"));

	// Create a default layout for the tab manager, this allows us to open the tabs by default
	const TSharedRef<FTabManager::FLayout> DefaultLayout = FTabManager::NewLayout("MergeGraphView_Layout")
		->AddArea(
			FTabManager::NewPrimaryArea()
			->Split(
				FTabManager::NewStack()
				->AddTab(MergeGraphTabId, ETabState::OpenedTab)
			)
		);

	TArray<FBlueprintRevPair> BlueprintsForDisplay;
	BlueprintsForDisplay.Add(FBlueprintRevPair{Data.BlueprintRemote, Data.RevisionRemote});
	BlueprintsForDisplay.Add(FBlueprintRevPair{Data.BlueprintBase, Data.RevisionBase});
	BlueprintsForDisplay.Add(FBlueprintRevPair{Data.BlueprintLocal, Data.RevisionLocal});

	for (int i = 0; i < 3; ++i)
	{
		DiffPanels.Add(FDiffPanel());
		FDiffPanel& NewPanel = DiffPanels[i];
		NewPanel.Blueprint = BlueprintsForDisplay[i].Blueprint;
		NewPanel.RevisionInfo = BlueprintsForDisplay[i].RevData;
		NewPanel.bShowAssetName = true;
	}

	// Setup a placeholder for the target graph editor
	TargetGraphEditorContainer = SNew(SBox)[
		SNew(SBorder).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString("Graph does not exist in target blueprint"))
		]
	];

	const auto GraphTab = TabManager->RestoreFrom(DefaultLayout, nullptr).ToSharedRef();

	for (auto& Panel: DiffPanels)
	{
		// @TODO: Move generating the blueprint panel to the Details (blueprint) view once support for this has been added
		Panel.GenerateMyBlueprintPanel();
		Panel.InitializeDiffPanel();
	}

	StatusWidget = SNew(STextBlock).Justification(ETextJustify::Right);

	// Focus the first graph in the list by default, 
	// this is to ensure that all UI elements are initialized
	FocusGraph(AllGraphNames.Array()[0]);

	// We get one tab container with the different tabs, and within this we add the splitter
	// The reason for this is so we could potentially create a fullscreen target tab
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			GraphTab
		]
		+SVerticalBox::Slot().AutoHeight()
		[
			StatusWidget.ToSharedRef()
		]
	];

	// @TODO: should this be passed in as an argument?
	MergeTreeWidget = SNew(SMergeTreeView, this);

	// Add all of our changes to the merge tree
	for (auto GraphHelper : GraphMergeHelpers)
	{
		auto GraphEntry = MakeShared<ChangeTreeEntryGraph>(GraphHelper);

		for (auto Change : GraphHelper->ChangeList)
		{
			GraphEntry->Children.Add(MakeShared<ChangeTreeEntryChange>(GraphHelper, Change));
		}

		MergeTreeWidget->Add(GraphEntry);
	}

	// Setup the side bar to list the differences and show details
	SideContainer->SetContent(
		MergeTreeWidget.ToSharedRef()
	);
}

void SMergeGraphView::FocusGraph(FName GraphName)
{
	// Only change if we focus a different graph
	if (CurrentGraphMergeHelper && CurrentGraphMergeHelper->GraphName == GraphName) return;

	// Setup the diff panels for the source graphs
	UEdGraph* RemoteGraph = FindGraphByName(*DiffPanels[0].Blueprint, GraphName);
	UEdGraph* BaseGraph = FindGraphByName(*DiffPanels[1].Blueprint, GraphName);
	UEdGraph* LocalGraph = FindGraphByName(*DiffPanels[2].Blueprint, GraphName);

	DiffPanels[0].GeneratePanel(RemoteGraph, BaseGraph);
	DiffPanels[1].GeneratePanel(BaseGraph, nullptr);
	DiffPanels[2].GeneratePanel(LocalGraph, BaseGraph);

	// Open the editor for the target graph
	UEdGraph* TargetGraph = FindGraphByName(*Data.BlueprintTarget, GraphName);
	TSharedPtr<SGraphEditor>* TargetEditor = TargetGraph ? TargetGraphEditorMap.Find(TargetGraph) : nullptr;
	if (TargetEditor != nullptr)
	{
		CurrentTargetGraphEditor = *TargetEditor;
		TargetGraphEditorContainer->SetContent(TargetEditor->ToSharedRef());
	}
	else
	{
		CurrentTargetGraphEditor = nullptr;
		TargetGraphEditorContainer->SetContent(
			SNew(SBorder).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString("Graph does not exist in target blueprint"))
			]
		);
	}

	// Update the diff list being shown to the one based on the selected graph
	for (const auto& MergeHelper : GraphMergeHelpers)
	{
		if (MergeHelper->GraphName == GraphName)
		{
			CurrentGraphMergeHelper = MergeHelper;
			break;
		}
	}
}

TSharedRef<SDockTab> SMergeGraphView::CreateMergeGraphTab(const FSpawnTabArgs& Args)
{
	auto PanelContainer = SNew(SSplitter);
	for (auto& Panel : DiffPanels)
	{
		PanelContainer->AddSlot()
		[
			SAssignNew(Panel.GraphEditorBorder, SBox)
			.VAlign(VAlign_Fill)
			[
				SBlueprintDiff::DefaultEmptyPanel()
			]
		];
	}

	return SNew(SDockTab)
	[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		+SSplitter::Slot()
		.Value(0.6f)
		[
			PanelContainer
		]
		+ SSplitter::Slot()
		.Value(0.4f)
		[
			TargetGraphEditorContainer.ToSharedRef()
		]
	];
}

static ECheckBoxState IsRadioChecked(TSharedPtr<MergeGraphChange> Row, EMergeState ButtonId)
{
   return (Row->MergeState == ButtonId) 
			? ECheckBoxState::Checked : ECheckBoxState::Unchecked;	
}

static void OnRadioChanged(ECheckBoxState NewRadioState, ChangeTreeEntryChange* ChangeEntry, EMergeState ButtonId)
{
	if (NewRadioState != ECheckBoxState::Checked) return;

	switch (ButtonId)
	{
		case EMergeState::Remote: ChangeEntry->ApplyRemote(); break;
		case EMergeState::Base:   ChangeEntry->Revert();      break;
		case EMergeState::Local:  ChangeEntry->ApplyLocal();  break;
	}

	// @TODO: Add status reporting
	//NotifyStatus(Ret, LOCTEXT("ErrorMessageToolbarApplyRemote", "Failed to apply remote change!."));
}

static bool IsRadioEnabled(TSharedPtr<MergeGraphChange> Row, EMergeState ButtonId, TSharedPtr<GraphMergeHelper> MergeHelper)
{
	switch(ButtonId)
	{
		case EMergeState::Remote: return MergeHelper->CanApplyRemoteChange(*Row);
		case EMergeState::Local:  return MergeHelper->CanApplyLocalChange(*Row);
		case EMergeState::Base:   return MergeHelper->CanRevertChange(*Row);
		default: return true;
	}
}

TSharedRef<SWidget> ChangeTreeEntryGraph::OnGenerateRow()
{
	return SNew(STextBlock).Text(FText::FromName(MergeHelper->GraphName));
}

void ChangeTreeEntryGraph::OnSelected(SMergeGraphView* GraphView)
{
	GraphView->FocusGraph(MergeHelper->GraphName);
}

TSharedRef<SWidget> ChangeTreeEntryChange::OnGenerateRow()
{
	const auto CreateCheckbox = 
		[this](FMergeDiffResult* DiffResult, EMergeState ButtonType, FLinearColor Color)
	{
		// Use an invisible checkbox to indicate that there is no change
		if (DiffResult && DiffResult->Type == EMergeDiffType::NO_DIFFERENCE)
		{
			return SNew(SCheckBox)
			.ForegroundColor(FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)))
			.BorderBackgroundColor(FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)))
			.IsEnabled(false);
		}

		return SNew(SCheckBox)
			.Type(ESlateCheckBoxType::CheckBox)
			.ForegroundColor(Color)
			.IsChecked_Static(&IsRadioChecked, Change, ButtonType)
			.OnCheckStateChanged_Static(&OnRadioChanged, this, ButtonType)
			.IsEnabled_Static(&IsRadioEnabled, Change, ButtonType, MergeHelper);
	};

	return SNew(SHorizontalBox)
	+SHorizontalBox::Slot()
	[
		SNew(STextBlock).Text(Change->Label).ColorAndOpacity(Change->DisplayColor)
	]
	+SHorizontalBox::Slot().AutoWidth()
	[
		CreateCheckbox(&Change->RemoteDiff, EMergeState::Remote, SoftBlue)
	]
	+SHorizontalBox::Slot().AutoWidth()
	[
		CreateCheckbox(nullptr, EMergeState::Base, SoftYellow)
	]
	+SHorizontalBox::Slot().AutoWidth()
	[
		CreateCheckbox(&Change->LocalDiff, EMergeState::Local, SoftGreen)
	];
}

void ChangeTreeEntryChange::OnSelected(SMergeGraphView* GraphView)
{
	GraphView->FocusGraph(MergeHelper->GraphName);
	GraphView->Highlight(*Change);
}

#undef LOCTEXT_NAMESPACE

END_SLATE_FUNCTION_BUILD_OPTIMIZATION