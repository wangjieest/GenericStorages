// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Launch/Resources/Version.h"
#include "UObject/Class.h"
#include "UnrealCompatibility.h"

#include "GenericSingletons.generated.h"

class UWorld;
class UGenericSingletons;

namespace GenericSingletons
{
GENERICSTORAGES_API UObject* DynamicReflectionImpl(const FString& TypeName, UClass* TypeClass = nullptr);
GENERICSTORAGES_API void SetWorldCleanup(FSimpleDelegate Cb, bool bEditorOnly = false);
}  // namespace GenericSingletons

template<typename T>
FORCEINLINE T* DynamicReflection(const FString& TypeName)
{
	return static_cast<T*>(GenericSingletons::DynamicReflectionImpl(TypeName, T::StaticClass()));
}

FORCEINLINE UScriptStruct* DynamicStruct(const FString& StructName)
{
	return DynamicReflection<UScriptStruct>(StructName);
}
FORCEINLINE UClass* DynamicClass(const FString& ClassName)
{
	return DynamicReflection<UClass>(ClassName);
}
FORCEINLINE UEnum* DynamicEnum(const FString& EnumName)
{
	return DynamicReflection<UEnum>(EnumName);
}

template<typename T, typename = void>
struct TGenericConstructionAOP;
// static T* CustomConstruct(const UObject* WorldContextObject, UClass* SubClass = nullptr);

DECLARE_DELEGATE_OneParam(FAsyncObjectCallback, class UObject*);
DECLARE_DELEGATE_OneParam(FAsyncBatchCallback, const TArray<UObject*>&);

// a simple and powerful singleton utility
UCLASS(Transient, meta = (DisplayName = "GenericSingletons"))
class GENERICSTORAGES_API UGenericSingletons final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UGenericSingletons();

protected:
	UFUNCTION(BlueprintCallable, Category = "Game", meta = (DisplayName = "RegisterAsSingleton", WorldContext = "WorldContextObject"))
	static bool K2_RegisterAsSingleton(UObject* Object, const UObject* WorldContextObject = nullptr, bool bReplaceExist = true)
	{
		UObject* Ret = Object ? RegisterAsSingletonImpl(Object, WorldContextObject, bReplaceExist) : nullptr;
		return (Ret && Ret == Object);
	}

public:  // C++
	UFUNCTION(BlueprintCallable, Category = "Game", meta = (DisplayName = "GetSingleton", WorldContext = "WorldContextObject", HidePin = "RegClass", DeterminesOutputType = "Class", DynamicOutputParam))
	static UObject* GetSingletonImpl(UClass* Class, const UObject* WorldContextObject = nullptr, bool bCreate = true, UClass* RegClass = nullptr);

	static UObject* RegisterAsSingletonImpl(UObject* Object, const UObject* WorldContextObject, bool bReplaceExist = true, UClass* InNativeClass = nullptr);

	template<typename T>
	FORCEINLINE static UObject* RegisterAsSingleton(T* Object, const UObject* WorldContextObject, bool bReplaceExist = true)
	{
		return RegisterAsSingletonImpl(Object, WorldContextObject, bReplaceExist, T::StaticClass());
	}

	template<typename T>
	static T* GetSingleton(const UObject* WorldContextObject, bool bCreate = true)
	{
		auto Ptr = Cast<T>(GetSingletonImpl(T::StaticClass(), WorldContextObject, false));
		if (IsValid(Ptr))
			return Ptr;

		return bCreate ? TryGetSingleton<T>(WorldContextObject, [&] { return TGenericConstructionAOP<T>::CustomConstruct(WorldContextObject, nullptr); }) : nullptr;
	}

	template<typename T, typename F>
	static T* TryGetSingleton(const UObject* WorldContextObject, const F& ConstructFunc)
	{
		static_assert(TIsDerivedFrom<typename TRemovePointer<decltype(ConstructFunc())>::Type, T>::IsDerived, "err");
		auto Mgr = GetWorldLocalManager(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		auto& Ptr = Mgr->Singletons.FindOrAdd(T::StaticClass());
		if (!IsValid(Ptr))
		{
			Ptr = ConstructFunc();
			if (ensureAlwaysMsgf(Ptr, TEXT("TryGetSingleton Failed %s"), *T::StaticClass()->GetName()))
				RegisterAsSingletonImpl(Ptr, WorldContextObject, true, T::StaticClass());
		}
		return (T*)Ptr;
	}

public:
	static TSharedPtr<struct FStreamableHandle> AsyncLoad(const TArray<FSoftObjectPath>& InPaths, FAsyncBatchCallback Cb, bool bSkipInvalid = false, TAsyncLoadPriority Priority = 0);

	static TSharedPtr<struct FStreamableHandle> AsyncLoad(const FString& InPath, FAsyncObjectCallback Cb, bool bSkipInvalid = false, TAsyncLoadPriority Priority = 0);

	static bool AsyncCreate(const UObject* BindedObject, const FString& InPath, FAsyncObjectCallback Cb);

	// AOP(compile time only), any advice for runtime dynamic AOP?
	template<typename T = UObject>
	static T* CreateInstance(const UObject* WorldContextObject, UClass* SubClass = nullptr)
	{
		check(!SubClass || ensureAlways(SubClass->IsChildOf<T>()));
		return TGenericConstructionAOP<T>::CustomConstruct(WorldContextObject, SubClass);
	}

	// async AOP(compile time only)
	template<typename T, typename F>
	static bool AsyncCreate(const UObject* BindedObject, const FString& InPath, F&& f)
	{
		FWeakObjectPtr WeakObj = BindedObject;
		auto Lambda = [WeakObj, f{MoveTemp(f)}](UObject* ResolvedObj) {
			if (!WeakObj.IsStale())
				f(CreateInstance<T>(WeakObj.Get(), Cast<UClass>(ResolvedObj)));
		};

		auto Handle = AsyncLoad(InPath, FAsyncObjectCallback::CreateLambda(Lambda));
		return Handle.IsValid();
	}

public:
	template<typename T>
	static FString GetTypedNameSafe(const T* Obj)
	{
		return Obj ? Obj->GetName() : T::StaticClass()->GetName();
	}

	static UObject* CreateInstanceImpl(const UObject* WorldContextObject, UClass* SubClass);
	template<typename T>
	static T* CreateInstanceImpl(const UObject* WorldContextObject, UClass* SubClass)
	{
		check(!SubClass || ensureAlways(SubClass->IsChildOf<T>()));
		return (T*)UGenericSingletons::CreateInstanceImpl(WorldContextObject, SubClass ? SubClass : T::StaticClass());
	}

private:
	static UGenericSingletons* GetWorldLocalManager(UWorld* World);

	UPROPERTY(Transient)
	TMap<UClass*, UObject*> Singletons;
};

template<typename T, typename V>
struct TGenericConstructionAOP
{
	static T* CustomConstruct(const UObject* WorldContextObject, UClass* SubClass = nullptr)
	{
		static_assert(TIsSame<V, void>::Value, "error");
		return UGenericSingletons::CreateInstanceImpl<T>(WorldContextObject, SubClass);
	}
};
