// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreUObject.h"

#include "Components/ActorComponent.h"

#include "ComponentPicker.generated.h"

USTRUCT(BlueprintType)
struct GENERICSTORAGES_API FComponentPicker
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "ComponentPicker")
	FName ComponentName;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "ComponentPicker")
	TSubclassOf<UActorComponent> ComponentClass;
#endif
	operator FName() const { return ComponentName; }

	UActorComponent* FindComponentByName(AActor* InActor) const;
	template<typename T>
	T* FindComponentByName(AActor* InActor) const
	{
		return FindComponentByName<T>(InActor, ComponentName);
	}

public:
	static UActorComponent* FindComponentByName(AActor* InActor, FName CompName, TSubclassOf<UActorComponent> InClass = {});

	template<typename T>
	static T* FindComponentByName(AActor* InActor, FName CompName)
	{
		static_assert(TIsDerivedFrom<T, UActorComponent>::IsDerived, "err");
		return Cast<T>(FindComponentByName(InActor, CompName, T::StaticClass()));
	}
};
