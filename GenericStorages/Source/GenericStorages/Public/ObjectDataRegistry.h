// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/Object.h"

#include "ObjectDataRegistry.generated.h"

namespace ObjectDataRegistry
{
template<typename T>
struct TObjectDataTypeName
{
	static auto GetFName() { return T::StaticStruct()->GetFName(); }
	template<typename... Args>
	static TSharedPtr<void> Create(Args&&... args)
	{
		using DT = TDecay<T>;
		return TSharedPtr<void>(new DT(MoveTempIfPossible(args)...), [](void* p) { delete static_cast<DT*>(p); });
	}
};
}  // namespace ObjectDataRegistry

#define OBJECT_DATA_DEF(T)                                                                                                 \
	namespace ObjectDataRegistry                                                                                           \
	{                                                                                                                      \
		template<>                                                                                                         \
		struct TObjectDataTypeName<T>                                                                                      \
		{                                                                                                                  \
			static auto GetFName() { return TEXT(#T); }                                                                    \
			template<typename... Args>                                                                                     \
			static TSharedPtr<void> Create(Args&&... args)                                                                 \
			{                                                                                                              \
				using DT = TDecay<T>;                                                                                      \
				return TSharedPtr<void>(new DT(MoveTempIfPossible(args)...), [](void* p) { delete static_cast<DT*>(p); }); \
			}                                                                                                              \
		};                                                                                                                 \
	}

// current support structs has none uobject refs. use weakobject instead!
struct FObjectDataRegistry
{
public:
	template<typename T>
	static T* FindStorageData(const UObject* Obj)
	{
		check(IsValid(Obj));
		return (T*)FindDataPtr(Obj, TObjectDataTypeName<T>::GetFName());
	}

	template<typename T, typename... Args>
	static T* GetStorageData(const UObject* Obj, Args&&... args)
	{
		check(IsValid(Obj));
		return (T*)GetDataPtr(Obj, TObjectDataTypeName<T>::GetFName(), TObjectDataTypeName<T>::Create(MoveTempIfPossible(args)...));
	}

	template<typename T>
	static bool RemoveStorageData(UObject* Obj)
	{
		check(IsValid(Obj));
		return DelDataPtr(Obj, TObjectDataTypeName<T>::GetFName());
	}

protected:
	friend class UObjectDataRegistryHelper;
	GENERICSTORAGES_API static void* GetDataPtr(const UObject* Obj, FName Key, const TFunctionRef<TSharedPtr<void>()>& Func);
	GENERICSTORAGES_API static void* FindDataPtr(const UObject* Obj, FName Key);
	GENERICSTORAGES_API static bool DelDataPtr(const UObject* Obj, FName Key);
};

// For Blueprint
UCLASS()
class UObjectDataRegistryHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

protected:
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CallableWithoutWorldContext, DefaultToSelf = "KeObj", CustomStructureParam = "Data", bWriteData = true))
	static void GetObjectData(const UObject* KeyObj, bool bWriteData, bool& bSucc, UPARAM(Ref) int32& Data);
	DECLARE_FUNCTION(execGetObjectData);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CallableWithoutWorldContext, DefaultToSelf = "KeObj", CustomStructureParam = "Data"))
	static void DelObjectData(const UObject* KeyObj, UPARAM(Ref) int32& Data);
	DECLARE_FUNCTION(execDelObjectData);

	UFUNCTION(BlueprintCallable, meta = (CallableWithoutWorldContext, DefaultToSelf = "KeObj"))
	static void DeleteObjectDataByName(const UObject* KeyObj, FName Name);

private:
	friend struct FObjectDataRegistry;
	UFUNCTION()
	void OnActorDestroyed(class AActor* InActor);
};
