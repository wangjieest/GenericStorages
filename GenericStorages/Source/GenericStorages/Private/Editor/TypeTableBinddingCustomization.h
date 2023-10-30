// Copyright GenericAbilities, Inc. All Rights Reserved.

#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"

#include "Editor/UnrealEditorUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "IPropertyTypeCustomization.h"
#include "Templates/SubclassOf.h"
#include "UObject/Class.h"

class FTypeToTableCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	TWeakObjectPtr<UScriptStruct> ScriptStruct;
	TWeakPtr<IPropertyHandle> RootPropertyHandle;
	TArray<TSharedPtr<FName>> Names;
	TSharedPtr<FName> InitValue;
	UnrealEditorUtils::FDatatableTypePicker StructTypePicker;
};
#endif