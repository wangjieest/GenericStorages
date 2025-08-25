// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "StaticProperty.h"

#include "SubSystemStorages.generated.h"

UCLASS()
class UScopeSharedStorage : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	template<typename T>
	static bool SetScopedSharedStorage(UObject* InScope, FName Key, const T& Val, bool bReplace = false)
	{
		return  SetScopedSharedStorageImpl(InScope, Key, GenericStorages::StaticProperty<T>(), std::addressof(Val), bReplace);
	}
	template<typename T>
	static const T* GetScopedSharedStorage(UObject* InScope, FName Key)
	{
		return static_cast<const T*>(GetScopedSharedStorageImpl(InScope, Key, GenericStorages::StaticProperty<T>()));
	}
	template<typename T>
	static const T& GetScopedSharedStorage(UObject* InScope, FName Key, T& Val)
	{
		auto Ret = static_cast<const T*>(GetScopedSharedStorageImpl(InScope, Key, GenericStorages::StaticProperty<T>()));
		return Ret ? *Ret : Val;
	}
	
protected:
	UFUNCTION(BlueprintCallable, Category = "ScopeSharedStorage", CustomThunk, meta=(WorldContext="InScope", bReplace="true", CustomStructureParam="Val"))
	static bool SetScopedSharedStorage(UObject* InScope, bool bReplace, const FName& InKey, const int32& Val);
	DECLARE_FUNCTION(execSetScopedSharedStorage);
	UFUNCTION(BlueprintCallable, Category = "ScopeSharedStorage", CustomThunk, meta=(WorldContext="InScope", CustomStructureParam="Val"))
	static bool GetScopedSharedStorage(UObject* InScope, const FName& InKey, UPARAM(Ref) int32& Val);
	DECLARE_FUNCTION(execGetScopedSharedStorage);
protected:
	static bool SetScopedSharedStorageImpl(UObject* InScope, const FName& InKey, const FProperty* Prop, const void* Addr, bool bReplace);
	static const void* GetScopedSharedStorageImpl(UObject* InScope, const FName& InKey, const FProperty* Prop);
};
