// Fill out your copyright notice in the Description page of Project Settings.

#include "SMergeTreeView.h"
#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

static TSharedRef<ITableRow> ChangeTreeOnGenerateRow(
	TSharedPtr<IMergeTreeEntry> Item, 
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
	[
		Item->OnGenerateRow()
	];
}

static void ChangeTreeOnSelectionChanged(TSharedPtr<IMergeTreeEntry> Item, 
	ESelectInfo::Type SelectInfo, 
	TSharedPtr<IMergeTreeEntry>* SelectionOut)
{
	if (SelectionOut) *SelectionOut = Item;
	if (!Item) return;

	Item->OnSelected();
}

static void ChangeTreeOnGetChildren(TSharedPtr<IMergeTreeEntry> Item,
	TArray<TSharedPtr<IMergeTreeEntry>>& Children)
{
	Children = Item->Children;
}

void SMergeTreeView::Construct(const FArguments& InArgs)
{
	Widget = SNew(STreeView<TSharedPtr<IMergeTreeEntry>>)
		.ItemHeight(24)
		.TreeItemsSource(&Data)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow_Static(&ChangeTreeOnGenerateRow)
		.OnSelectionChanged_Static(&ChangeTreeOnSelectionChanged, &SelectedEntry)
		.OnGetChildren_Static(&ChangeTreeOnGetChildren);

	ChildSlot
	[
		Widget.ToSharedRef()
	];
}
void SMergeTreeView::Add(TSharedPtr<IMergeTreeEntry> TreeEntry)
{
	Data.Add(TreeEntry);
}

void SMergeTreeView::OnToolBarPrev()
{
	// @TODO: Implement
}

void SMergeTreeView::OnToolBarNext()
{
	// @TODO: Implement
}

void SMergeTreeView::OnToolBarNextConflict()
{
	// @TODO: Implement
}

void SMergeTreeView::OnToolBarPrevConflict()
{
	// @TODO: Implement
}

void SMergeTreeView::OnToolbarApplyRemote()
{
	if (!SelectedEntry) return;
	SelectedEntry->ApplyRemote();

	// @TODO: Add status reporting
}

void SMergeTreeView::OnToolbarApplyLocal()
{
	if (!SelectedEntry) return;
	SelectedEntry->ApplyLocal();

	// @TODO: Add status reporting
}

void SMergeTreeView::OnToolbarRevert()
{
	if (!SelectedEntry) return;
	SelectedEntry->Revert();

	// @TODO: Add status reporting
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
