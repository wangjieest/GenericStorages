// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "Template/UnrealCompatibility.h"

#include "GenericSingletons.generated.h"

class UWorld;
class UGenericSingletons;
class FTimerManager;

namespace GenericStorages
{
GENERICSTORAGES_API bool CallOnPluginReady(FSimpleDelegate Delegate);

GENERICSTORAGES_API void CallOnEngineInitCompletedImpl(FSimpleDelegate& Lambda);

GENERICSTORAGES_API bool DelayExec(const UObject* InObj, FTimerDelegate InDelegate, float InDelay = 0.f, bool bEnsureExec = true);

#if WITH_EDITOR
GENERICSTORAGES_API void CallOnEditorMapOpendImpl(TDelegate<void(UWorld*)> Delegate);
#endif

GENERICSTORAGES_API FTimerManager* GetTimerManager(UWorld* InWorld);
GENERICSTORAGES_API bool SetTimer(FTimerHandle& InOutHandle,UWorld* InWorld, FTimerDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f);
GENERICSTORAGES_API UObject* CreateSingletonImpl(const UObject* WorldContextObject, UClass* Class);
}  // namespace GenericStorages

template<typename F>
void CallOnEditorMapOpend(F&& Lambda, UObject* InObj = nullptr)
{
#if WITH_EDITOR
	TDelegate<void(UWorld*)> Delegate;
	if (InObj)
		Delegate.BindWeakLambda(InObj, Forward<F>(Lambda));
	else
		Delegate.BindLambda(Forward<F>(Lambda));
	GenericStorages::CallOnEditorMapOpendImpl(MoveTemp(Delegate));
#endif
}

template<typename F>
void CallOnEngineInitCompleted(F&& Lambda, UObject* InObj = nullptr)
{
	FSimpleDelegate Delegate;
	if (InObj)
		Delegate.BindWeakLambda(InObj, Forward<F>(Lambda));
	else
		Delegate.BindLambda(Forward<F>(Lambda));
	GenericStorages::CallOnEngineInitCompletedImpl(Delegate);
}

template<typename F>
auto DelayExec(const UObject* InObj, F&& Lambda, float InDelay = 0.f, bool bEnsureExec = true)
{
	if (InObj)
		return GenericStorages::DelayExec(InObj, FTimerDelegate::CreateWeakLambda(const_cast<UObject*>(InObj), Forward<F>(Lambda)), InDelay, bEnsureExec);
	else
		return GenericStorages::DelayExec(InObj, FTimerDelegate::CreateLambda(Forward<F>(Lambda)), InDelay, bEnsureExec);
}

template<typename F>
auto CallOnWorldNextTick(const UObject* InObj, F&& Lambda, bool bEnsureExec = true)
{
	return DelayExec(InObj, Forward<F>(Lambda), 0.f, bEnsureExec);
}

namespace GenericSingletons
{
GENERICSTORAGES_API UObject* DynamicReflectionImpl(const FString& TypeName, UClass* TypeClass = nullptr);
GENERICSTORAGES_API void DeferredWorldCleanup(FSimpleDelegate Cb, FString Desc, bool bEditorOnly = false);
template<typename F>
void DeferredWorldCleanup(const TCHAR* Desc, F&& f, bool bEditorOnly = false)
{
	DeferredWorldCleanup(FSimpleDelegate::CreateLambda(Forward<F>(f)), Desc, bEditorOnly);
}
template<typename F>
void DeferredWorldCleanup(const TCHAR* Desc, UObject* InObj, F&& f, bool bEditorOnly = false)
{
	DeferredWorldCleanup(FSimpleDelegate::CreateWeakLambda(InObj, Forward<F>(f)), Desc, bEditorOnly);
}

template<typename T>
bool bGenericSingltonInCtor = false;
}  // namespace GenericSingletons

template<typename T = UStruct>
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
template<typename T, typename = void>
struct TGenericSingletonAOP;
// static T* CustomConstruct(const UObject* WorldContextObject, UClass* SubClass = nullptr);

DECLARE_DELEGATE_OneParam(FAsyncLoadObjCallback, UObject*);
DECLARE_DELEGATE_OneParam(FAsyncLoadClsCallback, UClass*);
DECLARE_DELEGATE_OneParam(FAsyncBatchObjCallback, TArray<UObject*>&);
DECLARE_DELEGATE_OneParam(FAsyncBatchClsCallback, TArray<UClass*>&);

#define USE_GENEIRC_SINGLETON_GUARD WITH_EDITOR

template<typename T>
class TGenericSingleBase
{
public:
	template<typename U = UObject>
	static T* GetSingleton(const U* WorldContextObject = nullptr, bool bCreate = true);

	TGenericSingleBase();
};

struct FObjConstructParameter
{
	mutable FName Name = NAME_None;
	mutable UClass* Class = nullptr;
	const FTransform* Trans = nullptr;

	FObjConstructParameter() = default;

	FObjConstructParameter(UClass* InClass, FName InName = NAME_None, const FTransform* InTrans = nullptr)
		: Name(InName)
		, Class(InClass)
		, Trans(InTrans)
	{
	}
	template<typename U>
	FObjConstructParameter(TSubclassOf<U> InClass, FName InName = NAME_None, const FTransform* InTrans = nullptr)
		: FObjConstructParameter(InClass.Get(), NAME_None, InTrans)
	{
	}
	FObjConstructParameter(UClass* InClass, const FTransform* InTrans)
		: FObjConstructParameter(InClass, NAME_None, InTrans)
	{
	}
	FObjConstructParameter(FName InName, UClass* InClass = nullptr, const FTransform* InTrans = nullptr)
		: FObjConstructParameter(InClass, InName, InTrans)
	{
	}
	FObjConstructParameter(FName InName, const FTransform* InTrans)
		: FObjConstructParameter(nullptr, InName, InTrans)
	{
	}
};

// a simple and powerful singleton utility
UCLASS(Transient, meta = (DisplayName = "GenericSingletons"))
class GENERICSTORAGES_API UGenericSingletons final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UGenericSingletons();

protected:
	static UObject* GetSingletonInternal(UClass* Class, const UObject* Ctx, bool bCreate, UClass* BaseNativeCls = nullptr, UClass* BaseBPCls = nullptr);
	static UObject* RegisterAsSingletonInternal(UObject* Object, const UObject* Ctx, bool bReplaceExist = true, UClass* BaseNativeCls = nullptr, UClass* BaseBPCls = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Game|GMP", meta = (DisplayName = "RegisterAsSingleton", WorldContext = "Ctx"))
	static bool K2_RegisterAsSingleton(UObject* Object, const UObject* Ctx = nullptr, bool bReplaceExist = true)
	{
		UObject* Ret = Object ? RegisterAsSingletonInternal(Object, Ctx, bReplaceExist) : nullptr;
		return (Ret && Ret == Object);
	}
	UFUNCTION(BlueprintCallable, Category = "Game|GMP", meta = (DisplayName = "UnregisterSingleton", WorldContext = "Ctx"))
	static bool K2_UnregisterSingleton(UObject* Object, const UObject* Ctx = nullptr) { return Object ? UnregisterSingletonImpl(Object, Ctx) : false; }

	UFUNCTION(BlueprintCallable, Category = "Game|GMP", meta = (DisplayName = "GetGMPSingleton", WorldContext = "Ctx", DeterminesOutputType = "Class", DynamicOutputParam))
	static UObject* K2_GetSingleton(UClass* Class, const UObject* Ctx = nullptr, bool bCreate = true);

	UFUNCTION(BlueprintCallable, Category = "Game|GMP", meta = (DisplayName = "GetGMPSingletonEx", WorldContext = "Ctx", AdvancedDisplay = "RegClass,bValid", DeterminesOutputType = "Class", DynamicOutputParam))
	static UObject* K2_GetSingletonEx(bool& bIsValid, UClass* Class, const UObject* Ctx = nullptr, bool bCreate = true, UClass* RegClass = nullptr)
	{
		UObject* Ret = nullptr;
		do
		{
#if WITH_EDITOR
			if (!ensure(Class && (!RegClass || Class->IsChildOf(RegClass))))
				break;
#endif
			Ret = GetSingletonInternal(Class, Ctx, bCreate, nullptr, RegClass);
		} while (false);
		bIsValid = IsValid(Ret);
		return Ret;
	}

	UFUNCTION(BlueprintPure, Category = "Game|GMP", meta = (DisplayName = "PureGetSingleton", WorldContext = "Ctx", AdvancedDisplay = "RegClass", DeterminesOutputType = "Class", DynamicOutputParam))
	static UObject* GetSingletonImplPure(UClass* Class, const UObject* Ctx = nullptr, bool bCreate = true, UClass* RegClass = nullptr)
	{
		bool bIsValid = false;
		return K2_GetSingletonEx(bIsValid, Class, Ctx, bCreate, RegClass);
	}

	UFUNCTION(BlueprintPure, Category = "Game|GMP", meta = (DisplayName = "PureInstanceSingleton", CallableWithoutWorldContext, DeterminesOutputType = "Class", DynamicOutputParam))
	static UObject* PureInstanceSingleton(UClass* Class, bool bCreate = true) { return K2_GetSingleton(Class, nullptr, bCreate); }

public:  // C++
	FORCEINLINE static UObject* FindSingleton(UClass* Class, const UObject* WorldContextObject) { return GetSingletonInternal(Class, WorldContextObject, false); }
	static UObject* GetSingletonImpl(UClass* Class, const UObject* WorldContextObject, UClass* RegClass = nullptr) { return GetSingletonInternal(Class, WorldContextObject, true, RegClass); }

	// InBaseClass == nullptr will use the first native ancestor type
	static UObject* RegisterAsSingletonImpl(UObject* Object, const UObject* WorldContextObject, bool bReplaceExist = true, UClass* InBaseClass = nullptr)
	{
		return RegisterAsSingletonInternal(Object, WorldContextObject, bReplaceExist, InBaseClass);
	}
	static bool UnregisterSingletonImpl(UObject* Object, const UObject* WorldContextObject, UClass* InBaseClass = nullptr);

#if USE_GENEIRC_SINGLETON_GUARD
	FORCEINLINE static UObject* FindSingleton(UClass* Class, const UWorld* InWorld) { return GetSingletonInternal(Class, CastChecked<UObject>(InWorld), false); }
	static UObject* GetSingletonImpl(UClass* Class, const UWorld* InWorld, UClass* RegClass = nullptr) { return GetSingletonInternal(Class, CastChecked<UObject>(InWorld), true, RegClass); }
	static UObject* RegisterAsSingletonImpl(UObject* Object, const UWorld* InWorld, bool bReplaceExist = true, UClass* InBaseClass = nullptr)
	{
		return RegisterAsSingletonImpl(Object, CastChecked<UObject>(InWorld), bReplaceExist, InBaseClass);
	}
	static bool UnregisterSingletonImpl(UObject* Object, const UWorld* InWorld, UClass* InBaseClass = nullptr) { return UnregisterSingletonImpl(Object, CastChecked<UObject>(InWorld), InBaseClass); }
#endif

public:
	template<typename T, typename U = UObject>
	FORCEINLINE static UObject* RegisterAsSingleton(T* Object, const U* WorldContextObject, bool bReplaceExist = true)
	{
		return RegisterAsSingletonImpl(Object, ConvertNullType(WorldContextObject), bReplaceExist, T::StaticClass());
	}
	template<typename T, typename U = UObject>
	FORCEINLINE static bool UnregisterSingleton(T* Object, const U* WorldContextObject)
	{
		return UnregisterSingletonImpl(Object, ConvertNullType(WorldContextObject), T::StaticClass());
	}

	template<typename T, typename U = UObject>
	static T* GetSingleton(const U* WorldContextObject, bool bCreate = true, TSubclassOf<T> InSubClass = nullptr)
	{
		auto InCtx = ConvertNullType(WorldContextObject);
		auto NativeClass = T::StaticClass();
		check(!InSubClass.Get() || InSubClass->IsChildOf(NativeClass));
		UClass* SubClass = *InSubClass ? *InSubClass : NativeClass;
		T* Ptr = Cast<T>(FindSingleton(SubClass, InCtx));
		if (bCreate && !IsValid(Ptr))
		{
			Ptr = TryGetSingleton<T>(
				InCtx,
				[&] { return TGenericSingletonAOP<T>::CustomConstruct(InCtx, InSubClass); },
				InSubClass);
		}
		return Ptr;
	}

	template<typename T, typename F, typename U = UObject>
	static T* TryGetSingleton(const U* WorldContextObject, const F& ConstructFunc, TSubclassOf<T> InSubClass = nullptr)
	{
		static_assert(TIsDerivedFrom<typename TRemovePointer<decltype(ConstructFunc())>::Type, T>::IsDerived, "err");
		auto NativeClass = T::StaticClass();
		check(!InSubClass.Get() || InSubClass->IsChildOf(NativeClass));
		UClass* SubClass = *InSubClass ? *InSubClass : NativeClass;

		auto InCtx = ConvertNullType(WorldContextObject);
		auto Mgr = GetSingletonsManager(InCtx);
		auto& Ptr = Mgr->Singletons.FindOrAdd(SubClass);
		if (!IsValid(Ptr))
		{
			Ptr = ConstructFunc();
			if (ensureAlwaysMsgf(Ptr, TEXT("TryGetSingleton Failed for Class : %s"), *SubClass->GetName()))
				RegisterAsSingletonImpl(Ptr, InCtx, true, SubClass);
		}
		return static_cast<T*>(Ptr);
	}

public:
	// Object
	static TSharedPtr<struct FStreamableHandle> AsyncLoadObj(const FSoftObjectPath& InPath, FAsyncLoadObjCallback Cb, bool bSkipInvalid = false, TAsyncLoadPriority Priority = 0);
	template<typename LambdaType>
	static FORCEINLINE auto AsyncLoadObj(const FSoftObjectPath& InPath, const UObject* Obj, LambdaType&& Cb, bool bSkipInvalid = false, TAsyncLoadPriority Priority = 0)
	{
		return AsyncLoadObj(InPath, CreateWeakLambda<FAsyncLoadObjCallback>(Obj, Forward<LambdaType>(Cb)), bSkipInvalid, Priority);
	}
	static TSharedPtr<struct FStreamableHandle> AsyncLoadObj(const TArray<FSoftObjectPath>& InPaths, FAsyncBatchObjCallback Cb, bool bSkipInvalid = false, TAsyncLoadPriority Priority = 0);
	template<typename LambdaType>
	static FORCEINLINE auto AsyncLoadObj(const TArray<FSoftObjectPath>& InPaths, const UObject* Obj, LambdaType&& Cb, bool bSkipInvalid = false, TAsyncLoadPriority Priority = 0)
	{
		return AsyncLoadObj(InPaths, CreateWeakLambda<FAsyncBatchObjCallback>(Obj, Forward<LambdaType>(Cb)), bSkipInvalid, Priority);
	}

	// Class
	static FORCEINLINE TSharedPtr<struct FStreamableHandle> AsyncLoadCls(const FSoftClassPath& InPath, FAsyncLoadClsCallback Cb, bool bSkipInvalid = false, TAsyncLoadPriority Priority = 0)
	{
		return AsyncLoadObj(InPath, MoveTemp(*reinterpret_cast<FAsyncLoadObjCallback*>(&Cb)), bSkipInvalid, Priority);
	}
	template<typename LambdaType>
	static FORCEINLINE auto AsyncLoadCls(const FSoftClassPath& InPath, const UObject* Obj, LambdaType&& Cb, bool bSkipInvalid = false, TAsyncLoadPriority Priority = 0)
	{
		return AsyncLoadCls(InPath, CreateWeakLambda<FAsyncLoadClsCallback>(Obj, Forward<LambdaType>(Cb)), bSkipInvalid, Priority);
	}
	static FORCEINLINE TSharedPtr<struct FStreamableHandle> AsyncLoadCls(const TArray<FSoftClassPath>& InPaths, FAsyncBatchClsCallback Cb, bool bSkipInvalid = false, TAsyncLoadPriority Priority = 0)
	{
		return AsyncLoadObj((const TArray<FSoftObjectPath>&)InPaths, MoveTemp(*reinterpret_cast<FAsyncBatchObjCallback*>(&Cb)), bSkipInvalid, Priority);
	}
	template<typename LambdaType>
	static FORCEINLINE auto AsyncLoadCls(const TArray<FSoftClassPath>& InPaths, const UObject* Obj, LambdaType&& Cb, bool bSkipInvalid = false, TAsyncLoadPriority Priority = 0)
	{
		return AsyncLoadCls(InPaths, CreateWeakLambda<FAsyncBatchClsCallback>(Obj, Forward<LambdaType>(Cb)), bSkipInvalid, Priority);
	}

	// AOP(compile time only), any advice for runtime dynamic AOP?
	template<typename T = UObject, typename U = UObject>
	static T* CreateInstance(const U* WorldContextObject, const FObjConstructParameter& Parameter = {})
	{
		check(!Parameter.Class || Parameter.Class->IsChildOf<T>());
		return TGenericConstructionAOP<T>::CustomConstruct(ConvertNullType(WorldContextObject), Parameter);
	}

	static bool AsyncCreate(const FSoftClassPath& InPath, FAsyncLoadObjCallback Cb, UObject* WorldContextObj = nullptr);
	template<typename F, typename T = UObject>
	static bool AsyncCreate(const FSoftClassPath& InPath, const UObject* ContextObj, F&& Cb)
	{
		return AsyncLoadCls(InPath, ContextObj, [ContextObj, Cb{MoveTemp(Cb)}](UClass* ResolvedCls) { Cb(CreateInstance<T>(ContextObj, ResolvedCls)); }).IsValid();
	}

public:
	template<typename T>
	static FString GetTypedNameSafe(const T* Obj = nullptr)
	{
		return Obj ? Obj->GetName() : T::StaticClass()->GetName();
	}

	static UObject* CreateInstanceImpl(const UObject* WorldContextObject, const FObjConstructParameter& Parameter);

#if USE_GENEIRC_SINGLETON_GUARD
	static UObject* CreateInstanceImpl(const UWorld* InWorld, const FObjConstructParameter& Parameter) { return CreateInstanceImpl(CastChecked<UObject>(InWorld), Parameter); }
#endif

	template<typename T, typename U = UObject>
	static T* CreateInstanceImpl(const U* WorldContextObject, const FObjConstructParameter& Parameter)
	{
		auto SubClass = Parameter.Class;
		check(!SubClass || SubClass->IsChildOf<T>());
		Parameter.Class = SubClass ? SubClass : T::StaticClass();
		return (T*)UGenericSingletons::CreateInstanceImpl(ConvertNullType(WorldContextObject), Parameter);
	}

	template<typename T = UObject, typename U = UObject>
	static T* CreateSingletonImpl(const U* WorldContextObject, UClass* SubClass)
	{
#if USE_GENEIRC_SINGLETON_GUARD
		GenericSingletons::bGenericSingltonInCtor<T> = true;
		ON_SCOPE_EXIT
		{
			GenericSingletons::bGenericSingltonInCtor<T> = false;
		};
#endif
		auto NativeClass = T::StaticClass();
		check(!SubClass || SubClass->IsChildOf(NativeClass));
		return (T*)UGenericSingletons::GetSingletonImpl(SubClass ? SubClass : NativeClass, ConvertNullType(WorldContextObject));
	}

private:
	static UGenericSingletons* GetSingletonsManager(const UObject* InObj);
#if USE_GENEIRC_SINGLETON_GUARD
	static UGenericSingletons* GetSingletonsManager(const UWorld* InWorld) { return GetSingletonsManager(CastChecked<UObject>(InWorld)); }
#endif
	UPROPERTY(Transient)
	TMap<UClass*, UObject*> Singletons;

#if USE_GENEIRC_SINGLETON_GUARD
	template<typename U, typename R = std::conditional_t<std::is_same<U, std::nullptr_t>::value, UObject, U>>
#else
	template<typename U, typename R = U>
#endif
	static R* ConvertNullType(U* InCtx)
	{
		return InCtx;
	}
};

template<typename T, typename V>
struct TGenericSingletonAOP
{
	template<typename U = UObject>
	static T* CustomConstruct(const U* WorldContextObject, UClass* SubClass = nullptr)
	{
		static_assert(std::is_same<V, void>::value, "error");
		return UGenericSingletons::CreateSingletonImpl<T>(WorldContextObject, SubClass);
	}
};

template<typename T, typename V>
struct TGenericConstructionAOP
{
	template<typename U = UObject>
	static T* CustomConstruct(const U* WorldContextObject, const FObjConstructParameter& Parameter = {})
	{
		static_assert(std::is_same<V, void>::value, "error");
		return UGenericSingletons::CreateInstanceImpl<T>(WorldContextObject, Parameter);
	}
};

template<typename T>
TGenericSingleBase<T>::TGenericSingleBase()
{
	static_assert(TIsDerivedFrom<T, UObject>::IsDerived && !std::is_same<T, UObject>::value, "err");
	auto This = static_cast<T*>(this);
#if USE_GENEIRC_SINGLETON_GUARD
	extern GENERICSTORAGES_API bool IsInSingletonCreatation(UObject*);
	checkf(GenericSingletons::bGenericSingltonInCtor<T> || This->HasAnyFlags(RF_ClassDefaultObject) || IsInSingletonCreatation(This), TEXT("SingletonConstructError"));
#endif
	// #if USE_GENEIRC_SINGLETON_GUARD
	// 	if (GenericSingletons::bGenericSingltonInCtor<T>)
	// #else
	// 	if (!This->HasAnyFlags(RF_ClassDefaultObject))
	// #endif
	// 	{
	// 		ensure(!UGenericSingletons::RegisterAsSingletonImpl(This, This->GetWorld(), false, StaticClass<T>()));
	// 	}
}

//////////////////////////////////////////////////////////////////////////

template<typename T>
template<typename U>
T* TGenericSingleBase<T>::GetSingleton(const U* WorldContextObject, bool bCreate)
{
#if USE_GENEIRC_SINGLETON_GUARD
	GenericSingletons::bGenericSingltonInCtor<T> = true;
	ON_SCOPE_EXIT
	{
		GenericSingletons::bGenericSingltonInCtor<T> = false;
	};
#endif
	return UGenericSingletons::GetSingleton<T>(WorldContextObject, bCreate);
}
