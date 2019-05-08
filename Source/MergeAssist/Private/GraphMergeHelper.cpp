// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphMergeHelper.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "GraphDiffControl.h"

#define LOCTEXT_NAMESPACE "GraphMergeHelper"

static UEdGraphPin* SafeFindPin(UEdGraphNode* Node, UEdGraphPin* Pin)
{
	if (!Node || !Pin) return nullptr;

	return Node->FindPin(Pin->PinName, Pin->Direction);
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

static bool DiffGraphsExt(UEdGraph * const LhsGraph, UEdGraph * const RhsGraph, TArray<FDiffSingleResult>& DiffsOut, TArray<FGraphDiffControl::FNodeMatch>& NodeMatchesOut)
{
	// Slightly modified version of GraphDiffControl::DiffGraphs
	// which exposes the NodeMatches to the caller

	bool bFoundDifferences = false;

	if (LhsGraph && RhsGraph)
	{
		NodeMatchesOut.Empty();
		TSet<UEdGraphNode const*> MatchedRhsNodes;

		FGraphDiffControl::FNodeDiffContext AdditiveDiffContext;
		AdditiveDiffContext.NodeTypeDisplayName = LOCTEXT("NodeDiffDisplayName", "Node");

		// march through the all the nodes in the rhs graph and look for matches 
		for (UEdGraphNode* const RhsNode : RhsGraph->Nodes)
		{
			if (RhsNode)
			{
				FGraphDiffControl::FNodeMatch NodeMatch = FGraphDiffControl::FindNodeMatch(LhsGraph, RhsNode, NodeMatchesOut);
				// if we found a corresponding node in the lhs graph, track it (so we
				// can prevent future matches with the same nodes)
				if (NodeMatch.IsValid())
				{
					NodeMatchesOut.Add(NodeMatch);
					MatchedRhsNodes.Add(NodeMatch.OldNode);
				}

				bFoundDifferences |= NodeMatch.Diff(AdditiveDiffContext, &DiffsOut);
			}
		}

		FGraphDiffControl::FNodeDiffContext SubtractiveDiffContext = AdditiveDiffContext;
		SubtractiveDiffContext.DiffMode = FGraphDiffControl::EDiffMode::Subtractive;
		SubtractiveDiffContext.DiffFlags = FGraphDiffControl::EDiffFlags::NodeExistance;

		// go through the lhs nodes to catch ones that may have been missing from the rhs graph
		for (UEdGraphNode* const LhsNode : LhsGraph->Nodes)
		{
			// if this node has already been matched, move on
			if ((LhsNode == nullptr) || MatchedRhsNodes.Find(LhsNode))
			{
				continue;
			}

			FGraphDiffControl::FNodeMatch NodeMatch = FGraphDiffControl::FindNodeMatch(RhsGraph, LhsNode, NodeMatchesOut);
			bFoundDifferences |= NodeMatch.Diff(SubtractiveDiffContext, &DiffsOut);
		}
	}

	// storing the graph name for all diff entries:
	const FName GraphName = LhsGraph ? LhsGraph->GetFName() : RhsGraph->GetFName();
	for( FDiffSingleResult& Entry : DiffsOut )
	{
		Entry.OwningGraph = GraphName;
	}

	return bFoundDifferences;
}

static TArray<TSharedPtr<MergeGraphChange>> GenerateChangeList(const TArray<FDiffSingleResult>& RemoteDifferences, const TArray<FDiffSingleResult>& LocalDifferences)
{
	TMap<const FDiffSingleResult*, const FDiffSingleResult*> ConflictMap;

	// Generate a mapping of all conflicts
	for (const auto& RemoteDiff : RemoteDifferences)
	{
		const FDiffSingleResult* ConflictingDifference = nullptr;

		for (const auto& LocalDiff : LocalDifferences)
		{
			// The conflict detection code is based on the code from SMergeGraphView.cpp
			// However it seems that both are affected by some of the inconsistencies in
			// the FGraphDiffControl::DiffGraphs implementation
			if (RemoteDiff.Node1 == LocalDiff.Node1)
			{
				const bool bIsRemoveDiff = RemoteDiff.Diff == EDiffType::NODE_REMOVED || LocalDiff.Diff == EDiffType::NODE_REMOVED;
				const bool bIsNodeMoveDiff = RemoteDiff.Diff == EDiffType::NODE_MOVED || LocalDiff.Diff == EDiffType::NODE_MOVED;

				// Check if both diffs effect the same pin, note that Pin1 can be set to nullptr
				// in this case the change effects the entire node, which for our purposes is the 
				// same as if they would be effecting the same pin
				const bool bAreEffectingSamePin = RemoteDiff.Pin1 == LocalDiff.Pin1;

				if ((bIsRemoveDiff || bAreEffectingSamePin) && !bIsNodeMoveDiff)
				{
					ConflictingDifference = &LocalDiff;
					break;
				}
			}
			else if (RemoteDiff.Pin1 != nullptr && (RemoteDiff.Pin1 == LocalDiff.Pin1))
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
			const FDiffSingleResult** ConflictingDiff = ConflictMap.Find(&Diff);

			FText Label = !ConflictingDiff ? Diff.DisplayString :
				FText::Format(LOCTEXT("ConflictIdentifier", "CONFLICT: {0} conflicts with {1}"), 
					(*ConflictingDiff)->DisplayString, Diff.DisplayString) ;

			auto NewEntry = TSharedPtr<MergeGraphChange>(new MergeGraphChange());
			NewEntry->Label = Label;
			NewEntry->DisplayColor = ConflictingDiff ? SoftRed : SoftBlue;

			NewEntry->RemoteDiff = Diff;
			NewEntry->LocalDiff = ConflictingDiff ? **ConflictingDiff : FDiffSingleResult{};
			NewEntry->bHasConflicts = ConflictingDiff != nullptr;

			Ret.Push(NewEntry);
		}

		for (const auto& Diff : LocalDifferences)
		{
			const FDiffSingleResult** ConflictingDiff = ConflictMap.Find(&Diff);

			// Since we already handled all the conflicts we can skip them for now
			if (!ConflictingDiff)
			{
				auto NewEntry = TSharedPtr<MergeGraphChange>(new MergeGraphChange());
				NewEntry->Label = Diff.DisplayString;
				NewEntry->DisplayColor = SoftGreen;
	
				NewEntry->RemoteDiff = FDiffSingleResult{};
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
			const EDiffType::Type AType = A->RemoteDiff.Diff ? A->RemoteDiff.Diff : A->LocalDiff.Diff;
			const EDiffType::Type BType = B->RemoteDiff.Diff ? B->RemoteDiff.Diff : B->LocalDiff.Diff;

			return AType < BType;
		}
	};

	Sort(Ret.GetData(), Ret.Num(), SortChangeByDisplayOrder());
	
	return Ret;
}

GraphMergeHelper::GraphMergeHelper(UEdGraph* RemoteGraph, UEdGraph* BaseGraph, UEdGraph* LocalGraph, UEdGraph* TargetGraph)
	: RemoteGraph(RemoteGraph)
	, BaseGraph(BaseGraph)
	, LocalGraph(LocalGraph)
	, TargetGraph(TargetGraph)
	, GraphName(TargetGraph->GetFName())
{
	// Clone the base graph into the target graph
	CloneGraphIntoGraph(BaseGraph, TargetGraph, BaseToTargetNodeMap);

	const auto GenerateDifferences = [](UEdGraph* NewGraph, UEdGraph* OldGraph, TMap<UEdGraphNode*, UEdGraphNode*>& NodeMappingOut)
	{
		TArray<FDiffSingleResult> Results;

		TArray<FGraphDiffControl::FNodeMatch> NodeMatches;
		DiffGraphsExt(OldGraph, NewGraph, Results, NodeMatches);
		
		struct SortDiff
		{
			bool operator()(const FDiffSingleResult& A, const FDiffSingleResult& B) const
			{
				return A.Diff < B.Diff;
			}
		};

		// Sort the results by the EDiffType, this is the order in which the different types 
		// should be presented to the user
		Sort(Results.GetData(), Results.Num(), SortDiff());

		// Convert the node matches into a mapping
		for (auto& NodeMatch : NodeMatches)
		{
			if (!NodeMatch.IsValid()) continue;

			NodeMappingOut.Add(NodeMatch.NewNode, NodeMatch.OldNode);
		}

		return Results;
	};

	TArray<FDiffSingleResult> RemoteDifferences;
	TArray<FDiffSingleResult> LocalDifferences;

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

	// If the node is from either the Base, Local, or Remote graphs, translate
	// it to a node on the base graph
	UEdGraphNode* BaseNode = nullptr;
	if (Node->GetGraph() == BaseGraph)
	{
		BaseNode = Node;		
	}
	else if (Node->GetGraph() == LocalGraph)
	{
		auto** FoundNode = LocalToBaseNodeMap.Find(Node);
		if (FoundNode) BaseNode = *FoundNode;
	}
	else if (Node->GetGraph() == RemoteGraph)
	{
		auto** FoundNode = RemoteToBaseNodeMap.Find(Node);
		if (FoundNode) BaseNode = *FoundNode;
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

bool GraphMergeHelper::ApplyDiff(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	switch (Diff.Diff)
	{
	case EDiffType::NODE_REMOVED:         return ApplyDiff_NODE_REMOVED(Diff, bCanWrite);
	case EDiffType::NODE_ADDED:           return ApplyDiff_NODE_ADDED(Diff, bCanWrite);
	case EDiffType::PIN_LINKEDTO_NUM_DEC: return ApplyDiff_PIN_LINKEDTO_NUM_DEC(Diff, bCanWrite);
	case EDiffType::PIN_LINKEDTO_NUM_INC: return ApplyDiff_PIN_LINKEDTO_NUM_INC(Diff, bCanWrite);
	case EDiffType::PIN_DEFAULT_VALUE:    return ApplyDiff_PIN_DEFAULT_VALUE(Diff, bCanWrite);
	case EDiffType::PIN_LINKEDTO_NODE:    return ApplyDiff_PIN_LINKEDTO_NODE(Diff, bCanWrite);
	case EDiffType::NODE_MOVED:           return ApplyDiff_NODE_MOVED(Diff, bCanWrite);
	case EDiffType::NODE_COMMENT:         return ApplyDiff_NODE_COMMENT(Diff, bCanWrite);
	default: return false;
	}
}

bool GraphMergeHelper::RevertDiff(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	switch (Diff.Diff)
	{
	case EDiffType::NODE_REMOVED:         return RevertDiff_NODE_REMOVED(Diff, bCanWrite);
	case EDiffType::NODE_ADDED:           return RevertDiff_NODE_ADDED(Diff, bCanWrite);
	case EDiffType::PIN_LINKEDTO_NUM_DEC: return RevertDiff_PIN_LINKEDTO_NUM_DEC(Diff, bCanWrite);
	case EDiffType::PIN_LINKEDTO_NUM_INC: return RevertDiff_PIN_LINKEDTO_NUM_INC(Diff, bCanWrite);
	case EDiffType::PIN_DEFAULT_VALUE:    return RevertDiff_PIN_DEFAULT_VALUE(Diff, bCanWrite);
	case EDiffType::PIN_LINKEDTO_NODE:    return RevertDiff_PIN_LINKEDTO_NODE(Diff, bCanWrite);
	case EDiffType::NODE_MOVED:           return RevertDiff_NODE_MOVED(Diff, bCanWrite);
	case EDiffType::NODE_COMMENT:         return RevertDiff_NODE_COMMENT(Diff, bCanWrite);
	default: return false;
	}
}

bool GraphMergeHelper::CloneToTarget(UEdGraphNode* SourceNode, bool bRestoreLinks, bool CanWrite, UEdGraphNode** OutNewNode)
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

/*****************************
 * ApplyDiff implementations *
 ****************************/

bool GraphMergeHelper::ApplyDiff_NODE_REMOVED(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Node1);
		
	if (!TargetNode) return false;		

	if (bCanWrite)
	{
		TargetNode->BreakAllNodeLinks();
		TargetGraph->RemoveNode(TargetNode);
		BaseToTargetNodeMap.Remove(Diff.Node1);
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_NODE_ADDED(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	// Update the mapping to reflect our new node
	UEdGraphNode* NewNode = nullptr;
	bool Ret = CloneToTarget(Diff.Node1, true, bCanWrite, &NewNode);
	if (NewNode && bCanWrite)
	{
		BaseToTargetNodeMap.Add(Diff.Node1, NewNode);
	}

	return Ret;
}

bool GraphMergeHelper::ApplyDiff_PIN_LINKEDTO_NUM_DEC(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Pin1->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.Pin1);

	if (!TargetPin) return false;

	TArray<UEdGraphPin*> LinksToRemove;

	// Find all links to remove, these are all the links from
	// the target pin, which do not exist in Pin2
	for (auto* LinkedPin : TargetPin->LinkedTo)
	{
		bool LinkFoundInPin2 = false;

		for (auto* SourceLink : Diff.Pin2->LinkedTo)
		{
			// We say that pins are the same if they and their parents have the same name
			if (LinkedPin->PinName == SourceLink->PinName
				&& LinkedPin->Direction == SourceLink->Direction
				&& LinkedPin->GetOwningNode()->GetName() == SourceLink->GetOwningNode()->GetName())
			{
				LinkFoundInPin2 = true;
				break;
			}
		}

		if (!LinkFoundInPin2)
		{
			LinksToRemove.Add(LinkedPin);
		}
	}

	if (bCanWrite)
	{
		for (auto* LinkToRemove : LinksToRemove)
		{
			TargetPin->BreakLinkTo(LinkToRemove);
		}	
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_PIN_LINKEDTO_NUM_INC(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Pin1->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.Pin1);

	if (!TargetPin) return false;

	// Add any missing links from the base pin to to target pin
	for (auto* SourceLink : Diff.Pin2->LinkedTo)
	{
		UEdGraphNode* TargetLinkNode = FindNodeInTargetGraph(SourceLink->GetOwningNode());
		UEdGraphPin* TargetLink = SafeFindPin(TargetLinkNode, SourceLink);

		if (!TargetLink) 
		{
			return false;
		}

		if (TargetLink && !TargetPin->LinkedTo.Contains(TargetLink))
		{
			if (bCanWrite)
			{
				TargetPin->MakeLinkTo(TargetLink);	
			}
		}
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_PIN_DEFAULT_VALUE(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Pin1->GetOwningNode());
	UEdGraphPin* TargetPin = TargetNode->FindPin(Diff.Pin1->PinName, Diff.Pin1->Direction);

	if (!TargetNode || !TargetPin) return false;

	if (bCanWrite)
	{
		// Copy all the values to the target pin
		// Only one of these values will be set, however the other 
		// values should be safe to copy, since they are initialized 
		// to zero values
		TargetPin->DefaultValue = Diff.Pin2->DefaultValue;
		TargetPin->DefaultObject = Diff.Pin2->DefaultObject;
		TargetPin->DefaultTextValue = Diff.Pin2->DefaultTextValue;	
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_PIN_LINKEDTO_NODE(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Pin1->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.Pin1);

	if (!TargetNode || !TargetPin) return false;

	// PIN_LINKEDTO_NODE assumes that the numbers of pins didn't change
	// this is a requirement for the diff to be generated. If this
	// doesn't hold true (for example because the user manually added a link
	// in the target graph) we can no longer determine which of the links has 
	// changed
	if(Diff.Pin1->LinkedTo.Num() != Diff.Pin2->LinkedTo.Num()
		|| Diff.Pin1->LinkedTo.Num() != TargetPin->LinkedTo.Num()) return false;

	// NOTE: If we can't find any links that changed we cannot apply them
	// We also only look at the first link that changed, this is 
	// since we do not have enough information to differentiate between the 
	// different links

	// Try to find the links that changed
	for (int i = 0; i < Diff.Pin1->LinkedTo.Num(); ++i)
	{
		UEdGraphPin* Pin1Link = Diff.Pin1->LinkedTo[i];
		UEdGraphPin* Pin2Link = Diff.Pin2->LinkedTo[i];

		// Skip unchanged links
		if (Pin1Link->PinName == Pin2Link->PinName
			&& Pin1Link->GetOwningNode()->GetName() == Pin2Link->GetOwningNode()->GetName())
		{
			continue;
		}

		// We found a pin that changed, now we need to find the matching target pin
		UEdGraphNode* Pin1NodeInTarget = FindNodeInTargetGraph(Pin1Link->GetOwningNode());
		UEdGraphNode* Pin2NodeInTarget = FindNodeInTargetGraph(Pin2Link->GetOwningNode());

		UEdGraphPin* Pin1LinkInTarget = SafeFindPin(Pin1NodeInTarget, Pin1Link);
		UEdGraphPin* Pin2LinkInTarget = SafeFindPin(Pin2NodeInTarget, Pin2Link);

		// Ensure that the link target is found, if any of the link targets
		// could not be found. We can't guarantee that the other matches
		// would be correct
		if (!Pin1LinkInTarget || !Pin2LinkInTarget) return false;

		if (bCanWrite)
		{
			TargetPin->BreakLinkTo(Pin1LinkInTarget);
			TargetPin->MakeLinkTo(Pin2LinkInTarget);
		}

		return true;
	}

	return false;
}

bool GraphMergeHelper::ApplyDiff_NODE_MOVED(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	// !IMPORTANT: For NODE_MOVED and NODE_COMMENT Node2 refers to the node in the base graph
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Node2);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->NodePosX = Diff.Node1->NodePosX;
		TargetNode->NodePosY = Diff.Node1->NodePosY;
	}

	return true;
}

bool GraphMergeHelper::ApplyDiff_NODE_COMMENT(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	// !IMPORTANT: For NODE_MOVED and NODE_COMMENT Node2 refers to the node in the base graph
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Node2);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->NodeComment = Diff.Node1->NodeComment;
	}

	return true;
}

/******************************
 * RevertDiff implementations *
 *****************************/

bool GraphMergeHelper::RevertDiff_NODE_REMOVED(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	// Update the mapping to reflect our new node
	UEdGraphNode* NewNode = nullptr;	
	
	bool Ret = CloneToTarget(Diff.Node1, true, bCanWrite, &NewNode);
	if (NewNode && bCanWrite)
	{
		BaseToTargetNodeMap.Add(Diff.Node1, NewNode);
	}

	return Ret;
}

bool GraphMergeHelper::RevertDiff_NODE_ADDED(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Node1);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->BreakAllNodeLinks();
		TargetGraph->RemoveNode(TargetNode);
		BaseToTargetNodeMap.Remove(Diff.Node1);
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_PIN_LINKEDTO_NUM_DEC(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Pin1->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.Pin1);

	if (!TargetPin) return false;

	// Add any missing links from the base pin to to target pin
	for (auto* BaseLink : Diff.Pin1->LinkedTo)
	{
		UEdGraphNode* TargetLinkNode = GetBaseNodeInTargetGraph(BaseLink->GetOwningNode());
		UEdGraphPin* TargetLink = SafeFindPin(TargetLinkNode, BaseLink);

		if (!TargetLink) 
		{
			return false;
		}

		if (TargetLink && !TargetPin->LinkedTo.Contains(TargetLink))
		{
			if (bCanWrite)
			{
				TargetPin->MakeLinkTo(TargetLink);	
			}
		}
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_PIN_LINKEDTO_NUM_INC(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Pin1->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.Pin1);

	if (!TargetPin) return false;

	TArray<UEdGraphPin*> LinksToRemove;

	// Find all links to remove, these are all the links from
	// the target pin, which do not exist in the base pin
	for (auto* LinkedPin : TargetPin->LinkedTo)
	{
		bool LinkFoundInPin2 = false;

		for (auto* BaseLink : Diff.Pin1->LinkedTo)
		{
			// We say that pins are the same if they and their parents have the same name
			if (LinkedPin->PinName == BaseLink->PinName
				&& LinkedPin->Direction == BaseLink->Direction
				&& LinkedPin->GetOwningNode()->GetName() == BaseLink->GetOwningNode()->GetName())
			{
				LinkFoundInPin2 = true;
				break;
			}
		}

		if (!LinkFoundInPin2)
		{
			LinksToRemove.Add(LinkedPin);
		}
	}

	if (bCanWrite)
	{
		for (auto* LinkToRemove : LinksToRemove)
		{
			TargetPin->BreakLinkTo(LinkToRemove);
		}	
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_PIN_DEFAULT_VALUE(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Pin1->GetOwningNode());
	UEdGraphPin* TargetPin = TargetNode->FindPin(Diff.Pin1->PinName, Diff.Pin1->Direction);

	if (!TargetNode || !TargetPin) return false;

	if (bCanWrite)
	{
		// Copy all the values to the target pin
		// Only one of these values will be set, however the other 
		// values should be safe to copy, since they are initialized 
		// to zero values
		TargetPin->DefaultValue = Diff.Pin1->DefaultValue;
		TargetPin->DefaultObject = Diff.Pin1->DefaultObject;
		TargetPin->DefaultTextValue = Diff.Pin1->DefaultTextValue;	
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_PIN_LINKEDTO_NODE(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Pin1->GetOwningNode());
	UEdGraphPin* TargetPin = SafeFindPin(TargetNode, Diff.Pin1);

	if (!TargetNode || !TargetPin) return false;

	// PIN_LINKEDTO_NODE assumes that the numbers of pins didn't change
	// this is a requirement for the diff to be generated. If this
	// doesn't hold true (for example because the user manually added a link
	// in the target graph) we can no longer determine which of the links has 
	// changed
	if(Diff.Pin1->LinkedTo.Num() != Diff.Pin2->LinkedTo.Num()
		|| Diff.Pin1->LinkedTo.Num() != TargetPin->LinkedTo.Num()) return false;

	// NOTE: If we can't find any links that changed we cannot apply them
	// We also only look at the first link that changed, this is 
	// since we do not have enough information to differentiate between the 
	// different links

	// Try to find the links that changed
	for (int i = 0; i < Diff.Pin1->LinkedTo.Num(); ++i)
	{
		UEdGraphPin* Pin1Link = Diff.Pin1->LinkedTo[i];
		UEdGraphPin* Pin2Link = Diff.Pin2->LinkedTo[i];

		// Skip unchanged links
		if (Pin1Link->PinName == Pin2Link->PinName
			&& Pin1Link->GetOwningNode()->GetName() == Pin2Link->GetOwningNode()->GetName())
		{
			continue;
		}

		// We found a pin that changed, now we need to find the matching target pin
		UEdGraphNode* Pin1NodeInTarget = FindNodeInTargetGraph(Pin1Link->GetOwningNode());
		UEdGraphNode* Pin2NodeInTarget = FindNodeInTargetGraph(Pin2Link->GetOwningNode());

		UEdGraphPin* Pin1LinkInTarget = SafeFindPin(Pin1NodeInTarget, Pin1Link);
		UEdGraphPin* Pin2LinkInTarget = SafeFindPin(Pin2NodeInTarget, Pin2Link);

		// Ensure that the link target is found, if any of the link targets
		// could not be found. We can't guarantee that the other matches
		// would be correct
		if (!Pin1LinkInTarget || !Pin2LinkInTarget) return false;

		if (bCanWrite)
		{
			TargetPin->BreakLinkTo(Pin2LinkInTarget);
			TargetPin->MakeLinkTo(Pin1LinkInTarget);
		}

		return true;
	}

	return false;
}

bool GraphMergeHelper::RevertDiff_NODE_MOVED(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	// !IMPORTANT: For NODE_MOVED and NODE_COMMENT Node2 refers to the node in the base graph
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Node2);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->NodePosX = Diff.Node2->NodePosX;
		TargetNode->NodePosY = Diff.Node2->NodePosY;
	}

	return true;
}

bool GraphMergeHelper::RevertDiff_NODE_COMMENT(const FDiffSingleResult& Diff, const bool bCanWrite)
{
	// !IMPORTANT: For NODE_MOVED and NODE_COMMENT Node2 refers to the node in the base graph
	UEdGraphNode* TargetNode = GetBaseNodeInTargetGraph(Diff.Node2);

	if (!TargetNode) return false;

	if (bCanWrite)
	{
		TargetNode->NodeComment = Diff.Node2->NodeComment;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE