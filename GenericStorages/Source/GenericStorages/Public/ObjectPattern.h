// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/EngineTypes.h"
#include "Templates/SubclassOf.h"

#include "ObjectPattern.generated.h"

template<typename T>
struct TEachObjectPattern;

USTRUCT()
struct GENERICSTORAGES_API FObjectPatternType
{
	GENERATED_BODY()
public:
	using FWeakObjectArray = TArray<FWeakObjectPtr>;
	TSharedPtr<FWeakObjectArray> Objects;
	bool IsValid() const { return Objects.IsValid(); }
	UObject* FirstObject() const
	{
		check(IsValid());
		return Objects->Num() > 0 ? (*Objects)[0].Get() : nullptr;
	}

	void Add(UObject* Obj) { Objects->Add(Obj); }
	void Remove(UObject* Obj) { Objects->Remove(Obj); }
	auto Find(UObject* Obj) { return Objects->Find(FWeakObjectPtr(Obj)); }

	FWeakObjectArray::TIterator CreateIterator();
};

//////////////////////////////////////////////////////////////////////////

UCLASS(Transient, meta = (NeuronAction, AsSingleton))
class GENERICSTORAGES_API UObjectPattern : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
protected:
	UObjectPattern();
	static UObjectPattern* Get(const UObject* Obj, bool bCreate = true);

	static UClass* FindFirstNativeClass(UClass* Class);

public:
	UFUNCTION(meta = (DisplayName = "GetObjects", WorldContext = "WorldContextObj", HidePin = "WorldContextObj", DeterminesOutputType = "Class", DynamicOutputParam))
	static TArray<UObject*> AllObject(const UObject* WorldContextObj, UClass* Class);

public:
	template<typename T, typename F>
	static void EachObject(const UObject* WorldContextObj, const F& f);

	static void EachObject(const UObject* WorldContextObj, UClass* Class, const TFunctionRef<void(UObject*)>& f);

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnEachObjectAction, UObject*, Obj);
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category = "Game", meta = (NeuronAction, DisplayName = "EachObject", WorldContext = "WorldContextObj", HidePin = "WorldContextObj"))
	static void EachObject(const UObject* WorldContextObj, UClass* Class, UPARAM(meta = (DeterminesOutputType = "Class", DynamicOutputParam = "Obj")) FOnEachObjectAction OnEachObj)
	{
		if (ensure(Class))
		{
			UObjectPattern::EachObject(WorldContextObj, Class, [&](auto a) { OnEachObj.ExecuteIfBound(a); });
		}
	}
	template<typename T>
	static T* GetObject(const UObject* WorldContextObj)
	{
		static_assert(TIsDerivedFrom<T, UObject>::IsDerived && !TIsSame<T, UObject>::Value, "err");
		if (!EditorIsGameWorld(WorldContextObj))
			return nullptr;

#if WITH_EDITOR
		if (auto Object = static_cast<T*>(GetObject(WorldContextObj, T::StaticClass())))
		{
			ensure(GIsEditor || TypeObject<T> == Object);
			return Object;
		}
#else
		if (ensure(TypeObject<T>.IsValid()))
			return TypeObject<T>.Get();
#endif
		return nullptr;
	}

public:
#if WITH_EDITOR
	static bool EditorIsGameWorld(const UObject* WorldContextObj);
#else
	static bool EditorIsGameWorld(const UObject* WorldContextObj) { return true; }
#endif

	template<typename T>
	static void SetObject(T* Obj)
	{
		if (!EditorIsGameWorld(Obj))
			return;

		if (!Obj->HasAnyFlags(RF_ArchetypeObject))
		{
#if WITH_EDITOR
			static_assert(TIsDerivedFrom<T, UObject>::IsDerived && !TIsSame<T, UObject>::Value, "err");
			check(IsValid(Obj));
			check(Obj->IsA(T::StaticClass()));
#if 0
			static_assert(std::is_final<T>::value, "err");
			check(FindFirstNativeClass(Obj->GetClass()) == T::StaticClass());
#endif
#endif
			ensure(GIsEditor || !TypeObject<T>.IsValid());
			TypeObject<T> = Obj;

			SetObject(Obj, T::StaticClass());
		}
	}

protected:
	friend class UComponentPattern;
	UFUNCTION(BlueprintCallable, Category = "Game", meta = (DisplayName = "GetObject", WorldContext = "WorldContextObj", HidePin = "WorldContextObj", DeterminesOutputType = "Class", DynamicOutputParam))
	static UObject* GetObject(const UObject* WorldContextObj, UClass* Class);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetObject", WorldContext = "Object", HidePin = "StopClass"), Category = "Game")
	static void SetObject(UObject* Object, UClass* StopClass = nullptr);

	template<typename T>
	static int32 GetTypeID()
	{
#if WITH_EDITOR
		ensure(TypeID<T> != 0 && GetTypeID(T::StaticClass()) == TypeID<T>);
#endif
		return TypeID<T>;
	}

	static int32 GetTypeID(UClass* Class);

	template<typename T>
	static auto NativeIterator()
	{
		auto Index = GetTypeID<T>();
		static_assert(TIsDerivedFrom<T, TEachObjectPattern<T>>::IsDerived, "err");
		static_assert(TIsDerivedFrom<T, UObject>::IsDerived && !TIsSame<T, UObject>::Value, "err");
		check(Index);
		return NativeIterator(Index);
	}
	static FObjectPatternType::FWeakObjectArray::TIterator NativeIterator(int32 Index);

	//////////////////////////////////////////////////////////////////////////
	template<typename T>
	friend struct TEachObjectPattern;
	template<typename T>
	friend struct TSingleObjectPattern;
	template<typename T>
	static void Register(T* Obj)
	{
#if WITH_EDITOR
		static_assert(TIsDerivedFrom<T, UObject>::IsDerived && !TIsSame<T, UObject>::Value, "err");
		check(IsValid(Obj));
		check(Obj->IsA(T::StaticClass()));
		if (Obj->HasAnyFlags(RF_Transactional))
			return;
#if 0
		static_assert(std::is_final<T>::value, "err");
		check(FindFirstNativeClass(Obj->GetClass()) == T::StaticClass());
#endif
#endif

		if (Obj->HasAnyFlags(RF_ArchetypeObject | RF_DefaultSubObject))
		{
			AddClassToRegistry(Obj, T::StaticClass(), TypeID<T>);
			ensure(TypeID<T> != 0);
		}
		else if (EditorIsGameWorld(Obj))
		{
			if (auto Mgr = Get(Obj))
				Mgr->AddObjectToRegistry(Obj, T::StaticClass());
		}
	}

	template<typename T>
	static void Unregister(T* Obj)
	{
		if (UObjectInitialized())
		{
#if WITH_EDITOR
			if (Obj->HasAnyFlags(RF_Transactional))
				return;
#endif
			if (Obj->HasAnyFlags(RF_ArchetypeObject | RF_DefaultSubObject))
			{
				if (!Obj->GetClass()->IsNative())
					RemoveClassFromRegistry(Obj, T::StaticClass());
			}
			else if (EditorIsGameWorld(Obj))
			{
				if (auto Mgr = Get(Obj))
					Mgr->RemoveObjectFromRegistry(Obj, T::StaticClass());
			}
		}
	}

	template<typename T>
	static void UnsetObject(T* Obj)
	{
		if (!EditorIsGameWorld(Obj))
			return;

		if (!Obj->HasAnyFlags(RF_ArchetypeObject))
		{
#if WITH_EDITOR
			static_assert(TIsDerivedFrom<T, UObject>::IsDerived && !TIsSame<T, UObject>::Value, "err");
			check(Obj->IsA(T::StaticClass()));

#if 0
			static_assert(std::is_final<T>::value, "err");
			check(FindFirstNativeClass(Obj->GetClass()) == T::StaticClass());
#endif
			if (GIsEditor)
			{
				if (auto Mgr = Get(Obj))
				{
					auto& Ref = Mgr->TypeObjects.FindOrAdd(T::StaticClass());
					ensure(Obj == Ref.Get());
					Ref = nullptr;
				}
			}
			else
#endif
			{
				ensure(Obj == TypeObject<T>);
				TypeObject<T> = nullptr;
			}
		}
	}

private:
	void AddObjectToRegistry(UObject* Object, UClass* InNativeClass);
	void RemoveObjectFromRegistry(UObject* Object, UClass* InNativeClass);
	UPROPERTY()
	TMap<UClass*, FObjectPatternType> Binddings;

private:
	static bool AddClassToRegistry(UObject* Object, UClass* InNativeClass, int32& ID);
	static bool RemoveClassFromRegistry(UObject* Object, UClass* InNativeClass);

	static FCriticalSection Critical;
	class FCSLock423 final
	{
	public:
		FCSLock423();
		~FCSLock423();
	};

	template<typename T>
	static int32 TypeID;

	TMap<TWeakObjectPtr<UClass>, FWeakObjectPtr> TypeObjects;
	template<typename T>
	static TWeakObjectPtr<T> TypeObject;
};

template<typename T, typename F>
void UObjectPattern::EachObject(const UObject* WorldContextObj, const F& f)
{
	if (!EditorIsGameWorld(WorldContextObj))
		return;

	FCSLock423 Lock;

#if WITH_EDITOR
	if (GIsEditor)
	{
		EachObject(WorldContextObj, T::StaticClass(), [&](auto a) { f(static_cast<T*>(a)); });
	}
	else
#endif
	{
		for (auto It = NativeIterator<T>(); It;)
		{
			if (auto a = It->Get())
			{
				f(static_cast<T*>(a));
				++It;
			}
			else
			{
				It.RemoveCurrent();
			}
		}
	}
}

template<typename T>
int32 UObjectPattern::TypeID;
template<typename T>
TWeakObjectPtr<T> UObjectPattern::TypeObject;

//////////////////////////////////////////////////////////////////////////
template<typename T>
struct TEachObjectPattern
{
protected:
	static auto Ctor(T* This) { return UObjectPattern::Register(This); }
	static auto Dtor(T* This) { return UObjectPattern::Unregister(This); }
};

template<typename T>
struct TSingleObjectPattern
{
protected:
	static auto Ctor(T* This) { return UObjectPattern::SetObject<T>(This); }
	static auto Dtor(T* This)
	{
#if WITH_EDITOR
		return UObjectPattern::UnsetObject<T>(This);
#endif
	}
};
