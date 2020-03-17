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
#include "GS_Compatibility.h"

#include "MemberDataRegitstry.generated.h"

USTRUCT(BlueprintType)
struct GENERICSTORAGES_API FMemberDataStorageBase
{
	GENERATED_BODY()
public:
	FMemberDataStorageBase();
	~FMemberDataStorageBase();
	FMemberDataStorageBase(FMemberDataStorageBase&& Other) = default;
	FMemberDataStorageBase& operator=(FMemberDataStorageBase&& Other) = default;
	FMemberDataStorageBase(const FMemberDataStorageBase& Other) = default;
	FMemberDataStorageBase& operator=(const FMemberDataStorageBase& Other) = default;

protected:
	FWeakObjectPtr Outer;
	friend struct FMemberDataRegitstry;
};

#define STATIC_VERIFY_STORAGE_TYPE(T) static_assert(TIsDerivedFrom<T, FMemberDataStorageBase>::IsDerived, "err")

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

struct GENERICSTORAGES_API FMemberDataRegitstry
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
		static auto Prop = FMemberDataRegitstry::FindStructProperty<C, T>(MemberStructName);
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

#define MEMBER_DATA_BIND(Class, Member) FMemberDataRegitstry::RegitserStorage<Class, decltype(Class::Member)>(this, GET_MEMBER_NAME_CHECKED(Class, Member), true)
