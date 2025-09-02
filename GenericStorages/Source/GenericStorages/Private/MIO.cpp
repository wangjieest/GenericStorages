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
UGameInstance* FindGameInstance(const UObject* InObj);
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
	ON_SCOPE_EXIT
	{
		mio::close_file_handle(handle);
	};
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
			ON_SCOPE_EXIT
			{
				map_source.unmap();
			};
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

namespace Hash
{
	struct sha256_context
	{
		uint8_t buf[64];
		uint32_t hash[8];
		uint32_t bits[2];
		uint32_t len;
		uint32_t rfu__;
		uint32_t W[64];
	};
	static const uint32_t K[64] = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
								   0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
								   0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
								   0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

	// -----------------------------------------------------------------------------
	inline uint8_t _shb(uint32_t x, uint32_t n)
	{
		return ((x >> (n & 31)) & 0xff);
	}  // _shb

	// -----------------------------------------------------------------------------
	inline uint32_t _shw(uint32_t x, uint32_t n)
	{
		return ((x << (n & 31)) & 0xffffffff);
	}  // _shw

	// -----------------------------------------------------------------------------
	inline uint32_t _r(uint32_t x, uint8_t n)
	{
		return ((x >> n) | _shw(x, 32 - n));
	}  // _r

	// -----------------------------------------------------------------------------
	inline uint32_t _Ch(uint32_t x, uint32_t y, uint32_t z)
	{
		return ((x & y) ^ ((~x) & z));
	}  // _Ch

	// -----------------------------------------------------------------------------
	inline uint32_t _Ma(uint32_t x, uint32_t y, uint32_t z)
	{
		return ((x & y) ^ (x & z) ^ (y & z));
	}  // _Ma

	// -----------------------------------------------------------------------------
	inline uint32_t _S0(uint32_t x)
	{
		return (_r(x, 2) ^ _r(x, 13) ^ _r(x, 22));
	}  // _S0

	// -----------------------------------------------------------------------------
	inline uint32_t _S1(uint32_t x)
	{
		return (_r(x, 6) ^ _r(x, 11) ^ _r(x, 25));
	}  // _S1

	// -----------------------------------------------------------------------------
	inline uint32_t _G0(uint32_t x)
	{
		return (_r(x, 7) ^ _r(x, 18) ^ (x >> 3));
	}  // _G0

	// -----------------------------------------------------------------------------
	inline uint32_t _G1(uint32_t x)
	{
		return (_r(x, 17) ^ _r(x, 19) ^ (x >> 10));
	}  // _G1

	// -----------------------------------------------------------------------------
	inline uint32_t _word(uint8_t* c)
	{
		return (_shw(c[0], 24) | _shw(c[1], 16) | _shw(c[2], 8) | (c[3]));
	}  // _word

	// -----------------------------------------------------------------------------
	static void _addbits(sha256_context* ctx, uint32_t n)
	{
		if (ctx->bits[0] > (0xffffffff - n))
		{
			ctx->bits[1] = (ctx->bits[1] + 1) & 0xFFFFFFFF;
		}
		ctx->bits[0] = (ctx->bits[0] + n) & 0xFFFFFFFF;
	}  // _addbits

	// -----------------------------------------------------------------------------
	static void _hash(sha256_context* ctx)
	{
		uint32_t a, b, c, d, e, f, g, h;
		uint32_t t[2];

		a = ctx->hash[0];
		b = ctx->hash[1];
		c = ctx->hash[2];
		d = ctx->hash[3];
		e = ctx->hash[4];
		f = ctx->hash[5];
		g = ctx->hash[6];
		h = ctx->hash[7];

		for (uint32_t i = 0; i < 64; i++)
		{
			if (i < 16)
			{
				ctx->W[i] = _word(&ctx->buf[_shw(i, 2)]);
			}
			else
			{
				ctx->W[i] = _G1(ctx->W[i - 2]) + ctx->W[i - 7] + _G0(ctx->W[i - 15]) + ctx->W[i - 16];
			}

			t[0] = h + _S1(e) + _Ch(e, f, g) + K[i] + ctx->W[i];
			t[1] = _S0(a) + _Ma(a, b, c);
			h = g;
			g = f;
			f = e;
			e = d + t[0];
			d = c;
			c = b;
			b = a;
			a = t[0] + t[1];
		}

		ctx->hash[0] += a;
		ctx->hash[1] += b;
		ctx->hash[2] += c;
		ctx->hash[3] += d;
		ctx->hash[4] += e;
		ctx->hash[5] += f;
		ctx->hash[6] += g;
		ctx->hash[7] += h;
	}  // _hash

	// -----------------------------------------------------------------------------
	void sha256_init(sha256_context* ctx)
	{
		if (ctx != NULL)
		{
			ctx->bits[0] = ctx->bits[1] = ctx->len = 0;
			ctx->hash[0] = 0x6a09e667;
			ctx->hash[1] = 0xbb67ae85;
			ctx->hash[2] = 0x3c6ef372;
			ctx->hash[3] = 0xa54ff53a;
			ctx->hash[4] = 0x510e527f;
			ctx->hash[5] = 0x9b05688c;
			ctx->hash[6] = 0x1f83d9ab;
			ctx->hash[7] = 0x5be0cd19;
		}
	}  // sha256_init

	// -----------------------------------------------------------------------------
	void sha256_hash(sha256_context* ctx, const void* data, size_t len)
	{
		const uint8_t* bytes = (const uint8_t*)data;

		if ((ctx != NULL) && (bytes != NULL) && (ctx->len < sizeof(ctx->buf)))
		{
			for (size_t i = 0; i < len; i++)
			{
				ctx->buf[ctx->len++] = bytes[i];
				if (ctx->len == sizeof(ctx->buf))
				{
					_hash(ctx);
					_addbits(ctx, sizeof(ctx->buf) * 8);
					ctx->len = 0;
				}
			}
		}
	}  // sha256_hash

	// -----------------------------------------------------------------------------
	void sha256_done(sha256_context* ctx, uint8_t* hash)
	{
		uint32_t i, j;

		if (ctx != NULL)
		{
			j = ctx->len % sizeof(ctx->buf);
			ctx->buf[j] = 0x80;
			for (i = j + 1; i < sizeof(ctx->buf); i++)
			{
				ctx->buf[i] = 0x00;
			}

			if (ctx->len > 55)
			{
				_hash(ctx);
				for (j = 0; j < sizeof(ctx->buf); j++)
				{
					ctx->buf[j] = 0x00;
				}
			}

			_addbits(ctx, ctx->len * 8);
			ctx->buf[63] = _shb(ctx->bits[0], 0);
			ctx->buf[62] = _shb(ctx->bits[0], 8);
			ctx->buf[61] = _shb(ctx->bits[0], 16);
			ctx->buf[60] = _shb(ctx->bits[0], 24);
			ctx->buf[59] = _shb(ctx->bits[1], 0);
			ctx->buf[58] = _shb(ctx->bits[1], 8);
			ctx->buf[57] = _shb(ctx->bits[1], 16);
			ctx->buf[56] = _shb(ctx->bits[1], 24);
			_hash(ctx);

			if (hash != NULL)
			{
				for (i = 0, j = 24; i < 4; i++, j -= 8)
				{
					hash[i + 0] = _shb(ctx->hash[0], j);
					hash[i + 4] = _shb(ctx->hash[1], j);
					hash[i + 8] = _shb(ctx->hash[2], j);
					hash[i + 12] = _shb(ctx->hash[3], j);
					hash[i + 16] = _shb(ctx->hash[4], j);
					hash[i + 20] = _shb(ctx->hash[5], j);
					hash[i + 24] = _shb(ctx->hash[6], j);
					hash[i + 28] = _shb(ctx->hash[7], j);
				}
			}
		}
	}  // sha256_done

	// -----------------------------------------------------------------------------
	void sha256(const void* data, size_t len, uint8_t* hash)
	{
		sha256_context ctx;

		sha256_init(&ctx);
		sha256_hash(&ctx, data, len);
		sha256_done(&ctx, hash);
	}  // sha256

	TArray<uint8> hmac_sha256(const uint8* Data, uint32 DataSize, const uint8* Key, uint32 KeySize)
	{
		constexpr uint32 BlockSize = 64;  // For SHA-256
		constexpr uint32 HashSize = 32;

		uint8 KeyBlock[BlockSize];
		FMemory::Memzero(KeyBlock, BlockSize);

		// Step 1: Key normalization
		if (KeySize > BlockSize)
		{
			sha256(Key, KeySize, KeyBlock);
		}
		else
		{
			FMemory::Memcpy(KeyBlock, Key, KeySize);
		}

		// Step 2: Create inner and outer pads
		uint8 Ipad[BlockSize];
		uint8 Opad[BlockSize];

		for (uint32 i = 0; i < BlockSize; ++i)
		{
			Ipad[i] = KeyBlock[i] ^ 0x36;
			Opad[i] = KeyBlock[i] ^ 0x5C;
		}

		// Step 3: Inner hash = SHA256( Ipad || Data )
		TArray<uint8> InnerBuffer;
		InnerBuffer.Append(Ipad, BlockSize);
		InnerBuffer.Append(Data, DataSize);

		uint8 InnerHash[HashSize];
		sha256(InnerBuffer.GetData(), InnerBuffer.Num(), InnerHash);

		// Step 4: Outer hash = SHA256( Opad || InnerHash )
		TArray<uint8> OuterBuffer;
		OuterBuffer.Append(Opad, BlockSize);
		OuterBuffer.Append(InnerHash, HashSize);

		uint8 FinalHash[HashSize];
		sha256(OuterBuffer.GetData(), OuterBuffer.Num(), FinalHash);

		// Return result
		TArray<uint8> Result;
		Result.Append(FinalHash, HashSize);
		return Result;
	}
}  // namespace Hash

FString GetFileHash(const TCHAR* Filename, const FString& HashType)
{
	if (HashType == TEXT("md5"))
	{
		FMD5 Md5Signature;
		bool bSucc = ChunkingFile(
			Filename,
			[&](TArrayView<const uint8> Data) { Md5Signature.Update(Data.GetData(), Data.Num()); },
			4 * 1024 * 1024);
		if (bSucc)
		{
			uint8 Digest[16];
			Md5Signature.Final(Digest);
			FString Md5Str;
			for (int32 i = 0; i < 16; i++)
			{
				Md5Str += FString::Printf(TEXT("%02x"), Digest[i]);
			}
			return Md5Str;
		}
	}
	else if (HashType == TEXT("sha1"))
	{
		FSHA1 Sha1Signature;
		bool bSucc = ChunkingFile(
			Filename,
			[&](TArrayView<const uint8> Data) { Sha1Signature.Update(Data.GetData(), Data.Num()); },
			4 * 1024 * 1024);
		if (bSucc)
		{
			return Sha1Signature.Finalize().ToString();
		}
	}
	else if (HashType == TEXT("sha256"))
	{
		using namespace Hash;
		sha256_context sha256_ctx;
		sha256_init(&sha256_ctx);
		bool bSucc = ChunkingFile(
			Filename,
			[&](TArrayView<const uint8> Data) { sha256_hash(&sha256_ctx, Data.GetData(), Data.Num()); },
			4 * 1024 * 1024);
		if (bSucc)
		{
			FSHA256Signature Sha256Signature;
			sha256_done(&sha256_ctx, Sha256Signature.Signature);
			return Sha256Signature.ToString();
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
