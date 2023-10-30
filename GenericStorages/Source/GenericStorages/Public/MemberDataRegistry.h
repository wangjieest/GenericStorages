// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WorldLocalStorages.h"

struct GENERICSTORAGES_API FWeakMemberDataTag
{
#if WITH_EDITOR
protected:
	FWeakObjectPtr Outer;

public:
	UObject* GetOuter() const { return Outer.Get(); }
#endif

public:
	FWeakMemberDataTag();
};

//////////////////////////////////////////////////////////////////////////
struct GENERICSTORAGES_API FWeakMemberData
{
protected:
	TWeakObjectPtr<UObject> Obj;
	void* Ptr = nullptr;
#if WITH_EDITOR
	FStructProperty* Prop = nullptr;
#endif
	bool IsValid() const { return (Obj.IsValid() && Ptr); }
};

template<typename T>
struct TWeakMemberData : public FWeakMemberData
{
protected:
	friend struct FMemberDataRegistry;
	static_assert(TIsDerivedFrom<T, FWeakMemberDataTag>::IsDerived, "err");

	T* GetMemberPtr() const
	{
#if WITH_EDITOR
		checkSlow(!Prop || (Prop->Struct && Prop->Struct->IsA<T>()));
		auto Ret = ensure(FWeakMemberData::IsValid()) ? Prop->ContainerPtrToValuePtr<T>(Obj.Get()) : nullptr;
		ensure(!Ret || Ret == Ptr);
		return Ret;
#else
		return IsValid() ? reinterpret_cast<T*>(Ptr) : nullptr;
#endif
	}

#if WITH_EDITOR
	template<typename C>
	static FStructProperty* FindStructProperty(FName PropName)
	{
		FStructProperty* Prop = FindFieldChecked<FStructProperty>(C::StaticClass(), PropName);
		checkSlow(Prop->Struct == T::StaticStruct());
		return Prop;
	}
#endif

	static T* GetStorage(const UObject* WorldContextObj) { return WorldLocalStorages::GetLocalValue<TWeakMemberData<T>>(WorldContextObj, false); }

	static T& GetStorage(T& Default, const UObject* WorldContextObj)
	{
		auto* Data = GetStorage(WorldContextObj);
		return Data ? *Data : Default;
	}

	template<typename C, std::enable_if_t<std::is_base_of<UObject, std::decay_t<C>>::value>* = nullptr>
	static bool RegisterStorage(C* This, T(C::*Data), FName DataName)
	{
#if WITH_EDITOR
		static FName TypeMemberName = DataName;
		ensure(TypeMemberName == DataName);
		static auto Prop = TWeakMemberData<T>::template FindStructProperty<C>(DataName);
		check(This && Prop);
		checkSlow(T::StaticStruct() == Prop->Struct);
		checkSlow(C::StaticClass() == Prop->GetOuter());
#endif
		if (!This->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && ensure(C::StaticClass() == This->GetClass()))
		{
			auto& WeakStore = WorldLocalStorages::GetLocalValue<TWeakMemberData<T>>(This);
			WeakStore.Obj = This;
			WeakStore.Ptr = &(This->*Data);
#if WITH_EDITOR
			WeakStore.Prop = Prop;
			ensureAlways(WeakStore.GetMemberPtr()->GetOuter() == This);
#endif
			return true;
		}
		return false;
	}
};

struct FMemberDataRegistry
{
	template<typename T>
	static T* GetStorage(const UObject* WorldContextObj)
	{
		return TWeakMemberData<T>::GetStorage(WorldContextObj);
	}

	template<typename T>
	static T& GetStorage(T& Default, const UObject* WorldContextObj)
	{
		return TWeakMemberData<T>::GetStorage(Default, WorldContextObj);
	}

	template<typename C, typename T>
	static bool RegisterStorage(C* This, T(C::*Data), FName DataName)
	{
		return TWeakMemberData<T>::RegisterStorage(This, Data, DataName);
	}
};

#define MEMBER_DATA_REGISTER(Class, Member) FMemberDataRegistry::RegisterStorage(this, &Class::Member, GET_MEMBER_NAME_CHECKED(Class, Member), true)
