namespace RakNet {struct RakPeerConfiguration;}
#include "bedrock/world/actor/player/inventory.h"
#include "bedrock/nbt/list_tag.h"
#include "endstone/core/inventory/player_inventory.h"
#include "endstone/core/inventory/item_stack.h"
#include "endstone/core/nbt.h"
extern "C" {
void save_inventory(const FillingContainer* p,const SaveContext* context,std::unique_ptr<ListTag>* output){*output=p->saveToTag(*context);}
void changed(Container* p,int slot){p->setContainerChanged(slot);}
void set(Container* p,int slot,const ItemStack* stack,bool balance){p->setItemWithForceBalance(slot,*stack,balance);}
const ItemStack* get(const Container* p,int slot){return &p->getItem(slot);}
int size(const Container* p){return p->getContainerSize();}
void tag_copy(const Tag* p,std::unique_ptr<Tag>* out){*out=p->copy();}
void tag_delete(Tag* p){delete p;}
endstone::nbt::Tag convert_tag(const Tag* p){return endstone::core::nbt::fromMinecraft(*p);}
endstone::core::EndstoneInventory* inventory_base(endstone::PlayerInventory* p){return static_cast<endstone::core::EndstonePlayerInventory*>(p);}
unsigned item_instance_size(){return sizeof(ItemInstance);}
unsigned inventory_size(){return sizeof(Inventory);}
}

extern "C" void tag_write(const Tag* p,IDataOutput* out){p->write(*out);}
extern "C" void output_write(IDataOutput* out,std::string_view s){out->writeString(s);out->writeLongString(s);out->writeFloat(1);out->writeDouble(2);out->writeByte(3);out->writeShort(4);out->writeInt(5);out->writeLongLong(6);out->writeBytes(s.data(),s.size());}
