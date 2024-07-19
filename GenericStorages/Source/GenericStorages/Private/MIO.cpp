#include "MIO.h"

#include "Containers/UnrealString.h"

#if PLATFORM_ANDROID
#include "Android/AndroidPlatformMisc.h"
#endif

#include "GenericStoragesLog.h"
#include "HAL/FileManager.h"
#include "mio/mmap.hpp"

#if PLATFORM_ANDROID
extern FString AndroidRelativeToAbsolutePath(bool bUseInternalBasePath, FString RelPath);
#endif

namespace MIO
{
template<typename ByteT, typename MapType = std::conditional_t<std::is_const<ByteT>::value, mio::ummap_source, mio::ummap_sink>>
class FMappedFileRegionImpl final
	: public IMappedFileRegion<ByteT>
	, private MapType
{
	static_assert(sizeof(ByteT) == sizeof(char), "err");
	FString FileInfo;

public:
	virtual FString& GetInfo() override { return FileInfo; }
	virtual ByteT* GetMappedPtr() override { return MapType::data(); }
	virtual int64 GetMappedSize() override { return MapType::mapped_length(); }

	~FMappedFileRegionImpl() = default;
	FMappedFileRegionImpl(const TCHAR* Filename, int64 Offset = 0, int64 BytesToMap = MAX_int64, bool bPreloadHint = false)
	{
		std::error_code ErrCode;
		FileInfo = Filename;
		MapType::map(Filename, Offset, BytesToMap, ErrCode);

		if (ErrCode)
		{
			UE_LOG(LogGenericStorages, Error, TEXT("OpenMapped Error : [%s] %d(%s)"), Filename, ErrCode.value(), ANSI_TO_TCHAR(ErrCode.message().c_str()));
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

bool ChunkingFile(const TCHAR* Filename, TArray64<uint8>& Buffer, const TFunctionRef<void(const TArray64<uint8>&)>& Lambda)
{
	check(Filename);
	if (!Buffer.Num())
		Buffer.SetNumUninitialized(2048);

	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(Filename));
	if (ensure(Reader))
	{
		int64 LeftSize = Reader->TotalSize();
		while (LeftSize > 0)
		{
			auto ActualSize = LeftSize > Buffer.Num() ? Buffer.Num() : LeftSize;
			Buffer.SetNumUninitialized(ActualSize, false);
			Reader->Serialize(Buffer.GetData(), ActualSize);
			Lambda(Buffer);
			LeftSize -= ActualSize;
		}
		return true;
	}
	return false;
}

GENERICSTORAGES_API void* OpenLockHandle(const TCHAR* Path, FString& ErrorCategory)
{
	ensure(FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path)));
	return mio::OpenLockHandle(Path, ErrorCategory);
}
GENERICSTORAGES_API void CloseLockHandle(void* InHandle)
{
	if (InHandle)
		mio::CloseLockHandle(InHandle);
}

FString ConvertToAbsolutePath(FString InPath)
{
#if PLATFORM_IOS
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
