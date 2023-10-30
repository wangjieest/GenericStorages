// Copyright GenericAbilities, Inc. All Rights Reserved.

#include "TypeTableBindding.h"

#include "GenericSingletons.h"

FTypeTableBindding& FTypeTableBindding::Get()
{
	static FTypeTableBindding Singleton;

	if (TrueOnFirstCall([] {}))
	{
		GenericSingletons::DeferredWorldCleanup(TEXT("TypeTableBindding"), [] {
			auto It = Singleton.Binddings.CreateIterator();
			while (It)
			{
				if (!It->Key.IsEmpty() && It->Value.IsValid())
				{
					++It;
				}
				else
				{
					It.RemoveCurrent();
				}
			}
		});
	}
	return Singleton;
}

UScriptStruct* FTypeTableBindding::FindType(UClass* Class)
{
	UScriptStruct* Ret = nullptr;
	if (Class)
	{
		if (auto Find = Get().Binddings.Find(Class->GetName()))
		{
			//ensure(*Find == S::StaticStruct())
			Ret = Find->Get();
		}
	}
	return Ret;
}

namespace TypeTableBindding
{
struct FKeyCache
{
	FName Base;
	FName Sub;

	FKeyCache(const FString& InBase, const FString& InSub)
		: Base(*InBase, InBase.Len())
		, Sub(*InSub, InSub.Len())
	{
	}
	FKeyCache(const FKeyCache&) = default;
	FKeyCache& operator=(const FKeyCache&) = default;
	FORCEINLINE friend bool operator==(const FKeyCache& Lhs, const FKeyCache& Rhs) { return (Lhs.Base == Rhs.Base) && (Lhs.Sub == Rhs.Sub); }
};

FORCEINLINE uint32 GetTypeHash(const FKeyCache& Key)
{
	return HashCombine(GetTypeHash(Key.Base), GetTypeHash(Key.Sub));
}

static TMap<TWeakObjectPtr<UDataTable>, TMap<FName, TMap<FKeyCache, FName>>> Lookup;
}  // namespace TypeTableBindding

UDataTable* FTableBinddingDeclaration::GetTableFromType(UClass* Class)
{
	return nullptr;
}

FName FTableBinddingDeclaration::GetRowName(UDataTable& Table, const TCHAR* InCategory, const TCHAR* InSubCategory)
{
	return NAME_None;
}
