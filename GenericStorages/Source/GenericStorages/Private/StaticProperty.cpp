// Copyright GenericStorages, Inc. All Rights Reserved.

#include "StaticProperty.h"
#include "StaticPropertyPrivate.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

namespace GenericStorages
{
	namespace Details
	{
		UStaticPropertiesContainer* GetPropertiesHolderImpl()
		{
			static auto Ret = [] {
				auto Container = NewObject<UStaticPropertiesContainer>();
				Container->AddToRoot();

				if (FPlatformProperties::RequiresCookedData() && GCreateGCClusters)
					Container->CreateCluster();

				return Container;
			}();
			return Ret;
		}
		UObject* GetPropertiesHolder()
		{
			return GetPropertiesHolderImpl();
		}

		const FProperty*& FindOrAddProperty(FName PropTypeName)
		{
			const FProperty*& Prop = GetPropertiesHolderImpl()->NameLookups.FindOrAdd(PropTypeName);
#if WITH_EDITOR
			if (GIsEditor && Prop)
			{
				if (auto StructProp = CastField<FStructProperty>(Prop))
				{
					if (!StructProp->Struct || StructProp->Struct->HasAnyFlags(RF_NewerVersionExists))
					{
						Prop = nullptr;
					}
				}
				else if (auto EnumProp = CastField<FEnumProperty>(Prop))
				{
					if (!EnumProp->GetEnum() || EnumProp->GetEnum()->HasAnyCastFlags(RF_NewerVersionExists))
					{
						Prop = nullptr;
					}
				}
				else if (auto ByteProp = CastField<FByteProperty>(Prop))
				{
					if (ByteProp->GetIntPropertyEnum() && ByteProp->GetIntPropertyEnum()->HasAnyCastFlags(RF_NewerVersionExists))
					{
						Prop = nullptr;
					}
				}
			}
#endif
			return Prop;
		}
		const FProperty* FindOrAddProperty(FName PropTypeName, FProperty* InTypeProp)
		{
			auto Find = GetPropertiesHolderImpl()->NameLookups.Find(PropTypeName);
			if (InTypeProp)
			{
				if (!Find)
				{
					Find = &GetPropertiesHolderImpl()->NameLookups.Add(PropTypeName, InTypeProp);
				}
				else if (!(*Find) || !VerifyPropertyType(*Find, InTypeProp->GetClass()))
				{
					*Find = InTypeProp;
				}
			}
			return Find ? *Find : InTypeProp;
		}
	}
}
	
FName UStaticPropertiesContainer::FindPropertyName(const FProperty* Property)
{
	auto Find = PropLookups.Find(Property);
	return Find ? *Find : NAME_None;
}

void UStaticPropertiesContainer::AddCppProperty(FProperty* Property)
{
	PropLookups.Add(Property, Property->GetFName());
	NameLookups.Add(Property->GetFName(), Property);
}
#include "SubSystemStorages.h"

DEFINE_FUNCTION(UScopeSharedStorage::execSetScopedSharedStorage)
{
	P_GET_OBJECT(UObject, InScope);
	P_GET_UBOOL(bReplace);
	P_GET_PROPERTY(FNameProperty, InKey);
	Stack.StepCompiledIn<FProperty>(nullptr);
	auto Prop = Stack.MostRecentProperty;
	auto Addr = Stack.MostRecentPropertyAddress;
	P_FINISH
	P_NATIVE_BEGIN
	*(bool*)RESULT_PARAM = SetScopedSharedStorageImpl(InScope, InKey, Prop, Addr, bReplace);
	P_NATIVE_END
}
DEFINE_FUNCTION(UScopeSharedStorage::execGetScopedSharedStorage)
{
	P_GET_OBJECT(UObject, InScope);
	P_GET_PROPERTY(FNameProperty, InKey);
	Stack.StepCompiledIn<FProperty>(nullptr);
	auto Prop = Stack.MostRecentProperty;
	auto Addr = Stack.MostRecentPropertyAddress;
	P_FINISH
	P_NATIVE_BEGIN
	auto Ret = GetScopedSharedStorageImpl(InScope, InKey, Prop);
	if (Ret)
	{
		Prop->CopyCompleteValue(Addr, Ret);
	}
	*(bool*)RESULT_PARAM = !!Ret;
	P_NATIVE_END
}

bool UScopeSharedStorage::SetScopedSharedStorageImpl(UObject* InScope, const FName& InKey, const FProperty* Prop, const void* Addr, bool bReplace)
{
	if (!ensure(InScope && Prop && Addr))
	{
		return false;
	}
	if (auto Ins = Cast<UGameInstance>(InScope))
	{
		return FSubsystemStorageUtils::SetKeyValue(Ins->GetSubsystem<UGameInstanceSubSystemStorages>(), InKey, Prop, Addr, bReplace);
	}
	else if (auto LP = Cast<ULocalPlayer>(InScope))
	{
		return FSubsystemStorageUtils::SetKeyValue(LP->GetSubsystem<UPlayerSubSystemStorages>(), InKey, Prop, Addr, bReplace);
	}
	else if (UWorld* World = InScope->GetWorld())
	{
		return FSubsystemStorageUtils::SetKeyValue(World->GetSubsystem<UWorldSubSystemStorages>(), InKey, Prop, Addr, bReplace);
	}
	return false;
}

const void* UScopeSharedStorage::GetScopedSharedStorageImpl(UObject* InScope, const FName& InKey, const FProperty* Prop)
{
	if (!ensure(InScope && Prop))
	{
		return nullptr;
	}
	
	if (auto Ins = Cast<UGameInstance>(InScope))
	{
		return FSubsystemStorageUtils::GetValue(Ins->GetSubsystem<UGameInstanceSubSystemStorages>(), InKey,Prop);
	}
	else if (auto LP = Cast<ULocalPlayer>(InScope))
	{
		return FSubsystemStorageUtils::GetValue(LP->GetSubsystem<UPlayerSubSystemStorages>(), InKey,Prop);
	}
	else if (UWorld* World = InScope->GetWorld())
	{
		return FSubsystemStorageUtils::GetValue(World->GetSubsystem<UWorldSubSystemStorages>(), InKey,Prop);
	}
	return nullptr;
}
