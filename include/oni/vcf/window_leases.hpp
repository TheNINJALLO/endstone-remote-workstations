#pragma once
#include "core.hpp"

namespace oni::vcf {
// Protocol 2169 closes carry a small window ID, not a session generation.
// Retain only IDs VCF actually owned, then compare with the current native
// manager. Never consume a legitimate close for that same current window.
class WindowLeases {
    std::map<std::pair<std::string,uint8_t>,uint64_t> retired_;
public:
    void expire(uint64_t now){std::erase_if(retired_,[now](const auto& row){return row.second<=now;});}
    void retire(std::string player,uint8_t window,uint64_t deadline,uint64_t now){
        require(!player.empty()&&player.size()<=128&&window>=1&&window<=99&&deadline>now);
        expire(now);auto key=std::make_pair(std::move(player),window);
        require(retired_.contains(key)||retired_.size()<10000,VCF_CAPACITY);
        retired_[std::move(key)]=deadline;
    }
    bool reserved(std::string_view player,uint8_t window,uint64_t now)const{
        auto it=retired_.find({std::string(player),window});return it!=retired_.end()&&it->second>now;
    }
    bool blocks_close(std::string_view player,uint8_t incoming,bool manager_active,uint8_t current,uint64_t now)const{
        return manager_active&&incoming!=current&&reserved(player,incoming,now);
    }
    // A genuine observed native open can reuse a retired ID through another
    // plugin. Relinquish that ID rather than owning the other plugin's close.
    void observed_open(std::string_view player,uint8_t window){retired_.erase({std::string(player),window});}
    size_t size()const{return retired_.size();}
};
}
