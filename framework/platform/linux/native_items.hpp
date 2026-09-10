#pragma once
#include <oni/vcf/core.hpp>
#include <oni/vcf/packet_items.hpp>
#include <endstone/inventory/item_stack.h>
namespace endstone {class Player;}
namespace oni::vcf::platform::linux_native {
// Owns a native 152-byte ItemStack, not the SDK's 128-byte ItemInstance.
// Never move the native object's bytes or use a public metadata conversion.
class NativeItem {
 friend class NativeItems;
 alignas(8) std::array<uint8_t,152> storage_{};
 uintptr_t destructor_=0;
 NativeItem()=default;
public:
 ~NativeItem();
 NativeItem(const NativeItem&)=delete;NativeItem&operator=(const NativeItem&)=delete;
 uintptr_t address()const{return reinterpret_cast<uintptr_t>(storage_.data());}
};
class NativeItems {
 struct Impl;std::shared_ptr<const Impl> impl_;
public:
 NativeItems();
 void verify()const;
 uintptr_t container(endstone::Player&)const;
 uintptr_t slot_address(uintptr_t inventory,uint32_t slot)const;
 std::vector<uint8_t> save(uintptr_t item)const;
 std::vector<uint8_t> slot(uintptr_t inventory,uint32_t slot)const;
 // Rejects a BDS-normalized/lossy reconstruction before any live write.
 std::unique_ptr<NativeItem> reconstruct(std::span<const uint8_t>)const;
 endstone::ItemStack copy(const NativeItem&)const;
 std::vector<uint8_t> save(const endstone::ItemStack&)const;
 // Detached native NetworkItemStackDescriptor construction preserves BDS's
 // item-specific userdata. An explicit presentation ID never authorizes a
 // write. native_descriptor() obtains the actual current slot's stack ID.
 wire::Descriptor descriptor(std::span<const uint8_t>,int32_t presentation_id)const;
 wire::Descriptor native_descriptor(uintptr_t inventory,uint32_t slot)const;
};
}
