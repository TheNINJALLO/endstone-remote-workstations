#include "held_items.hpp"
#include <endstone/inventory/item_stack.h>
#include <endstone/player.h>
#ifdef _WIN32
#include <Windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#endif
namespace oni::vcf::held {
namespace {
std::array<uint8_t,32> digest(std::span<const uint8_t> bytes){
 std::array<uint8_t,32> result{};
#ifdef _WIN32
 require(bytes.size()<=ULONG_MAX,VCF_CAPACITY);BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,VCF_INTERNAL);
 if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm,0);throw Error{VCF_INTERNAL};}
 const bool ok=BCryptHashData(hash,const_cast<uint8_t*>(bytes.data()),static_cast<ULONG>(bytes.size()),0)>=0
  &&BCryptFinishHash(hash,result.data(),static_cast<ULONG>(result.size()),0)>=0;
 BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);require(ok,VCF_INTERNAL);
#else
 unsigned length=0;require(EVP_Digest(bytes.data(),bytes.size(),result.data(),&length,EVP_sha256(),nullptr)==1&&length==result.size(),VCF_INTERNAL);
#endif
 return result;
}
Frozen snapshot(const std::optional<endstone::ItemStack>& item){
 require(item.has_value(),VCF_NOT_FOUND);return freeze(item->getType().getId(),item->getAmount(),item->getData(),item->getNbt());
}
}
vcf_held_info inspect(endstone::Player& player){
 require(player.isValid()&&!player.isDead(),VCF_CLOSED);
 require(player.hasPermission("remoteworkstations.use"),VCF_DENIED);
 auto& inventory=player.getInventory();const auto slot=inventory.getHeldItemSlot();require(slot>=0&&slot<9,VCF_UNAVAILABLE);
 auto first=inventory.getItem(slot);require(first.has_value(),VCF_NOT_FOUND);
 const std::string identifier=first->getType().getId();
 const auto canonical=kind(identifier);const auto* row=resolve(canonical);require(row,VCF_UNAVAILABLE);
 require(player.hasPermission(std::string(row->permission)),VCF_DENIED);
 auto before=snapshot(first),hand=snapshot(inventory.getItemInMainHand()),after=snapshot(inventory.getItem(slot));
 require(inventory.getHeldItemSlot()==slot&&before==hand&&before==after,VCF_STALE);
 require(player.isValid()&&!player.isDead(),VCF_CLOSED);
 require(player.hasPermission("remoteworkstations.use")&&player.hasPermission(std::string(row->permission)),VCF_DENIED);
 vcf_held_info result{};result.size=sizeof(result);result.version=VCF_ABI_VERSION;
 result.slot=static_cast<uint32_t>(slot);result.amount=before.amount;result.auxiliary=before.auxiliary;
 result.metadata_bytes=static_cast<uint32_t>(before.metadata.size());result.native_open_available=0;
 auto copy=[](auto& target,const std::string& value){require(value.size()<sizeof(target),VCF_CAPACITY);std::memcpy(target,value.c_str(),value.size()+1);};
 copy(result.canonical_id,before.canonical_id);copy(result.identifier,before.identifier);copy(result.durable_id,before.durable_id);
 const auto hash=digest(digest_input(before));std::copy(hash.begin(),hash.end(),result.digest);
 return result;
}
}
