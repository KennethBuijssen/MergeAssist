// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GraphDiffControl.h"

class UEdGraphNode;

enum struct MergeDiffType
{
	NO_DIFFERENCE,
	
	NODE_REMOVED,
	NODE_ADDED,

	PIN_REMOVED,
	PIN_ADDED,
	
	LINK_REMOVED,
	LINK_ADDED,
	
	// Current expressed as a pair of LINK_ADDED and LINK_REMOVED
	// this is done to give higher resolution, since we would need to guess
	// which of these pairs the user would assume to be the same
	// /*PIN_*/LINK_CHANGED

	PIN_DEFAULT_VALUE_CHANGED,	

	//PIN_TYPE_CATEGORY,
	//PIN_TYPE_SUBCATEGORY,
	//PIN_TYPE_SUBCATEGORY_OBJECT,
	//PIN_TYPE_IS_ARRAY,
	//PIN_TYPE_IS_REF,

	NODE_MOVED,
	NODE_COMMENT_CHANGED,

	// Currently only used when internal properties changed
	// we can't resolve them, but would be nice to show when this is the case
	// used by UAIGraphNode, and UK2Node_MathExpression
	//NODE_PROPERTY -> NODE_INTERNAL_CHANGE
};

struct PinMatch
{
	UEdGraphPin* OldPin;
	UEdGraphPin* NewPin;
};

struct LinkMatch
{
	UEdGraphPin* OldLinkSource;
	UEdGraphPin* OldLinkTarget;

	UEdGraphPin* NewLinkSource;
	UEdGraphPin* NewLinkTarget;
};

struct FMergeDiffResult
{
	// Type of the diff
	MergeDiffType Type;

	// Node data
	UEdGraphNode* NodeOld;
	UEdGraphNode* NodeNew;

	// Pin data
	UEdGraphPin* PinOld;
	UEdGraphPin* PinNew;

	// Link data
	UEdGraphPin* LinkTargetOld;
	UEdGraphPin* LinkTargetNew;

	// Display data
	FText DisplayString;
	FText ToolTip;
	FLinearColor DisplayColor;
};

class FMergeDiffResults
{
public:
	FMergeDiffResults(TArray<FMergeDiffResult>* ResultsOut = nullptr)
		: ResultArray(ResultsOut)
		, bHasFoundDiffs(false) 
	{}

	void Add(const FMergeDiffResult& Result)
	{
		if (Result.Type == MergeDiffType::NO_DIFFERENCE) return;

		bHasFoundDiffs = true;

		if (ResultArray) ResultArray->Add(Result);
	}

	bool CanStoreResults() const { return ResultArray != nullptr; }

	int32 Num() const { return ResultArray ? ResultArray->Num() : 0; }
	bool HasFoundDiffs() const { return bHasFoundDiffs; }

private:
	TArray<FMergeDiffResult>* ResultArray;
	bool bHasFoundDiffs;
};

/**
 * 
 */
struct FDiffHelper
{
	// !IMPORTANT: If using FNodeMatch data, normalize the Additive and Subtractive states before
	// calling this
	static void DiffNodes(
		UEdGraphNode* OldNode, 
		UEdGraphNode* NewNode, 
		FMergeDiffResults& DiffsOut);

	static void DiffPins(
		UEdGraphPin* OldPin,
		UEdGraphPin* NewPin,
		FMergeDiffResults& DiffsOut);

	static void DiffLink(
		LinkMatch LinkMatch,
		FMergeDiffResults& DiffsOut);

	// @TODO: Use the initial full diff as a cache, since this gives more accurate results (since it's a 2 step process)
	// Also by doing this we remain consistent in the matches, avoiding issues where 2 nodes might match
	// for the links, but not for the nodes. Potentially causing weird situations?
	static bool BetterNodeMatch(UEdGraphNode* OldNode, UEdGraphNode* NewNode);
};
