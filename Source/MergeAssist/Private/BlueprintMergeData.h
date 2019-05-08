// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IAssetTypeActions.h"

class FBlueprintEditor;

struct FBlueprintSelection
{
	FBlueprintSelection()
		: BlueprintLocal(nullptr)
		, RevisionLocal(FRevisionInfo::InvalidRevision())
		, BlueprintBase(nullptr)
		, RevisionBase(FRevisionInfo::InvalidRevision())
		, BlueprintRemote(nullptr)
		, RevisionRemote(FRevisionInfo::InvalidRevision())
		, BlueprintTarget(nullptr)
	{
		
	}

	FBlueprintSelection( 
		  const class UBlueprint* BlueprintLocal
		, const class UBlueprint* BlueprintBase
		, FRevisionInfo	          RevisionBase
		, const class UBlueprint* BlueprintRemote
		, FRevisionInfo           RevisionRemote
		, class UBlueprint* BlueprintTarget
	)
		: BlueprintLocal(BlueprintLocal)
		, RevisionLocal(FRevisionInfo::InvalidRevision())
		, BlueprintBase(BlueprintBase)
		, RevisionBase(RevisionBase)
		, BlueprintRemote(BlueprintRemote)
		, RevisionRemote(RevisionRemote)
		, BlueprintTarget(BlueprintTarget)
	{
	}

	const class UBlueprint* BlueprintLocal;
	FRevisionInfo           RevisionLocal;

	const class UBlueprint* BlueprintBase;
	FRevisionInfo           RevisionBase;

	const class UBlueprint* BlueprintRemote;
	FRevisionInfo           RevisionRemote;

	class UBlueprint* BlueprintTarget;
};

DECLARE_DELEGATE(FOnMergeNodeSelected);

