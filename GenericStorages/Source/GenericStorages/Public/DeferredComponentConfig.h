// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/World.h"
#include "Templates/SubclassOf.h"
#include "UnrealCompatibility.h"

#if UE_4_20_OR_LATER
#include "AI/NavigationSystemBase.h"
#endif

class AActor;
class UActorComponent;

namespace DeferredComponentRegistry
{
GENERICSTORAGES_API void AppendDeferredComponents(AActor& Actor);

// Default Hook On PostInitializeComponents or OnActorSpawned
template<typename T, typename V = void>
struct TOnComponentInitialized
{
	static constexpr bool bNeedInit = true;
	static void Bind()
	{
//#if WITH_DELEGATE_ON_ACTOR_COMPONENTS_INITIALIZED
#if 0
		// Modify Actor.cpp in Engine Source
		#if WITH_DELEGATE_ON_ACTOR_COMPONENTS_INITIALIZED
		DECLARE_DELEGATE_OneParam(FOnActorComponentsInitializedDelegate, AActor&)
		static FOnActorComponentsInitializedDelegate OnActorComponentsInitialized;
		ENGINE_API void SetOnActorComponentsInitializedDelegate(FOnActorComponentsInitializedDelegate Delegate) { OnActorComponentsInitialized = MoveTemp(Delegate); }
		#endif

		// add below to last of AActor::InitializeComponents
		#if WITH_DELEGATE_ON_ACTOR_COMPONENTS_INITIALIZED
		OnActorComponentsInitialized.ExecuteIfBound(*this);
		#endif
//#endif
		extern ENGINE_API void SetOnActorComponentsInitializedDelegate(TDelegate<void(AActor&)>);
		SetOnActorComponentsInitializedDelegate(TDelegate<void(AActor&)>::CreateStatic(&AppendDeferredComponents));
#elif UE_4_20_OR_LATER
		class UHackNavigationSystemBase : public UNavigationSystemBase
		{
		public:
			using UNavigationSystemBase::OnActorRegisteredDelegate;
		};

		if (UHackNavigationSystemBase::OnActorRegisteredDelegate().IsBound())
		{
			// PostInitializeComponents
			static auto Delegate = MoveTemp(UHackNavigationSystemBase::OnActorRegisteredDelegate());
			UHackNavigationSystemBase::OnActorRegisteredDelegate().BindLambda([](AActor& Actor) {
				AppendDeferredComponents(Actor);
				Delegate.Execute(Actor);
			});
		}
		else
#endif
		{
			// OnActorSpawned
			FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([](UWorld* World) {
				if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
					World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([](AActor* a) { AppendDeferredComponents(*a); }));
			});
		}
	}
};  // namespace DeferredComponentRegistry

#if 0
/*
Modify Actor in Engine Source

//declare static delegate in AActor
AActor
{
	DECLARE_DELEGATE_OneParam(FOnActorComponentsInitializedDelegate,AActor&)
	static FOnActorComponentsInitializedDelegate OnActorComponentsInitialized;
	static void SetOnActorComponentsInitializedDelegate(FOnActorComponentsInitializedDelegate Delegate)
	{
		OnActorComponentsInitialized = MoveTemp(Delegate);
	}
}

// add below to last of AActor::InitializeComponents
OnActorComponentsInitialized.ExecuteIfBound(*this)
*/

template<typename T>
struct TOnComponentInitialized<T, VoidType<decltype(&T::SetOnActorComponentsInitializedDelegate)>>
{
	static constexpr bool bNeedInit = false;
	static void Bind()
	{
		// add notify code in engine source in :InitializeComponents
		T::SetOnActorComponentsInitializedDelegate(TDelegate<void(AActor&)>::CreateStatic(&AppendDeferredComponents));
	}
};
#endif
}  // namespace DeferredComponentRegistry
