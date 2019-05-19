// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FDiffHelper.h"

class UEdGraph;
class UEdGraphNode;

static const FLinearColor SoftRed = FColor(0xF4, 0x43, 0x36);
static const FLinearColor SoftBlue = FColor(0x21, 0x96, 0xF3);
static const FLinearColor SoftGreen = FColor(0x4C, 0xAF, 0x50);
static const FLinearColor SoftYellow = FColor(0xFF, 0xE4, 0xB5);

enum struct EMergeState
{
	Base = 0,
	Remote,
	Local
};

struct MergeGraphChange
{
	FText Label;
	FLinearColor DisplayColor;

	FMergeDiffResult RemoteDiff;
	FMergeDiffResult LocalDiff;

	bool bHasConflicts;
	EMergeState MergeState;
};

class GraphMergeHelper
{
public:
	GraphMergeHelper(UEdGraph* RemoteGraph, UEdGraph* BaseGraph, UEdGraph* LocalGraph, UEdGraph* TargetGraph);
	~GraphMergeHelper() = default;

	bool CanApplyRemoteChange(MergeGraphChange& Change);
	bool CanApplyLocalChange(MergeGraphChange& Change);
	bool CanRevertChange(MergeGraphChange& Change);

	bool ApplyRemoteChange(MergeGraphChange& Change);
	bool ApplyLocalChange(MergeGraphChange& Change);
	bool RevertChange(MergeGraphChange& Change);

	bool ExistsInRemote() const { return RemoteGraph != nullptr; }
	bool ExistsInLocal() const {return LocalGraph != nullptr; }
	bool ExistsInBase() const {return BaseGraph != nullptr; }

	bool HasRemoteChanges() const {return bHasRemoteChanges; }
	bool HasLocalChanges() const { return bHasLocalChanges; }
	bool HasConflicts() const { return bHasConflicts; }

	UEdGraphNode* FindNodeInTargetGraph(UEdGraphNode* Node);

public:
	const FName GraphName;
	TArray<TSharedPtr<MergeGraphChange>> ChangeList;

private:
	UEdGraphNode* GetBaseNodeInTargetGraph(UEdGraphNode* SourceNode);

	bool ApplyDiff(const FMergeDiffResult& Diff, const bool bCanWrite);
	bool RevertDiff(const FMergeDiffResult& Diff, const bool bCanWrite);

	bool CloneToTarget(UEdGraphNode* SourceNode, bool bRestoreLinks, bool CanWrite, UEdGraphNode** OutNewNode = nullptr);

	// Graphs
	UEdGraph* const RemoteGraph;
	UEdGraph* const BaseGraph;
	UEdGraph* const LocalGraph;
	UEdGraph* const TargetGraph;	

	bool bHasRemoteChanges;
	bool bHasLocalChanges;
	bool bHasConflicts;

	// Mapping of nodes between the different maps, note that we only need 
	// to map towards the target graph, so remote/local -> base -> target
	TMap<UEdGraphNode*, UEdGraphNode*> BaseToTargetNodeMap;
	TMap<UEdGraphNode*, UEdGraphNode*> RemoteToBaseNodeMap;
	TMap<UEdGraphNode*, UEdGraphNode*> LocalToBaseNodeMap;

#if 0
	bool ApplyDiff_NODE_REMOVED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool ApplyDiff_NODE_ADDED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool ApplyDiff_PIN_REMOVED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool ApplyDiff_PIN_ADDED(const FDiffSingleResult& Diff, const bool bCanWrite);
	//bool ApplyDiff_PIN_LINKEDTO_NUM_DEC(const FDiffSingleResult& Diff, const bool bCanWrite);
	//bool ApplyDiff_PIN_LINKEDTO_NUM_INC(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool ApplyDiff_LINK_ADDED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool ApplyDiff_LINK_REMOVED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool ApplyDiff_PIN_DEFAULT_VALUE(const FDiffSingleResult& Diff, const bool bCanWrite);
	//bool ApplyDiff_PIN_LINKEDTO_NODE(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool ApplyDiff_NODE_MOVED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool ApplyDiff_NODE_COMMENT(const FDiffSingleResult& Diff, const bool bCanWrite);

	bool RevertDiff_NODE_REMOVED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool RevertDiff_NODE_ADDED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool RevertDiff_PIN_REMOVED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool RevertDiff_PIN_ADDED(const FDiffSingleResult& Diff, const bool bCanWrite);
	//bool RevertDiff_PIN_LINKEDTO_NUM_DEC(const FDiffSingleResult& Diff, const bool bCanWrite);
	//bool RevertDiff_PIN_LINKEDTO_NUM_INC(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool RevertDiff_LINK_ADDED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool RevertDiff_LINK_REMOVED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool RevertDiff_PIN_DEFAULT_VALUE(const FDiffSingleResult& Diff, const bool bCanWrite);
	//bool RevertDiff_PIN_LINKEDTO_NODE(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool RevertDiff_NODE_MOVED(const FDiffSingleResult& Diff, const bool bCanWrite);
	bool RevertDiff_NODE_COMMENT(const FDiffSingleResult& Diff, const bool bCanWrite);
#endif
};
