// Copyright GenericStorages, Inc. All Rights Reserved.

#include "MemberDataRegistry.h"
#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/World.h"
#include "GenericSingletons.h"
#include "UObject/UObjectThreadContext.h"
#endif

FWeakMemberDataTag::FWeakMemberDataTag()
{
#if WITH_EDITOR
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	UObject* Initializer = ThreadContext.IsInConstructor ? ThreadContext.TopInitializer()->GetObj() : nullptr;
	if (!Outer.IsValid())
		Outer = Initializer;
#endif
}
