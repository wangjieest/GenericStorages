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

#include "ObjectDataRegistry.h"

#include "Misc/CoreDelegates.h"
#include "Templates/SharedPointer.h"
#include "GenericSingletons.h"
#include "GameFramework/Actor.h"

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

protected:
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
#if ENGINE_MINOR_VERSION >= 23
		// TODO IsInGarbageCollectorThread()
		static TLockFreePointerListUnordered<FWeakObjectPtr, PLATFORM_CACHE_LINE_SIZE> GameThreadObjects;
		if (IsInGameThread())
#else
		check(IsInGameThread());
#endif
		{
#if ENGINE_MINOR_VERSION >= 23
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
#if ENGINE_MINOR_VERSION >= 23
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
	friend class UObjectDataRegisterUtil;
};

TUniquePtr<FObjectDataStorage> Mgr;

static TUniquePtr<FObjectDataStorage>& GetStorage(bool bCreate = false)
{
	if (bCreate && !Mgr)
	{
		Mgr = MakeUnique<FObjectDataStorage>();
		FCoreDelegates::OnPreExit.AddLambda([] {
			if (ensureAlways(Mgr))
			{
				Mgr.Reset();
			}
		});
	}
	return Mgr;
}
}  // namespace ObjectDataRegistry

void UObjectDataRegisterUtil::OnActorDestroyed(AActor* InActor)
{
	if (auto& Mgr = ObjectDataRegistry::GetStorage())
		Mgr->RemoveObject(FWeakObjectPtr(InActor));
}

void UObjectDataRegisterUtil::SetOnDestroyed(AActor* InActor)
{
	if (InActor)
		InActor->OnDestroyed.AddUniqueDynamic(GetMutableDefault<UObjectDataRegisterUtil>(), &UObjectDataRegisterUtil::OnActorDestroyed);
}

DEFINE_FUNCTION(UObjectDataRegisterUtil::execGetObjectData)
{
	P_GET_OBJECT(UObject, Obj);
	P_GET_UBOOL(bWriteData);
	P_GET_UBOOL_REF(bSucc);
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<UStructProperty>(nullptr);
	if (auto Prop = Cast<UStructProperty>(Stack.MostRecentProperty))
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

DEFINE_FUNCTION(UObjectDataRegisterUtil::execDeleteObjectData)
{
	P_GET_OBJECT(UObject, Obj);
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<UStructProperty>(nullptr);
	if (auto Prop = Cast<UStructProperty>(Stack.MostRecentProperty))
	{
		if (auto& Mgr = ObjectDataRegistry::GetStorage())
			Mgr->RemoveObjectData(Obj, Prop->Struct->GetFName());
	}
}

void UObjectDataRegisterUtil::DeleteObjectDataByName(const UObject* KeyObj, FName Name)
{
	FObjectDataRegitstry::DelDataPtr(KeyObj, Name);
}

void* FObjectDataRegitstry::FindDataPtr(const UObject* Obj, FName Key)
{
	if (auto& Mgr = ObjectDataRegistry::GetStorage())
	{
		return Mgr->GetObjectData(Obj, Key);
	}
	return nullptr;
}

void* FObjectDataRegitstry::GetDataPtr(const UObject* Obj, FName Key, const TFunctionRef<TSharedPtr<void>()>& Func)
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
				UObjectDataRegisterUtil::SetOnDestroyed(const_cast<AActor*>(Actor));

			Mgr->AddObjectData(Obj, Key, MoveTemp(Data));
		}
	}
	return Ret;
}

bool FObjectDataRegitstry::DelDataPtr(const UObject* Obj, FName Key)
{
	if (auto& Mgr = ObjectDataRegistry::GetStorage())
	{
		return Mgr->RemoveObjectData(Obj, Key);
	}
	return false;
}
