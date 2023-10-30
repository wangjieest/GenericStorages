// Copyright GenericStorages, Inc. All Rights Reserved.

#include "DataTablePickerCustomization.h"

#if WITH_EDITOR
#include "Slate.h"

#include "DataTableEditorUtils.h"
#include "DataTablePicker.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Level.h"
#include "Engine/Selection.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "IPropertyUtilities.h"
#include "Internationalization/Text.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "SNameComboBox.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"

namespace DataTablePickerCustomization
{
#if UE_4_20_OR_LATER
using SNameComboBox = ::SNameComboBox;
#else
class SNameComboBox : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(FString, FGetNameComboLabel, TSharedPtr<FName>);
	typedef TSlateDelegates<TSharedPtr<FName>>::FOnSelectionChanged FOnNameSelectionChanged;

	SLATE_BEGIN_ARGS(SNameComboBox)
			: _ColorAndOpacity(FSlateColor::UseForeground())
			, _ContentPadding(FMargin(4.0, 2.0))
			, _OnGetNameLabelForItem()
		{}
			SLATE_ARGUMENT(TArray<TSharedPtr<FName>>*, OptionsSource)
			SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
			SLATE_ATTRIBUTE(FMargin, ContentPadding)
			SLATE_EVENT(FOnNameSelectionChanged, OnSelectionChanged)
			SLATE_EVENT(FOnComboBoxOpening, OnComboBoxOpening)
			SLATE_ARGUMENT(TSharedPtr<FName>, InitiallySelectedItem)
			SLATE_EVENT(FGetNameComboLabel, OnGetNameLabelForItem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SelectionChanged = InArgs._OnSelectionChanged;
		GetTextLabelForItem = InArgs._OnGetNameLabelForItem;

		// Then make widget
		this->ChildSlot
		[
			SAssignNew(NameCombo, SComboBox<TSharedPtr<FName>>)
			.OptionsSource(InArgs._OptionsSource)
			.OnGenerateWidget(this, &SNameComboBox::MakeItemWidget)
			.OnSelectionChanged(this, &SNameComboBox::OnSelectionChanged)
			.OnComboBoxOpening(InArgs._OnComboBoxOpening)
			.InitiallySelectedItem(InArgs._InitiallySelectedItem)
			.ContentPadding(InArgs._ContentPadding)
			[
				SNew(STextBlock)
				.ColorAndOpacity(InArgs._ColorAndOpacity)
				.Text(this, &SNameComboBox::GetSelectedNameLabel)
			]
		];
		SelectedItem = NameCombo->GetSelectedItem();
	}

	TSharedRef<SWidget> MakeItemWidget(TSharedPtr<FName> NameItem)
	{
		check(NameItem.IsValid());

		return SNew(STextBlock)
		.Text(this, &SNameComboBox::GetItemNameLabel, NameItem);
	}

	void SetSelectedItem(TSharedPtr<FName> NewSelection) { NameCombo->SetSelectedItem(NewSelection); }
	TSharedPtr<FName> GetSelectedItem() { return SelectedItem; }
	void RefreshOptions() { NameCombo->RefreshOptions(); }
	void ClearSelection() { NameCombo->ClearSelection(); }

private:
	TSharedPtr<FName> OnGetSelection() const { return SelectedItem; }
	void OnSelectionChanged(TSharedPtr<FName> Selection, ESelectInfo::Type SelectInfo)
	{
		if (Selection.IsValid())
		{
			SelectedItem = Selection;
		}
		SelectionChanged.ExecuteIfBound(Selection, SelectInfo);
	}
	FText GetSelectedNameLabel() const
	{
		TSharedPtr<FName> StringItem = NameCombo->GetSelectedItem();
		return GetItemNameLabel(StringItem);
	}

	FText GetItemNameLabel(TSharedPtr<FName> NameItem) const
	{
		if (!NameItem.IsValid())
		{
			return FText::GetEmpty();
		}

		return (GetTextLabelForItem.IsBound()) ? FText::FromString(GetTextLabelForItem.Execute(NameItem)) : FText::FromName(*NameItem);
	}

private:
	FGetNameComboLabel GetTextLabelForItem;
	TSharedPtr<FName> SelectedItem;
	TArray<TSharedPtr<FName>> Names;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> NameCombo;
	FOnNameSelectionChanged SelectionChanged;
};
#endif
}  // namespace DataTablePickerCustomization

TSharedRef<IPropertyTypeCustomization> FDataTablePickerCustomization::MakeInstance()
{
	return MakeShareable(new FDataTablePickerCustomization);
}

void FDataTablePickerCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		RootPropertyHandle = PropertyHandle;

		auto OrignalPtr = UnrealEditorUtils::GetPropertyAddress<FDataTablePicker>(PropertyHandle);
		check(OrignalPtr);

		StructTypePicker.Init(PropertyHandle);
		auto Utilities = CustomizationUtils.GetPropertyUtilities();
		auto DataPropertyHandle = PropertyHandle->GetChildHandle("DataTable", false);

		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.MaxDesiredWidth(0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(120)
			[
				StructTypePicker.MakePropertyWidget(DataPropertyHandle)
			]
		];
	}
}

//////////////////////////////////////////////////////////////////////////
TSharedRef<IPropertyTypeCustomization> FDataTablePathPickerCustomization::MakeInstance()
{
	return MakeShareable(new FDataTablePathPickerCustomization());
}

void FDataTablePathPickerCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		RootPropertyHandle = PropertyHandle;

		auto OrignalPtr = UnrealEditorUtils::GetPropertyAddress<FDataTablePathPicker>(PropertyHandle);
		check(OrignalPtr);

		StructTypePicker.Init(PropertyHandle);

		auto Utilities = CustomizationUtils.GetPropertyUtilities();

		auto DataPropertyHandle = PropertyHandle->GetChildHandle("DataTablePath", false);
		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.MaxDesiredWidth(0.f)

		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(120)

			[
				StructTypePicker.MakePropertyWidget(DataPropertyHandle)
			]
		];
	}
}

//////////////////////////////////////////////////////////////////////////
TSharedRef<IPropertyTypeCustomization> FDataTableRowNamePickerCustomization::MakeInstance()
{
	return MakeShareable(new FDataTableRowNamePickerCustomization);
}

inline void FDataTableRowNamePickerCustomization::PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info)
{
#if WITH_EDITORONLY_DATA
	auto ThisProperty = RootPropertyHandle.Pin();
	if (ThisProperty.IsValid())
	{
		if (auto Ptr = UnrealEditorUtils::GetPropertyAddress<FDataTableRowNamePicker>(ThisProperty))
		{
			if (Changed && (Changed == Ptr->DataTablePath.Get()) && (FDataTableEditorUtils::EDataTableChangeInfo::RowList == Info))
			{
				auto R = Utilities.Pin();
				if (R.IsValid())
					R->RequestRefresh();
			}
		}
	}
#endif
}

void FDataTableRowNamePickerCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		RootPropertyHandle = PropertyHandle;
		auto* OrignalPtr = UnrealEditorUtils::GetPropertyAddress<FDataTableRowNamePicker>(PropertyHandle);
		check(OrignalPtr);
		TSharedPtr<FName> CurrentlySelectedName;
		if (auto Table = OrignalPtr->DataTablePath.Get())
		{
			PreloadObject(Table);
			NameList.Empty();
			for (auto a : Table->GetRowNames())
			{
				auto Ptr = MakeShared<FName>(a);
				NameList.Add(Ptr);
				if (OrignalPtr->RowName == a)
					CurrentlySelectedName = Ptr;
			}
		}

		StructTypePicker.Init(PropertyHandle);

		Utilities = CustomizationUtils.GetPropertyUtilities();
		StructTypePicker.OnChanged.BindLambda([this] {
			auto ThisProperty = RootPropertyHandle.Pin();
			if (ThisProperty.IsValid())
			{
				if (auto Ptr = UnrealEditorUtils::GetPropertyAddress<FDataTableRowNamePicker>(ThisProperty))
				{
					{
						UnrealEditorUtils::FScopedPropertyTransaction Scoped(ThisProperty);
						Ptr->RowName = NAME_None;
#if WITH_EDITORONLY_DATA
						Ptr->DataTablePath = nullptr;
#endif
					}
					TArray<UObject*> OuterObjects;
					ThisProperty->GetOuterObjects(OuterObjects);
					for (auto& a : OuterObjects)
						a->MarkPackageDirty();
				}
			}
		});

		auto SoftPropertyHandle = PropertyHandle->GetChildHandle("DataTablePath", false);
		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.MaxDesiredWidth(0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(60)
			[
				SNew(DataTablePickerCustomization::SNameComboBox)
				.ContentPadding(FMargin(6.0f, 2.0f))
				.OptionsSource(&NameList)
				.InitiallySelectedItem(CurrentlySelectedName)
				.OnSelectionChanged(this, &FDataTableRowNamePickerCustomization::ComboBoxSelectionChanged)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(160)
			[
				StructTypePicker.MakePropertyWidget(SoftPropertyHandle)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(120)
			[
				StructTypePicker.MakeWidget()
			]
		];
	}
}

void FDataTableRowNamePickerCustomization::ComboBoxSelectionChanged(TSharedPtr<FName> NameItem, ESelectInfo::Type SelectInfo)
{
	FName Name = NameItem.IsValid() ? *NameItem : NAME_None;
	auto ThisProperty = RootPropertyHandle.Pin();
	if (ThisProperty.IsValid())
	{
		if (auto Ptr = UnrealEditorUtils::GetPropertyAddress<FDataTableRowNamePicker>(ThisProperty))
		{
			if (Ptr->RowName != Name)
			{
				UnrealEditorUtils::FScopedPropertyTransaction Scoped(ThisProperty);
				Ptr->RowName = Name;
				TArray<UObject*> OuterObjects;
				ThisProperty->GetOuterObjects(OuterObjects);
				for (auto& a : OuterObjects)
					a->MarkPackageDirty();
			}
		}
	}
}

void FDataTableRowNamePickerCustomization::PreloadObject(UObject* ReferencedObject)
{
	if ((ReferencedObject != NULL) && ReferencedObject->HasAnyFlags(RF_NeedLoad))
	{
		ReferencedObject->GetLinker()->Preload(ReferencedObject);
	}
}

//////////////////////////////////////////////////////////////////////////
TSharedRef<IPropertyTypeCustomization> FDataTableRowPickerCustomization::MakeInstance()
{
	return MakeShareable(new FDataTableRowPickerCustomization);
}

inline void FDataTableRowPickerCustomization::PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info)
{
	auto ThisProperty = RootPropertyHandle.Pin();
	if (ThisProperty.IsValid())
	{
		if (auto Ptr = UnrealEditorUtils::GetPropertyAddress<FDataTableRowPicker>(ThisProperty))
		{
			if (Changed && (Changed == Ptr->DataTable) && (FDataTableEditorUtils::EDataTableChangeInfo::RowList == Info))
			{
				auto R = Utilities.Pin();
				if (R.IsValid())
					R->RequestRefresh();
			}
		}
	}
}

void FDataTableRowPickerCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		RootPropertyHandle = PropertyHandle;
		FDataTableRowPicker* OrignalPtr = UnrealEditorUtils::GetPropertyAddress<FDataTableRowPicker>(PropertyHandle);
		check(OrignalPtr);
		TSharedPtr<FName> CurrentlySelectedName;
		if (auto Table = OrignalPtr->DataTable)
		{
			PreloadObject(Table);
			NameList.Empty();
			for (auto a : Table->GetRowNames())
			{
				auto Ptr = MakeShared<FName>(a);
				NameList.Add(Ptr);
				if (OrignalPtr->RowName == a)
					CurrentlySelectedName = Ptr;
			}
		}

		StructTypePicker.Init(PropertyHandle);

		Utilities = CustomizationUtils.GetPropertyUtilities();
		StructTypePicker.OnChanged.BindLambda([this] {
			auto ThisProperty = RootPropertyHandle.Pin();
			if (ThisProperty.IsValid())
			{
				if (auto Ptr = UnrealEditorUtils::GetPropertyAddress<FDataTableRowPicker>(ThisProperty))
				{
					{
						UnrealEditorUtils::FScopedPropertyTransaction Scoped(ThisProperty);
						Ptr->DataTable = nullptr;
						Ptr->RowName = NAME_None;
					}
					// Utilities->ForceRefresh();
					TArray<UObject*> OuterObjects;
					ThisProperty->GetOuterObjects(OuterObjects);
					for (auto& a : OuterObjects)
						a->MarkPackageDirty();
				}
			}
		});

		auto TablePropertyHandle = PropertyHandle->GetChildHandle("DataTable", false);
		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.MaxDesiredWidth(0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(60)
			[
				SNew(DataTablePickerCustomization::SNameComboBox)
				.ContentPadding(FMargin(6.0f, 2.0f))
				.OptionsSource(&NameList)
				.InitiallySelectedItem(CurrentlySelectedName)
				.OnSelectionChanged(this, &FDataTableRowPickerCustomization::ComboBoxSelectionChanged)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(160)
			[
				StructTypePicker.MakePropertyWidget(TablePropertyHandle)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(120)
			[
				StructTypePicker.MakeWidget()
			]
		];
	}
}

void FDataTableRowPickerCustomization::ComboBoxSelectionChanged(TSharedPtr<FName> NameItem, ESelectInfo::Type SelectInfo)
{
	FName Name = NameItem.IsValid() ? *NameItem : NAME_None;
	auto ThisProperty = RootPropertyHandle.Pin();
	if (ThisProperty.IsValid())
	{
		if (auto Ptr = UnrealEditorUtils::GetPropertyAddress<FDataTableRowPicker>(ThisProperty))
		{
			if (Ptr->RowName != Name)
			{
				UnrealEditorUtils::FScopedPropertyTransaction Scoped(ThisProperty);
				Ptr->RowName = Name;
				TArray<UObject*> OuterObjects;
				ThisProperty->GetOuterObjects(OuterObjects);
				for (auto& a : OuterObjects)
					a->MarkPackageDirty();
			}
		}
	}
}

void FDataTableRowPickerCustomization::PreloadObject(UObject* ReferencedObject)
{
	if ((ReferencedObject != NULL) && ReferencedObject->HasAnyFlags(RF_NeedLoad))
	{
		ReferencedObject->GetLinker()->Preload(ReferencedObject);
	}
}
#endif
