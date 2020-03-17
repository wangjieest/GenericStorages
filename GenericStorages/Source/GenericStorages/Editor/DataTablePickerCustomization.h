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

#pragma once
#include "CoreMinimal.h"

#if WITH_EDITOR
#	include "CoreUObject.h"

#	include "DataTableEditorUtils.h"
#	include "GS_EditorUtils.h"
#	include "IDetailCustomization.h"
#	include "IPropertyTypeCustomization.h"
#	include "Templates/SubclassOf.h"

class UScriptStruct;
class FDataTablePickerCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}

	TWeakPtr<IPropertyHandle> RootPropertyHandle;
	GS_EditorUtils::FDatatableTypePicker StructTypePicker;
};

class FDataTablePathPickerCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}

	TWeakPtr<IPropertyHandle> RootPropertyHandle;
	GS_EditorUtils::FDatatableTypePicker StructTypePicker;
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
	GS_EditorUtils::FDatatableTypePicker StructTypePicker;
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
	GS_EditorUtils::FDatatableTypePicker StructTypePicker;
};
#endif
