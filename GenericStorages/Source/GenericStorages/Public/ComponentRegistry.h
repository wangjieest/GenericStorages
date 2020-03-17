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
