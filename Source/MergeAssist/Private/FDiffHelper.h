// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

// NOTE: Keep this in order of importance, that way we can use it to sort
// the order in which to display the different diff types to the user
enum struct EMergeDiffType
{
	NO_DIFFERENCE = 0,
	NODE_REMOVED,
	NODE_ADDED,
	PIN_REMOVED,
	PIN_ADDED,
	PIN_DEFAULT_VALUE,	
	LINK_REMOVED,
	LINK_ADDED,
	//PIN_TYPE_CATEGORY,
	//PIN_TYPE_SUBCATEGORY,
	//PIN_TYPE_SUBCATEGORY_OBJECT,
	//PIN_TYPE_IS_ARRAY,
	//PIN_TYPE_IS_REF,
	NODE_MOVED,
	NODE_COMMENT,

	// Currently only used when internal properties changed
	// we can't resolve them, but would be nice to show when this is the case
	// used by UAIGraphNode, and UK2Node_MathExpression
	//NODE_PROPERTY -> NODE_INTERNAL_CHANGE
};

enum struct ENodeMatchStrategy
{
	NONE = 0,
	EXACT = 1 << 0,
	APPROXIMATE = 1 << 1,

	ALL = -1
};

static bool IsFlagSet(ENodeMatchStrategy Mask, ENodeMatchStrategy Flag)
{
	return static_cast<int>(Mask) & static_cast<int>(Flag);
}

struct FNodeMatch
{
	UEdGraphNode* OldNode;
	UEdGraphNode* NewNode;

	bool IsValid() const { return OldNode && NewNode; }
};

struct FPinMatch
{
	UEdGraphPin* OldPin;
	UEdGraphPin* NewPin;

	bool IsValid() const { return OldPin && NewPin; }
};

// Link between two pins in a graph
struct FGraphLink
{
	UEdGraphPin* SourcePin;
	UEdGraphPin* TargetPin;

	bool IsValid() const { return SourcePin && TargetPin; }
};

inline bool operator==(const FGraphLink& Lhs, const FGraphLink& Rhs)
{
	return Lhs.SourcePin == Rhs.SourcePin && Lhs.TargetPin == Rhs.TargetPin;
}

struct FLinkMatch
{
	FGraphLink OldLink;
	FGraphLink NewLink;

	bool IsValid() const { return OldLink.IsValid() && NewLink.IsValid(); }
};

struct FMergeDiffResult
{
	// Type of the diff
	EMergeDiffType Type;

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
	FLinearColor DisplayColor;
};

class FMergeDiffResults
{
public:
	FMergeDiffResults(TArray<FMergeDiffResult>* ResultsOut = nullptr)
		: ResultArray(ResultsOut)
		, NumDiffsFound(0) 
	{}

	void Add(const FMergeDiffResult& Result)
	{
		if (Result.Type == EMergeDiffType::NO_DIFFERENCE) return;

		NumDiffsFound++;

		if (ResultArray) ResultArray->Add(Result);
	}

	bool CanStoreResults() const { return ResultArray != nullptr; }

	int32 NumStored() const { return ResultArray ? ResultArray->Num() : 0; }
	int32 NumFound() const { return NumDiffsFound; }
	bool HasFoundDiffs() const { return NumDiffsFound > 0; }

private:
	TArray<FMergeDiffResult>* ResultArray;
	int32 NumDiffsFound;
};

struct FDiffHelper
{
	static void DiffGraphs(
		UEdGraph* OldGraph,
		UEdGraph* NewGraph,
		FMergeDiffResults& DiffsOut,
		ENodeMatchStrategy MatchStrategy = ENodeMatchStrategy::ALL,
		TArray<FNodeMatch>* NodeMatchesOut = nullptr,
		TArray<UEdGraphNode*>* UnmatchedOldNodesOut = nullptr,
		TArray<UEdGraphNode*>* UnmatchedNewNodesOut = nullptr);

	static void DiffNodes(
		UEdGraphNode* OldNode, 
		UEdGraphNode* NewNode, 
		FMergeDiffResults& DiffsOut);

	static void DiffPins(
		UEdGraphPin* OldPin,
		UEdGraphPin* NewPin,
		FMergeDiffResults& DiffsOut);

	static void DiffLinks(
		const FGraphLink& OldLink,
		const FGraphLink& NewLink,
		FMergeDiffResults& DiffsOut);

	static bool IsExactNodeMatch(const UEdGraphNode* OldNode, const UEdGraphNode* NewNode);

	static TArray<FNodeMatch> FindNodeMatches(
		UEdGraph* OldGraph,
		UEdGraph* NewGraph,
		ENodeMatchStrategy MatchStrategy = ENodeMatchStrategy::ALL,
		TArray<UEdGraphNode*>* OutUnmatchedOldNodes = nullptr,
		TArray<UEdGraphNode*>* OutUnmatchedNewNodes = nullptr);

	static TArray<FPinMatch> FindPinMatches(
		UEdGraphNode* OldNode,
		UEdGraphNode* NewNode,
		TArray<UEdGraphPin*>* OutUnmatchedOldPins = nullptr,
		TArray<UEdGraphPin*>* OutUnmatchedNewPins = nullptr);

	static TArray<FLinkMatch> FindLinkMatches(
		UEdGraphPin* OldPin,
		UEdGraphPin* NewPin,
		TArray<FGraphLink>* OutUnmatchedOldLinks = nullptr,
		TArray<FGraphLink>* OutUnmatchedNewLinks = nullptr);

	static TArray<FNodeMatch> FindExactNodeMatches(
		TArray<UEdGraphNode*>& UnmatchedOldNodes,
		TArray<UEdGraphNode*>& UnmatchedNewNodes
	);

	static TArray<FNodeMatch> FindApproximateNodeMatches(
		TArray<UEdGraphNode*>& UnmatchedOldNodes,
		TArray<UEdGraphNode*>& UnmatchedNewNodes
	);

	static TArray<FNodeMatch> FindApproximateNodeMatchesBetweenNodesOfTheSameType(
		const TArrayView<UEdGraphNode*>& UnmatchedOldNodesOfType, 
		const TArrayView<UEdGraphNode*>& UnmatchedNewNodesOfType
	);
	
	// Matches the nodes based on exact match, or class and title
	static bool WeakNodeMatch(UEdGraphNode* OldNode, UEdGraphNode* NewNode);
};
