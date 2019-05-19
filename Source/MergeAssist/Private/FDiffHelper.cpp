// Fill out your copyright notice in the Description page of Project Settings.

#include "FDiffHelper.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

#define LOCTEXT_NAMESPACE "DiffHelper"

static void DiffR_NodeRemoved(FMergeDiffResults& Results, UEdGraphNode* NodeRemoved);
static void DiffR_NodeAdded(FMergeDiffResults& Results, UEdGraphNode* NodeAdded);

static void DiffR_PinRemoved(FMergeDiffResults& Results, UEdGraphPin* OldPin);
static void DiffR_PinAdded(FMergeDiffResults& Results, UEdGraphPin* NewPin);

static void DiffR_LinkRemoved(FMergeDiffResults& Results, const FLinkMatch& LinkMatch);
static void DiffR_LinkAdded(FMergeDiffResults& Results, const FLinkMatch& LinkMatch);

static void DiffR_PinDefaultChanged(FMergeDiffResults& Results, UEdGraphPin* OldPin, UEdGraphPin* NewPin);

static void DiffR_NodeMoved(FMergeDiffResults& Results, UEdGraphNode* OldNode, UEdGraphNode* NewNode);
static void DiffR_NodeCommentChanged(FMergeDiffResults& Results, UEdGraphNode* OldNode, UEdGraphNode* NewNode);

template<class MatchType, class ItemType, typename Predicate>
TArray<MatchType> FindItemMatchesByPredicate(
	TArray<ItemType>& OutUnmatchedOldItems,
	TArray<ItemType>& OutUnmatchedNewItems,
	Predicate Pred)
{
	TArray<MatchType> ItemMatches;

	// Go trough all the items in the old items, and try 
	// to match them with an item from the new items
	for (int32 i = 0; i < OutUnmatchedOldItems.Num(); ++i)
	{
		ItemType OldItem = OutUnmatchedOldItems[i];
		ItemType* FoundItem = OutUnmatchedNewItems.FindByPredicate([Pred, OldItem](ItemType NewItem)
		{
			return Pred(OldItem, NewItem);
		});

		if (FoundItem)
		{
			MatchType Match = { OldItem, *FoundItem };
			ItemMatches.Add(Match);

			// Since we matched some items they should no longer be in 
			// the unmatched item lists
			OutUnmatchedOldItems.RemoveSingle(OldItem);
			OutUnmatchedNewItems.RemoveSingle(*FoundItem);

			// Since we just removed the current element in the loop
			// we need to decrement the counter by one. This ensures 
			// that the next element remains consistent
			--i;

			continue;
		}
	}

	return ItemMatches;
}

void FDiffHelper::DiffGraphs(
	UEdGraph* OldGraph, 
	UEdGraph* NewGraph,
	FMergeDiffResults& DiffsOut,
	ENodeMatchStrategy MatchStrategy,
	TArray<FNodeMatch>* NodeMatchesOut,
	TArray<UEdGraphNode*>* UnmatchedOldNodesOut,
	TArray<UEdGraphNode*>* UnmatchedNewNodesOut)
{
	// Ensure that both graphs exist
	if (!OldGraph || !NewGraph) return;
	
	// To start, we mark all nodes at unmatched
	TArray<UEdGraphNode*> UnmatchedOldNodes;
	TArray<UEdGraphNode*> UnmatchedNewNodes;

	TArray<FNodeMatch> NodeMatches = FindNodeMatches(
		OldGraph, NewGraph, MatchStrategy,
		&UnmatchedOldNodes, &UnmatchedNewNodes
	);

	// Diff all the matched nodes
	for (auto Match : NodeMatches)
	{
		DiffNodes(Match.OldNode, Match.NewNode, DiffsOut);
	}

	// We also need to diff all the unmatched nodes
	// this is to generate NODE_ADDED and NODE_REMOVED diffs	
	for (auto* UnmatchedOldNode : UnmatchedOldNodes)
	{
		DiffNodes(UnmatchedOldNode, nullptr, DiffsOut);	
	}

	for (auto* UnmatchedNewNode : UnmatchedNewNodes)
	{
		DiffNodes(nullptr, UnmatchedNewNode, DiffsOut);
	}

	// Output the output values if they are requested
	if (NodeMatchesOut)       *NodeMatchesOut       = NodeMatches;
	if (UnmatchedOldNodesOut) *UnmatchedOldNodesOut = UnmatchedOldNodes;
	if (UnmatchedNewNodesOut) *UnmatchedNewNodesOut = UnmatchedNewNodes;
}

void FDiffHelper::DiffNodes(
	UEdGraphNode* OldNode, 
	UEdGraphNode* NewNode, 
	FMergeDiffResults& DiffsOut)
{
	// Ensure that at least one of the nodes is passed in
	if (!OldNode && !NewNode) return;

	if (NewNode == nullptr)
	{
		DiffR_NodeRemoved(DiffsOut, OldNode);
		return;
	}

	if (OldNode == nullptr)
	{
		DiffR_NodeAdded(DiffsOut, NewNode);
		return;
	}
	
	if (NewNode->NodeComment != OldNode->NodeComment)
	{
		DiffR_NodeCommentChanged(DiffsOut, OldNode, NewNode);
	}

	if (NewNode->NodePosX != OldNode->NodePosX || NewNode->NodePosY != OldNode->NodePosY)
	{
		DiffR_NodeMoved(DiffsOut, OldNode, NewNode);
	}

	{
		TArray<UEdGraphPin*> UnmatchedOldPins;
		TArray<UEdGraphPin*> UnmatchedNewPins;
		TArray<FPinMatch> PinMatches = FindPinMatches(OldNode, NewNode, &UnmatchedOldPins, &UnmatchedNewPins);

		// Generate invalid pin matches for all the unmatched pins
		// this is to generate PIN_ADDED and PIN_REMOVED diffs	
		for (auto UnmatchedOldPin : UnmatchedOldPins)
		{
			FPinMatch InvalidMatch = {};
			InvalidMatch.OldPin = UnmatchedOldPin;
			PinMatches.Add(InvalidMatch);
		}

		for (auto UnmatchedNewPin : UnmatchedNewPins)
		{
			FPinMatch InvalidMatch = {};
			InvalidMatch.NewPin = UnmatchedNewPin;
			PinMatches.Add(InvalidMatch);
		}

		for (auto PinMatch : PinMatches)
		{
			DiffPins(PinMatch.OldPin, PinMatch.NewPin, DiffsOut);
		}
	}

	// Node internal
	{
		// @TODO: Implement and check what can be done with the results of this change
		// since they are different for every note. There doesn't seem to be much that 
		// can be done with them
		//OldNode->FindDiffs(NewNode, DiffsOut);	

		// Since this uses the default Diff types we will need to translate
	}
}

void FDiffHelper::DiffPins(
	UEdGraphPin* OldPin, 
	UEdGraphPin* NewPin,
	FMergeDiffResults& DiffsOut)
{
	// Ensure that at least one pin is passed in
	if (!OldPin && !NewPin) return;

	if (NewPin == nullptr)
	{
		DiffR_PinRemoved(DiffsOut, OldPin);
		return;
	}

	if (OldPin == nullptr)
	{
		DiffR_PinAdded(DiffsOut, NewPin);
		return;
	}

	// @TODO: Implement PinType support
	// this is skipped for now since we don't support it in the merge tool

	const bool DefaultValueChanged = OldPin->DefaultObject != NewPin->DefaultObject
		|| !OldPin->DefaultTextValue.EqualTo(NewPin->DefaultTextValue)
		|| OldPin->DefaultValue != NewPin->DefaultValue;

	// We only care if the default value changed if the new pin doesn't have any links
	// this is because of the new pin has links the default value is hidden from the user
	if (NewPin->LinkedTo.Num() == 0 && DefaultValueChanged)
	{
		DiffR_PinDefaultChanged(DiffsOut, OldPin, NewPin);
	}

	{
		TArray<FGraphLink> UnmatchedOldLinks;
		TArray<FGraphLink> UnmatchedNewLinks;
		TArray<FLinkMatch> LinkMatches = FindLinkMatches(OldPin, NewPin, &UnmatchedOldLinks, &UnmatchedNewLinks);
		
		// Generate invalid link matches for all the unmatched pins
		// this is to generate PIN_ADDED and PIN_REMOVED diffs	
		for (auto UnmatchedOldLink : UnmatchedOldLinks)
		{
			FLinkMatch InvalidMatch = {};
			InvalidMatch.OldLink = UnmatchedOldLink;
			InvalidMatch.NewLink.SourcePin = NewPin;
			LinkMatches.Add(InvalidMatch);
		}

		for (auto UnmatchedNewLink : UnmatchedNewLinks)
		{
			FLinkMatch InvalidMatch = {};
			InvalidMatch.OldLink.SourcePin = OldPin;
			InvalidMatch.NewLink = UnmatchedNewLink;
			LinkMatches.Add(InvalidMatch);
		}
		
		for (auto LinkMatch : LinkMatches)
		{
			DiffLinks(LinkMatch.OldLink, LinkMatch.NewLink, DiffsOut);
		}
	}
}

void FDiffHelper::DiffLinks(
	const FGraphLink& OldLink,
	const FGraphLink& NewLink, 
	FMergeDiffResults& DiffsOut)
{
	// ensure that at least one target got passed in
	if (!OldLink.TargetPin && !NewLink.TargetPin) return;

	if (NewLink.TargetPin == nullptr)
	{
		DiffR_LinkRemoved(DiffsOut, FLinkMatch{OldLink, NewLink});
		return;
	}

	if (OldLink.TargetPin == nullptr)
	{
		DiffR_LinkAdded(DiffsOut, FLinkMatch{OldLink, NewLink});
		return;
	}
}

bool FDiffHelper::IsExactNodeMatch(const UEdGraphNode* OldNode, const UEdGraphNode* NewNode)
{
	// Nodes with different classes (types) can never be a match
	if (OldNode->GetClass() != NewNode->GetClass()) return false;

	// nodes with the same GUID are a match
	if (NewNode->NodeGuid == OldNode->NodeGuid) return true;

	// we could be diffing two completely separate assets, this makes sure both 
	// nodes historically belong to the same graph
	const bool bIsIntraAssetDiff = (NewNode->GetGraph()->GraphGuid == OldNode->GetGraph()->GraphGuid);

	// If both nodes belong to the same graph, and have the same FName we know 
	// that they are the same node 
	return bIsIntraAssetDiff && NewNode->GetFName() == OldNode->GetFName();
}

TArray<FNodeMatch> FDiffHelper::FindNodeMatches(
		UEdGraph* OldGraph,
		UEdGraph* NewGraph,
		ENodeMatchStrategy MatchStrategy,
		TArray<UEdGraphNode*>* OutUnmatchedOldNodes,
		TArray<UEdGraphNode*>* OutUnmatchedNewNodes)
{
	TArray<UEdGraphNode*> UnmatchedOldNodes = OldGraph->Nodes;
	TArray<UEdGraphNode*> UnmatchedNewNodes = NewGraph->Nodes;

	TArray<FNodeMatch> NodeMatches;

	// Call the different match algorithms based on the strategy
	if (IsFlagSet(MatchStrategy, ENodeMatchStrategy::EXACT))
	{
		NodeMatches.Append(FindExactNodeMatches(UnmatchedOldNodes, UnmatchedNewNodes));
	}

	if (IsFlagSet(MatchStrategy, ENodeMatchStrategy::APPROXIMATE))
	{
		NodeMatches.Append(FindApproximateNodeMatches(UnmatchedOldNodes, UnmatchedNewNodes));
	}

	// Output the output values if they are requested
	if (OutUnmatchedOldNodes) *OutUnmatchedOldNodes = UnmatchedOldNodes;
	if (OutUnmatchedNewNodes) *OutUnmatchedNewNodes = UnmatchedNewNodes;
	
	return NodeMatches;
}

TArray<FPinMatch> FDiffHelper::FindPinMatches(
		UEdGraphNode* OldNode,
		UEdGraphNode* NewNode,
		TArray<UEdGraphPin*>* OutUnmatchedOldPins,
		TArray<UEdGraphPin*>* OutUnmatchedNewPins)
{
	const auto IsVisiblePredicate = [](UEdGraphPin* Pin)
	{
		return Pin && !Pin->bHidden;
	}; 
	
	// Gather all the visible pins
	auto UnmatchedOldPins = OldNode->Pins.FilterByPredicate(IsVisiblePredicate);
	auto UnmatchedNewPins = NewNode->Pins.FilterByPredicate(IsVisiblePredicate);

	auto PinMatches = FindItemMatchesByPredicate<FPinMatch>(UnmatchedOldPins, UnmatchedNewPins, 
		[](	UEdGraphPin* OldPin, UEdGraphPin* NewPin)
		{
			return OldPin && NewPin && OldPin->PinName == NewPin->PinName;
		});
	
	// Output the output values if they are requested
	if (OutUnmatchedOldPins) *OutUnmatchedOldPins = UnmatchedOldPins;
	if (OutUnmatchedNewPins) *OutUnmatchedNewPins = UnmatchedNewPins;

	return PinMatches;
}

TArray<FLinkMatch> FDiffHelper::FindLinkMatches(
		UEdGraphPin* OldPin,
		UEdGraphPin* NewPin,
		TArray<FGraphLink>* OutUnmatchedOldLinks,
		TArray<FGraphLink>* OutUnmatchedNewLinks)
{
	const auto GetAllGraphLinks = [](UEdGraphPin* Pin)
	{
		TArray<FGraphLink> Ret;
		for (auto* Target : Pin->LinkedTo)
		{
			Ret.Add(FGraphLink{Pin, Target});
		}
		return Ret;
	};

	auto UnmatchedOldLinks = GetAllGraphLinks(OldPin);
	auto UnmatchedNewLinks = GetAllGraphLinks(NewPin);

	auto LinkMatches = FindItemMatchesByPredicate<FLinkMatch>(UnmatchedOldLinks, UnmatchedNewLinks,
		[](const FGraphLink& OldLink, const FGraphLink& NewLink)
		{
			// If the target have the same name, direction, and owner
			// then we are convinced they are the same target
			return OldLink.TargetPin->Direction == NewLink.TargetPin->Direction
				&& OldLink.TargetPin->PinName == NewLink.TargetPin->PinName
				&& WeakNodeMatch(OldLink.TargetPin->GetOwningNode(), NewLink.TargetPin->GetOwningNode());
		});

	// Output the output values if they are requested
	if (OutUnmatchedOldLinks) *OutUnmatchedOldLinks = UnmatchedOldLinks;
	if (OutUnmatchedNewLinks) *OutUnmatchedNewLinks = UnmatchedNewLinks;

	return LinkMatches;
}

TArray<FNodeMatch> FDiffHelper::FindExactNodeMatches(TArray<UEdGraphNode*>& UnmatchedOldNodes, TArray<UEdGraphNode*>& UnmatchedNewNodes)
{
	return FindItemMatchesByPredicate<FNodeMatch>(UnmatchedOldNodes, UnmatchedNewNodes, 
		[](	UEdGraphNode* OldNode, UEdGraphNode* NewNode)
		{
			return OldNode && NewNode && IsExactNodeMatch(OldNode, NewNode);
		});
}

TArray<FNodeMatch> FDiffHelper::FindApproximateNodeMatches(TArray<UEdGraphNode*>& UnmatchedOldNodes, TArray<UEdGraphNode*>& UnmatchedNewNodes)
{
	const auto CompareNodeType = [](UEdGraphNode& NodeA, UEdGraphNode& NodeB)
	{
		const auto TitleA = NodeA.GetNodeTitle(ENodeTitleType::FullTitle);
		const auto TitleB = NodeB.GetNodeTitle(ENodeTitleType::FullTitle);

		return NodeA.GetClass() != NodeB.GetClass()
			? NodeA.GetClass() < NodeB.GetClass()
			: TitleA.CompareTo(TitleB) < 0;
	};

	// We presort all the unmatched nodes, so we can efficiently process them by type
	Sort(UnmatchedOldNodes.GetData(), UnmatchedOldNodes.Num(), CompareNodeType);
	Sort(UnmatchedNewNodes.GetData(), UnmatchedNewNodes.Num(), CompareNodeType);

	TArray<FNodeMatch> Matches;

	int32 TypeIndicatorOffset = 0;
	while (TypeIndicatorOffset < UnmatchedOldNodes.Num())
	{
		// We pick a node from the old nodes to use as our type indicator
		auto* NodeType = UnmatchedOldNodes[TypeIndicatorOffset];

		const auto IsSameNodeType = [NodeType](UEdGraphNode* Node)
		{
			const auto TitleA = NodeType->GetNodeTitle(ENodeTitleType::FullTitle);
			const auto TitleB = Node->GetNodeTitle(ENodeTitleType::FullTitle);

			return NodeType->GetClass() == Node->GetClass() && TitleA.EqualTo(TitleB);
		};

		// Determine the range of old nodes we are looking at since the type was 
		// sourced from this array, we know we have at least 1 node of this type
		const auto IndexOldNodesFirst = UnmatchedOldNodes.IndexOfByPredicate(IsSameNodeType);
		const auto IndexOldNodesLast = UnmatchedOldNodes.FindLastByPredicate(IsSameNodeType);		
		const auto NumOldNodes = 1 + (IndexOldNodesLast - IndexOldNodesFirst); 

		const auto IndexNewNodesFirst = UnmatchedNewNodes.IndexOfByPredicate(IsSameNodeType);
		const auto IndexNewNodesLast = UnmatchedNewNodes.FindLastByPredicate(IsSameNodeType);		
		const auto NumNewNodes = 1 + (IndexNewNodesLast - IndexNewNodesFirst); 

		// We always need to increment our type indicator offset
		TypeIndicatorOffset += NumOldNodes;

		// We only need to check if the values for the new nodes are valid
		// since it's guaranteed that the values for the old nodes are
		if (IndexNewNodesFirst == INDEX_NONE || IndexNewNodesLast == INDEX_NONE) continue;

		auto OldNodesView = TArrayView<UEdGraphNode*>(&UnmatchedOldNodes[IndexOldNodesFirst], NumOldNodes);
		auto NewNodesView = TArrayView<UEdGraphNode*>(&UnmatchedNewNodes[IndexNewNodesFirst], NumNewNodes);

		auto SubMatches = FindApproximateNodeMatchesBetweenNodesOfTheSameType(OldNodesView, NewNodesView);
		Matches.Append(SubMatches);		
	}

	// Remove all nodes we managed to match from the unmatched nodes
	for (auto Match : Matches)
	{
		UnmatchedOldNodes.Remove(Match.OldNode);
		UnmatchedNewNodes.Remove(Match.NewNode);
	}
	
	return Matches;
}

TArray<FNodeMatch> FDiffHelper::FindApproximateNodeMatchesBetweenNodesOfTheSameType(
	const TArrayView<UEdGraphNode*>& UnmatchedOldNodesOfType, 
	const TArrayView<UEdGraphNode*>& UnmatchedNewNodesOfType)
{
	struct PotentialNodeMatch
	{
		UEdGraphNode* OldNode;
		UEdGraphNode* NewNode;
		int32 DiffCount;
	};

	// Generate all potential matches, and assign them a weight based on the number of diffs
	TArray<PotentialNodeMatch> PotentialMatches;
	for (auto OldNode : UnmatchedOldNodesOfType)
	{
		for (auto NewNode : UnmatchedNewNodesOfType)
		{
			FMergeDiffResults Results = FMergeDiffResults();
			DiffNodes(OldNode, NewNode, Results);

			PotentialMatches.Add(
				PotentialNodeMatch
				{
					OldNode, NewNode, Results.NumFound()
				}
			);
		}
	}

	// Sort the potential matches based on the lowest DiffCount, this ensures 
	// that when looping, the first match we encounter is the best match
	PotentialMatches.Sort([](const PotentialNodeMatch& MatchA, const PotentialNodeMatch& MatchB)
	{
		return MatchA.DiffCount < MatchB.DiffCount;
	});

	// Try and find the best matches
	TArray<FNodeMatch> NodeMatches;
	while(PotentialMatches.Num())
	{
		auto& BestMatch = PotentialMatches[0];

		FNodeMatch Match = {};
		Match.OldNode = BestMatch.OldNode;
		Match.NewNode = BestMatch.NewNode;
		NodeMatches.Add(Match);

		// Remove all matches which have overlap with our best match
		PotentialMatches.RemoveAll([BestMatch](PotentialNodeMatch& PotentialMatch)
		{
			return BestMatch.OldNode == PotentialMatch.OldNode
				|| BestMatch.NewNode == PotentialMatch.NewNode;
		});
	}

	return NodeMatches;
}

bool FDiffHelper::WeakNodeMatch(UEdGraphNode* OldNode, UEdGraphNode* NewNode)
{
	if (IsExactNodeMatch(OldNode, NewNode)) return true;

	const auto TitleA = OldNode->GetNodeTitle(ENodeTitleType::FullTitle);
	const auto TitleB = NewNode->GetNodeTitle(ENodeTitleType::FullTitle);

	return OldNode->GetClass() == NewNode->GetClass() && TitleA.EqualTo(TitleB);
}

/*******************************************************************************
* Static helper function implementations
*******************************************************************************/

void DiffR_NodeRemoved(FMergeDiffResults& Results, UEdGraphNode* NodeRemoved)
{
	FMergeDiffResult Diff = {};
	Diff.Type    = EMergeDiffType::NODE_REMOVED;
	Diff.NodeOld = NodeRemoved;
	
	// Only generate the display data if it will be stored
	if (Results.CanStoreResults())
	{
		// @TODO: Generate proper display data
		Diff.DisplayString = FText::FromString("NodeRemoved");
		Diff.DisplayColor = FLinearColor::White;
		Diff.ToolTip = FText();	
	}

	Results.Add(Diff);
}

void DiffR_NodeAdded(FMergeDiffResults& Results, UEdGraphNode* NodeAdded)
{
	// @TODO: Implement
	FMergeDiffResult Diff = {};
	Diff.Type    = EMergeDiffType::NODE_ADDED;
	Diff.NodeNew = NodeAdded;
	
	// Only generate the display data if it will be stored
	if (Results.CanStoreResults())
	{
		// @TODO: Generate proper display data
		Diff.DisplayString = FText::FromString("NodeAdded");
		Diff.DisplayColor = FLinearColor::White;
		Diff.ToolTip = FText();	
	}

	Results.Add(Diff);
}

void DiffR_PinRemoved(FMergeDiffResults& Results, UEdGraphPin* OldPin)
{
	FMergeDiffResult Diff = {};
	Diff.Type   = EMergeDiffType::PIN_REMOVED;
	Diff.PinOld = OldPin;

	// Only generate the display data if it will be stored
	if (Results.CanStoreResults())
	{
		// @TODO: Generate proper display data
		Diff.DisplayString = FText::FromString("PinRemoved");
		Diff.DisplayColor = FLinearColor::White;
		Diff.ToolTip = FText();	
	}

	Results.Add(Diff);
}

void DiffR_PinAdded(FMergeDiffResults& Results, UEdGraphPin* NewPin)
{
	FMergeDiffResult Diff = {};
	Diff.Type   = EMergeDiffType::PIN_ADDED;
	Diff.PinNew = NewPin;

	// Only generate the display data if it will be stored
	if (Results.CanStoreResults())
	{
		// @TODO: Generate proper display data
		Diff.DisplayString = FText::FromString("PinAdded");
		Diff.DisplayColor = FLinearColor::White;
		Diff.ToolTip = FText();	
	}

	Results.Add(Diff);
}

void DiffR_LinkRemoved(FMergeDiffResults& Results, const FLinkMatch& LinkMatch)
{
	FMergeDiffResult Diff = {};
	Diff.Type          = EMergeDiffType::LINK_REMOVED;
	Diff.PinOld        = LinkMatch.OldLink.SourcePin;
	Diff.PinNew        = LinkMatch.NewLink.SourcePin;
	Diff.LinkTargetOld = LinkMatch.OldLink.TargetPin;
	Diff.LinkTargetNew = LinkMatch.NewLink.TargetPin;

	// Only generate the display data if it will be stored
	if (Results.CanStoreResults())
	{
		// @TODO: Generate proper display data
		Diff.DisplayString = FText::FromString("LinkRemoved");
		Diff.DisplayColor = FLinearColor::White;
		Diff.ToolTip = FText();	
	}

	Results.Add(Diff);
}

void DiffR_LinkAdded(FMergeDiffResults& Results, const FLinkMatch& LinkMatch)
{
	FMergeDiffResult Diff = {};
	Diff.Type          = EMergeDiffType::LINK_ADDED;
	Diff.PinOld        = LinkMatch.OldLink.SourcePin;
	Diff.PinNew        = LinkMatch.NewLink.SourcePin;
	Diff.LinkTargetOld = LinkMatch.OldLink.TargetPin;
	Diff.LinkTargetNew = LinkMatch.NewLink.TargetPin;

	// Only generate the display data if it will be stored
	if (Results.CanStoreResults())
	{
		// @TODO: Generate proper display data
		Diff.DisplayString = FText::FromString("LinkAdded");
		Diff.DisplayColor = FLinearColor::White;
		Diff.ToolTip = FText();	
	}

	Results.Add(Diff);
}

void DiffR_PinDefaultChanged(FMergeDiffResults& Results, UEdGraphPin* OldPin, UEdGraphPin* NewPin)
{
	FMergeDiffResult Diff = {};
	Diff.Type          = EMergeDiffType::PIN_DEFAULT_VALUE_CHANGED;
	Diff.PinOld        = OldPin;
	Diff.PinNew        = NewPin;

	// Only generate the display data if it will be stored
	if (Results.CanStoreResults())
	{
		// @TODO: Generate proper display data
		Diff.DisplayString = FText::FromString("PinDefaultValue");
		Diff.DisplayColor = FLinearColor::White;
		Diff.ToolTip = FText();	
	}

	Results.Add(Diff);
}

void DiffR_NodeMoved(FMergeDiffResults& Results, UEdGraphNode* OldNode, UEdGraphNode* NewNode)
{
	FMergeDiffResult Diff = {};
	Diff.Type    = EMergeDiffType::NODE_MOVED;
	Diff.NodeOld = OldNode;
	Diff.NodeNew = NewNode;

	// Only generate the display data if it will be stored
	if (Results.CanStoreResults())
	{
		// @TODO: Generate proper display data
		Diff.DisplayString = FText::FromString("NodeMoved");
		Diff.DisplayColor = FLinearColor::White;
		Diff.ToolTip = FText();	
	}

	Results.Add(Diff);
}

void DiffR_NodeCommentChanged(FMergeDiffResults& Results, UEdGraphNode* OldNode, UEdGraphNode* NewNode)
{
	FMergeDiffResult Diff = {};
	Diff.Type    = EMergeDiffType::NODE_COMMENT_CHANGED;
	Diff.NodeOld = OldNode;
	Diff.NodeNew = NewNode;

	// Only generate the display data if it will be stored
	if (Results.CanStoreResults())
	{
		// @TODO: Generate proper display data
		Diff.DisplayString = FText::FromString("NodeCommentChanged");
		Diff.DisplayColor = FLinearColor::White;
		Diff.ToolTip = FText();	
	}

	Results.Add(Diff);
}

#undef LOCTEXT_NAMESPACE