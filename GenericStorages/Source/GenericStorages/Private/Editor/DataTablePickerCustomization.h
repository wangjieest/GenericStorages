// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "CoreUObject.h"

#include "DataTableEditorUtils.h"
#include "Editor/UnrealEditorUtils.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "Templates/SubclassOf.h"
#include "UnrealCompatibility.h"

class UScriptStruct;
class FDataTablePickerCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}

	TWeakPtr<IPropertyHandle> RootPropertyHandle;
	UnrealEditorUtils::FDatatableTypePicker StructTypePicker;
};

class FDataTablePathPickerCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}

	TWeakPtr<IPropertyHandle> RootPropertyHandle;
	UnrealEditorUtils::FDatatableTypePicker StructTypePicker;
};

class FDataTableRowNamePickerCustomization
	: public IPropertyTypeCustomization
	, public FDataTableEditorUtils::INotifyOnDataTableChanged
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	void PreChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info) {}

	void PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info);
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	void ComboBoxSelectionChanged(TSharedPtr<FName> StringItem, ESelectInfo::Type SelectInfo);
	TWeakPtr<IPropertyUtilities> Utilities;
	TArray<TSharedPtr<FName>> NameList;
	// Ensures the specified object is preloaded.  ReferencedObject can be NULL.
	void PreloadObject(UObject* ReferencedObject);
	FString ObjectPath;
	TWeakPtr<IPropertyHandle> RootPropertyHandle;
	UnrealEditorUtils::FDatatableTypePicker StructTypePicker;
};

class FDataTableRowPickerCustomization
	: public IPropertyTypeCustomization
	, public FDataTableEditorUtils::INotifyOnDataTableChanged

{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	void PreChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info) {}

	void PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info);
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}

	TWeakPtr<IPropertyUtilities> Utilities;
	void ComboBoxSelectionChanged(TSharedPtr<FName> StringItem, ESelectInfo::Type SelectInfo);
	TArray<TSharedPtr<FName>> NameList;
	// Ensures the specified object is preloaded.  ReferencedObject can be NULL.
	void PreloadObject(UObject* ReferencedObject);
	FString ObjectPath;
	TWeakPtr<IPropertyHandle> RootPropertyHandle;
	UnrealEditorUtils::FDatatableTypePicker StructTypePicker;
};
#endif
