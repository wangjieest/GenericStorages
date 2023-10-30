// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "CoreUObject.h"

#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "Templates/SubclassOf.h"

class UScriptStruct;
class FGenericSystemConfigCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	void BrowseTo();
	TWeakPtr<IPropertyHandle> RootPropertyHandle;
	UClass* SystemClass = nullptr;
};

#endif
