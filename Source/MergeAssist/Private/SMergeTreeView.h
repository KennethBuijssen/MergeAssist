// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "Widgets/Views/STreeView.h"
#include <functional>

struct IMergeTreeEntry
{
	IMergeTreeEntry() : bHighLight(false) {}
	virtual ~IMergeTreeEntry() = default;

	// The actual functions doing the heavy lifting
	virtual TSharedRef<SWidget> OnGenerateRow() = 0;
	virtual void OnSelected() = 0;

	virtual bool ApplyRemote() { return false; }
	virtual bool ApplyLocal()  { return false; }
	virtual bool Revert()      { return false; }

	bool bHighLight;
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

	// Set the highlight value 
	template<typename Predicate>
	void HighlightByPredicate(Predicate Pred)
	{
		std::function<void(TArray<TSharedPtr<IMergeTreeEntry>>&)> Highlight = 
			[&Highlight, Pred](TArray<TSharedPtr<IMergeTreeEntry>>& Entries)
		{
			for (auto Entry : Entries)
			{
				Entry->bHighLight = Pred(Entry);
				Highlight(Entry->Children);
			}
		};

		Highlight(Data);
	}

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
