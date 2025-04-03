// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Templates/UnrealTypeTraits.h"
#include "UnrealCompatibility.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "WorldLocalStorages.generated.h"

#define WITH_LOCALSTORAGE_MULTIMODULE_SUPPORT WITH_EDITOR
namespace WorldLocalStorages
{
struct FLocalStorageOps;
struct Dummy
{
};
static auto StaticFuncAddRef = &AddStructReferencedObjectsOrNot<WorldLocalStorages::Dummy>;

template<typename U>
struct TContextPolicy;

template<>
struct TContextPolicy<UWorld>
{
	using CtxType = UWorld;
	static UWorld* GetCtx(const UObject* InCtx) { return (InCtx && InCtx->IsValidLowLevelFast()) ? InCtx->GetWorld() : nullptr; }
};
template<>
struct TContextPolicy<UGameInstance>
{
	using CtxType = UGameInstance;
	static UGameInstance* GetCtx(const UObject* InCtx)
	{
		auto World = TContextPolicy<UWorld>::GetCtx(InCtx);
		return World ? World->GetGameInstance() : nullptr;
	}
};

}  // namespace WorldLocalStorages

UINTERFACE(BlueprintType)
class UGenericInstanceInc : public UInterface
{
	GENERATED_BODY()
};

class IGenericInstanceInc
{
	GENERATED_BODY()
protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnGencreicInstanceCreated(UObject* InCtx);

	UFUNCTION(BlueprintImplementableEvent)
	void OnGencreicSingletonCreated(UObject* InCtx);
};

UCLASS(Transient)
class GENERICSTORAGES_API UGenericLocalStore : public UObject
{
	GENERATED_BODY()
public:
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
	{
		UGenericLocalStore* This = CastChecked<UGenericLocalStore>(InThis);
		This->AddReference(Collector);
		Super::AddReferencedObjects(InThis, Collector);
	}

	using CtxType = UObject;
	static UObject* GetCtx(const UObject* InCtx)
	{
		UWorld* World = WorldLocalStorages::TContextPolicy<UWorld>::GetCtx(InCtx);
		return World && World->IsGameWorld() ? GetPieObject(World) : World;
	}

	static UWorld* GetGameWorldChecked();

public:
	UPROPERTY(Transient)
	TArray<UObject*> ObjectHolders;

protected:
	static UObject* GetPieObject(const UWorld* InCtx);
	void AddReference(FReferenceCollector& Collector) { FuncAddRef(Store.Get(), Collector); }
	friend struct ::WorldLocalStorages::FLocalStorageOps;
	static void BindObjectReference(UObject* World, UObject* Obj);
	TSharedPtr<void> Store;
	decltype(WorldLocalStorages::StaticFuncAddRef) FuncAddRef = WorldLocalStorages::StaticFuncAddRef;
};

UCLASS(Transient)
class GENERICSTORAGES_API UGenericLocalStoreEditor : public UGenericLocalStore
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	static UObject* GetCtx(const UObject* InCtx)
	{
		if (GIsEditor && !InCtx)
		{
			return GetEditorWorld();
		}
		else
		{
			return Super::GetCtx(InCtx);
		}
	}

protected:
	static UWorld* GetEditorWorld(bool bEnsureIsGWorld = false);
#endif
};

template<typename T, typename V = void>
struct TTraitsWorldLocalStoragePOD
{
	enum
	{
		Value = TIsIntegral<T>::Value || TIsPointer<T>::Value
	};
};

namespace WorldLocalStorages
{
GENERICSTORAGES_API UObject* CreateInstanceImpl(const UObject* WorldContextObject, UClass* InCls);

template<typename T>
T* CreateInstance(const UObject* WorldContextObject, UClass* InCls = nullptr)
{
	auto Class = InCls;
#if WITH_EDITOR
	check(!Class || Class->IsChildOf<T>());
#endif
	Class = Class ? Class : T::StaticClass();
	return Cast<T>(CreateInstanceImpl(WorldContextObject, Class));
}

struct GENERICSTORAGES_API FLocalStorageOps
{
protected:
	template<typename K, typename F, typename U>
	static auto& FindOrAdd(K& ThisStorage, U* InCtx, const F& AddCell)
	{
		for (int32 i = 0; i < ThisStorage.Num(); ++i)
		{
			auto Ctx = ThisStorage[i].WeakCtx;
			if (!Ctx.IsStale(true))
			{
				if (InCtx == Ctx.Get())
					return ThisStorage[i].Value;
			}
			else
			{
				ThisStorage.RemoveAtSwap(i);
				--i;
			}
		}
		return AddCell();
	}
	template<typename K, typename U>
	static auto& FindOrAdd(K& ThisStorage, U* InCtx)
	{
		using RetType = decltype(Add_GetRef(ThisStorage).Value);
		return FindOrAdd(ThisStorage, InCtx, [&]() -> RetType& {
			auto& Ref = Add_GetRef(ThisStorage);
			if (InCtx)
				Ref.WeakCtx = InCtx;
			return Ref.Value;
		});
	}

	template<typename K, typename U>
	static void RemoveValue(K& ThisStorage, U* InCtx)
	{
		if (!InCtx)
			return;

		for (int32 i = 0; i < ThisStorage.Num(); ++i)
		{
			auto Ctx = ThisStorage[i].WeakCtx;
			if (!Ctx.IsStale(true))
			{
				if (InCtx == Ctx.Get())
				{
					ThisStorage.RemoveAtSwap(i);
					return;
				}
			}
			else
			{
				ThisStorage.RemoveAtSwap(i);
				--i;
			}
		}
	}
	static void BindObjectReference(UObject* Ctx, UObject* Obj)
	{
		if (auto Store = Cast<UGenericLocalStore>(Ctx))
		{
			Store->ObjectHolders.AddUnique(Obj);
		}
		else
		{
			UGenericLocalStore::BindObjectReference(Ctx, Obj);
		}
	}
	template<typename T>
	static void BindObjectReference(UObject* Ctx, UGenericLocalStore* Obj, TSharedRef<T>& Store)
	{
		Obj->FuncAddRef = &AddStructReferencedObjectsOrNot<T>;
		Obj->Store = Store;
		BindObjectReference(Ctx, Obj);
	}

#if WITH_LOCALSTORAGE_MULTIMODULE_SUPPORT
private:
	friend struct FStorageUtils;
	static void* GetStorageImpl(const UWorld* InCtx, const char* TypeName, TFunctionRef<void*()> InCtor, void (*InDtor)(void*));
	static void* GetStorageImpl(const UGameInstance* InCtx, const char* TypeName, TFunctionRef<void*()> InCtor, void (*InDtor)(void*));
	static void* GetStorageImpl(const UObject* InCtx, const char* TypeName, TFunctionRef<void*()> InCtor, void (*InDtor)(void*));

protected:
	template<typename TRet, typename T, typename CtxType>
	static TRet& GetStorage(const CtxType* InCtx)
	{
		auto Ret = GetStorageImpl(
			InCtx,
			ITS::TypeStr<T>(),
			[]() -> void* { return new TRet(); },
			[](void* Data) { delete reinterpret_cast<TRet*>(Data); });
		return *reinterpret_cast<TRet*>(Ret);
	}
#endif
};

template<typename T, uint8 N = 4, typename P = TContextPolicy<UObject>, typename V = void>
struct TGenericLocalStorage;

// UObject
template<typename T, uint8 N, typename P>
struct TGenericLocalStorage<T, N, P, typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived>::Type> : public FLocalStorageOps
{
public:
	T* GetLocalValue(const UObject* WorldContextObj, bool bCreate = true)
	{
		auto Ctx = P::GetCtx(WorldContextObj);
		check(!Ctx || IsValid(Ctx));
		auto& Ptr = FLocalStorageOps::FindOrAdd(GetStorage(Ctx), Ctx);
		if (!Ptr.IsValid() && bCreate)
		{
			auto Obj = static_cast<T*>(CreateInstanceImpl(Ctx, T::StaticClass()));
			check(Obj);
			Ptr = Obj;
			FLocalStorageOps::BindObjectReference(Ctx, Obj);
			return Obj;
		}
		return Ptr.Get();
	}

	void RemoveLocalValue(const UObject* WorldContextObj)
	{
		auto Ctx = P::GetCtx(WorldContextObj);
		FLocalStorageOps::RemoveValue(GetStorage(Ctx), Ctx);
	}

	template<typename F, typename = std::enable_if_t<!std::is_same<F, bool>::value>>
	T* GetLocalValue(const UObject* WorldContextObj, F&& f)
	{
		auto Ctx = P::GetCtx(WorldContextObj);
		check(!Ctx || IsValid(Ctx));
		auto& Ptr = FLocalStorageOps::FindOrAdd(GetStorage(Ctx), Ctx);
		if (!Ptr.IsValid())
		{
			auto Obj = f();
			check(Obj);
			Ptr = Obj;
			FLocalStorageOps::BindObjectReference(Ctx, Obj);
			return Obj;
		}
		return Ptr.Get();
	}

protected:
	struct FStorePair
	{
		TWeakObjectPtr<typename P::CtxType> WeakCtx;
		TWeakObjectPtr<T> Value;
	};

#if WITH_LOCALSTORAGE_MULTIMODULE_SUPPORT
	template<typename U>
	TArray<FStorePair, TInlineAllocator<N>>& GetStorage(const U* WorldContextObj)
	{
		return FLocalStorageOps::GetStorage<TArray<FStorePair, TInlineAllocator<N>>, T>(WorldContextObj);
	}
#else
	TArray<FStorePair, TInlineAllocator<N>> Storage;
	TArray<FStorePair, TInlineAllocator<N>>& GetStorage(const UObject* WorldContextObj) { return Storage; }
#endif
};

// Struct
template<typename T, uint8 N, typename P>
struct TGenericLocalStorage<T, N, P, typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived && !TTraitsWorldLocalStoragePOD<T>::Value>::Type> : public FLocalStorageOps
{
public:
	template<typename... TArgs>
	T& GetLocalValue(const UObject* WorldContextObj, TArgs&&... Args)
	{
		auto Ctx = P::GetCtx(WorldContextObj);
		check(!Ctx || IsValid(Ctx));
		auto& Ptr = FLocalStorageOps::FindOrAdd(GetStorage(Ctx), Ctx);
		if (!Ptr.IsValid())
		{
			auto Obj = NewObject<UGenericLocalStore>();
			auto SP = MakeShared<T>(Forward<TArgs>(Args)...);
			Ptr = SP;
			FLocalStorageOps::BindObjectReference(Ctx, Obj, SP);
			return *SP;
		}
		return *Ptr.Pin();
	}

	void RemoveLocalValue(const UObject* WorldContextObj)
	{
		auto Ctx = P::GetCtx(WorldContextObj);
		FLocalStorageOps::RemoveValue(GetStorage(Ctx), Ctx);
	}

protected:
	struct FStorePair
	{
		TWeakObjectPtr<typename P::CtxType> WeakCtx;
		TWeakPtr<T> Value;
	};
#if WITH_LOCALSTORAGE_MULTIMODULE_SUPPORT
	template<typename U>
	TArray<FStorePair, TInlineAllocator<N>>& GetStorage(const U* WorldContextObj)
	{
		return FLocalStorageOps::GetStorage<TArray<FStorePair, TInlineAllocator<N>>, T>(WorldContextObj);
	}
#else
	TArray<FStorePair, TInlineAllocator<N>> Storage;
	TArray<FStorePair, TInlineAllocator<N>>& GetStorage(const UObject* WorldContextObj) { return Storage; }
#endif
};

// PODs
template<typename T, uint8 N, typename P>
struct TGenericLocalStorage<T, N, P, typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived && TTraitsWorldLocalStoragePOD<T>::Value>::Type> : public FLocalStorageOps
{
public:
	template<typename... U>
	T& GetLocalValue(const UObject* WorldContextObj, U&&... Val)
	{
		static_assert(TIsPODType<T>::Value, "err");
		auto Ctx = P::GetCtx(WorldContextObj);
		check(!Ctx || IsValid(Ctx));
		auto& StorageRef = GetStorage(Ctx);
		return FLocalStorageOps::FindOrAdd(StorageRef, Ctx, [&]() -> T& {
			auto& Ref = Add_GetRef(StorageRef);
			Ref.WeakCtx = Ctx;
			Ref.Value = T(Forward<U>(Val)...);
			return Ref.Value;
		});
	}

	void RemoveLocalValue(const UObject* WorldContextObj)
	{
		auto Ctx = P::GetCtx(WorldContextObj);
		FLocalStorageOps::RemoveValue(GetStorage(Ctx), Ctx);
	}

	// return true after set val when not equal
	template<typename... U>
	bool CompareAndSet(const UObject* WorldContextObj, const T& ValToCmp, U&&... Val)
	{
		auto& Ref = GetLocalValue(WorldContextObj, Forward<U>(Val)...);
		if (ValToCmp == Ref)
			return false;
		Ref = ValToCmp;
		return true;
	}

protected:
	struct FStorePair
	{
		TWeakObjectPtr<typename P::CtxType> WeakCtx;
		T Value;
	};
#if WITH_LOCALSTORAGE_MULTIMODULE_SUPPORT
	template<typename U>
	TArray<FStorePair, TInlineAllocator<N>>& GetStorage(const U* WorldContextObj)
	{
		return FLocalStorageOps::GetStorage<TArray<FStorePair, TInlineAllocator<N>>, T>(WorldContextObj);
	}
#else
	TArray<FStorePair, TInlineAllocator<N>> Storage;
	TArray<FStorePair, TInlineAllocator<N>>& GetStorage(const UObject* WorldContextObj) { return Storage; }
#endif
};
//////////////////////////////////////////////////////////////////////////
template<typename T, uint8 N = 4, typename P = TContextPolicy<UGameInstance>, typename V = void>
using TGenericGameLocalStorage = TGenericLocalStorage<T, N, P, V>;

template<typename T, uint8 N = 4, typename P = TContextPolicy<UWorld>, typename V = void>
using TGenericWorldLocalStorage = TGenericLocalStorage<T, N, P, V>;

template<typename T, uint8 N = 4, typename P = UGenericLocalStore, typename V = void>
using TGenericPieLocalStorage = WorldLocalStorages::TGenericLocalStorage<T, N, P, V>;

namespace Internal
{
	template<typename T>
	TGenericWorldLocalStorage<T> GlobalWorldStorage;
}  // namespace Internal

template<typename T, typename... TArgs>
decltype(auto) GetLocalValue(const UObject* Context, TArgs&&... Args)
{
	return Internal::GlobalWorldStorage<T>.GetLocalValue(Context, Forward<TArgs>(Args)...);
}
template<typename T>
void RemoveLocalValue(const UObject* Context)
{
	return Internal::GlobalWorldStorage<T>.RemoveLocalValue(Context);
}

template<typename T, typename... TArgs>
decltype(auto) GetCurrentLocalValue(TArgs&&... Args)
{
	return Internal::GlobalWorldStorage<T>.GetLocalValue(UGenericLocalStore::GetGameWorldChecked(), Forward<TArgs>(Args)...);
}

#if 0
// static usage : world local storage
static TGenericLocalStorage<T> WorldLocalStorage;
#endif
}  // namespace WorldLocalStorages

//////////////////////////////////////////////////////////////////////////
namespace GenericLocalStorages
{
namespace Internal
{
	template<typename T>
	WorldLocalStorages::TGenericGameLocalStorage<T> GlobalGameStorage;
	//////////////////////////////////////////////////////////////////////////

	template<typename T>
	WorldLocalStorages::TGenericPieLocalStorage<T> GlobalPieLocalStorage;
}  // namespace Internal
	
template<typename T, typename... TArgs>
decltype(auto) GetPieLocalValue(const UObject* Context, TArgs&&... Args)
{
	return Internal::GlobalPieLocalStorage<T>.GetLocalValue(Context, Forward<TArgs>(Args)...);
}
template<typename T>
void RemovePieLocalValue(const UObject* Context)
{
	return Internal::GlobalPieLocalStorage<T>.RemoveLocalValue(Context);
}
//////////////////////////////////////////////////////////////////////////
template<typename T, typename... TArgs>
decltype(auto) GetGameValue(const UObject* Context, TArgs&&... Args)
{
	return Internal::GlobalGameStorage<T>.GetLocalValue(Context, Forward<TArgs>(Args)...);
}
template<typename T>
void RemoveGameValue(const UObject* Context)
{
	return Internal::GlobalGameStorage<T>.RemoveLocalValue(Context);
}
template<typename T, typename... TArgs>
decltype(auto) GetCurrentGameValue(TArgs&&... Args)
{
	return Internal::GlobalGameStorage<T>.GetLocalValue(UGenericLocalStore::GetGameWorldChecked(), Forward<TArgs>(Args)...);
}

}  // namespace GenericStorages
