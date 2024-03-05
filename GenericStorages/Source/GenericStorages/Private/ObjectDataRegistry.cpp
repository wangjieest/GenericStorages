// Copyright GenericStorages, Inc. All Rights Reserved.

#include "ObjectDataRegistry.h"

#include "GameFramework/Actor.h"
#include "GenericSingletons.h"
#include "Misc/CoreDelegates.h"
#include "PrivateFieldAccessor.h"
#include "Templates/SharedPointer.h"
#include "UnrealCompatibility.h"
#include "WorldLocalStorages.h"

namespace ObjectDataRegistry
{
using DataType = TSharedPtr<void>;
using KeyType = FName;

struct FObjectDataStorage : public FUObjectArray::FUObjectDeleteListener
{
	virtual ~FObjectDataStorage()
	{
		if (bListened && UObjectInitialized())
			DisableListener();
	}

	TMap<FWeakObjectPtr, TMap<KeyType, DataType>> ObjectDataStorage;
	TMap<KeyType, TArray<FWeakObjectPtr>> KeyCache;
	bool bListened = false;

	virtual void NotifyUObjectDeleted(const UObjectBase* ObjectBase, int32 Index) override
	{
		auto Object = static_cast<const UObject*>(ObjectBase);
		OnObjectRemoved(Object);
	}

	void OnObjectRemoved(const UObject* Object)
	{
#if UE_4_23_OR_LATER
		// TODO IsInGarbageCollectorThread()
		static TLockFreePointerListUnordered<FWeakObjectPtr, PLATFORM_CACHE_LINE_SIZE> GameThreadObjects;
		if (IsInGameThread())
#else
		check(IsInGameThread());
#endif
		{
#if UE_4_23_OR_LATER
			if (!GameThreadObjects.IsEmpty())
			{
				TArray<FWeakObjectPtr*> Objs;
				GameThreadObjects.PopAll(Objs);
				for (auto Ptr : Objs)
				{
					RemoveObject(*Ptr);
					delete Ptr;
				}
			}
#endif
			RemoveObject(Object);

			if (ObjectDataStorage.Num() == 0)
				DisableListener();
		}
#if UE_4_23_OR_LATER
		else
		{
			GameThreadObjects.Push(new FWeakObjectPtr(Object));
		}
#endif
	}
	void OnUObjectArrayShutdown() { DisableListener(); }
	void DisableListener()
	{
		if (bListened)
		{
			bListened = false;
			GUObjectArray.RemoveUObjectDeleteListener(this);
		}
	}
	void EnableListener()
	{
		if (!bListened)
		{
			bListened = true;
			GUObjectArray.AddUObjectDeleteListener(this);
		}
	}
	bool RemoveObject(const FWeakObjectPtr& Object)
	{
		if (auto Find = ObjectDataStorage.Find(Object))
		{
			for (auto& a : *Find)
				KeyCache.Remove(a.Key);
			ObjectDataStorage.Remove(Object);
			return true;
		}
		return false;
	}
	bool RemoveKey(const KeyType& Key)
	{
		if (auto Find = KeyCache.Find(Key))
		{
			for (auto& Obj : *Find)
			{
				if (auto Found = ObjectDataStorage.Find(Obj))
					Found->Remove(Key);
			}
			KeyCache.Remove(Key);
			return true;
		}
		return false;
	}

public:
	void AddObjectData(const FWeakObjectPtr& Object, const KeyType& Key, DataType Data)
	{
		check(Object.IsValid() && Key.IsValid());
		if (ObjectDataStorage.Num() == 0)
			EnableListener();
		ObjectDataStorage.FindOrAdd(Object).Add(Key, Data);
		KeyCache.FindOrAdd(Key).Add(Object);
	}

	void* GetObjectData(const FWeakObjectPtr& Object, const KeyType& Key)
	{
		if (auto Find = ObjectDataStorage.Find(Object))
		{
			if (auto Found = Find->Find(Key))
			{
				return Found->Get();
			}
		}
		return nullptr;
	}
	bool RemoveObjectData(const FWeakObjectPtr& Object, const KeyType& Key)
	{
		check(Object.IsValid() && Key.IsValid());
		if (auto Find = ObjectDataStorage.Find(Object))
		{
			if (auto Found = Find->Find(Key))
			{
				Find->Remove(Key);
				if (auto Cache = KeyCache.Find(Key))
					Cache->Remove(Object);
				if (ObjectDataStorage.Num() == 0)
					DisableListener();
				return true;
			}
		}
		return false;
	}
};

TUniquePtr<FObjectDataStorage> GlobalMgr;

TUniquePtr<FObjectDataStorage>& GetStorage(bool bCreate = false)
{
	if (bCreate && !GlobalMgr)
	{
		GlobalMgr = MakeUnique<FObjectDataStorage>();
		FCoreDelegates::OnPreExit.AddLambda([] {
			if (ensureAlways(GlobalMgr))
			{
				GlobalMgr.Reset();
			}
		});
	}
	return GlobalMgr;
}
}  // namespace ObjectDataRegistry

void* FObjectDataRegistry::FindDataPtr(const UObject* Obj, FName Key)
{
	if (auto& Mgr = ObjectDataRegistry::GetStorage())
	{
		return Mgr->GetObjectData(Obj, Key);
	}
	return nullptr;
}

bool FObjectDataRegistry::DelDataPtr(const UObject* Obj, FName Key)
{
	if (auto& Mgr = ObjectDataRegistry::GetStorage())
	{
		return Mgr->RemoveObjectData(Obj, Key);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

void UObjectDataRegistryHelper::OnActorDestroyed(AActor* InActor)
{
	if (auto& Mgr = ObjectDataRegistry::GetStorage())
		Mgr->RemoveObject(FWeakObjectPtr(InActor));
}

DEFINE_FUNCTION(UObjectDataRegistryHelper::execGetObjectData)
{
	P_GET_OBJECT(UObject, Obj);
	P_GET_UBOOL(bWriteData);
	P_GET_UBOOL_REF(bSucc);
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	if (auto Prop = CastField<FStructProperty>(Stack.MostRecentProperty))
	{
		auto& Mgr = ObjectDataRegistry::GetStorage(true);
		void* Ptr = Mgr->GetObjectData(Obj, Prop->Struct->GetFName());
		if (Ptr)
		{
			if (!bWriteData)
				Prop->CopyCompleteValueToScriptVM(Stack.MostRecentPropertyAddress, Ptr);
			else
				Prop->CopyCompleteValueFromScriptVM(Ptr, Stack.MostRecentPropertyAddress);
			bSucc = true;
		}
		else if (auto Ops = Prop->Struct->GetCppStructOps())
		{
			Ptr = FMemory::Malloc(Ops->GetSize(), Ops->GetAlignment());
			Ops->Construct(Ptr);
			auto SPtr = TSharedPtr<void>(Ptr, [Ops](void* p) {
				if (Ops->HasDestructor())
					Ops->Destruct(p);
				FMemory::Free(p);
			});
			Prop->CopyCompleteValueFromScriptVM(Ptr, Stack.MostRecentPropertyAddress);
			Mgr->AddObjectData(Obj, Prop->Struct->GetFName(), SPtr);
			bSucc = true;
		}
		else
		{
			bSucc = false;
		}
	}
	else
	{
		bSucc = false;
	}
}

DEFINE_FUNCTION(UObjectDataRegistryHelper::execDelObjectData)
{
	P_GET_OBJECT(UObject, Obj);
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	if (auto Prop = CastField<FStructProperty>(Stack.MostRecentProperty))
	{
		if (auto& Mgr = ObjectDataRegistry::GetStorage())
			Mgr->RemoveObjectData(Obj, Prop->Struct->GetFName());
	}
}

void UObjectDataRegistryHelper::DeleteObjectDataByName(const UObject* KeyObj, FName Name)
{
	FObjectDataRegistry::DelDataPtr(KeyObj, Name);
}

GS_PRIVATEACCESS_FUNCTION_NAME(UObjectDataRegistryHelper, OnActorDestroyed, void(AActor*))
void* FObjectDataRegistry::GetDataPtr(const UObject* Obj, FName Key, const TFunctionRef<TSharedPtr<void>()>& Func)
{
	void* Ret = nullptr;
	if (auto& Mgr = ObjectDataRegistry::GetStorage(true))
	{
		Ret = Mgr->GetObjectData(Obj, Key);
		if (!Ret)
		{
			auto Data = Func();
			Ret = Data.Get();
			if (auto Actor = Cast<AActor>(Obj))
			{
				static FName OnActorDestroyedName = PrivateAccess::UObjectDataRegistryHelper::OnActorDestroyed;
				auto Helper = UGenericSingletons::GetSingleton<UObjectDataRegistryHelper>(Obj);
				auto& OnDestroyed = const_cast<AActor*>(Actor)->OnDestroyed;
				if (OnDestroyed.Contains(Helper, OnActorDestroyedName))
				{
#if UE_5_03_OR_LATER
					TBaseDynamicDelegate<FNotThreadSafeDelegateMode, void, AActor*> Delegate;
#else
					TBaseDynamicDelegate<FWeakObjectPtr, void, AActor*> Delegate;
#endif
					Delegate.BindUFunction(Helper, OnActorDestroyedName);
					OnDestroyed.Add(Delegate);
				}
			}
			Mgr->AddObjectData(Obj, Key, MoveTemp(Data));
		}
	}
	return Ret;
}
