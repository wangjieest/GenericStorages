// Copyright GenericAbilities, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "Engine/DataTable.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "TypeTableBindding.generated.h"

USTRUCT()
struct FTypeTableConfig
{
	GENERATED_BODY()
public:
	// FindBy
	bool operator==(UClass* Class) const { return Class && Class == Type; }

	UPROPERTY(EditDefaultsOnly, Category = "TypeTableConfig")
	UClass* Type = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "TypeTableConfig")
	UDataTable* Table = nullptr;
};

struct GENERICSTORAGES_API FTypeTableBindding
{
public:
	static UScriptStruct* FindType(UClass* Class);

protected:
	static FTypeTableBindding& Get();

	//////////////////////////////////////////////////////////////////////////
	template<typename C, typename T>
	static bool AddType(TSubclassOf<C> Class)
	{
		static_assert(TIsDerivedFrom<T, FTableRowBase>::IsDerived, "err");
		if (ensure(Class))
		{
			auto ClassName = Class->GetName();
			if (!(ClassName.StartsWith(TEXT("SKEL_")) && ClassName.EndsWith(TEXT("_C"))))
			{
				auto& Value = Get().Binddings.FindOrAdd(Class->GetName());
				auto SrciptStruct = T::StaticStruct();
				if (!Value.IsValid() || Value.Get() == SrciptStruct)
				{
					Value = SrciptStruct;
					return true;
				}
			}
		}
		return false;
	}

protected:
	friend class FTypeToTableCustomization;
	TMap<FString, TWeakObjectPtr<UScriptStruct>> Binddings;
};

struct FTableBinddingDeclaration
{
	static UDataTable* GetTableFromType(UClass* Class);
	static FName GetRowName(UDataTable& Table, const TCHAR* Category, const TCHAR* SubCategory);
};

template<typename D>
struct GENERICSTORAGES_API TTableBinddingDeclaration : public FTableBinddingDeclaration
{
protected:
	using FTableBinddingDeclaration::GetRowName;
	using FTableBinddingDeclaration::GetTableFromType;

	template<typename T>
	static const T* FindTableRow(const UObject* TypeObject, const TCHAR* Category, const TCHAR* SubCategory)
	{
		check(TypeObject);
		if (auto Table = D::GetTableFromType(TypeObject->GetClass()))
		{
			auto RowName = D::GetRowName(*Table, Category, SubCategory);
			return Table->template FindRow<T>(RowName, *TypeObject->GetName());
		}
		return nullptr;
	}

	template<typename T, typename C>
	static bool BindDeclaration(UObject* This)
	{
#if WITH_EDITOR
		static auto FindFirstNativeClass = [](UClass* Class) {
			for (; Class; Class = Class->GetSuperClass())
			{
				if (0 != (Class->ClassFlags & CLASS_Native))
				{
					break;
				}
			}
			return Class;
		};
		check(IsValid(This));
		check(FindFirstNativeClass(This->GetClass()) == C::StaticClass());
		return FTypeTableBindding::AddType<C, T>(This->GetClass());
#else
		return true;
#endif
	}
};

//////////////////////////////////////////////////////////////////////////
#if 0
// Decl Binddings
template<typename T, typename C>
class TExecutionBaseBindding : public TableBinddingDeclaration
{
public:
#if WITH_EDITOR
	TExecutionBaseBindding() { TableBinddingDeclaration::BindDeclaration<T, C>(static_cast<C*>(this)); }
#endif

protected:
	const UDataTable* FindTable() const { return TableBinddingDeclaration::GetTableFromTypeImpl(static_cast<const C*>(this)->GetClass()); }
	const T* FindRowData(const TCHAR* SubCategory = nullptr) const { return TableBinddingDeclaration::GetRowForExecution<T>(static_cast<const C*>(this), *static_cast<const C*>(this)->GetClass()->GetName(), SubCategory); }
};

// Decl Binddings
template<typename T, typename C>
class TExecutionPhaseBindding : public TableBinddingDeclaration
{
public:
#if WITH_EDITOR
	TExecutionPhaseBindding() { TableBinddingDeclaration::BindDeclaration<T, C>(static_cast<C*>(this)); }
#endif

protected:
	const UDataTable* FindTable() const { return TableBinddingDeclaration::GetTableFromTypeImpl(static_cast<const C*>(this)->GetClass()); }
	const T* FindRowData(const UGameplayEffectExecutionCalculation* ExectuionBase = nullptr) const
	{
		return TableBinddingDeclaration::GetRowForExecution<T>(static_cast<const C*>(this), ExectuionBase ? *ExectuionBase->GetClass()->GetName() : nullptr, *static_cast<const C*>(this)->GetClass()->GetName());
	}
};
#endif
