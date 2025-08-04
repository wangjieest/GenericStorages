#include "MIO.h"

#include "Containers/UnrealString.h"
#include "GenericStoragesLog.h"
#include "HAL/FileManager.h"
#include "mio/mmap.hpp"
#include "WorldLocalStorages.h"
#include "UnrealCompatibility.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/SecureHash.h"

#if PLATFORM_ANDROID
#include "Android/AndroidPlatformMisc.h"
extern FString AndroidRelativeToAbsolutePath(bool bUseInternalBasePath, FString RelPath);
#endif

namespace GenericStorages
{
UGameInstance* FindGameInstance(UObject* InObj);
}
namespace MIO
{
FString ConvertToAbsolutePath(FString InPath)
{
#if PLATFORM_IOS || PLATFORM_ANDROID
	InPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*InPath);
#elif PLATFORM_ANDROID
	InPath = AndroidRelativeToAbsolutePath(false, InPath);
#elif PLATFORM_WINDOWS
#else
#endif
	InPath = FPaths::ConvertRelativePathToFull(InPath);
	return InPath;
}

const FString& FullProjectSavedDir()
{
	static FString FullSavedDirPath = [] {
#if UE_BUILD_SHIPPING
		FString ProjSavedDir = FPaths::ProjectDir() / TEXT("Saved/");
#else
		FString ProjSavedDir = FPaths::ProjectSavedDir();
#endif
		return ConvertToAbsolutePath(ProjSavedDir);
	}();
	return FullSavedDirPath;
}

template<typename ByteT, typename MapType = std::conditional_t<std::is_const<ByteT>::value, mio::ummap_source, mio::ummap_sink>>
class FMappedFileRegionImpl final
	: public IMappedFileRegion<ByteT>
	, private MapType
{
	static_assert(sizeof(ByteT) == sizeof(char), "err");
	FString AbsolutePath;

public:
	virtual FString& GetInfo() override { return AbsolutePath; }
	virtual ByteT* GetMappedPtr() override { return MapType::data(); }
	virtual int64 GetMappedSize() override { return MapType::mapped_length(); }

	~FMappedFileRegionImpl() = default;
	FMappedFileRegionImpl(const TCHAR* Filename, int64 Offset = 0, int64 BytesToMap = MAX_int64, bool bPreloadHint = false)
	{
		std::error_code ErrCode;
		AbsolutePath = ConvertToAbsolutePath(Filename);
		MapType::map(*AbsolutePath, Offset, BytesToMap, ErrCode);
		if (ErrCode)
		{
			UE_LOG(LogGenericStorages, Error, TEXT("OpenMapped Error : [%s] %d(%s)"), *AbsolutePath, ErrCode.value(), ANSI_TO_TCHAR(ErrCode.message().c_str()));
		}
	}
};

TUniquePtr<IMappedFileRegion<const uint8>> OpenMappedRead(const TCHAR* Filename, int64 Offset /*= 0*/, int64 BytesToMap /*= 0*/, bool bPreloadHint /*= false*/)
{
	return MakeUnique<FMappedFileRegionImpl<const uint8>>(Filename, Offset, BytesToMap, bPreloadHint);
}

TUniquePtr<IMappedFileRegion<uint8>> OpenMappedWrite(const TCHAR* Filename, int64 Offset /*= 0*/, int64 BytesToMap /*= MAX_int64*/, bool bPreloadHint /*= false*/)
{
	return MakeUnique<FMappedFileRegionImpl<uint8>>(Filename, Offset, BytesToMap, bPreloadHint);
}

int32 ReadLines(const TCHAR* Filename, const TFunctionRef<void(const TArray<uint8>&)>& Lambda, char Dim /*= '\n'*/)
{
	FMappedFileRegionImpl<const uint8> MapFile{Filename};
	auto Lines = 0;
	if (!MapFile.GetMappedPtr())
		return 0;

	TArray<uint8> Buffer;
	Buffer.Reserve(2048);
	auto Ptr = MapFile.GetMappedPtr();
	auto Size = MapFile.GetMappedSize();
	int64 i = 0u;
	while (i < Size)
	{
		auto Ch = Ptr[i];
		if (Ch == Dim)
		{
			if (Buffer.Num() > 0)
			{
				++Lines;
				Lambda(Buffer);
				Buffer.Reset();
			}
			continue;
		}
		Buffer.Add(Ptr[i]);
	}

	if (Buffer.Num() > 0)
	{
		++Lines;
		Lambda(Buffer);
	}
	return Lines;
}

int32 WriteLines(const TCHAR* Filename, const TArray<TArray<uint8>>& Lines, char Dim /*= '\n'*/)
{
	int32 LineCount = 0;
	TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(Filename));
	if (Writer)
	{
		for (int64 i = 0; i < Lines.Num(); ++i)
		{
			auto& Buffer = Lines[i];
			if (Buffer.Num() > 0)
			{
				++LineCount;
				Writer->Serialize((void*)Buffer.GetData(), Buffer.Num());
				Writer->Serialize(&Dim, 1);
			}
		}
	}
	return LineCount;
}

bool SetFileSize(const TCHAR* Filename, int64 NewSize, bool bAllowShrink)
{
	auto AbsolutePath = ConvertToAbsolutePath(Filename);
	auto errcode = mio::set_file_size(*AbsolutePath, static_cast<uint32>(NewSize), bAllowShrink);
	return !errcode;
}

bool ChunkingFile(const TCHAR* Filename, const TFunctionRef<void(TArrayView<const uint8>)>& Lambda, int32 InSize)
{
	std::error_code error_code;
	auto AbsolutePath = ConvertToAbsolutePath(Filename);
	auto handle = mio::detail::open_file(*AbsolutePath, mio::access_mode::read, error_code);
	if (error_code)
	{
		UE_LOG(LogGenericStorages, Error, TEXT("Failed to map file: %s"), *AbsolutePath);
		return false;
	}
	ON_SCOPE_EXIT{mio::close_file_handle(handle);};
	mio::ummap_source map_source;
	auto file_size = mio::detail::query_file_size(handle, error_code);
	if (error_code)
	{
		UE_LOG(LogGenericStorages, Error, TEXT("Failed to query file size: %s"), *AbsolutePath);
		return false;
	}
	decltype(file_size) offset = 0;
	for (; offset < file_size; offset += InSize)
	{
		auto size_to_read = FMath::Min<decltype(file_size)>(file_size - offset, InSize);
		if (size_to_read > 0)
		{
			map_source.map(handle, offset, size_to_read, error_code);
			ON_SCOPE_EXIT{map_source.unmap();};
			if (error_code)
			{
				UE_LOG(LogGenericStorages, Error, TEXT("Failed to map file region: %lld-%lld %s"), offset, size_to_read, Filename);
				return false;
			}
			Lambda(TArrayView<const uint8>(map_source.data(), size_to_read));
		}
	}
	return true;
}
FString GetFileHash(const TCHAR* Filename, const FString& HashType)
{
	if (HashType == TEXT("md5"))
	{
		FMD5 MD5Context;
		bool bSucc = ChunkingFile(Filename, [&](TArrayView<const uint8> Data) { MD5Context.Update(Data.GetData(), Data.Num()); }, 4 * 1024 * 1024);
		if (bSucc)
		{
			uint8 Digest[16];
			MD5Context.Final(Digest);
			FString MD5;
			for (int32 i = 0; i < 16; i++)
			{
				MD5 += FString::Printf(TEXT("%02x"), Digest[i]);
			}
			return MD5;
		}
	}
	else if (HashType == TEXT("sha1"))
	{
		FSHA1 SHA1Context;
		bool bSucc = ChunkingFile(Filename, [&](TArrayView<const uint8> Data) { SHA1Context.Update(Data.GetData(), Data.Num()); }, 4 * 1024 * 1024);
		if (bSucc)
		{
			return SHA1Context.Finalize().ToString();
		}
	}
	return TEXT("");
}

void* OpenLockHandle(const TCHAR* Path, FString& ErrorCategory)
{
	auto AbsolutePath = ConvertToAbsolutePath(Path);
	ensure(FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(AbsolutePath)));
	return mio::OpenLockHandle(*AbsolutePath, ErrorCategory);
}
void CloseLockHandle(void* InHandle)
{
	if (InHandle)
		mio::CloseLockHandle(InHandle);
}

struct FProcessLockIndexImpl : public FProcessLockIndex
{
	void* Handle = nullptr;
	FProcessLockIndexImpl(void* InHandle, int32 Idx, int32 PIEInstance = 0)
		: Handle(InHandle)
	{
		PIEIndex = PIEInstance;
		Index = Idx;
	}
	~FProcessLockIndexImpl()
	{
		if (Handle)
			CloseLockHandle(Handle);
	}
};

template<bool bFromCmd = true>
bool GetGlobalSystemIndexHandleImpl(void*& OutHandle, int32& OutIndex, const TCHAR* Key, int32 MaxTries = 1024)
{
	int64 StartIndex = 0;
	if constexpr (bFromCmd)
	{
		if (FCommandLine::IsInitialized())
		{
			FParse::Value(FCommandLine::Get(), TEXT("-GlobalSystemIndex="), StartIndex);
			StartIndex = FMath::Max(int64(0), StartIndex);
		}
	}

	auto LockDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), FString::Printf(TEXT(".indexlock"))));
	for (int32 Index = StartIndex; Index < MaxTries; ++Index)
	{
		FString ErrorMsg;
		FString TestFileName = FString::Printf(TEXT("%s%s"), Key ? Key : TEXT("_GlobalIndex_"), *LexToString(Index));
		if (auto Handle = MIO::OpenLockHandle(*FPaths::Combine(LockDir, FString::Printf(TEXT("%s.lock"), *TestFileName)), ErrorMsg))
		{
			OutIndex = Index;
			OutHandle = Handle;
			return true;
		}
	}
	ensureMsgf(false, TEXT("Failed for GetGlobalSystemIndexHandle(%s, %s)"), Key ? Key : TEXT("_GlobalIndex_"), *LexToString(MaxTries));
	return false;
}

int32 GetProcessUniqueIndex()
{
	static TSharedPtr<FProcessLockIndexImpl> ProcessUniqueLock;
	if (ProcessUniqueLock)
		return ProcessUniqueLock->Index;

	void* Handle = nullptr;
	int32 Index = 0;
	if (GetGlobalSystemIndexHandleImpl<false>(Handle, Index, TEXT("_ProcessUniqueIndex_")))
	{
		ProcessUniqueLock = MakeShared<FProcessLockIndexImpl>(Handle, Index);
	}
	return Index;
}

TSharedPtr<FProcessLockIndex> GetGlobalSystemIndexHandle(const TCHAR* Key, int32 MaxTries)
{
	void* Handle = nullptr;
	int32 Index = 0;
	if (GetGlobalSystemIndexHandleImpl(Handle, Index, Key, MaxTries))
	{
		return MakeShared<FProcessLockIndexImpl>(Handle, Index);
	}
	return nullptr;
}
FProcessLockIndex* GetGameInstanceIndexHandle(const UObject* InCtx, const TCHAR* Key, int32 MaxTries /*= 1024*/)
{
	static WorldLocalStorages::TGenericGameLocalStorage<FProcessLockIndexImpl> Containers;
	UGameInstance* Ins = GenericStorages::FindGameInstance((UObject*)InCtx);
	void* Handle = nullptr;
	int32 Index = 0;
	if (Key)
	{
		GetGlobalSystemIndexHandleImpl(Handle, Index, Key, MaxTries);
	}
	else
	{
		Index = GetProcessUniqueIndex();
	}
	auto Lambda = [&] { return new FProcessLockIndexImpl(Handle, Index, Ins ? Ins->GetWorldContext()->PIEInstance : UE::GetPlayInEditorID()); };
	return &Containers.GetLocalValue(Ins, Lambda);
}

FMappedBuffer::FMappedBuffer(FGuid InId, uint32 InCapacity, const TCHAR* SubDir)
	: ReadIdx(0)
	, WriteIdx(0)
{
	auto RegionSize = FMath::RoundUpToPowerOfTwo(FMath::Max(PLATFORM_IOS ? 4096u * 4 : 4096u, InCapacity));
	FString SaveDir = FPaths::Combine(FullProjectSavedDir(), SubDir ? SubDir : TEXT("Mapped"));
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SaveDir);
	Region = MIO::OpenMappedWrite(*FPaths::Combine(SaveDir, InId.IsValid() ? InId.ToString() : FGuid::NewGuid().ToString()), 0, RegionSize);
}

bool FMappedBuffer::IsValid() const
{
	return Region.IsValid();
}

bool FMappedBuffer::WillWrap(uint32 InSize) const
{
	return ensure(InSize < GetRegionSize()) && (WriteIdx + InSize >= GetRegionSize());
}

bool FMappedBuffer::WillFull(uint32 InSize, bool bContinuous) const
{
	return (!ensure(InSize < GetRegionSize())) || (Num() + InSize < GetRegionSize()) || (bContinuous && InSize >= ReadIdx);
}

uint8* FMappedBuffer::Write(const void* InVal, uint32 InSize, bool bContinuous)
{
	auto RegionSize = GetRegionSize();
	if (!ensure(RegionSize > 0))
		return nullptr;

	bool bTestFull = WillFull(InSize, bContinuous);
	auto AddrToWrite = &GetBuffer(WriteIdx);
	if (!WillWrap(InSize))
	{
		if (InVal)
		{
			FMemory::Memcpy(AddrToWrite, InVal, InSize);
		}
		WriteIdx += InSize;
	}
	else if (bContinuous)
	{
		AddrToWrite = &GetBuffer(0);
		WriteIdx = InSize % FMath::Max(RegionSize, 1u) + 1;
		if (InVal)
		{
			FMemory::Memcpy(AddrToWrite, InVal, WriteIdx - 1);
		}
	}
	else
	{
		auto FirstPart = RegionSize - WriteIdx;
		auto LeftSize = InSize - FirstPart;
		if (InVal)
		{
			FMemory::Memcpy(AddrToWrite, InVal, FirstPart);
			FMemory::Memcpy(&GetBuffer(0), (uint8*)InVal + FirstPart, LeftSize);
		}
		WriteIdx = LeftSize;
	}

	if (bTestFull)
		ReadIdx = (WriteIdx + 1) % FMath::Max(RegionSize, 1u);
	return AddrToWrite;
}

uint32 FMappedBuffer::ReadUntil(uint8 TestChar, TFunctionRef<void(uint8)> Op)
{
	if (!IsWrap())
	{
		for (auto i = ReadIdx; i < WriteIdx; ++i)
		{
			auto Char = GetBuffer(i);
			if (Char == TestChar)
				return i - ReadIdx;
			Op(Char);
		}
	}
	else
	{
		for (auto i = ReadIdx; i < GetRegionSize(); ++i)
		{
			auto Char = GetBuffer(i);
			if (GetBuffer(i) == TestChar)
				return i - ReadIdx;
			Op(Char);
		}
		for (auto i = 0u; i < WriteIdx; ++i)
		{
			auto Char = GetBuffer(i);
			if (GetBuffer(i) == TestChar)
				return i + (GetRegionSize() - ReadIdx);
			Op(Char);
		}
	}
	return 0u;
}

uint32 FMappedBuffer::ReadUntil(uint8 TestChar)
{
	return ReadUntil(TestChar, [](auto) {});
}

uint32 FMappedBuffer::WriteImpl(uint32 InLen, const void* InBuf)
{
	const bool bIsEmplty = IsEmpty();
	if (bIsEmplty)
	{
		*reinterpret_cast<decltype(InLen)*>(GetPtr() + WriteIdx) = InLen;
		WriteIdx += sizeof(InLen);
		FMemory::Memcpy(&GetBuffer(WriteIdx), InBuf, InLen);
		WriteIdx += InLen;
	}
	return 0u;
}

uint8& FMappedBuffer::GetBuffer(int32 Idx) const
{
	return *(GetPtr() + Idx);
}

uint32 FMappedBuffer::GetRegionSize() const
{
	return Region ? Region->GetMappedSize() : 0u;
}

uint8* FMappedBuffer::GetPtr() const
{
	check(IsValid());
	return Region->GetMappedPtr();
}

}  // namespace MIO
