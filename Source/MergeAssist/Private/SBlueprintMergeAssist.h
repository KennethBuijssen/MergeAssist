// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "SBox.h"

#include "BlueprintMergeData.h"
#include "Unreal/MergeUtils.h"

struct FAssetRevisionInfo;

/**
 * 
 */
class SBlueprintMergeAssist : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintMergeAssist)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const FBlueprintSelection& InData);

private:
	bool bIsPickingAssets = true; 
	FBlueprintSelection Data;

	/** UI Callbacks */
	void OnToolbarNext();
	void OnToolbarPrev();
	void OnToolbarNextConflict();
	void OnToolbarPrevConflict();

	void OnToolbarApplyRemote();
	void OnToolbarApplyLocal();
	void OnToolbarRevert();

	void OnToolbarFinishMerge();

	bool IsSelectingAssets() const;

	void OnStartMerge();
	void OnFinishMerge();
	void OnCancelMerge();

	/** Asset picker */
	void OnMergeAssetSelected(EMergeAssetId::Type AssetId, const FAssetRevisionInfo& AssetInfo);
	bool IsActivelyMerging() const;

	/** Paths of the different Blueprint assets which will be merged */
	FString RemotePath;
	FString BasePath;
	FString LocalPath;

	TSharedPtr<SBox> MainContainer;
	TSharedPtr<SBox> SideContainer; 

	void OnModeChanged();

	/** Different sub controls for the different stages of merging */
	TSharedPtr<SWidget> AssetPickerControl;
	TSharedPtr<class SMergeGraphView> GraphControl;
};
