#include "inventory_revision.hpp"
#include <oni/vcf/storage.hpp>
#include <iostream>
#include <stdexcept>
using namespace oni::vcf;
using namespace oni::vcf::inventory;
namespace {
void check(bool yes){if(!yes)throw std::runtime_error("inventory lease assertion failed");}
template<class F>void refused(vcf_status expected,F f){try{f();throw std::runtime_error("expected inventory refusal");}catch(const Error&e){check(e.status==expected);}}
void lifecycle(){
 auto revision=std::make_shared<Revision>();auto baseline=revision->capture();
 revision->validate(baseline); // Empty manager polls create no Mutation.
 {Mutation setter(revision,Revision::Kind::slot,7);refused(VCF_REENTRANT,[&]{revision->capture();});
  {Mutation notification(revision,Revision::Kind::slot,7);refused(VCF_REENTRANT,[&]{revision->validate(baseline);});}
  refused(VCF_REENTRANT,[&]{revision->capture();});
 }
 auto after=revision->capture();check(after.slots[7]==4&&after.revision==4&&after.request_revision==0);
 refused(VCF_STALE,[&]{revision->validate(baseline);});
 baseline=after;
 // Native bundle extraction changes no slot epoch but must stale the lease.
 {Mutation request(revision,Revision::Kind::request);}
 after=revision->capture();check(after.slots==baseline.slots&&after.request_revision==2);
 refused(VCF_STALE,[&]{revision->validate(baseline);});
 baseline=after;
 // A -> B -> A has equal saved bytes but two distinct mutation boundaries.
 for(int i=0;i<2;++i){Mutation moved(revision,Revision::Kind::slot,19);}
 refused(VCF_STALE,[&]{revision->validate(baseline);});
 baseline=revision->capture();{Mutation invalid_slot(revision,Revision::Kind::slot,-1);}
 check(revision->capture().slots==baseline.slots);refused(VCF_STALE,[&]{revision->validate(baseline);});
 baseline=revision->capture();try{Mutation failed(revision,Revision::Kind::request);throw 7;}catch(int){}
 refused(VCF_STALE,[&]{revision->validate(baseline);});baseline=revision->capture();revision->validate(baseline);
 revision->retire();refused(VCF_CLOSED,[&]{revision->validate(baseline);});
 auto replacement=std::make_shared<Revision>();replacement->capture();refused(VCF_CLOSED,[&]{revision->capture();});
}
void concurrency(){
 auto revision=std::make_shared<Revision>();
 vcf_status status=VCF_OK;
 std::thread reader([&]{try{revision->capture();}catch(const Error&e){status=e.status;}});reader.join();check(status==VCF_WRONG_THREAD);
 revision->capture(); // A rejected worker read alone does not poison the owner.
 std::thread writer([&]{Mutation write(revision,Revision::Kind::slot,7);});writer.join();
 refused(VCF_QUARANTINED,[&]{revision->capture();});
 // Callback keeps its clock alive even if disconnect retires/removes the map.
 auto owner=std::make_shared<Revision>();std::weak_ptr<Revision> weak=owner;
 {Mutation callback(owner,Revision::Kind::request);owner->retire();owner.reset();check(!weak.expired());}
 check(weak.expired());
 auto unbalanced=std::make_shared<Revision>();unbalanced->end(Revision::Kind::slot,0);
 refused(VCF_QUARANTINED,[&]{unbalanced->capture();});
}
void reservations(){
 using namespace oni::vcf::storage;
 State actual;actual.generation=1;actual.next_identity=2;actual.storage.resize(27);
 actual.player[7]={{{"minecraft:stone",6,64,{}},VCF_INSERT|VCF_EXTRACT},1};
 auto before=actual;auto revision=std::make_shared<Revision>();auto stamp=revision->capture();int writes=0;
 Request request{-1,-1,{{Kind::place,{12,7,std::nullopt,1},Reference{7,0,std::nullopt,0},1,false}}};
 auto writer=[&](auto&,auto& next){++writes;actual=next;stamp=revision->capture();return VCF_OK;};
 auto guard=[&]{revision->validate(stamp);};
 Reservation aba(actual,Layout("chest"),[&]{return actual;},writer,guard);
 for(int i=0;i<2;++i){Mutation moved(revision,Revision::Kind::slot,7);}
 check(actual==before);refused(VCF_STALE,[&]{aba.apply(request,1,0);});check(aba.closed()&&!aba.quarantined()&&writes==0);
 stamp=revision->capture();bool mutate_in_reader=false;
 Reservation in_place(actual,Layout("chest"),[&]{if(mutate_in_reader){Mutation edit(revision,Revision::Kind::request);}return actual;},writer,guard);
 mutate_in_reader=true;refused(VCF_STALE,[&]{in_place.apply(request,1,0);});check(writes==0&&actual==before);
 stamp=revision->capture();int guards=0;
 Reservation before_write(actual,Layout("chest"),[&]{return actual;},writer,[&]{if(++guards==4){Mutation edit(revision,Revision::Kind::request);}guard();});
 refused(VCF_STALE,[&]{before_write.apply(request,1,0);});check(writes==0&&actual==before);
 stamp=revision->capture();Reservation valid(actual,Layout("chest"),[&]{return actual;},writer,guard);
 check(valid.apply(request,1,0).committed&&writes==1);check(valid.apply(request,1,0).replayed&&writes==1);
 Mutation native_request(revision,Revision::Kind::request);
 refused(VCF_REENTRANT,[&]{valid.apply(request,1,0);});check(writes==1);
}
}
int main(){try{lifecycle();concurrency();reservations();std::cout<<"Inventory leases and reservation admission: ABA, in-place edits, nested requests, unwind, retirement and wrong-thread writes passed\n";}
 catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error&e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
