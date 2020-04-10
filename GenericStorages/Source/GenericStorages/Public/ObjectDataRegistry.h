// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ObjectDataRegistry.generated.h"

template<typename T>
struct TObjectDataRegName
{
	static auto GetFName() { return T::StaticStruct()->GetFName(); }
	template<typename... Args>
	static TSharedPtr<void> Create(Args&&... args)
	{
		using DT = TDecay<T>;
		return TSharedPtr<void>(new DT(MoveTempIfPossible(args)...), [](void* p) { delete static_cast<DT*>(p); });
	}
};

#define OBJECT_DATA_DEF(T)                                                                                             \
	template<>                                                                                                         \
	struct TObjectDataRegName<T>                                                                                       \
	{                                                                                                                  \
		static auto GetFName() { return TEXT(#T); }                                                                    \
		template<typename... Args>                                                                                     \
		static TSharedPtr<void> Create(Args&&... args)                                                                 \
		{                                                                                                              \
			using DT = TDecay<T>;                                                                                      \
			return TSharedPtr<void>(new DT(MoveTempIfPossible(args)...), [](void* p) { delete static_cast<DT*>(p); }); \
		}                                                                                                              \
	};

// current support structs has none uobject refs. use weakobject instead!
struct FObjectDataRegitstry
{
public:
	template<typename T>
	static T* FindStorageData(const UObject* Obj)
	{
		check(IsValid(Obj));
		return (T*)FindDataPtr(Obj, TObjectDataRegName<T>::GetFName());
	}

	template<typename T, typename... Args>
	static T* GetStorageData(const UObject* Obj, Args&&... args)
	{
		check(IsValid(Obj));
		return (T*)GetDataPtr(Obj, TObjectDataRegName<T>::GetFName(), TObjectDataRegName<T>::Create(MoveTempIfPossible(args)...));
	}

	template<typename T>
	static bool RemoveStorageData(UObject* Obj)
	{
		check(IsValid(Obj));
		return DelDataPtr(Obj, TObjectDataRegName<T>::GetFName());
	}

protected:
	friend class UObjectDataRegisterUtil;
	GENERICSTORAGES_API static void* GetDataPtr(const UObject* Obj, FName Key, const TFunctionRef<TSharedPtr<void>()>& Func);
	GENERICSTORAGES_API static void* FindDataPtr(const UObject* Obj, FName Key);
	GENERICSTORAGES_API static bool DelDataPtr(const UObject* Obj, FName Key);
};

OBJECT_DATA_DEF(FObjectDataRegitstry);

UCLASS()
class UObjectDataRegisterUtil : public UObject
{
	GENERATED_BODY()
public:
	static void SetOnDestroyed(class AActor* InActor);

protected:
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CallableWithoutWorldContext, DefaultToSelf = "KeObj", CustomStructureParam = "Data", bWriteData = true))
	static void GetObjectData(const UObject* KeyObj, bool bWriteData, bool& bSucc, UPARAM(Ref) int32& Data);
	DECLARE_FUNCTION(execGetObjectData);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CallableWithoutWorldContext, DefaultToSelf = "KeObj", CustomStructureParam = "Data"))
	static void DeleteObjectData(const UObject* KeyObj, UPARAM(Ref) int32& Data);
	DECLARE_FUNCTION(execDeleteObjectData);
	UFUNCTION(BlueprintCallable, meta = (CallableWithoutWorldContext, DefaultToSelf = "KeObj"))
	static void DeleteObjectDataByName(const UObject* KeyObj, FName Name);

	UFUNCTION()
	void OnActorDestroyed(class AActor* InActor);
};
