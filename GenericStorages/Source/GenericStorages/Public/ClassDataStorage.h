// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include <memory>

namespace ClassStorage
{
// each type (USTRUCT or UCLASS) would has a storage place in the type-tree for lookup fast
template<typename T>
struct TClassStorageImpl
{
protected:
	using StorageDataType = T;
	struct StorageContainerType;

	// there is issue with TSharedPtr, farword declare in class template caused incomplete type usage
	using DataPtrType = std::shared_ptr<StorageContainerType>;
	static auto* GetRaw(const DataPtrType& Ptr) { return Ptr.get(); }
	template<typename... Args>
	static DataPtrType AllocShared(Args&&... args)
	{
		return std::make_shared<StorageContainerType>(std::forward<Args>(args)...);
	}
	struct ClassIterator
	{
		explicit ClassIterator(const DataPtrType& InPtr)
			: Ptr(InPtr)
		{
		}
		// we do not return sharedptr type.
		FORCEINLINE const StorageContainerType* operator*() const { return GetRaw(Ptr); }
		FORCEINLINE const StorageContainerType* operator->() const { return GetRaw(Ptr); }
		FORCEINLINE explicit operator bool() { return Ptr != nullptr; }
		FORCEINLINE ClassIterator& operator++()
		{
			Ptr = Ptr->Super;
			return *this;
		}

	private:
		DataPtrType Ptr;
	};

	//////////////////////////////////////////////////////////////////////////
	struct StorageContainerType
	{
		TWeakObjectPtr<const UStruct> KeyClass;
		DataPtrType Super;
		StorageDataType Data;

		const StorageContainerType* GetSuper() const { return GetRaw(Super); }
	};
	TMap<TWeakObjectPtr<const UStruct>, DataPtrType> PersistentData;
	TMap<TWeakObjectPtr<const UStruct>, DataPtrType> RegisteredData;
	mutable TMap<TWeakObjectPtr<const UStruct>, DataPtrType> FastLookupTable;
	mutable bool bEnableAdd = true;

	//////////////////////////////////////////////////////////////////////////
	DataPtrType* Add(TMap<TWeakObjectPtr<const UStruct>, DataPtrType>& Regs, const UStruct* Class, bool bEnsure = true)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("TClassDataStorage::Add %s"), *Class->GetName());
#endif
		if (bEnsure && !ensureAlwaysMsgf(bEnableAdd, TEXT("TClassDataStorage cannot add data anymore")))
			return nullptr;

		auto& Ptr = Regs.FindOrAdd(Class);
		if (!Ptr)
		{
			Ptr = AllocShared();
			Ptr->KeyClass = Class;
			for (auto& a : Regs)
			{
				if (a.Key.IsValid() && a.Key->IsChildOf(Class) && a.Key.Get() != Class)
				{
					for (auto CurPtr = a.Value; CurPtr; CurPtr = CurPtr->Super)
					{
						auto SuperPtr = CurPtr->Super;
						if (!SuperPtr)
						{
#if !UE_BUILD_SHIPPING
							UE_LOG(LogTemp, Log, TEXT("TClassDataStorage::AddParent %s -> %s"), *CurPtr->KeyClass->GetName(), *Class->GetName());
#endif
							CurPtr->Super = Ptr;
							break;
						}
						else if (Class == SuperPtr->KeyClass)
						{
#if !UE_BUILD_SHIPPING
							UE_LOG(LogTemp, Log, TEXT("TClassDataStorage::AlreadyAdded %s -> %s"), *CurPtr->KeyClass->GetName(), *Class->GetName());
#endif
							break;
						}
						else if (SuperPtr->KeyClass.IsValid() && Class->IsChildOf(SuperPtr->KeyClass.Get()))
						{
#if !UE_BUILD_SHIPPING
							UE_LOG(LogTemp, Log, TEXT("TClassDataStorage::Inserted %s->  -> %s"), *CurPtr->KeyClass->GetName(), *Class->GetName(), *SuperPtr->KeyClass->GetName());
#endif
							Ptr->Super = CurPtr->Super;
							CurPtr->Super = Ptr;
							break;
						}
					}
				}
			}
			if (!Ptr->Super)
			{
				for (auto ParentClass = Class->GetSuperStruct(); ParentClass != nullptr; ParentClass = ParentClass->GetSuperStruct())
				{
					if (auto InnerPtr = Regs.Find(ParentClass))
					{
#if !UE_BUILD_SHIPPING
						UE_LOG(LogTemp, Log, TEXT("TClassDataStorage::AddedChild %s -> %s"), *Class->GetName(), *ParentClass->GetName());
#endif
						Ptr->Super = *InnerPtr;
						break;
					}
				}
			}
		}
		return &Ptr;
	};

	DataPtrType Find(const UStruct* Class, bool bDisableAdd = true) const
	{
		// as first query, no more add needed
		if (bDisableAdd)
			EnableAdd(false);
		checkSlow(Class);

		if (auto FindPtr = FastLookupTable.Find(Class))
			return *FindPtr;

		DataPtrType& Ptr = FastLookupTable.Add(Class);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("TClassDataStorage::FastLookupTable Add %s"), *Class->GetName());
#endif
		for (auto ParentClass = Class->GetSuperStruct(); ParentClass != nullptr; ParentClass = ParentClass->GetSuperStruct())
		{
			if (auto InnerPtr = RegisteredData.Find(ParentClass))
			{
#if !UE_BUILD_SHIPPING
				UE_LOG(LogTemp, Log, TEXT("TClassDataStorage::FastLookupTable %s -> %s"), *Class->GetName(), *ParentClass->GetName());
#endif
				Ptr = *InnerPtr;
				break;
			}
		}
		return Ptr;
	}
	void Clear()
	{
		UE_LOG(LogTemp, Log, TEXT("TClassDataStorage::Clear"));
		FastLookupTable.Reset();
		RegisteredData.Reset();
		EnableAdd(true);

		for (const auto& a : PersistentData)
		{
			if (a.Key.IsValid())
				RegisteredData.Add(a.Key.Get(), AllocShared(*a.Value));
		}

		for (auto& Pair : RegisteredData)
		{
			DataPtrType& Value = Pair.Value;
			if (Value && Value->Super)
			{
				auto SuperClass = Value->Super->KeyClass;
				Value->Super = nullptr;
				if (auto Ptr = RegisteredData.Find(SuperClass))
					Value->Super = *Ptr;
			}
		}
	}

public:
	void Cleanup() { Clear(); }
	bool EnableAdd(bool bNewEanbled) const
	{
		bool bEanbled = bEnableAdd;
		bEnableAdd = bNewEanbled;
		return bEanbled;
	}

	StorageDataType* FindData(const UStruct* Class) const
	{
		auto Ptr = Find(Class);
		return Ptr ? &Ptr->Data : (StorageDataType*)nullptr;
	}
	auto CreateIterator(const UStruct* Class) const { return ClassIterator(Find(Class)); }
	auto CreateIterator() const { return RegisteredData.CreateConstIterator(); }
	auto GetKeys()
	{
		TArray<TWeakObjectPtr<const UStruct>> Keys;
		RegisteredData.GetKeys(Keys);
		return Keys;
	}
	template<typename F /*void(T&)*/>
	void ModifyData(const UStruct* Class, bool bPersistent, const F& Fun, bool bPrompt = true)
	{
		if (ensureAlways(Class))
		{
			if (bPersistent)
			{
				if (auto DataPtr = Add(PersistentData, Class, bPrompt))
				{
					Fun((*DataPtr)->Data);
				}
			}

			if (auto DataPtr = Add(RegisteredData, Class, bPrompt))
			{
				Fun((*DataPtr)->Data);
				FastLookupTable.FindOrAdd(Class) = *DataPtr;
			}
		}
	}
	template<typename F>
	void ModifyCurrent(const UStruct* Class, const F& Fun, bool bEnable = false)
	{
		if (ensureAlways(Class))
		{
			if (auto ClassPtr = Find(Class))
			{
				Fun(ClassPtr->Data, true);
				FastLookupTable.FindOrAdd(Class) = ClassPtr;
			}
			else
			{
				if (bEnable)
					EnableAdd(true);
				if (auto DataPtr = Add(RegisteredData, Class))
				{
					Fun((*DataPtr)->Data, false);
					FastLookupTable.FindOrAdd(Class) = *DataPtr;
				}
			}
		}
	}
};

}  // namespace ClassStorage
