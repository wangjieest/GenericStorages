/****************************************************************************
Copyright (c) 2017-2027 GenericStorages

author: wangjieest

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

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
