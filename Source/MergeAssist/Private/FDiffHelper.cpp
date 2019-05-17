// Fill out your copyright notice in the Description page of Project Settings.

#include "FDiffHelper.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

#define LOCTEXT_NAMESPACE "DiffHelper"

static void DiffR_NodeRemoved(FMergeDiffResults& Results, UEdGraphNode* NodeRemoved);
static void DiffR_NodeAdded(FMergeDiffResults& Results, UEdGraphNode* NodeAdded);

static void DiffR_PinRemoved(FMergeDiffResults& Results, UEdGraphPin* OldPin);
static void DiffR_PinAdded(FMergeDiffResults& Results, UEdGraphPin* NewPin);

static void DiffR_LinkRemoved(FMergeDiffResults& Results, const LinkMatch& LinkMatch);
static void DiffR_LinkAdded(FMergeDiffResults& Results, const LinkMatch& LinkMatch);

static void DiffR_PinDefaultChanged(FMergeDiffResults& Results, UEdGraphPin* OldPin, UEdGraphPin* NewPin);

static void DiffR_NodeMoved(FMergeDiffResults& Results, UEdGraphNode* OldNode, UEdGraphNode* NewNode);
static void DiffR_NodeCommentChanged(FMergeDiffResults& Results, UEdGraphNode* OldNode, UEdGraphNode* NewNode);

static TArray<PinMatch> GeneratePinMatches(UEdGraphNode* OldNode, UEdGraphNode* NewNode);
static TArray<LinkMatch> GenerateLinkMatches(UEdGraphPin* OldPin, UEdGraphPin* NewPin);

void FDiffHelper::DiffNodes(
	UEdGraphNode* OldNode, 
	UEdGraphNode* NewNode, 
	FMergeDiffResults& DiffsOut)
{
	using EDiffFlags = FGraphDiffControl::EDiffFlags;

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
		TArray<PinMatch> PinMatches = GeneratePinMatches(OldNode, NewNode);
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
	const auto InitialDiffCount = DiffsOut.Num();

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
		TArray<LinkMatch> LinkMatches = GenerateLinkMatches(OldPin, NewPin);
		for (auto LinkMatch : LinkMatches)
		{
			DiffLink(LinkMatch, DiffsOut);
		}
	}
}

void FDiffHelper::DiffLink(
	LinkMatch LinkMatch, 
	FMergeDiffResults& DiffsOut)
{
	// ensure that at least one target got passed in
	if (!LinkMatch.OldLinkTarget && !LinkMatch.NewLinkTarget) return;
	const auto InitialDiffCount = DiffsOut.Num();

	if (LinkMatch.NewLinkTarget == nullptr)
	{
		DiffR_LinkRemoved(DiffsOut, LinkMatch);
		return;
	}

	if (LinkMatch.OldLinkTarget == nullptr)
	{
		DiffR_LinkAdded(DiffsOut, LinkMatch);
		return;
	}
}

bool FDiffHelper::BetterNodeMatch(UEdGraphNode* OldNode, UEdGraphNode* NewNode)
{
	// @TODO: Actually implement this
	return FGraphDiffControl::IsNodeMatch(OldNode, NewNode);
}

/*******************************************************************************
* Static helper function implementations
*******************************************************************************/

void DiffR_NodeRemoved(FMergeDiffResults& Results, UEdGraphNode* NodeRemoved)
{
	FMergeDiffResult Diff = {};
	Diff.Type    = MergeDiffType::NODE_REMOVED;
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
	Diff.Type    = MergeDiffType::NODE_ADDED;
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
	Diff.Type   = MergeDiffType::PIN_REMOVED;
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
	Diff.Type   = MergeDiffType::PIN_ADDED;
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

void DiffR_LinkRemoved(FMergeDiffResults& Results, const LinkMatch& LinkMatch)
{
	FMergeDiffResult Diff = {};
	Diff.Type          = MergeDiffType::LINK_REMOVED;
	Diff.PinOld        = LinkMatch.OldLinkSource;
	Diff.PinNew        = LinkMatch.NewLinkSource;
	Diff.LinkTargetOld = LinkMatch.OldLinkTarget;
	Diff.LinkTargetNew = LinkMatch.NewLinkTarget;

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

void DiffR_LinkAdded(FMergeDiffResults& Results, const LinkMatch& LinkMatch)
{
	FMergeDiffResult Diff = {};
	Diff.Type          = MergeDiffType::LINK_ADDED;
	Diff.PinOld        = LinkMatch.OldLinkSource;
	Diff.PinNew        = LinkMatch.NewLinkSource;
	Diff.LinkTargetOld = LinkMatch.OldLinkTarget;
	Diff.LinkTargetNew = LinkMatch.NewLinkTarget;

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
	Diff.Type          = MergeDiffType::PIN_DEFAULT_VALUE_CHANGED;
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
	Diff.Type    = MergeDiffType::NODE_MOVED;
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
	Diff.Type    = MergeDiffType::NODE_COMMENT_CHANGED;
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

TArray<PinMatch> GeneratePinMatches(UEdGraphNode* OldNode, UEdGraphNode* NewNode)
{
	const auto IsVisiblePredicate = [](UEdGraphPin* Pin)
	{
		return Pin && !Pin->bHidden;
	}; 

	// Gather all the visible pins
	auto OldPins = OldNode->Pins.FilterByPredicate(IsVisiblePredicate);
	auto NewPins = NewNode->Pins.FilterByPredicate(IsVisiblePredicate);

	// We try and create matching pairs for the pins based on their names
	// if we cannot find a pin with the same name we assume
	// that the pin was either added or removed, this is indicated 
	// by leaving the other pin set to a nullptr
	TArray<PinMatch> PinMatches;
	for (auto OldPin : OldPins)
	{
		PinMatch Match = {};
		Match.OldPin = OldPin;

		UEdGraphPin** FoundPin = NewPins.FindByPredicate([OldPin](UEdGraphPin* NewPin)
		{
			return OldPin->PinName == NewPin->PinName;
		});

		if (FoundPin)
		{
			Match.NewPin = *FoundPin;

			// We remove the matched pin from the list of new pins 
			// to ensure we never match the same pin twice
			NewPins.Remove(Match.NewPin);
		}

		PinMatches.Add(Match);
	}

	// Add all leftover pins in the new node
	for (auto NewPin : NewPins)
	{
		PinMatch Match = {};
		Match.NewPin = NewPin;
		PinMatches.Add(Match);
	}

	return PinMatches;
}

TArray<LinkMatch> GenerateLinkMatches(UEdGraphPin* OldPin, UEdGraphPin* NewPin)
{
	auto OldLinkTargets = OldPin->LinkedTo;
	auto NewLinkTargets = NewPin->LinkedTo;

	// We try and create matching pairs of the links
	TArray<LinkMatch> LinkMatches;
	for (auto OldLinkTarget : OldLinkTargets)
	{
		LinkMatch Match = {};
		Match.OldLinkSource = OldPin;
		Match.OldLinkTarget = OldLinkTarget;
		Match.NewLinkSource = NewPin;

		// Try and find the matching pin
		UEdGraphPin** FoundPin = NewLinkTargets.FindByPredicate([OldLinkTarget](UEdGraphPin* NewLinkTarget)
		{
			// If the target have the same name, direction, and owner
			// then we are convinced they are the same target
			return OldLinkTarget->Direction == NewLinkTarget->Direction
				&& OldLinkTarget->PinName == NewLinkTarget->PinName
				&& FDiffHelper::BetterNodeMatch(OldLinkTarget->GetOwningNode(), NewLinkTarget->GetOwningNode());
		});

		if (FoundPin)
		{
			Match.NewLinkTarget = *FoundPin;

			// We remove the matched target from the list of new targets
			// this is to ensure we never match the same target twice
			NewLinkTargets.Remove(Match.NewLinkTarget);
		}

		LinkMatches.Add(Match);
	}


	// @TODO: Reconsider if we need this data
	// Add all the leftover target in the new pin
	for (auto NewLinkTarget : NewLinkTargets)
	{
		LinkMatch Match = {};
		Match.OldLinkSource = OldPin;
		Match.NewLinkSource = NewPin;
		Match.NewLinkTarget = NewLinkTarget;

		LinkMatches.Add(Match);
	}

	return LinkMatches;
}

#undef LOCTEXT_NAMESPACE