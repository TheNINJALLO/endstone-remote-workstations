#include "item_save.hpp"
#include "inventory_watch.hpp"
#include "native_items.hpp"
#include <endstone/player.h>
#include <openssl/evp.h>
#include <cstring>
namespace oni::vcf::platform::linux_native {
namespace {
std::array<uint8_t,32> digest(std::span<const uint8_t> bytes){
 std::array<uint8_t,32> result{};unsigned length=0;
 require(EVP_Digest(bytes.data(),bytes.size(),result.data(),&length,EVP_sha256(),nullptr)==1&&length==result.size(),VCF_INTERNAL);return result;
}
}
InventoryItemSnapshot read_inventory_item(endstone::Player& player,uint32_t slot){
 require(slot<36);require(player.isValid()&&!player.isDead(),VCF_CLOSED);
 require(player.hasPermission("remoteworkstations.use")&&player.hasPermission("remoteworkstations.inventory.read"),VCF_DENIED);
 auto watch=watch_inventory(player);const auto stamp=watch.capture();
 static const NativeItems bridge;bridge.verify();const auto container=bridge.container(player);
 const auto before=bridge.slot(container,slot);auto item=player.getInventory().getItem(static_cast<int>(slot));require(item.has_value(),VCF_NOT_FOUND);
 const auto identifier=std::string(item->getType().getId());require(!identifier.empty()&&identifier.size()<128,VCF_CAPACITY);
 auto bytes=bridge.save(*item);require(bytes==before&&bytes==bridge.slot(container,slot),VCF_STALE);
 require(bridge.container(player)==container,VCF_STALE);
 require(player.hasPermission("remoteworkstations.use")&&player.hasPermission("remoteworkstations.inventory.read"),VCF_DENIED);
 watch.validate(player,stamp);
 InventoryItemSnapshot result;result.info.size=sizeof(result.info);result.info.version=VCF_ABI_VERSION;
 result.info.slot=slot;result.info.amount=static_cast<uint32_t>(item->getAmount());result.info.auxiliary=item->getData();
 result.info.nbt_format=VCF_BDS_ITEM_SAVE_NBT;result.info.nbt_bytes=static_cast<uint32_t>(bytes.size());
 std::memcpy(result.info.identifier,identifier.c_str(),identifier.size()+1);const auto hash=digest(bytes);std::copy(hash.begin(),hash.end(),result.info.digest);
 result.nbt=std::move(bytes);return result;
}
std::function<void()> observe_inventory_item(endstone::Player& player,uint32_t slot,std::function<endstone::Player*()> resolve){
 require(slot<36&&bool(resolve));auto watch=watch_inventory(player);const auto stamp=watch.capture();
 auto initial=read_inventory_item(player,slot);watch.validate(player,stamp);
 return [watch,stamp,initial=std::move(initial),slot,resolve=std::move(resolve)]{
  auto* current=resolve();require(current,VCF_CLOSED);watch.validate(*current,stamp);
  auto saved=read_inventory_item(*current,slot);
  require(saved.nbt==initial.nbt,VCF_STALE);watch.validate(*current,stamp);
 };
}
}
