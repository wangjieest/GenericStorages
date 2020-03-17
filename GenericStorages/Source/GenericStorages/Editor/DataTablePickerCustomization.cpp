/****************************************************************************
Copyright (c) 2017-2027 GenericStorages

author: wangjieest

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "DataTablePickerCustomization.h"

#if WITH_EDITOR
#	include "Slate.h"

#	include "DataTableEditorUtils.h"
#	include "DetailCategoryBuilder.h"
#	include "DetailLayoutBuilder.h"
#	include "Editor.h"
#	include "Engine/BlueprintGeneratedClass.h"
#	include "Engine/Level.h"
#	include "Engine/Selection.h"
#	include "Engine/SimpleConstructionScript.h"
#	include "Engine/World.h"
#	include "EngineUtils.h"
#	include "GameFramework/Actor.h"
#	include "IDetailChildrenBuilder.h"
#	include "IPropertyTypeCustomization.h"
#	include "IPropertyUtilities.h"
#	include "Internationalization/Text.h"
#	include "PropertyCustomizationHelpers.h"
#	include "PropertyHandle.h"
#	include "SNameComboBox.h"
#	include "UObject/LinkerLoad.h"
#	include "UObject/ObjectMacros.h"
#	include "UObject/UnrealType.h"
#	include "DataTablePicker.h"

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
		auto OuterObject = OuterObjects[0];

		auto OrignalPtr = GS_EditorUtils::GetStructPropertyAddress<FDataTablePicker>(PropertyHandle, OuterObject);
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
		auto OuterObject = OuterObjects[0];

		auto OrignalPtr = GS_EditorUtils::GetStructPropertyAddress<FDataTablePathPicker>(PropertyHandle, OuterObject);
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
#	if WITH_EDITORONLY_DATA
	if (auto ThisProperty = RootPropertyHandle.Pin())
	{
		if (auto Ptr = GS_EditorUtils::GetStructPropertyAddress<FDataTableRowNamePicker>(ThisProperty))
		{
			if (Changed && (Changed == Ptr->DataTablePath.Get()) && (FDataTableEditorUtils::EDataTableChangeInfo::RowList == Info))
			{
				if (auto R = Utilities.Pin())
					R->RequestRefresh();
			}
		}
	}
#	endif
}

void FDataTableRowNamePickerCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		RootPropertyHandle = PropertyHandle;
		auto OuterObject = OuterObjects[0];
		auto* OrignalPtr = GS_EditorUtils::GetStructPropertyAddress<FDataTableRowNamePicker>(PropertyHandle, OuterObject);
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
			if (auto ThisProperty = RootPropertyHandle.Pin())
			{
				if (auto Ptr = GS_EditorUtils::GetStructPropertyAddress<FDataTableRowNamePicker>(ThisProperty))
				{
					{
						GS_EditorUtils::FScopedPropertyTransaction Scoped(ThisProperty);
						Ptr->RowName = NAME_None;
#	if WITH_EDITORONLY_DATA
						Ptr->DataTablePath = nullptr;
#	endif
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
				SNew(SNameComboBox)
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
	if (auto ThisProperty = RootPropertyHandle.Pin())
	{
		if (auto Ptr = GS_EditorUtils::GetStructPropertyAddress<FDataTableRowNamePicker>(ThisProperty))
		{
			if (Ptr->RowName != Name)
			{
				GS_EditorUtils::FScopedPropertyTransaction Scoped(ThisProperty);
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
	if (auto ThisProperty = RootPropertyHandle.Pin())
	{
		if (auto Ptr = GS_EditorUtils::GetStructPropertyAddress<FDataTableRowPicker>(ThisProperty))
		{
			if (Changed && (Changed == Ptr->DataTable) && (FDataTableEditorUtils::EDataTableChangeInfo::RowList == Info))
			{
				if (auto R = Utilities.Pin())
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
		auto OuterObject = OuterObjects[0];
		FDataTableRowPicker* OrignalPtr = GS_EditorUtils::GetStructPropertyAddress<FDataTableRowPicker>(PropertyHandle, OuterObject);
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
			if (auto ThisProperty = RootPropertyHandle.Pin())
			{
				if (auto Ptr = GS_EditorUtils::GetStructPropertyAddress<FDataTableRowPicker>(ThisProperty))
				{
					{
						GS_EditorUtils::FScopedPropertyTransaction Scoped(ThisProperty);
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
				SNew(SNameComboBox)
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
	if (auto ThisProperty = RootPropertyHandle.Pin())
	{
		if (auto Ptr = GS_EditorUtils::GetStructPropertyAddress<FDataTableRowPicker>(ThisProperty))
		{
			if (Ptr->RowName != Name)
			{
				GS_EditorUtils::FScopedPropertyTransaction Scoped(ThisProperty);
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
