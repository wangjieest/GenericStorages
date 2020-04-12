// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UnrealCompatibility.h"

#include "MemberDataRegistry.generated.h"

USTRUCT(BlueprintType)
struct GENERICSTORAGES_API FMemberDataRegistryTag
{
	GENERATED_BODY()
public:
	FMemberDataRegistryTag();
	~FMemberDataRegistryTag();
	FMemberDataRegistryTag(FMemberDataRegistryTag&& Other) = default;
	FMemberDataRegistryTag& operator=(FMemberDataRegistryTag&& Other) = default;
	FMemberDataRegistryTag(const FMemberDataRegistryTag& Other) = default;
	FMemberDataRegistryTag& operator=(const FMemberDataRegistryTag& Other) = default;

protected:
	FWeakObjectPtr Outer;
	friend struct FMemberDataRegistry;
};

#define STATIC_VERIFY_STORAGE_TYPE(T) static_assert(TIsDerivedFrom<T, FMemberDataRegistryTag>::IsDerived, "err")

//////////////////////////////////////////////////////////////////////////

// Obj and Prop
USTRUCT()
struct GENERICSTORAGES_API FMemberDataRegistryType
{
	GENERATED_BODY()
public:
	UPROPERTY()
	UObject* Obj = nullptr;

	FStructProperty* Prop = nullptr;

	template<typename T>
	T* GetDataPtr() const
	{
		STATIC_VERIFY_STORAGE_TYPE(T);
		checkSlow(Prop && Prop->Struct && Prop->Struct->IsA<T>());
		return ensure(IsValid(Obj)) ? Prop->ContainerPtrToValuePtr<T>(Obj) : nullptr;
	}
};

UCLASS(Transient)
class UMemberDataRegistryStorage : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient)
	TMap<UScriptStruct*, FMemberDataRegistryType> DataStore;
};

//////////////////////////////////////////////////////////////////////////

struct GENERICSTORAGES_API FMemberDataRegistry
{
public:
	template<typename T>
	static T* GetStorage(const UObject* WorldContextObj = nullptr)
	{
		STATIC_VERIFY_STORAGE_TYPE(T);
		if (FMemberDataRegistryType* Found = GetStorageCell(T::StaticStruct(), WorldContextObj))
		{
			return Found->GetDataPtr<T>();
		}
		return nullptr;
	}

	template<typename T>
	static T& GetStorage(T& Defualt, const UObject* WorldContextObj = nullptr)
	{
		if (T* Data = GetStorage<T>(WorldContextObj))
		{
			return *Data;
		}
		return Defualt;
	}

	template<typename C, typename T>
	static bool RegitserStorage(UObject* Object, FName MemberStructName, bool bReplaceExist = false)
	{
		STATIC_VERIFY_STORAGE_TYPE(T);
#if WITH_EDITOR
		static FName TypeName = PropName;
		ensure(TypeName == PropName);
#endif
		static auto Prop = FMemberDataRegistry::FindStructProperty<C, T>(MemberStructName);
		check(Object && Prop);
		if (!Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			checkSlow(T::StaticStruct() == Prop->Struct);
			checkSlow(C::StaticClass() == Prop->GetOuter());
			if (ensure(C::StaticClass() == Object->GetClass()))
			{
				return SetStorageCell(T::StaticStruct(), Object, Prop, bReplaceExist);
			}
		}
		return false;
	}

protected:
	template<typename C, typename T>
	static FStructProperty* FindStructProperty(FName PropName)
	{
		FStructProperty* TheProp = [&] {
			FStructProperty* Prop = FindFieldChecked<FStructProperty>(C::StaticClass(), PropName);
			checkSlow(Prop->Struct == T::StaticStruct());
			return Prop;
		}();
		return TheProp;
	}

	static FMemberDataRegistryType* GetStorageCell(UScriptStruct* Struct, const UObject* WorldContextObj = nullptr);
	static bool SetStorageCell(UScriptStruct* Struct, UObject* Object, FStructProperty* Prop, bool bReplaceExist);
};

#define MEMBER_DATA_BIND(Class, Member) FMemberDataRegistry::RegitserStorage<Class, decltype(Class::Member)>(this, GET_MEMBER_NAME_CHECKED(Class, Member), true)
