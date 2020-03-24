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

#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Templates/UnrealTypeTraits.h"

#include "WorldLocalStorages.generated.h"

UCLASS()
class UGenericWorldLocalStore : public UObject
{
	GENERATED_BODY()
public:
	// TODO : AddReferencedObjects
	TSharedPtr<void> Store;
};

namespace WorldLocalStorages
{
template<typename T, uint8 N = 4, typename V = void>
struct TGenericWorldLocalStorage;

static inline UGameInstance* FindGameInstance()
{
	UGameInstance* Instance = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		ensureAlwaysMsgf(!GIsInitialLoad && GEngine, TEXT("Is it needed to get singleton before engine initialized?"));
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			auto CurWorld = Context.World();
			if (IsValid(CurWorld))
			{
				if (CurWorld->IsGameWorld())
				{
					if (Context.WorldType == EWorldType::PIE /*&& Context.PIEInstance == 0*/)
					{
						World = CurWorld;
						break;
					}

					if (Context.WorldType == EWorldType::Game)
					{
						World = CurWorld;
						break;
					}

					if (CurWorld->GetNetMode() == ENetMode::NM_Standalone || (CurWorld->GetNetMode() == ENetMode::NM_Client && Context.PIEInstance == 2))
					{
						World = CurWorld;
						break;
					}
				}
			}
		}
		Instance = World ? World->GetGameInstance() : nullptr;
	}
	else
#endif
	{
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			Instance = GameEngine->GameInstance;
		}
	}
	return Instance;
}

struct FWorldLocalStorageOps
{
protected:
	template<typename K>
	static auto& FindOrAdd(K& ThisStorage, UWorld* InWorld)
	{
		for (int32 i = 0; i < ThisStorage.Num(); ++i)
		{
			auto World = ThisStorage[i].WeakWorld;
			if (!World.IsStale(true))
			{
				if (InWorld == World.Get())
					return ThisStorage[i].Object;
			}
			else
			{
				ThisStorage.RemoveAt(i);
				--i;
			}
		}
		auto& Ref = ThisStorage.AddDefaulted_GetRef();
		if (IsValid(InWorld))
			Ref.WeakWorld = InWorld;
		return Ref.Object;
	}
	template<typename K>
	static void Remove(K& ThisStorage, UWorld* InWorld)
	{
		if (!IsValid(InWorld))
			return;

		for (int32 i = 0; i < ThisStorage.Num(); ++i)
		{
			auto World = ThisStorage[i].WeakWorld;
			if (!World.IsStale(true))
			{
				if (InWorld == World.Get())
				{
					ThisStorage.RemoveAt(i);
					return;
				}
			}
			else
			{
				ThisStorage.RemoveAt(i);
				--i;
			}
		}
	}
};

// UObject
template<typename T, uint8 N>
struct TGenericWorldLocalStorage<T, N, typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived>::Type> : public FWorldLocalStorageOps
{
public:
	T* GetObject(const UObject* WorldContextObj)
	{
		UWorld* World = WorldContextObj ? WorldContextObj->GetWorld() : nullptr;
		return GetObject(World);
	}

	void Remove(const UObject* WorldContextObj)
	{
		UWorld* World = WorldContextObj ? WorldContextObj->GetWorld() : nullptr;
		FWorldLocalStorageOps::Remove(Storage, World);
	}

protected:
	T* GetObject(UWorld* World)
	{
		check(!World || IsValid(World));
		auto& Ptr = FindOrAdd(Storage, World);
		if (!Ptr.IsValid())
		{
			if (!IsValid(World))
			{
				auto Instance = FindGameInstance();
				ensureMsgf(GIsEditor || Instance != nullptr, TEXT("Instance Error"));
				auto Obj = NewObject<T>();
				Ptr = Obj;
				if (Instance)
				{
					Instance->RegisterReferencedObject(Obj);
				}
				else
				{
					Obj->AddToRoot();
				}
			}
			else
			{
				auto Obj = NewObject<T>(World);
				Ptr = Obj;
				World->ExtraReferencedObjects.AddUnique(Obj);
			}
			check(Ptr.IsValid());
		}
		return Ptr.Get();
	}

	struct FStorePair
	{
		TWeakObjectPtr<UWorld> WeakWorld;
		TWeakObjectPtr<T> Object;
	};
	TArray<FStorePair, TInlineAllocator<N>> Storage;
};

// Struct
template<typename T, uint8 N>
struct TGenericWorldLocalStorage<T, N, typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived>::Type> : public FWorldLocalStorageOps
{
public:
	T* GetObject(const UObject* WorldContextObj)
	{
		UWorld* World = WorldContextObj ? WorldContextObj->GetWorld() : nullptr;
		return GetObject(World);
	}

	void Remove(const UObject* WorldContextObj)
	{
		UWorld* World = WorldContextObj ? WorldContextObj->GetWorld() : nullptr;
		FWorldLocalStorageOps::Remove(Storage, World);
	}

protected:
	template<typename... TArgs>
	T* GetObject(UWorld* World, TArgs&&... Args)
	{
		check(!World || IsValid(World));
		auto& Ptr = FindOrAdd(Storage, World);
		if (!Ptr.IsValid())
		{
			auto Obj = NewObject<UGenericWorldLocalStore>();
			auto SP = MakeShared<T>(Forward<TArgs>(Args)...);
			check(SP);
			Ptr = SP;
			Obj->Store = SP;
			if (!IsValid(World))
			{
				auto Instance = FindGameInstance();
				ensureMsgf(GIsEditor || Instance != nullptr, TEXT("Instance Error"));
				if (Instance)
				{
					Instance->RegisterReferencedObject(Obj);
				}
				else
				{
					Obj->AddToRoot();
				}
			}
			else
			{
				World->ExtraReferencedObjects.AddUnique(Obj);
			}
		}
		return SP.Get();
	}

	struct FStorePair
	{
		TWeakObjectPtr<UWorld> WeakWorld;
		TWeakPtr<T> Object;
	};
	TArray<FStorePair, TInlineAllocator<N>> Storage;
};

//////////////////////////////////////////////////////////////////////////
template<typename T>
TGenericWorldLocalStorage<T> Storage;

template<typename T, typename... TArgs>
T* GetObject(const UObject* Context, TArgs&&... Args)
{
	return Storage<T>.GetObject(Context, Forward<TArgs>(Args)...);
}
template<typename T>
void RemoveObject(const UObject* Context)
{
	return Storage<T>.Remove(Context);
}

//////////////////////////////////////////////////////////////////////////
#if 0
// world local storage
static TGenericWorldLocalStorage<T> WorldLocalStorage;
#endif
};  // namespace WorldLocalStorages
