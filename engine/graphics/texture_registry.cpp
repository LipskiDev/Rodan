#include "texture_registry.h"
#include <stdexcept>

namespace Rodan {

	void TextureRegistry::Initialize(Velos::RHI::IDevice* device, const TextureRegistryDesc& desc)
	{
		if (device == nullptr || desc.capacity == 0) {
			throw std::invalid_argument("Invalid texture registry description");
		}

		device_ = device;
		capacity_ = desc.capacity;
		slots_.resize(capacity_);
		nextUnusedSlot_ = 2; // Reserve slot 0 and 1 for fallback.
		std::unique_ptr<IUploadContext> upload = device_->CreateUploadContext(512);
		upload->Begin();
		const std::uint8_t whitePixel[4] = { 255, 255, 255, 255 };
		const std::uint8_t neutralNormalPixel[4] = { 255 / 2, 255 / 2, 255, 255 };
		fallbackTexture_ = CreateTexture2D(device, upload.get(),
			TextureDesc{
				.width = 1,
				.height = 1,
				.format = Format::RGBA8_UNORM,
				.minFilter = Filter::Linear,
				.magFilter = Filter::Linear,
				.addressU = SamplerAddressMode::Repeat,
				.addressV = SamplerAddressMode::Repeat,
				.addressW = SamplerAddressMode::Repeat,
				.debugName = "Fallback White Texture",
			},
			whitePixel, 4);
		fallbackNormalTexture_ = CreateTexture2D(device, upload.get(),
			TextureDesc{
				.width = 1,
				.height = 1,
				.format = Format::RGBA8_UNORM,
				.minFilter = Filter::Linear,
				.magFilter = Filter::Linear,
				.addressU = SamplerAddressMode::Repeat,
				.addressV = SamplerAddressMode::Repeat,
				.addressW = SamplerAddressMode::Repeat,
				.debugName = "Fallback Normal Texture",
			},
			neutralNormalPixel, 4);
		upload->Flush();

		BindingDesc bindingDesc{};
		bindingDesc.binding = 0;
		bindingDesc.count = desc.capacity;
		bindingDesc.flags = BindingFlags::PartiallyBound | BindingFlags::UpdateAfterBind;
		bindingDesc.visibility = ShaderStage::Fragment;
		bindingDesc.type = BindingType::CombinedImageSampler;

		bindingLayout_ = device_->CreateBindingLayout(
			{
				.bindings = &bindingDesc,
				.bindingCount = 1,
				.debugName = "Global Texture Layout",
			}
		);

		BindingPoolSize poolSize{};
		poolSize.type = BindingType::CombinedImageSampler;
		poolSize.count = desc.capacity;

		bindingPool_ = device_->CreateBindingPool({
			.poolSizes = &poolSize,
			.poolSizeCount = 1,
			.maxSets = 1,
			.flags = BindingPoolFlags::UpdateAfterBind,
			.debugName = "Global Texture Pool"
		});

		bindingSet_ = device_->AllocateBindingSet({
			.pool = bindingPool_,
			.layout = bindingLayout_,
			.debugName = "Global Texture Set",
		});

		slots_[0].occupied = true;
		WriteDescriptor(0, {
			.sampler = fallbackTexture_.sampler,
			.imageView = fallbackTexture_.view,
			.imageLayout = ImageLayout::ShaderReadOnly,
			});

		slots_[1].occupied = true;
		WriteDescriptor(1, {
				.sampler = fallbackNormalTexture_.sampler,
				.imageView = fallbackNormalTexture_.view,
				.imageLayout = ImageLayout::ShaderReadOnly,
			});

	}

	void TextureRegistry::Shutdown() {
		if (device_ == nullptr) {
			return;
		}

		device_->DestroyBindingPool(bindingPool_);
		device_->DestroyBindingLayout(bindingLayout_);

		DestroyTexture(device_, fallbackTexture_);
		DestroyTexture(device_, fallbackNormalTexture_);

		slots_.clear();
		bindingSet_ = {};
		bindingPool_ = {};
		bindingLayout_ = {};
		device_ = nullptr;
		capacity_ = 0;
		textureCount_ = 0;
		nextUnusedSlot_ = 0;
	}

	TextureHandle TextureRegistry::RegisterTexture(Texture& texture)
	{
		if (nextUnusedSlot_ >= capacity_) {
			throw std::runtime_error("Texture registry is full");
		}

		const uint32_t index = nextUnusedSlot_++;
		Slot& slot = slots_[index];

		slot.occupied = true;
		++textureCount_;

		WriteDescriptor(index, {
			.sampler = texture.sampler,
			.imageView = texture.view,
			.imageLayout = ImageLayout::ShaderReadOnly,
			});
		
		return TextureHandle{
			.index = index,
			.generation = slot.generation,
		};
	}

	void TextureRegistry::UpdateTexture(TextureHandle handle, Texture& texture)
	{
		if (!IsValid(handle)) {
			throw std::runtime_error("Invalid texture handle");
		}

		WriteDescriptor(handle.index, {
			.sampler = texture.sampler,
			.imageView = texture.view,
			.imageLayout = ImageLayout::ShaderReadOnly,
		});
	}

	void TextureRegistry::ReleaseTexture(TextureHandle handle)
	{
	}
	
	bool TextureRegistry::IsValid(TextureHandle handle) const {
		return handle.index < slots_.size() &&
			slots_[handle.index].occupied &&
			slots_[handle.index].generation == handle.generation;
	}

	uint32_t TextureRegistry::GetTextureIndex(TextureHandle handle) const {
		if (!IsValid(handle)) {
			return 0; // Fallback texture
		}

		return handle.index;
	}

	void TextureRegistry::WriteDescriptor(uint32_t index, const Velos::RHI::BindingImageInfo& texture)
	{
		device_->UpdateBindingSet({
			.dstSet = bindingSet_,
			.binding = 0,
			.arrayElement = index,
			.type = BindingType::CombinedImageSampler,
			.imageInfo = &texture,
			.descriptorCount = 1,
		});
	}

	TextureRegistry& GetTextureRegistry()
	{
		static TextureRegistry instance;
		return instance;
	}

}
