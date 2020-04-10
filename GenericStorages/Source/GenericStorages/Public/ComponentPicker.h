// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

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
	UPROPERTY(EditAnywhere, Category = "Config")
	FName ComponentName;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UActorComponent> ComponentClass;
#endif
	operator FName() const { return ComponentName; }

	UActorComponent* FindComponentByName(AActor* InActorz) const;
	template<typename T>
	T* FindComponentByName(AActor* InActor) const
	{
		return Cast<T>(FindComponentByName(InActor));
	}
};
