// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"

#include "STreeView.h"

struct IMergeTreeEntry
{
	virtual ~IMergeTreeEntry() = default;

	// The actual functions doing the heavy lifting
	virtual TSharedRef<SWidget> OnGenerateRow() = 0;
	virtual void OnSelected() = 0;

	virtual bool ApplyRemote() { return false; }
	virtual bool ApplyLocal()  { return false; }
	virtual bool Revert()      { return false; }
	
	TArray<TSharedPtr<IMergeTreeEntry>> Children;
};

class SMergeTreeView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMergeTreeView)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	void Add(TSharedPtr<IMergeTreeEntry> TreeEntry);

	void OnToolBarPrev();
	void OnToolBarNext();
	void OnToolBarNextConflict();
	void OnToolBarPrevConflict();

	void OnToolbarApplyRemote();
	void OnToolbarApplyLocal();
	void OnToolbarRevert();

private:
	TArray<TSharedPtr<IMergeTreeEntry>> Data;
	TSharedPtr<STreeView<TSharedPtr<IMergeTreeEntry>>> Widget;

	TSharedPtr<IMergeTreeEntry> SelectedEntry;
};
