// Copyright UnrealCompatibility, Inc. All Rights Reserved.

#pragma once

#if !defined(UNREAL_COMPONENT_COMPATIBILITY_GUARD_H)
#define UNREAL_COMPONENT_COMPATIBILITY_GUARD_H

#include "Launch/Resources/Version.h"
#include "PrivateFieldAccessor.h"
#include "UObject/UnrealType.h"
//
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#if UE_4_24_OR_LATER
inline void SetIsReplicatedByDefault(UActorComponent* Comp, const bool bNewReplicates)
{
	if (LIKELY(Comp->HasAnyFlags(RF_NeedInitialization)))
	{
		Comp->bReplicates = bNewReplicates;
	}
	else
	{
		ensureMsgf(false, TEXT("SetIsReplicatedByDefault should only be called during Component Construction. Class=%s"), *GetPathNameSafe(Comp->GetClass()));
		SetIsReplicated(bNewReplicates);
	}
}

inline void SetHidden(AActor* Actor, bool bInHidden)
{
	Actor->bHidden = bInHidden;
}

inline void SetReplicatingMovement(AActor* Actor, bool bInReplicateMovement)
{
	Actor->bReplicateMovement = bInReplicateMovement;
}

inline void SetCanBeDamaged(AActor* Actor, bool bInCanBeDamaged)
{
	Actor->bCanBeDamaged = bInCanBeDamaged;
}

inline void SetRole(AActor* Actor, ENetRole InRole)
{
	Actor->Role = InRole;
}

inline FRepMovement& GetReplicatedMovement_Mutable(AActor* Actor)
{
	return Actor->ReplicatedMovement;
}

inline void SetReplicatedMovement(AActor* Actor, const FRepMovement& InReplicatedMovement)
{
	GetReplicatedMovement_Mutable(Actor) = InReplicatedMovement;
}

inline void SetInstigator(AActor* Actor, APawn* InInstigator)
{
	Actor->Instigator = InInstigator;
}
inline APawn* GetInstigator(const AActor* Actor)
{
	return Actor->Instigator;
}

inline AController* GetInstigatorController(const AActor* Actor)
{
	return Actor->Instigator ? Actor->Instigator->Controller : nullptr;
}

#else  // NGINE_MINOR_VERSION >= 24

inline void SetIsReplicatedByDefault(UActorComponent* Comp, const bool bNewReplicates)
{
	struct UHookActorComponent : public UActorComponent
	{
		using UActorComponent::SetIsReplicatedByDefault;
	};
	static_cast<UHookActorComponent*>(Comp)->SetIsReplicatedByDefault(bNewReplicates);
}

inline void SetHidden(AActor* Actor, bool bInHidden)
{
	Actor->SetHidden(bInHidden);
}

inline void SetReplicatingMovement(AActor* Actor, bool bInReplicateMovement)
{
	Actor->SetReplicatingMovement(bInReplicateMovement);
}

inline void SetCanBeDamaged(AActor* Actor, bool bInCanBeDamaged)
{
	Actor->SetCanBeDamaged(bInCanBeDamaged);
}

inline void SetRole(AActor* Actor, ENetRole InRole)
{
	Actor->SetRole(InRole);
}

inline FRepMovement& GetReplicatedMovement_Mutable(AActor* Actor)
{
	return Actor->GetReplicatedMovement_Mutable();
}

inline void SetReplicatedMovement(AActor* Actor, const FRepMovement& InReplicatedMovement)
{
	Actor->SetReplicatedMovement(InReplicatedMovement);
}

inline void SetInstigator(AActor* Actor, APawn* InInstigator)
{
	Actor->SetInstigator(InInstigator);
}
inline APawn* GetInstigator(const AActor* Actor)
{
	return Actor->GetInstigator();
}

inline AController* GetInstigatorController(const AActor* Actor)
{
	return Actor->GetInstigatorController();
}
#endif

#endif  // !defined(UNREAL_COMPONENT_COMPATIBILITY_GUARD_H)
