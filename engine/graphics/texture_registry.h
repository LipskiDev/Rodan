#pragma once

#include "rhi/handles.h"
#include "texture.h"

namespace Rodan {

	struct TextureHandle {
		uint32_t index = ~0u;
		uint32_t generation = 0;

		[[nodiscard]] bool IsValid() const {
			return index != ~0u;
		}
	};

	struct TextureRegistryDesc {
		uint32_t capacity = 4096;
		const char* debugName = "Global Texture Registry";
	};

	class TextureRegistry {
	public:
		void Initialize(Velos::RHI::IDevice* device, const TextureRegistryDesc& desc);

		void Shutdown();

		[[nodiscard]] TextureHandle RegisterTexture(Texture& texture);

		void UpdateTexture(TextureHandle handle, Texture& texture);

		void ReleaseTexture(TextureHandle handle);

		[[nodiscard]] bool IsValid(TextureHandle handle) const;

		[[nodiscard]] uint32_t GetTextureIndex(TextureHandle handle) const;

		[[nodiscard]] Velos::RHI::BindingSetHandle GetBindingSet() const {
			return bindingSet_;
		}

		[[nodiscard]] Velos::RHI::BindingLayoutHandle GetBindingLayout() const{
			return bindingLayout_;
		}

		[[nodiscard]] Velos::RHI::BindingPoolHandle GetBindingPool() const {
			return bindingPool_;
		}

		[[nodiscard]] uint32_t GetCapacity() const {
			return capacity_;
		}

		[[nodiscard]] uint32_t GetTextureCount() const {
			return textureCount_;
		}

	private:
		struct Slot{
			uint32_t generation = 1;
			bool occupied = false;
		};

		void WriteDescriptor(uint32_t index, const Velos::RHI::BindingImageInfo& texture);

		Velos::RHI::IDevice* device_ = nullptr;

		Velos::RHI::BindingLayoutHandle bindingLayout_;
		Velos::RHI::BindingPoolHandle bindingPool_;
		Velos::RHI::BindingSetHandle bindingSet_;

		Texture fallbackTexture_;
		Texture fallbackNormalTexture_;

		std::vector<Slot> slots_;
		uint32_t nextUnusedSlot_ = 0;
		uint32_t textureCount_ = 0;
		uint32_t capacity_ = 0;
	};

	TextureRegistry& GetTextureRegistry();

}
