#include "bedrock/nbt/nbt_io.h"
#include "bedrock/nbt/compound_tag.h"
#include "bedrock/world/item/item_stack.h"
#include "endstone/core/inventory/item_stack.h"
extern "C" {
unsigned item_size(){return sizeof(::ItemStack);}
unsigned result_size(){return sizeof(Bedrock::Result<std::unique_ptr<::Tag>>);}
unsigned result_string_size(){return sizeof(Bedrock::Result<std::string>);}
unsigned result_void_size(){return sizeof(Bedrock::Result<void>);}
Bedrock::Result<uint8_t> success_byte(){return uint8_t{10};}
Bedrock::Result<void> success_void(){return {};}
Bedrock::Result<std::unique_ptr<CompoundTag>> read_compound(IDataInput* p,std::string* name){return NbtIo::readNamedCompoundTag(*p,*name);}
Bedrock::Result<void> load(::Tag* p,IDataInput* input){return p->load(*input);}
void destroy_item(::ItemStack* p){p->~ItemStack();}
endstone::ItemStack public_copy(const ItemStackBase* p){return endstone::core::EndstoneItemStack::fromMinecraft(*p);}
}
