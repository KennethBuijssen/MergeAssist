// Fill out your copyright notice in the Description page of Project Settings.

#include "SMergeTreeView.h"
#include "SlateOptMacros.h"
#include "EditorStyle.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

static TSharedRef<ITableRow> ChangeTreeOnGenerateRow(
	TSharedPtr<IMergeTreeEntry> Item, 
	const TSharedRef<STableViewBase>& OwnerTable)
{
	// We use a lambda to calculate the color when a row is highlighted
	// this ensures that the color is dynamically updated whenever we set 
	// the highlight flag
	auto CalcHighlightColor = [Item]()
	{
		return Item->bHighLight ? FColor(0xFF, 0x00, 0x00, 0x60) : FColor(0x00, 0x00, 0x00, 0x00);		
	};

	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
	[
		// Add a border around the row, as a way to add highlights
		SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(0.0f)
		.BorderBackgroundColor_Lambda(CalcHighlightColor)
		[
			Item->OnGenerateRow()
		]
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
		.ItemHeight(20)
		.TreeItemsSource(&Data)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow_Static(&ChangeTreeOnGenerateRow)
		.OnSelectionChanged_Static(&ChangeTreeOnSelectionChanged, &SelectedEntry)
		.OnGetChildren_Static(&ChangeTreeOnGetChildren);

	ChildSlot
	[
		// Add a darker background behind the tree view, this helps the text stand out more
		SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			Widget.ToSharedRef()
		]		
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
