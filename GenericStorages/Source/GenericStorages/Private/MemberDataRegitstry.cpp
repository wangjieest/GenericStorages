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

#include "MemberDataRegitstry.h"

#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/World.h"
#include "UObject/UObjectThreadContext.h"
#include "GenericSingletons.h"

namespace MemberDataRegitstry
{
static UMemberDataRegistryStorage* GetDataStorage(const UObject* WorldContextObj)
{
	return UGenericSingletons::GetSingleton<UMemberDataRegistryStorage>(WorldContextObj);
}

static TMap<FWeakObjectPtr, TArray<TWeakObjectPtr<UScriptStruct>>> Structs;

static void Remove(FWeakObjectPtr WeakObject)
{
	if (auto Storage = UGenericSingletons::GetSingleton<UMemberDataRegistryStorage>(WeakObject.Get(), false))
	{
		TArray<TWeakObjectPtr<UScriptStruct>> Arr;
		if (Structs.RemoveAndCopyValue(WeakObject, Arr))
		{
			for (auto& a : Arr)
			{
				if (a.IsValid())
					Storage->DataStore.Remove(a.Get());
			}
		}
	}
}
}  // namespace MemberDataRegitstry

FMemberDataStorageBase::FMemberDataStorageBase()
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	UObject* Initializer = ThreadContext.IsInConstructor ? ThreadContext.TopInitializerChecked().GetObj() : nullptr;
	if (!Outer.IsValid())
		Outer = Initializer;
}

FMemberDataStorageBase::~FMemberDataStorageBase()
{
	if (Outer.IsStale())
	{
		MemberDataRegitstry::Remove(Outer);
	}
}

FMemberDataRegistryType* FMemberDataRegitstry::GetStorageCell(UScriptStruct* Struct, const UObject* WorldContextObj)
{
	return MemberDataRegitstry::GetDataStorage(WorldContextObj)->DataStore.Find(Struct);
}

bool FMemberDataRegitstry::SetStorageCell(UScriptStruct* Struct, UObject* Object, FStructProperty* Prop, bool bReplaceExist)
{
	auto& Data = MemberDataRegitstry::GetDataStorage(Object)->DataStore.FindOrAdd(Struct);
	if (bReplaceExist || ensure(!Data.Obj || Data.Obj == Object))
	{
		Data.Obj = Object;
		Data.Prop = Prop;
		Prop->ContainerPtrToValuePtr<FMemberDataStorageBase>(Object)->Outer = Object;
		return true;
	}
	return false;
}
