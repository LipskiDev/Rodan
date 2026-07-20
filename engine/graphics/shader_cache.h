#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <rhi/types.h>
#include <shader/shader_compiler.h>
#include <array>
#include <optional>
#include <filesystem>

namespace Rodan {
	struct ShaderCacheKeyInput {
		uint32_t cacheFormatVersion = 1;

		std::string path;
		Velos::RHI::ShaderStage stage = Velos::RHI::ShaderStage::None;
		Velos::ShaderSourceLanguage language = Velos::ShaderSourceLanguage::GLSL;
		Velos::ShaderBinaryFormat outputFormat = Velos::ShaderBinaryFormat::Spirv;

		std::string entryPoint = "main";

		std::string compilerName;
		std::string compilerVersion;
	};

	struct ShaderCacheKey {
		uint64_t low = 0;
		uint64_t high = 0;

		auto operator<=>(const ShaderCacheKey&) const = default;
	};

	struct ShaderCacheKeyHash {
		std::size_t operator()(const ShaderCacheKey& key) const noexcept {
			const auto lowHash = std::hash<uint64_t>{}(key.low);
			const auto highHash = std::hash<uint64_t>{}(key.high);

			return lowHash ^
				(highHash + static_cast<std::size_t>(0x9e3779b9U) +
					(lowHash << 6U) + (lowHash >> 2U));
		}
	};

	class ShaderCache {
	public:
		static Velos::ShaderCompileOutput
			LoadOrCompile(const Velos::ShaderCompileInput& input);

		ShaderCacheKey BuildKey(const ShaderCacheKeyInput &input);

		std::optional<Velos::ShaderCompileOutput>
			TryLoad(
				const ShaderCacheKey& key,
				const std::filesystem::path& sourcePath) const;

		bool Store(
			const ShaderCacheKey& key,
			const std::filesystem::path& sourcePath,
			const Velos::ShaderCompileOutput& output);
	};
}
