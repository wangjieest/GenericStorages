// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AI/NavigationSystemBase.h"
#include "Engine/World.h"
#include "Templates/SubclassOf.h"
#include "GS_TypeTraits.h"

class AActor;
class UActorComponent;

namespace ComponentBookKeeper
{
GENERICSTORAGES_API void AppendDeferComponents(AActor& Actor);

template<typename T, typename V = void>
struct TOnComponentInitialized
{
	static constexpr bool bNeedInit = true;
	static void Bind()
	{
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
				AppendDeferComponents(Actor);
				Delegate.Execute(Actor);
			});
		}
		else
		{
			// OnActorSpawned
			FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([](UWorld* World) { World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([](AActor* a) { AppendDeferComponents(*a); })); });
		}
	}
};

template<typename T>
struct TOnComponentInitialized<T, VoidType<decltype(&T::SetOnComponentInitializedDelegate)>>
{
	static constexpr bool bNeedInit = false;
	static void Bind()
	{
		// add notify code in engine source in :InitializeComponents
		T::SetOnComponentInitializedDelegate(TBaseDelegate<void, AActor&>::CreateStatic(&AppendDeferComponents));
	}
};

}  // namespace ComponentBookKeeper
