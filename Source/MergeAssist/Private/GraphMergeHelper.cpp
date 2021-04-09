// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphMergeHelper.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "GraphMergeHelper"

static UEdGraphPin* SafeFindPin(UEdGraphNode* Node, UEdGraphPin* Pin)
{
	if (!Node || !Pin) return nullptr;

	UEdGraphPin* FoundPin = Node->FindPin(Pin->PinName, Pin->Direction);

	if (!FoundPin) return nullptr;

	// Ensure that the pins also have the same type
	if (Pin->PinType != FoundPin->PinType) return nullptr;

	return FoundPin;
}

static void CloneGraphIntoGraph(UEdGraph* FromGraph, UEdGraph* TargetGraph, TMap<UEdGraphNode*, UEdGraphNode*>& NodeMappingOut)
{
	// Clear the target graph
	while(TargetGraph->Nodes.Num()) TargetGraph->RemoveNode(TargetGraph->Nodes[0]);

	// Create a copy of the base graph to duplicate all the nodes from
	UEdGraph* TmpGraph = FEdGraphUtilities::CloneGraph(FromGraph, nullptr);
		
	// Since we pop from the temp graph we need to match nodes in reverse order
	int Index = TmpGraph->Nodes.Num();

	// Clone all the nodes to the target graph
	while (TmpGraph->Nodes.Num())
	{
		UEdGraphNode* BaseNode = FromGraph->Nodes[--Index];
		UEdGraphNode* NewNode = TmpGraph->Nodes.Pop(false);
		
		// Rename and add the node to the target graph
		NewNode->Rename(nullptr, TargetGraph);
		TargetGraph->Nodes.Add(NewNode);
		
		// Keep a mapping so we can always find our nodes in the target map
		NodeMappingOut.Add(BaseNode, NewNode);
	}

	// Notify the target graph that it was changed
	TargetGraph->NotifyGraphChanged();
}

static TArray<TSharedPtr<MergeGraphChange>> GenerateChangeList(const TArray<FMergeDiffResult>& RemoteDifferences, const TArray<FMergeDiffResult>& LocalDifferences)
{
	TMap<const FMergeDiffResult*, const FMergeDiffResult*> ConflictMap;

	// Generate a mapping of all conflicts
	for (const auto& RemoteDiff : RemoteDifferences)
	{
		const FMergeDiffResult* ConflictingDifference = nullptr;

		for (const auto& LocalDiff : LocalDifferences)
		{
			// The conflict detection code is based on the code from SMergeGraphView.cpp
			// However it seems that both are affected by some of the inconsistencies in
			// the FGraphDiffControl::DiffGraphs implementation
			if (RemoteDiff.NodeOld == LocalDiff.NodeOld)
			{
				const bool bIsRemoveDiff = RemoteDiff.Type == EMergeDiffType::NODE_REMOVED || LocalDiff.Type == EMergeDiffType::NODE_REMOVED;
				const bool bIsNodeMoveDiff = RemoteDiff.Type == EMergeDiffType::NODE_MOVED || LocalDiff.Type == EMergeDiffType::NODE_MOVED;

				// Check if both diffs effect the same pin, note that Pin1 can be set to nullptr
				// in this case the change effects the entire node, which for our purposes is the 
				// same as if they would be effecting the same pin
				const bool bAreEffectingSamePin = RemoteDiff.PinOld == LocalDiff.PinOld;

				if ((bIsRemoveDiff || bAreEffectingSamePin) && !bIsNodeMoveDiff)
				{
					ConflictingDifference = &LocalDiff;
					break;
				}
			}
			else if (RemoteDiff.PinOld != nullptr && (RemoteDiff.PinOld == LocalDiff.PinOld))
			{
				// it's possible the users made the same change to the same pin, but given the wide
				// variety of changes that can be made to a pin it is difficult to identify the change 
				// as identical, for now I'm just flagging all changes to the same pin as a conflict:
				ConflictingDifference = &LocalDiff;
				break;
			}
		}

		if (ConflictingDifference != nullptr)
		{
			ConflictMap.Add(&RemoteDiff, ConflictingDifference);
			ConflictMap.Add(ConflictingDifference, &RemoteDiff);
		}
	}

	// Generate a list of all changes
	TArray<TSharedPtr<MergeGraphChange>> Ret;
	{
		for (const auto& Diff : RemoteDifferences)
		{
			const FMergeDiffResult** ConflictingDiff = ConflictMap.Find(&Diff);

			FText Label = !ConflictingDiff ? Diff.DisplayString :
				FText::Format(LOCTEXT("ConflictIdentifier", "CONFLICT: '{0}' conflicts with '{1}'"), 
					(*ConflictingDiff)->DisplayString, Diff.DisplayString) ;

			auto NewEntry = TSharedPtr<MergeGraphChange>(new MergeGraphChange());
			NewEntry->Label = Label;
			NewEntry->DisplayColor = Diff.DisplayColor;

			NewEntry->RemoteDiff = Diff;
			NewEntry->LocalDiff = ConflictingDiff ? **ConflictingDiff : FMergeDiffResult{};
			NewEntry->bHasConflicts = ConflictingDiff != nullptr;

			Ret.Push(NewEntry);
		}

		for (const auto& Diff : LocalDifferences)
		{
			const FMergeDiffResult** ConflictingDiff = ConflictMap.Find(&Diff);

			// Since we already handled all the conflicts we can skip them for now
			if (!ConflictingDiff)
			{
				auto NewEntry = TSharedPtr<MergeGraphChange>(new MergeGraphChange());
				NewEntry->Label = Diff.DisplayString;
				NewEntry->DisplayColor = Diff.DisplayColor;
	
				NewEntry->RemoteDiff = FMergeDiffResult{};
				NewEntry->LocalDiff = Diff;
				NewEntry->bHasConflicts = false;

				Ret.Push(NewEntry);
			}			
		}
	}

	// Sort the combined list of all changes, this ensure that the order in which they are displayed 
	// to the user is maintained
	struct SortChangeByDisplayOrder
	{
		bool operator() (const TSharedPtr<MergeGraphChange>& A, const TSharedPtr<MergeGraphChange>& B) const
		{
			const EMergeDiffType AType = A->RemoteDiff.Type != EMergeDiffType::NO_DIFFERENCE ? A->RemoteDiff.Type : A->LocalDiff.Type;
			const EMergeDiffType BType = B->RemoteDiff.Type != EMergeDiffType::NO_DIFFERENCE ? B->RemoteDiff.Type : B->LocalDiff.Type;

			return AType < BType;
		}
	};

	Sort(Ret.GetData(), Ret.Num(), SortChangeByDisplayOrder());
	
	return Ret;
}

GraphMergeHelper::GraphMergeHelper(UEdGraph* RemoteGraph, UEdGraph* BaseGraph, UEdGraph* LocalGraph, UEdGraph* TargetGraph)
	: GraphName(TargetGraph->GetFName())
	, RemoteGraph(RemoteGraph)
	, BaseGraph(BaseGraph)
	, LocalGraph(LocalGraph)
	, TargetGraph(TargetGraph)
{
	// Clone the base graph into the target graph
	CloneGraphIntoGraph(BaseGraph, TargetGraph, BaseToTargetNodeMap);

	const auto GenerateDifferences = [](UEdGraph* NewGraph, UEdGraph* OldGraph, TMap<UEdGraphNode*, UEdGraphNode*>& NodeMappingOut)
	{
		TArray<FMergeDiffResult> Results;
		FMergeDiffResults DiffResults = FMergeDiffResults(&Results);
		TArray<FNodeMatch> NodeMatches;

		// Diff the graphs, and collect both the diffs and node matches
		FDiffHelper::DiffGraphs(OldGraph, NewGraph, DiffResults, ENodeMatchStrategy::ALL, &NodeMatches);

		// Sort the results by the EMergeDiffType, this is the order in which the 
		// different types should be displayed to the user
		Sort(Results.GetData(), Results.Num(), 
			[](const FMergeDiffResult& A, const FMergeDiffResult& B)
		{
			return A.Type < B.Type;
		});

		// Convert the node matches into a node mapping
		// this can be used to later figure out which nodes we are talking about
		for (const auto& NodeMatch : NodeMatches)
		{
			if (!NodeMatch.IsValid()) continue;

			NodeMappingOut.Add(NodeMatch.NewNode, NodeMatch.OldNode);
		}

		return Results;
	};

	TArray<FMergeDiffResult> RemoteDifferences;
	TArray<FMergeDiffResult> LocalDifferences;

	if (RemoteGraph && BaseGraph)
	{
		RemoteDifferences = GenerateDifferences(RemoteGraph, BaseGraph, RemoteToBaseNodeMap);
		bHasRemoteChanges = RemoteDifferences.Num() != 0;
	}

	if (LocalGraph && BaseGraph)
	{
		LocalDifferences = GenerateDifferences(LocalGraph, BaseGraph, LocalToBaseNodeMap);
		bHasLocalChanges = LocalDifferences.Num() != 0;
	}

	ChangeList = GenerateChangeList(RemoteDifferences, LocalDifferences);

	// Check if any of the changes contain conflicts, if this is the case then 
	// mark the graph as containing conflicts
	for (const auto& Change : ChangeList)
	{
		if (Change->bHasConflicts)
		{
			bHasConflicts = true;
			break;
		}
	}
}

bool GraphMergeHelper::CanApplyRemoteChange(MergeGraphChange & Change)
{
	if (Change.MergeState == EMergeState::Remote) return true;

	// We do not check if the local change is already applied.
	// This means that if we can not apply the remote change 
	// right now, this might still be possible after we revert
	// the local change.
	return ApplyDiff(Change.RemoteDiff, false);
}

bool GraphMergeHelper::CanApplyLocalChange(MergeGraphChange& Change)
{
	if (Change.MergeState == EMergeState::Local) return true;

	// We do not check if the remote change is already applied.
	// This means that if we can not apply the local change 
	// right now, this might still be possible after we revert
	// the remote change.
	return ApplyDiff(Change.LocalDiff, false);
}

bool GraphMergeHelper::CanRevertChange(MergeGraphChange& Change)
{
	if (Change.MergeState == EMergeState::Remote)
	{
		return RevertDiff(Change.RemoteDiff, false);	
	}
	else if (Change.MergeState == EMergeState::Local)
	{
		return RevertDiff(Change.LocalDiff, false);
	}
	
	// If neither the Remote or local diff are applied, that means 
	// we are currently in the base state. So reverting always 
	// succeeds
	return true;
}

bool GraphMergeHelper::ApplyRemoteChange(MergeGraphChange& Change)
{
	// If the change is currently applied as a local change
	if (Change.MergeState == EMergeState::Local)
	{
		if (!RevertChange(Change)) return false;
	}

	// Apply the diff, if we are in the base state
	if (Change.MergeState == EMergeState::Base)
	{
		const bool Ret = ApplyDiff(Change.RemoteDiff, true);
		if (Ret) Change.MergeState = EMergeState::Remote;
		return Ret;
	}

	return false;
}

bool GraphMergeHelper::ApplyLocalChange(MergeGraphChange & Change)
{
	// If the change is currently applied as a remote change
	if (Change.MergeState == EMergeState::Remote)
	{
		if (!RevertChange(Change)) return false;
	}

	// Apply the diff, if we are in the base state
	if (Change.MergeState == EMergeState::Base)
	{
		const bool Ret = ApplyDiff(Change.LocalDiff, true);
		if (Ret) Change.MergeState = EMergeState::Local;
		return Ret;
	}

	return false;}

bool GraphMergeHelper::RevertChange(MergeGraphChange & Change)
{
	// If there is a change applied revert it to the base state
	if (Change.MergeState == EMergeState::Remote)
	{
		const bool Ret = RevertDiff(Change.RemoteDiff, true);
		if (Ret) Change.MergeState = EMergeState::Base;
		return Ret;
	}
	
	if (Change.MergeState == EMergeState::Local)
	{
		const bool Ret = RevertDiff(Change.LocalDiff, true);
		if (Ret) Change.MergeState = EMergeState::Base;
		return Ret;
	}

	return true;
}

UEdGraphNode* GraphMergeHelper::FindNodeInTargetGraph(UEdGraphNode* Node)
{
	if (Node == nullptr) return nullptr;

	// If the node is already in the target graph we can just return itself
	if (Node->GetGraph() == TargetGraph)
	{
		return Node;
	}

	// Check if the node is newly added, in this case we have a direct
	// mapping between the node and target graph
	auto** FoundNode = NewNodesInTargetGraph.Find(Node);
	if (FoundNode) return *FoundNode;

	// If the node is from either the Base, Local, or Remote graphs, translate
	// it to a node on the base graph
	UEdGraphNode* BaseNode = nullptr;
	if (Node->GetGraph() == BaseGraph)
	{
		BaseNode = Node;		
	}
	else if (Node->GetGraph() == LocalGraph)
	{
		auto** FoundNode1 = LocalToBaseNodeMap.Find(Node);
		if (FoundNode1) BaseNode = *FoundNode1;
	}
	else if (Node->GetGraph() == RemoteGraph)
	{
		auto** FoundNode2 = RemoteToBaseNodeMap.Find(Node);
		if (FoundNode2) BaseNode = *FoundNode2;
	}

	// If we don't know what node it would be in the base graph, we can't 
	// determine which node it would be on the target graph
	if (!BaseNode) return nullptr;

	auto** TargetNode = BaseToTargetNodeMap.Find(BaseNode);
	return TargetNode ? *TargetNode : nullptr;
}

UEdGraphNode* GraphMergeHelper::GetBaseNodeInTargetGraph(UEdGraphNode* SourceNode)
{
	if (SourceNode == nullptr) return nullptr;

	// Try and find the node in the target graph
	// NOTE: This would only support nodes in the target graph which are 
	// created through the GraphMergeHelper
	UEdGraphNode** TargetNode = BaseToTargetNodeMap.Find(SourceNode);

	if (TargetNode) return *TargetNode;

	// We could not find the node in the target graph
	return nullptr;
}

bool GraphMergeHelper::ApplyDiff(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	switch (Diff.Type)
	{
	case EMergeDiffType::NODE_REMOVED:      return ApplyDiff_NODE_REMOVED     (Diff, bCanWrite);
	case EMergeDiffType::NODE_ADDED:        return ApplyDiff_NODE_ADDED       (Diff, bCanWrite);
	case EMergeDiffType::PIN_REMOVED:       return ApplyDiff_PIN_REMOVED      (Diff, bCanWrite);
	case EMergeDiffType::PIN_ADDED:         return ApplyDiff_PIN_ADDED        (Diff, bCanWrite);
	case EMergeDiffType::LINK_REMOVED:      return ApplyDiff_LINK_REMOVED     (Diff, bCanWrite);
	case EMergeDiffType::LINK_ADDED:        return ApplyDiff_LINK_ADDED       (Diff, bCanWrite);
	case EMergeDiffType::PIN_DEFAULT_VALUE: return ApplyDiff_PIN_DEFAULT_VALUE(Diff, bCanWrite);
	case EMergeDiffType::NODE_MOVED:        return ApplyDiff_NODE_MOVED       (Diff, bCanWrite);
	case EMergeDiffType::NODE_COMMENT:      return ApplyDiff_NODE_COMMENT     (Diff, bCanWrite);
	default: return false;
	}
}

bool GraphMergeHelper::RevertDiff(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	switch (Diff.Type)
	{
	case EMergeDiffType::NODE_REMOVED:      return RevertDiff_NODE_REMOVED     (Diff, bCanWrite);
	case EMergeDiffType::NODE_ADDED:        return RevertDiff_NODE_ADDED       (Diff, bCanWrite);
	case EMergeDiffType::PIN_REMOVED:       return RevertDiff_PIN_REMOVED      (Diff, bCanWrite);
	case EMergeDiffType::PIN_ADDED:         return RevertDiff_PIN_ADDED        (Diff, bCanWrite);
	case EMergeDiffType::LINK_REMOVED:      return RevertDiff_LINK_REMOVED     (Diff, bCanWrite);
	case EMergeDiffType::LINK_ADDED:        return RevertDiff_LINK_ADDED       (Diff, bCanWrite);
	case EMergeDiffType::PIN_DEFAULT_VALUE: return RevertDiff_PIN_DEFAULT_VALUE(Diff, bCanWrite);
	case EMergeDiffType::NODE_MOVED:        return RevertDiff_NODE_MOVED       (Diff, bCanWrite);
	case EMergeDiffType::NODE_COMMENT:      return RevertDiff_NODE_COMMENT     (Diff, bCanWrite);
	default: return false;
	}
}

bool GraphMergeHelper::CloneToTarget(UEdGraphNode* SourceNode, bool bRestoreLinks, const bool CanWrite, UEdGraphNode** OutNewNode)
{
	// Make sure we have a source node we need to copy
	// and that this node does not already exist in the target graph
	{
		UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(SourceNode);
		if (!SourceNode || TargetNode) return false;
	}

	// Ensure that we can duplicate the source node, and log an error when this is not the case
	// I've yet to encounter any nodes which can not be duplicated
	check(SourceNode->CanDuplicateNode());

	// This is all the checking we can do before commiting to changes
	if (!CanWrite) return true;

	UEdGraphNode* NewNode = nullptr;

	// Clone the node to the target graph
	{
		TSet<UObject*> NodesToExport;
		NodesToExport.Add(SourceNode);

		FString ExportString;
		FEdGraphUtilities::ExportNodesToText(NodesToExport, ExportString);

		TSet<UEdGraphNode*> ImportedNodes;
		FEdGraphUtilities::ImportNodesFromText(TargetGraph, ExportString, ImportedNodes);
		FEdGraphUtilities::PostProcessPastedNodes(ImportedNodes);

		// ensure that we only ended up importing a single node to the target graph
		check(ImportedNodes.Num() == 1);

		NewNode = ImportedNodes.Array()[0];
	}

	// Guard against any failures when cloning the node into the target graph
	if (!NewNode) return false;

	// Return a pointer to the new node
	if (OutNewNode) *OutNewNode = NewNode;

	// Restore any links to other nodes in the target graph
	if (bRestoreLinks)
	{
		// Guard against changes in the number of pins
		ensure(SourceNode->Pins.Num() == NewNode->Pins.Num());

		// Try to link as many pins as possible
		for (int i = 0; i < SourceNode->Pins.Num(); ++i)
		{
			UEdGraphPin* SrcPin = SourceNode->Pins[i];
			UEdGraphPin* NewPin = NewNode->Pins[i];

			// Break all existing links, since we need to set them up to 
			// connect with nodes in the target graph
			NewPin->BreakAllPinLinks();
			for (UEdGraphPin* SrcLink : SrcPin->LinkedTo)
			{
				// Find the node on the other end of the link in the target graph
				if (UEdGraphNode* NewLinkNode = GetBaseNodeInTargetGraph(SrcLink->GetOwningNode()))
				{
					// Try and find a pin with the same name and direction
					UEdGraphPin* NewLink = NewLinkNode->FindPin(SrcLink->PinName, SrcLink->Direction);

					// If we found a matching pin, and it's of the same type as the src link 
					// we are certain to a reasonable degree that it is the same pin, and that 
					// to relevant changes have been made to the pin.
					if (NewLink && NewLink->PinType == SrcLink->PinType)
					{
						NewPin->MakeLinkTo(NewLink);
					}
				}
			}
		}
	}
	return true;
}

bool GraphMergeHelper::ApplyDiff_NODE_REMOVED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.NodeOld);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->BreakAllNodeLinks();
		TargetGraph->RemoveNode(TargetNode);
		BaseToTargetNodeMap.Remove(Diff.NodeOld);
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_NODE_ADDED(const FMergeDiffResult& Diff, const bool bCanWrite)
{	
	UEdGraphNode* NewNode = nullptr;
	
	const bool Ret = CloneToTarget(Diff.NodeNew, true, bCanWrite, &NewNode);
	if (NewNode && bCanWrite)
	{
		// Update the mapping to reflect our new node
		NewNodesInTargetGraph.Add(Diff.NodeNew, NewNode);
	}

	return Ret;
}

bool GraphMergeHelper::ApplyDiff_PIN_REMOVED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.PinOld->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.PinOld);

	if (!TargetPin) return false;

	if (bCanWrite)
	{
		TargetNode->RemovePin(TargetPin);
		TargetNode->GetGraph()->NotifyGraphChanged();
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_PIN_ADDED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = FindNodeInTargetGraph(Diff.PinNew->GetOwningNode());

	if (!TargetNode) return false;

	// Ensure that the pin name is not already used
	if (TargetNode->FindPin(Diff.PinNew->PinName)) return false;

	if (bCanWrite)
	{
		auto Pin = TargetNode->CreatePin(
			Diff.PinNew->Direction,
			Diff.PinNew->PinType.PinCategory, 
			Diff.PinNew->PinType.PinSubCategory, 
			Diff.PinNew->PinType.PinSubCategoryObject.Get(), 
			Diff.PinNew->PinName, 
			UEdGraphNode::FCreatePinParams(Diff.PinNew->PinType));

		// We need to manually notify the graph that is was changed
		// since CreatePin does not do this internally
		TargetNode->GetGraph()->NotifyGraphChanged();

		return Pin != nullptr;
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_LINK_REMOVED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.PinOld->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.PinOld);

	if (!TargetPin) return false;

	UEdGraphPin** FoundLinkTarget = TargetPin->LinkedTo.FindByPredicate(
		[Diff, this](UEdGraphPin* Pin)
	{
		return Pin 
			&& Pin->PinName         == Diff.LinkTargetOld->PinName
			&& Pin->Direction       == Diff.LinkTargetOld->Direction
			&& Pin->PinType         == Diff.LinkTargetOld->PinType
			&& Pin->GetOwningNode() == FindNodeInTargetGraph(Diff.LinkTargetOld->GetOwningNode());
	});

	// Check if we could find the link target in the target graph
	if (!FoundLinkTarget) return false;

	if (bCanWrite)
	{
		TargetPin->BreakLinkTo(*FoundLinkTarget);
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_LINK_ADDED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.PinOld->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.PinOld);

	if (!TargetPin) return false;

	UEdGraphNode* LinkTargetNode = FindNodeInTargetGraph(Diff.LinkTargetNew->GetOwningNode());
	UEdGraphPin* LinkTargetPin = SafeFindPin(LinkTargetNode, Diff.LinkTargetNew);

	// Make sure the link target can be found in the target graph
	if (!LinkTargetPin) return false;

	if (bCanWrite)
	{
		TargetPin->MakeLinkTo(LinkTargetPin);
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_PIN_DEFAULT_VALUE(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.PinOld->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.PinOld);

	if (!TargetNode || !TargetPin) return false;

	if (bCanWrite)
	{
		// Copy all the values to the target pin
		// Only one of these values will be set, however the other 
		// values should be safe to copy, since they are initialized 
		// to zero values
		TargetPin->DefaultValue = Diff.PinNew->DefaultValue;
		TargetPin->DefaultObject = Diff.PinNew->DefaultObject;
		TargetPin->DefaultTextValue = Diff.PinNew->DefaultTextValue;	
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_NODE_MOVED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.NodeOld);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->NodePosX = Diff.NodeNew->NodePosX;
		TargetNode->NodePosY = Diff.NodeNew->NodePosY;
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_NODE_COMMENT(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.NodeOld);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->NodeComment = Diff.NodeNew->NodeComment;
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_NODE_REMOVED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* NewNode = nullptr;

	const bool Ret = CloneToTarget(Diff.NodeOld, true, bCanWrite, &NewNode);
	if (NewNode && bCanWrite)
	{
		// Update the mapping to reflect our new node
		BaseToTargetNodeMap.Add(Diff.NodeOld, NewNode);
	}

	return Ret;
}

bool GraphMergeHelper::RevertDiff_NODE_ADDED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = FindNodeInTargetGraph(Diff.NodeNew);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->BreakAllNodeLinks();
		TargetGraph->RemoveNode(TargetNode);
		BaseToTargetNodeMap.Remove(Diff.NodeNew);
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_PIN_REMOVED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.PinOld->GetOwningNode());

	if (!TargetNode) return false;

	// Ensure that the pin name is not already used
	if (TargetNode->FindPin(Diff.PinOld->PinName)) return false;

	if (bCanWrite)
	{
		auto Pin = TargetNode->CreatePin(
			Diff.PinOld->Direction,
			Diff.PinOld->PinType.PinCategory, 
			Diff.PinOld->PinType.PinSubCategory, 
			Diff.PinOld->PinType.PinSubCategoryObject.Get(), 
			Diff.PinOld->PinName, 
			UEdGraphNode::FCreatePinParams(Diff.PinOld->PinType));

		// We need to manually notify the graph that is was changed
		// since CreatePin does not do this internally
		TargetNode->GetGraph()->NotifyGraphChanged();

		return Pin != nullptr;
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_PIN_ADDED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = FindNodeInTargetGraph(Diff.PinNew->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.PinNew);

	if (!TargetPin) return false;

	if (bCanWrite)
	{
		TargetNode->RemovePin(TargetPin);
		TargetNode->GetGraph()->NotifyGraphChanged();
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_LINK_REMOVED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.PinOld->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.PinOld);

	if (!TargetPin) return false;

	UEdGraphNode* LinkTargetNode = GetBaseNodeInTargetGraph(Diff.LinkTargetOld->GetOwningNode());
	UEdGraphPin* LinkTargetPin = SafeFindPin(LinkTargetNode, Diff.LinkTargetOld);

	// Make sure the link target can be found in the target graph
	if (!LinkTargetPin) return false;

	if (bCanWrite)
	{
		TargetPin->MakeLinkTo(LinkTargetPin);
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_LINK_ADDED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.PinOld->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.PinOld);

	if (!TargetPin) return false;

	UEdGraphPin** FoundLinkTarget = TargetPin->LinkedTo.FindByPredicate(
		[Diff, this](UEdGraphPin* Pin)
	{
		return Pin 
			&& Pin->PinName         == Diff.LinkTargetNew->PinName
			&& Pin->Direction       == Diff.LinkTargetNew->Direction
			&& Pin->PinType         == Diff.LinkTargetNew->PinType
			&& Pin->GetOwningNode() == FindNodeInTargetGraph(Diff.LinkTargetNew->GetOwningNode());
	});

	// Check if we could find the link target in the target graph
	if (!FoundLinkTarget) return false;

	if (bCanWrite)
	{
		TargetPin->BreakLinkTo(*FoundLinkTarget);
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_PIN_DEFAULT_VALUE(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.PinOld->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.PinOld);

	if (!TargetNode || !TargetPin) return false;

	if (bCanWrite)
	{
		// Copy all the values to the target pin
		// Only one of these values will be set, however the other 
		// values should be safe to copy, since they are initialized 
		// to zero values
		TargetPin->DefaultValue = Diff.PinOld->DefaultValue;
		TargetPin->DefaultObject = Diff.PinOld->DefaultObject;
		TargetPin->DefaultTextValue = Diff.PinOld->DefaultTextValue;	
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_NODE_MOVED(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.NodeOld);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->NodePosX = Diff.NodeOld->NodePosX;
		TargetNode->NodePosY = Diff.NodeOld->NodePosY;
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_NODE_COMMENT(const FMergeDiffResult& Diff, const bool bCanWrite)
{
	// !IMPORTANT: For NODE_MOVED and NODE_COMMENT Node2 refers to the node in the base graph
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.NodeOld);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->NodeComment = Diff.NodeOld->NodeComment;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE