#pragma once

#include "CoreMinimal.h"
#include "UnrealCompatibility.h"
#if UE_4_25_OR_LATER
#include "UObject/UnrealType.h"
#include "UObject/WeakFieldPtr.h"

struct FGMPPropertyInfo
{
public:
	FGMPPropertyInfo()
		: Property()
		, ArrayIndex(INDEX_NONE)
	{
	}

	FGMPPropertyInfo(TWeakFieldPtr<FProperty> InProperty, int32 InArrayIndex = INDEX_NONE)
		: Property(InProperty)
		, ArrayIndex(InArrayIndex)
	{
	}

	bool operator==(const FGMPPropertyInfo& Other) const { return Property == Other.Property && ArrayIndex == Other.ArrayIndex; }

	bool operator!=(const FGMPPropertyInfo& Other) const { return !(*this == Other); }

	TWeakFieldPtr<FProperty> Property;
	int32 ArrayIndex;
};

class FGMPPropertyPath
{
public:
	static TSharedRef<FGMPPropertyPath> CreateEmpty() { return MakeShareable(new FGMPPropertyPath()); }

	static TSharedRef<FGMPPropertyPath> Create(const TWeakFieldPtr<FProperty> Property)
	{
		TSharedRef<FGMPPropertyPath> NewPath = MakeShareable(new FGMPPropertyPath());

		FGMPPropertyInfo NewPropInfo;
		NewPropInfo.Property = Property;
		NewPropInfo.ArrayIndex = INDEX_NONE;

		NewPath->Properties.Add(NewPropInfo);

		return NewPath;
	}

	int32 GetNumProperties() const { return Properties.Num(); }

	const FGMPPropertyInfo& GetPropertyInfo(int32 index) const { return Properties[index]; }

	const FGMPPropertyInfo& GetLeafMostProperty() const { return Properties[Properties.Num() - 1]; }

	const FGMPPropertyInfo& GetRootProperty() const { return Properties[0]; }

	TSharedRef<FGMPPropertyPath> ExtendPath(const FGMPPropertyInfo& NewLeaf)
	{
		TSharedRef<FGMPPropertyPath> NewPath = MakeShareable(new FGMPPropertyPath());

		NewPath->Properties = Properties;
		NewPath->Properties.Add(NewLeaf);

		return NewPath;
	}

	TSharedRef<FGMPPropertyPath> ExtendPath(const TSharedRef<FGMPPropertyPath>& Extension)
	{
		TSharedRef<FGMPPropertyPath> NewPath = MakeShareable(new FGMPPropertyPath());

		NewPath->Properties = Properties;

		for (int Index = Extension->GetNumProperties() - 1; Index >= 0; Index--)
		{
			NewPath->Properties.Add(Extension->GetPropertyInfo(Index));
		}

		return NewPath;
	}

	TSharedRef<FGMPPropertyPath> TrimPath(const int32 AmountToTrim)
	{
		TSharedRef<FGMPPropertyPath> NewPath = MakeShareable(new FGMPPropertyPath());

		NewPath->Properties = Properties;

		for (int Count = 0; Count < AmountToTrim; Count++)
		{
			NewPath->Properties.Pop();
		}

		return NewPath;
	}

	TSharedRef<FGMPPropertyPath> TrimRoot(const int32 AmountToTrim)
	{
		TSharedRef<FGMPPropertyPath> NewPath = CreateEmpty();

		for (int Count = AmountToTrim; Count < Properties.Num(); Count++)
		{
			NewPath->Properties.Add(Properties[Count]);
		}

		return NewPath;
	}

	FString ToString(const TCHAR* Separator = TEXT("->")) const
	{
		FString NewDisplayName;
		bool FirstAddition = true;
		for (int PropertyIndex = 0; PropertyIndex < Properties.Num(); PropertyIndex++)
		{
			const FGMPPropertyInfo& PropInfo = Properties[PropertyIndex];
			if (!(PropInfo.Property->IsA(FArrayProperty::StaticClass()) && PropertyIndex != Properties.Num() - 1))
			{
				if (!FirstAddition)
				{
					NewDisplayName += Separator;
				}

				NewDisplayName += PropInfo.Property->GetFName().ToString();

				if (PropInfo.ArrayIndex != INDEX_NONE)
				{
					NewDisplayName += FString::Printf(TEXT("[%d]"), PropInfo.ArrayIndex);
				}

				FirstAddition = false;
			}
		}

		return NewDisplayName;
	}

	/**
	* Add another property to be associated with this path
	*/
	void AddProperty(const FGMPPropertyInfo& InProperty) { Properties.Add(InProperty); }

	static bool AreEqual(const TSharedRef<FGMPPropertyPath>& PathLhs, const TSharedRef<FGMPPropertyPath>& PathRhs)
	{
		bool Result = false;

		if (PathLhs->GetNumProperties() == PathRhs->GetNumProperties())
		{
			bool FoundMatch = true;
			for (int Index = PathLhs->GetNumProperties() - 1; Index >= 0; --Index)
			{
				const FGMPPropertyInfo& LhsPropInfo = PathLhs->GetPropertyInfo(Index);
				const FGMPPropertyInfo& RhsPropInfo = PathRhs->GetPropertyInfo(Index);

				if (LhsPropInfo.Property != RhsPropInfo.Property || LhsPropInfo.ArrayIndex != RhsPropInfo.ArrayIndex)
				{
					FoundMatch = false;
					break;
				}
			}

			if (FoundMatch)
			{
				Result = true;
			}
		}

		return Result;
	}

private:
	TArray<FGMPPropertyInfo> Properties;
};

FORCEINLINE bool operator==(const FGMPPropertyPath& LHS, const FGMPPropertyPath& RHS)
{
	if (LHS.GetNumProperties() != RHS.GetNumProperties())
	{
		return false;
	}

	for (int32 i = 0, end = RHS.GetNumProperties(); i != end; ++i)
	{
		if (LHS.GetPropertyInfo(i) != RHS.GetPropertyInfo(i))
		{
			return false;
		}
	}

	return true;
}

FORCEINLINE bool operator!=(const FGMPPropertyPath& LHS, const FGMPPropertyPath& RHS)
{
	return !(LHS == RHS);
}

FORCEINLINE uint32 GetTypeHash(FGMPPropertyPath const& Path)
{
	//lets concat the name and just hash the string
	uint32 Ret = 0;
	for (int32 i = 0, end = Path.GetNumProperties(); i != end; ++i)
	{
		if (Path.GetPropertyInfo(i).Property.IsValid())
		{
			Ret = Ret ^ GetTypeHash(Path.GetPropertyInfo(i).Property->GetFName());
		}
	}
	return Ret;
}
#endif
