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

#include "UnrealCompatibility.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Launch/Resources/Version.h"
#include "UObject/Class.h"

#include "GenericSingletons.generated.h"

class UWorld;
class UGenericSingletons;

namespace GenericSingletons
{
GENERICSTORAGES_API UObject* DynamicReflectionImpl(const FString& TypeName, UClass* TypeClass = nullptr);
GENERICSTORAGES_API UGenericSingletons* GetManager(UWorld* World, bool bEnsure);
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
struct TSingletonConstructAction;
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

		return bCreate ? TryGetSingleton<T>(WorldContextObject, [&] { return TSingletonConstructAction<T>::CustomConstruct(WorldContextObject, nullptr); }) : nullptr;
	}

	template<typename T, typename F>
	static T* TryGetSingleton(const UObject* WorldContextObject, const F& ConstructFunc)
	{
		static_assert(std::is_convertible<decltype(ConstructFunc()), T*>::value, "err");
		auto Mgr = GenericSingletons::GetManager(WorldContextObject ? WorldContextObject->GetWorld() : nullptr, true);
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

	// AOP(native only)
	template<typename T = UObject>
	static T* CreateInstance(const UObject* WorldContextObject, UClass* SubClass = nullptr)
	{
		check(!SubClass || ensureAlways(SubClass->IsChildOf<T>()));
		return TSingletonConstructAction<T>::CustomConstruct(WorldContextObject, SubClass);
	}

	// async AOP(native only)
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

	template<typename T>
	static FString GetTypedNameSafe(const T* Obj)
	{
		return Obj ? Obj->GetName() : T::StaticClass()->GetName();
	}

private:
	UPROPERTY(Transient)
	TMap<UClass*, UObject*> Singletons;

	template<typename T, typename>
	friend struct TSingletonConstructAction;

	static UObject* CreateInstanceImpl(const UObject* WorldContextObject, UClass* SubClass);
	template<typename T>
	static T* CreateInstanceImpl(const UObject* WorldContextObject, UClass* SubClass)
	{
		check(!SubClass || ensureAlways(SubClass->IsChildOf<T>()));
		return (T*)UGenericSingletons::CreateInstanceImpl(WorldContextObject, SubClass ? SubClass : T::StaticClass());
	}
};

template<typename T, typename V>
struct TSingletonConstructAction
{
	static T* CustomConstruct(const UObject* WorldContextObject, UClass* SubClass = nullptr)
	{
		static_assert(std::is_same<V, void>::value, "error");
		return UGenericSingletons::CreateInstanceImpl<T>(WorldContextObject, SubClass);
	}
};
