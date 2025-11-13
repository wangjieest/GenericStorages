// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ObjectPattern.h"

#include "ComponentPattern.generated.h"

UCLASS(Transient, meta = (NeuronAction))
class GENERICSTORAGES_API UComponentPattern final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	template<typename T, typename F>
	static void EachComponent(const UObject* WorldContextObj, const F& f)
	{
		static_assert(TIsDerivedFrom<T, UActorComponent>::IsDerived && !std::is_same<T, UActorComponent>::value, "err");
		UObjectPattern::EachObject<T>(WorldContextObj, f);
	}

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnEachComponentAction, UActorComponent*, Comp);
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Game", meta = (NeuronAction, DisplayName = "EachComponent", WorldContext = "WorldContextObj", HidePin = "WorldContextObj"))
	static void EachComponent(const UObject* WorldContextObj, TSubclassOf<UActorComponent> Class, UPARAM(meta = (DeterminesOutputType = "Class", DynamicOutputParam = "Comp")) FOnEachComponentAction OnEachComp)
	{
		if (ensure(Class))
		{
			UObjectPattern::EachObject(WorldContextObj, Class, [&](auto a) { OnEachComp.ExecuteIfBound(static_cast<UActorComponent*>(a)); });
		}
	}

	template<typename T>
	static T* GetComponent(const UObject* WorldContextObject)
	{
		static_assert(TIsDerivedFrom<T, UActorComponent>::IsDerived && !std::is_same<T, UActorComponent>::value, "err");
		return UObjectPattern::GetObject<T>(WorldContextObject);
	}

protected:
	UFUNCTION(BlueprintCallable, Category = "Game", meta = (DisplayName = "GetComponents", WorldContext = "WorldContextObj", HidePin = "WorldContextObj", DeterminesOutputType = "Class", DynamicOutputParam))
	static TArray<UActorComponent*> AllComponent(const UObject* WorldContextObj, TSubclassOf<UActorComponent> Class)
	{
		TArray<UActorComponent*> Ret;
		UObjectPattern::EachObject(WorldContextObj, Class, [&](auto a) { Ret.Add(static_cast<UActorComponent*>(a)); });
		return Ret;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetComponent", WorldContext = "WorldContextObj", HidePin = "WorldContextObj", DeterminesOutputType = "Class", DynamicOutputParam))
	static UActorComponent* GetComponent(const UObject* WorldContextObj, TSubclassOf<UActorComponent> Class) { return static_cast<UActorComponent*>(UObjectPattern::GetObject(WorldContextObj, Class)); }
};

template<typename T, typename V = void>
struct TEachComponentPattern : public TEachObjectPattern<T>
{
	TEachComponentPattern()
	{
		static_assert(TIsDerivedFrom<T, UActorComponent>::IsDerived && !std::is_same<T, UActorComponent>::value, "err");
		Ctor(static_cast<T*>(this));
	}
	~TEachComponentPattern() { Dtor(static_cast<T*>(this)); }

protected:
	using TEachObjectPattern<T>::Ctor;
	using TEachObjectPattern<T>::Dtor;
};

template<typename T, typename V = void>
struct TSingleComponentPattern : public TSingleObjectPattern<T>
{
	TSingleComponentPattern()
	{
		static_assert(TIsDerivedFrom<T, UActorComponent>::IsDerived && !std::is_same<T, UActorComponent>::value, "err");
		Ctor(static_cast<T*>(this));
	}
	~TSingleComponentPattern() { Dtor(static_cast<T*>(this)); }
};
