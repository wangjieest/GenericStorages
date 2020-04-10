// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#include "ComponentPicker.h"

#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"
#include "UnrealCompatibility.h"

UActorComponent* FComponentPicker::FindComponentByName(AActor* InActor) const
{
	if (!InActor || !ComponentName.IsValid() || ComponentName.IsNone())
		return nullptr;

	FObjectPropertyBase* ObjProp = FindFProperty<FObjectPropertyBase>(InActor->GetClass(), ComponentName);
	if (ObjProp)
	{
		return Cast<UActorComponent>(ObjProp->GetObjectPropertyValue_InContainer(InActor));
	}

	for (auto& Comp : InActor->GetComponents())
	{
		if (Comp->GetFName() == ComponentName)
			return Comp;
	}

	return nullptr;
}
