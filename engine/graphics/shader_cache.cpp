#include "shader_cache.h"
#include <xxhash.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace Rodan {

    namespace {

        constexpr std::array<char, 8> kCacheMagic{
            'R', 'D', 'N', 'S', 'H', 'D', 'R', '1'};
        constexpr uint32_t kCacheFileVersion = 1;
		constexpr std::string_view kCompilerName = "Velos::ShaderCompiler";
		constexpr std::string_view kCompilerCacheVersion = "1";
        constexpr uint64_t kMaxCacheCollectionElements = 1'000'000;
        constexpr uint64_t kMaxCacheStringBytes = 16 * 1024 * 1024;

        std::filesystem::path CacheDirectory() {
            return std::filesystem::current_path() / ".cache" / "shaders";
        }

        std::string KeyString(const ShaderCacheKey& key) {
            std::ostringstream stream;
            stream << std::hex << std::setfill('0')
                << std::setw(16) << key.high
                << std::setw(16) << key.low;
            return stream.str();
        }

		[[maybe_unused]] std::string FormatMilliseconds(double milliseconds) {
			std::ostringstream stream;
			stream << std::fixed << std::setprecision(3) << milliseconds;
			return stream.str();
		}

        std::filesystem::path CachePath(const ShaderCacheKey& key) {
            return CacheDirectory() / (KeyString(key) + ".bin");
        }

        template<typename T>
            requires std::is_integral_v<T>
        void WriteInteger(std::ostream& stream, T value) {
            using Unsigned = std::make_unsigned_t<T>;
            const auto encoded = static_cast<Unsigned>(value);

            for (std::size_t byte = 0; byte < sizeof(T); ++byte) {
                stream.put(static_cast<char>(encoded >> (byte * 8U)));
            }

            if (!stream) {
                throw std::runtime_error("Failed to write shader cache file");
            }
        }

        template<typename T>
            requires std::is_integral_v<T>
        bool ReadInteger(std::istream& stream, T& value) {
            using Unsigned = std::make_unsigned_t<T>;
            Unsigned decoded = 0;

            for (std::size_t byte = 0; byte < sizeof(T); ++byte) {
                const int character = stream.get();
                if (character == std::char_traits<char>::eof()) {
                    return false;
                }
                decoded |= static_cast<Unsigned>(
                    static_cast<unsigned char>(character)) << (byte * 8U);
            }

            value = static_cast<T>(decoded);
            return true;
        }

        void WriteString(std::ostream& stream, std::string_view value) {
            WriteInteger<uint64_t>(stream, value.size());
            stream.write(value.data(), static_cast<std::streamsize>(value.size()));
            if (!stream) {
                throw std::runtime_error("Failed to write shader cache string");
            }
        }

        bool ReadString(std::istream& stream, std::string& value) {
            uint64_t size = 0;
            if (!ReadInteger(stream, size) || size > kMaxCacheStringBytes) {
                return false;
            }

            value.resize(static_cast<std::size_t>(size));
            stream.read(value.data(), static_cast<std::streamsize>(size));
            return static_cast<bool>(stream);
        }

        bool GetSourceMetadata(
            const std::filesystem::path& sourcePath,
            uint64_t& size,
            int64_t& writeTime) {
            std::error_code error;
            size = std::filesystem::file_size(sourcePath, error);
            if (error) {
                return false;
            }

            const auto timestamp = std::filesystem::last_write_time(sourcePath, error);
            if (error) {
                return false;
            }

            writeTime = static_cast<int64_t>(timestamp.time_since_epoch().count());
            return true;
        }

        struct XXH3StateDeleter {
            void operator()(XXH3_state_t* state) const noexcept {
                XXH3_freeState(state);
            }
        };

        using XXH3State =
            std::unique_ptr<XXH3_state_t, XXH3StateDeleter>;

        void AddBytes(
            XXH3_state_t& state,
            const void* data,
            size_t size) {
            if (XXH3_128bits_update(&state, data, size) == XXH_ERROR) {
                throw std::runtime_error("Failed to update shader cache hash");
            }
        }

        void AddUint64(XXH3_state_t& state, uint64_t value) {
            // Explicit little-endian serialization makes the key portable.
            const std::array<uint8_t, 8> bytes = {
                static_cast<uint8_t>(value),
                static_cast<uint8_t>(value >> 8),
                static_cast<uint8_t>(value >> 16),
                static_cast<uint8_t>(value >> 24),
                static_cast<uint8_t>(value >> 32),
                static_cast<uint8_t>(value >> 40),
                static_cast<uint8_t>(value >> 48),
                static_cast<uint8_t>(value >> 56),
            };

            AddBytes(state, bytes.data(), bytes.size());
        }

        void AddUint32(XXH3_state_t& state, uint32_t value) {
            const std::array<uint8_t, 4> bytes = {
                static_cast<uint8_t>(value),
                static_cast<uint8_t>(value >> 8),
                static_cast<uint8_t>(value >> 16),
                static_cast<uint8_t>(value >> 24),
            };

            AddBytes(state, bytes.data(), bytes.size());
        }

        void AddString(
            XXH3_state_t& state,
            std::string_view value) {
            // Including the length prevents ambiguous combinations:
            // ("ab", "c") versus ("a", "bc").
            AddUint64(state, static_cast<uint64_t>(value.size()));
            AddBytes(state, value.data(), value.size());
        }

        template<typename Enum>
            requires std::is_enum_v<Enum>
        void AddEnum(XXH3_state_t& state, Enum value) {
            using Underlying = std::underlying_type_t<Enum>;

            AddUint64(
                state,
                static_cast<uint64_t>(
                    static_cast<Underlying>(value)));
        }

    } // namespace


	ShaderCacheKey ShaderCache::BuildKey(const ShaderCacheKeyInput &input)
	{
		XXH3State state{XXH3_createState()};

        if (!state) {
            throw std::runtime_error("Failed to create XXH3 state");
        }

        if (XXH3_128bits_reset(state.get()) == XXH_ERROR) {
            throw std::runtime_error(
                "Failed to initialize XXH3 state");
        }

        AddUint32(*state, input.cacheFormatVersion);
        AddString(*state, input.path);
        AddEnum(*state, input.stage);
        AddEnum(*state, input.language);
        AddEnum(*state, input.outputFormat);
        AddString(*state, input.entryPoint);
        AddString(*state, input.compilerName);
        AddString(*state, input.compilerVersion);

        const XXH128_hash_t digest =
            XXH3_128bits_digest(state.get());

        return {
            .low = digest.low64,
            .high = digest.high64,
        };
	}

	Velos::ShaderCompileOutput ShaderCache::LoadOrCompile(
		const Velos::ShaderCompileInput& input) {
		const auto startTime = std::chrono::steady_clock::now();
		const auto elapsedMilliseconds = [&startTime] {
			return std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - startTime).count();
		};

		if (input.path.empty()) {
			throw std::invalid_argument(
				"ShaderCache::LoadOrCompile: shader path is empty");
		}

		const std::filesystem::path normalizedPath =
			std::filesystem::weakly_canonical(input.path);

		ShaderCache cache;
		const ShaderCacheKey key = cache.BuildKey({
			.cacheFormatVersion = kCacheFileVersion,
			.path = normalizedPath.string(),
			.stage = input.stage,
			.language = input.language,
			.outputFormat = input.outputFormat,
			.entryPoint = input.entryPoint,
			.compilerName = std::string(kCompilerName),
			.compilerVersion = std::string(kCompilerCacheVersion),
		});

		if (auto cached = cache.TryLoad(key, normalizedPath)) {
#if defined(RODAN_DEBUG)
			std::cout << "[ShaderCache] HIT   "
				<< normalizedPath.string() << " -> "
				<< CachePath(key).filename().string() << " ("
				<< FormatMilliseconds(elapsedMilliseconds()) << " ms)\n";
#endif
			return std::move(*cached);
		}

#if defined(RODAN_DEBUG)
		std::cout << "[ShaderCache] MISS  "
			<< normalizedPath.string() << " -> compiling\n";
#endif
		auto output = Velos::ShaderCompiler::CompileFile({
			.path = normalizedPath.string(),
			.stage = input.stage,
			.entryPoint = input.entryPoint,
			.language = input.language,
			.outputFormat = input.outputFormat,
		});
		const bool stored = cache.Store(key, normalizedPath, output);
#if defined(RODAN_DEBUG)
		std::cout << "[ShaderCache] " << (stored ? "STORE " : "SKIP  ")
			<< normalizedPath.string();
		if (stored) {
			std::cout << " -> " << CachePath(key).filename().string();
		}
		std::cout << " (" << FormatMilliseconds(elapsedMilliseconds())
			<< " ms total)\n";
#else
		(void)stored;
#endif
		return output;
	}
    std::optional<Velos::ShaderCompileOutput> ShaderCache::TryLoad(const ShaderCacheKey& key, const std::filesystem::path& sourcePath) const
    {
        uint64_t sourceSize = 0;
        int64_t sourceWriteTime = 0;
        if (!GetSourceMetadata(sourcePath, sourceSize, sourceWriteTime)) {
            return std::nullopt;
        }

        std::ifstream stream(CachePath(key), std::ios::binary);
        if (!stream) {
            return std::nullopt;
        }

        std::array<char, kCacheMagic.size()> magic{};
        stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));

        uint32_t version = 0;
        uint64_t storedLow = 0;
        uint64_t storedHigh = 0;
        uint64_t storedSourceSize = 0;
        int64_t storedWriteTime = 0;
        if (!stream || magic != kCacheMagic ||
            !ReadInteger(stream, version) || version != kCacheFileVersion ||
            !ReadInteger(stream, storedLow) || storedLow != key.low ||
            !ReadInteger(stream, storedHigh) || storedHigh != key.high ||
            !ReadInteger(stream, storedSourceSize) || storedSourceSize != sourceSize ||
            !ReadInteger(stream, storedWriteTime) || storedWriteTime != sourceWriteTime) {
            return std::nullopt;
        }

        uint64_t spirvWordCount = 0;
        if (!ReadInteger(stream, spirvWordCount) ||
            spirvWordCount == 0 ||
            spirvWordCount > kMaxCacheCollectionElements) {
            return std::nullopt;
        }

        Velos::ShaderCompileOutput output;
        output.spirv.resize(static_cast<std::size_t>(spirvWordCount));
        for (uint32_t& word : output.spirv) {
            if (!ReadInteger(stream, word)) {
                return std::nullopt;
            }
        }

        uint32_t reflectionStage = 0;
        if (!ReadString(stream, output.reflection.entryPoint) ||
            !ReadInteger(stream, reflectionStage)) {
            return std::nullopt;
        }
        output.reflection.stage = static_cast<Velos::RHI::ShaderStage>(reflectionStage);

        uint64_t resourceCount = 0;
        if (!ReadInteger(stream, resourceCount) ||
            resourceCount > kMaxCacheCollectionElements) {
            return std::nullopt;
        }
        output.reflection.resources.resize(static_cast<std::size_t>(resourceCount));
        for (auto& resource : output.reflection.resources) {
            uint32_t type = 0;
            uint32_t stage = 0;
            if (!ReadString(stream, resource.name) ||
                !ReadInteger(stream, type) ||
                !ReadInteger(stream, resource.set) ||
                !ReadInteger(stream, resource.binding) ||
                !ReadInteger(stream, resource.arraySize) ||
                !ReadInteger(stream, stage)) {
                return std::nullopt;
            }
            resource.type = static_cast<Velos::ShaderResourceType>(type);
            resource.stage = static_cast<Velos::RHI::ShaderStage>(stage);
        }

        uint64_t pushConstantCount = 0;
        if (!ReadInteger(stream, pushConstantCount) ||
            pushConstantCount > kMaxCacheCollectionElements) {
            return std::nullopt;
        }
        output.reflection.pushConstants.resize(
            static_cast<std::size_t>(pushConstantCount));
        for (auto& range : output.reflection.pushConstants) {
            uint32_t stage = 0;
            if (!ReadInteger(stream, range.offset) ||
                !ReadInteger(stream, range.size) ||
                !ReadInteger(stream, stage)) {
                return std::nullopt;
            }
            range.stage = static_cast<Velos::RHI::ShaderStage>(stage);
        }

        return output;
    }
    bool ShaderCache::Store(const ShaderCacheKey& key, const std::filesystem::path& sourcePath, const Velos::ShaderCompileOutput& output)
    {
        uint64_t sourceSize = 0;
        int64_t sourceWriteTime = 0;
        if (!GetSourceMetadata(sourcePath, sourceSize, sourceWriteTime)) {
            return false;
        }

        std::error_code error;
        std::filesystem::create_directories(CacheDirectory(), error);
        if (error) {
            return false;
        }

        const auto cachePath = CachePath(key);
        auto temporaryPath = cachePath;
        temporaryPath += ".tmp";

        try {
            std::ofstream stream(
                temporaryPath,
                std::ios::binary | std::ios::trunc);
            if (!stream) {
                return false;
            }

            stream.write(kCacheMagic.data(),
                static_cast<std::streamsize>(kCacheMagic.size()));
            WriteInteger(stream, kCacheFileVersion);
            WriteInteger(stream, key.low);
            WriteInteger(stream, key.high);
            WriteInteger(stream, sourceSize);
            WriteInteger(stream, sourceWriteTime);

            WriteInteger<uint64_t>(stream, output.spirv.size());
            for (uint32_t word : output.spirv) {
                WriteInteger(stream, word);
            }

            WriteString(stream, output.reflection.entryPoint);
            WriteInteger(stream, static_cast<uint32_t>(output.reflection.stage));

            WriteInteger<uint64_t>(stream, output.reflection.resources.size());
            for (const auto& resource : output.reflection.resources) {
                WriteString(stream, resource.name);
                WriteInteger(stream, static_cast<uint32_t>(resource.type));
                WriteInteger(stream, resource.set);
                WriteInteger(stream, resource.binding);
                WriteInteger(stream, resource.arraySize);
                WriteInteger(stream, static_cast<uint32_t>(resource.stage));
            }

            WriteInteger<uint64_t>(stream, output.reflection.pushConstants.size());
            for (const auto& range : output.reflection.pushConstants) {
                WriteInteger(stream, range.offset);
                WriteInteger(stream, range.size);
                WriteInteger(stream, static_cast<uint32_t>(range.stage));
            }

            stream.close();
            if (!stream) {
                std::filesystem::remove(temporaryPath, error);
                return false;
            }

            std::filesystem::remove(cachePath, error);
            error.clear();
            std::filesystem::rename(temporaryPath, cachePath, error);
            if (error) {
                std::filesystem::remove(temporaryPath, error);
				return false;
            }
			return true;
        } catch (const std::exception&) {
            std::filesystem::remove(temporaryPath, error);
			return false;
        }
    }
}
