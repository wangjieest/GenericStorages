// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/DataTable.h"
#include "Templates/SubclassOf.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "DataTablePicker.generated.h"

//////////////////////////////////////////////////////////////////////////
// meta=(DataTableType="RowSructType")
USTRUCT(BlueprintType)
struct GENERICSTORAGES_API FDataTablePicker
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataTablePicker")
	UDataTable* DataTable;
};

// Table+Row
USTRUCT(BlueprintType)
struct GENERICSTORAGES_API FDataTableRowPicker
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataTablePicker")
	UDataTable* DataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataTablePicker")
	FName RowName;
};

USTRUCT(BlueprintType)
struct GENERICSTORAGES_API FDataTablePathPicker
{
	GENERATED_BODY()
public:
	// SoftPath
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataTablePicker")
	TSoftObjectPtr<UDataTable> DataTablePath;
};

USTRUCT(BlueprintType)
struct GENERICSTORAGES_API FDataTableRowNamePicker
{
	GENERATED_BODY()
public:
	// RowName
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataTablePicker")
	FName RowName;

#if WITH_EDITORONLY_DATA
	// EditorOnly,Limit DataTable type
	UPROPERTY(EditAnywhere, Category = "DataTablePicker")
	TSoftObjectPtr<UDataTable> DataTablePath;
#endif
};
