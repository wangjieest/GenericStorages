// Copyright GenericStorages, Inc. All Rights Reserved.

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
	template<typename... TArgs>
	static TSharedPtr<void> Create(TArgs&&... Args)
	{
		using DT = TDecay<T>;
		return TSharedPtr<void>(new DT(Forward<TArgs>(Args)...), [](void* Ptr) { delete static_cast<DT*>(Ptr); });
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
			template<typename... TArgs>                                                                                    \
			static TSharedPtr<void> Create(TArgs&&... Args)                                                                \
			{                                                                                                              \
				using DT = TDecay<T>;                                                                                      \
				return TSharedPtr<void>(new DT(Forward<TArgs>(Args)...), [](void* Ptr) { delete static_cast<DT*>(Ptr); }); \
			}                                                                                                              \
		};                                                                                                                 \
	}

// currently it does not support the object refs in ustruct. use weakobjectptr instead!
struct FObjectDataRegistry
{
public:
	template<typename T>
	static T* FindStorageData(const UObject* Obj)
	{
		check(IsValid(Obj));
		return (T*)FindDataPtr(Obj, ObjectDataRegistry::TObjectDataTypeName<T>::GetFName());
	}

	template<typename T, typename... TArgs>
	static T* GetStorageData(const UObject* Obj, TArgs&&... Args)
	{
		check(IsValid(Obj));
		return (T*)GetDataPtr(Obj, ObjectDataRegistry::TObjectDataTypeName<T>::GetFName(), ObjectDataRegistry::TObjectDataTypeName<T>::Create(Forward<TArgs>(Args)...));
	}

	template<typename T>
	static bool RemoveStorageData(UObject* Obj)
	{
		check(IsValid(Obj));
		return DelDataPtr(Obj, ObjectDataRegistry::TObjectDataTypeName<T>::GetFName());
	}

protected:
	friend class UObjectDataRegistryHelper;
	GENERICSTORAGES_API static void* GetDataPtr(const UObject* Obj, FName Key, const TFunctionRef<TSharedPtr<void>()>& Func);
	GENERICSTORAGES_API static void* FindDataPtr(const UObject* Obj, FName Key);
	GENERICSTORAGES_API static bool DelDataPtr(const UObject* Obj, FName Key);
};

// For Blueprint
UCLASS()
class UObjectDataRegistryHelper final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

protected:
	UFUNCTION(BlueprintCallable, Category = "ObjectDataRegistryHelper", CustomThunk, meta = (CallableWithoutWorldContext, DefaultToSelf = "KeObj", CustomStructureParam = "Data", bWriteData = true))
	static void GetObjectData(const UObject* KeyObj, bool bWriteData, bool& bSucc, UPARAM(Ref) int32& Data);
	DECLARE_FUNCTION(execGetObjectData);
	UFUNCTION(BlueprintCallable, Category = "ObjectDataRegistryHelper", CustomThunk, meta = (CallableWithoutWorldContext, DefaultToSelf = "KeObj", CustomStructureParam = "Data"))
	static void DelObjectData(const UObject* KeyObj, UPARAM(Ref) int32& Data);
	DECLARE_FUNCTION(execDelObjectData);

	UFUNCTION(BlueprintCallable, Category = "ObjectDataRegistryHelper", meta = (CallableWithoutWorldContext, DefaultToSelf = "KeObj"))
	static void DeleteObjectDataByName(const UObject* KeyObj, FName Name);

private:
	UFUNCTION()
	void OnActorDestroyed(class AActor* InActor);
};
