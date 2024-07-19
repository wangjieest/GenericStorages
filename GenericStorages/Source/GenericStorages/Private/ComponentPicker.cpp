// Copyright GenericStorages, Inc. All Rights Reserved.

#include "ComponentPicker.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"
#include "Template/UnrealCompatibility.h"

UActorComponent* FComponentPicker::FindComponentByName(AActor* InActor) const
{
	return FindComponentByName(InActor, ComponentName);
}

UActorComponent* FComponentPicker::FindComponentByName(AActor* InActor, FName CompName, TSubclassOf<UActorComponent> InClass)
{
	if (!InActor || !CompName.IsValid() || CompName.IsNone())
		return nullptr;

	auto ActorClass = InActor->GetClass();
	if (InActor->HasAnyFlags(RF_ClassDefaultObject))
	{
		if (UBlueprintGeneratedClass* BBGClass = Cast<UBlueprintGeneratedClass>(ActorClass))
		{
			const TArray<USCS_Node*>& ActorBlueprintNodes = BBGClass->SimpleConstructionScript->GetAllNodes();
			for (USCS_Node* Node : ActorBlueprintNodes)
			{
				if (Node->GetVariableName() == CompName && (!InClass || Node->ComponentClass->IsChildOf(InClass)))
				{
					return Node->GetActualComponentTemplate(BBGClass);
				}
			}
		}
	}

	FObjectPropertyBase* ObjProp = FindFProperty<FObjectPropertyBase>(ActorClass, CompName);
	if (ObjProp)
	{
		return Cast<UActorComponent>(ObjProp->GetObjectPropertyValue_InContainer(InActor));
	}

	for (auto& Comp : InActor->GetComponents())
	{
		if (Comp->GetFName() == CompName && !(InClass || Comp->IsA(InClass.Get())))
		{
			return Comp;
		}
	}

	return nullptr;
}
