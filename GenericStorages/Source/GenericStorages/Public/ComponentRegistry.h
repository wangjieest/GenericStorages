// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ObjectRegistry.h"

#include "ComponentRegistry.generated.h"

UCLASS(Transient)
class GENERICSTORAGES_API UComponentRegistry final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	template<typename T, typename F>
	static void EachComponent(const UObject* WorldContextObj, const F& f)
	{
		static_assert(TIsDerivedFrom<T, UActorComponent>::IsDerived, "err");
		UObjectRegistry::EachObject<T>(WorldContextObj, f);
	}

	template<typename T>
	static T* GetComponent(const UObject* WorldContextObject)
	{
		static_assert(TIsDerivedFrom<T, UActorComponent>::IsDerived, "err");
		return UObjectRegistry::GetObject<T>(WorldContextObject);
	}

protected:
	UFUNCTION(BlueprintCallable, Category = "Game", meta = (DisplayName = "GetComponents", WorldContext = "WorldContextObj", HidePin = "WorldContextObj", DeterminesOutputType = "Class", DynamicOutputParam))
	static TArray<UActorComponent*> AllComponent(const UObject* WorldContextObj, TSubclassOf<UActorComponent> Class)
	{
		TArray<UActorComponent*> Ret;
		UObjectRegistry::EachObject(WorldContextObj, Class, [&](auto a) { Ret.Add(static_cast<UActorComponent*>(a)); });
		return Ret;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetComponent", WorldContext = "WorldContextObj", HidePin = "WorldContextObj", DeterminesOutputType = "Class", DynamicOutputParam))
	static UActorComponent* GetComponent(const UObject* WorldContextObj, TSubclassOf<UActorComponent> Class) { return static_cast<UActorComponent*>(UObjectRegistry::GetObject(WorldContextObj, Class)); }
};

template<typename T, typename V = void>
struct TEachComponentRegister : public TEachObjectHelper<T>
{
	TEachComponentRegister()
	{
		static_assert(TIsDerivedFrom<T, UActorComponent>::IsDerived, "err");
		Ctor(static_cast<T*>(this));
	}
	~TEachComponentRegister() { Dtor(static_cast<T*>(this)); }

protected:
	using TEachObjectHelper<T>::Ctor;
	using TEachObjectHelper<T>::Dtor;
};

template<typename T, typename V = void>
struct TSingleComponentRegister : public TSingleObjectHelper<T>
{
	TSingleComponentRegister()
	{
		static_assert(TIsDerivedFrom<T, UActorComponent>::IsDerived, "err");
		Ctor(static_cast<T*>(this));
	}
	~TSingleComponentRegister() { Dtor(static_cast<T*>(this)); }
};
