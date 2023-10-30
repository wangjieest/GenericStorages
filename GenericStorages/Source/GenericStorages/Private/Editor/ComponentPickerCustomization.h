// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if WITH_EDITOR
#include "CoreUObject.h"

#include "Components/ActorComponent.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "Templates/SubclassOf.h"

class FComponentPickerCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	TArray<TSharedPtr<FName>> Names;
	TWeakPtr<IPropertyHandle> RootPropertyHandle;
	TSubclassOf<UActorComponent> FilterClass;
	FText CurText;
	static TArray<FName> GetNames(AActor* Actor, TSubclassOf<AActor> InActorClass, TSubclassOf<UActorComponent> InClass, FName ExInclueName = NAME_None);

	template<typename T>
	static TArray<FName> GetNames(const TSubclassOf<AActor> InActorClass, FName ExInclueName = NAME_None)
	{
		return GetNames(InActorClass, T::StaticClass(), ExInclueName);
	}
};
#endif
