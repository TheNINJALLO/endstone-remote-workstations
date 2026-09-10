#include <oni/vcf/core.hpp>
#include <oni/vcf/window_leases.hpp>
#include <iostream>
#include <stdexcept>
using namespace oni::vcf;
namespace {
void check(bool value) { if(!value)throw std::runtime_error("lifecycle assertion failed"); }
template<class F> void refused(vcf_status status,F f) {
    try { f(); throw std::runtime_error("expected refusal"); }
    catch(const Error& e) { check(e.status==status); }
}
struct Fixture {
    bool permitted=true;
    int opens=0,closes=0,events=0;
    Engine engine;
    vcf_handle owner;
    Fixture():engine(host()),owner(engine.consumer("consumer")){}
    Host host() {
        Host h;
        h.consumer_allowed=[](auto){return true;};
        h.permission=[this](auto,auto){return permitted;};
        h.open=[this](const Session&){++opens;return VCF_PENDING;};
        h.close=[this](const Session&){++closes;return VCF_PENDING;};
        return h;
    }
    vcf_handle prepare(std::string player="player") {
        Session s;s.kind="craft";s.player=std::move(player);s.mode=VCF_NATIVE_CONTEXT;
        s.callback=[](void* p,const vcf_event*)->vcf_status{++*static_cast<int*>(p);return VCF_OK;};
        s.context=&events;return engine.prepare(owner,std::move(s));
    }
};
void pending_and_cancel() {
    Fixture f;auto a=f.prepare();f.engine.open(f.owner,a);f.engine.close(f.owner,a);f.engine.tick();
    check(f.opens==0&&f.closes==0&&f.engine.session(f.owner,a).state==VCF_TERMINAL);
    a=f.prepare();auto b=f.prepare();f.engine.open(f.owner,a);f.engine.open(f.owner,b);f.engine.tick();
    check(f.opens==1&&f.engine.session(f.owner,a).state==VCF_OPENING);
    check(f.engine.session(f.owner,b).result==VCF_CONFLICT);
    f.engine.close(f.owner,a);f.engine.tick();check(f.closes==1&&f.engine.session(f.owner,a).state==VCF_CLOSING);
    refused(VCF_STALE,[&]{f.engine.opened(f.owner,a,VCF_OK);});
    f.engine.retired(f.owner,a,VCF_CLOSED);auto n=f.events;
    refused(VCF_STALE,[&]{f.engine.retired(f.owner,a,VCF_CLOSED);});check(f.events==n);
}
void permissions_and_release() {
    Fixture f;auto a=f.prepare();f.engine.open(f.owner,a);f.engine.tick();
    f.permitted=false;f.engine.opened(f.owner,a,VCF_OK);
    check(f.engine.session(f.owner,a).state==VCF_CLOSING&&f.events==0);
    f.engine.tick();check(f.closes==1);f.engine.retired(f.owner,a,VCF_CLOSED);
    check(f.events==1&&f.engine.session(f.owner,a).result==VCF_DENIED);
    f.permitted=true;a=f.prepare();f.engine.open(f.owner,a);f.engine.tick();
    f.engine.release(f.owner);check(f.closes==2&&f.engine.session_count()==0);
    refused(VCF_NOT_FOUND,[&]{f.engine.opened(f.owner,a,VCF_OK);});
}
void disable_inside_callback() {
    Fixture f;
    f.engine.action(f.owner,"disable","",false,[](void* p,const vcf_event*)->vcf_status{
        auto& f=*static_cast<Fixture*>(p);
        refused(VCF_REENTRANT,[&]{f.engine.release(f.owner);});
        f.engine.release_named("consumer");
        refused(VCF_CLOSED,[&]{f.prepare();});
        return VCF_OK;
    },&f);
    f.engine.invoke(f.owner,"player","disable");f.engine.tick();
    // Deferred destruction must finish on the next server turn without ever
    // invoking the revoked callback or retaining a callable consumer pointer.
    f.engine.tick();check(f.events==0&&f.engine.session_count()==0);
}
void retired_window_closes(){
    WindowLeases leases;leases.retire("player",71,61000,1000);
    check(leases.blocks_close("player",71,true,72,2000));
    check(!leases.blocks_close("other_player",71,true,72,2000));
    check(!leases.blocks_close("player",71,true,71,2000));
    check(!leases.blocks_close("player",71,false,72,2000));
    check(!leases.blocks_close("player",70,true,72,2000));
    leases.observed_open("player",71);check(!leases.reserved("player",71,2000));
    leases.retire("player",71,61000,2000);leases.expire(61000);check(leases.size()==0);
}
void synchronous_host_completion(){
    Engine* current=nullptr;int events=0;
    Host h;h.consumer_allowed=[](auto){return true;};h.permission=[](auto,auto){return true;};
    h.open=[&](const Session& s){current->retired(s.owner,s.id,VCF_CLOSED);return VCF_OK;};
    Engine e(h);current=&e;auto owner=e.consumer("consumer");
    Session s;s.player="player";s.kind="craft";s.mode=VCF_NATIVE_CONTEXT;
    s.callback=[](void* p,const vcf_event*)->vcf_status{++*static_cast<int*>(p);return VCF_OK;};s.context=&events;
    auto id=e.prepare(owner,s);e.open(owner,id);e.tick();
    check(events==1&&e.session(owner,id).state==VCF_TERMINAL&&e.session(owner,id).result==VCF_CLOSED);
    e.close(owner,id);e.tick();check(events==1);
}
void menu_action_ticket_lifecycle(){
    Host h;bool permitted=true;int calls=0;vcf_status action_result=VCF_OK;
    h.consumer_allowed=[](auto){return true;};h.permission=[&](auto,auto){return permitted;};
    h.open=[](auto&){return VCF_OK;};h.close=[](auto&){return VCF_OK;};
    Engine e(h);auto caller=e.consumer("caller"),target=e.consumer("target");
    struct State{int* calls;vcf_status* result;};State state{&calls,&action_result};
    e.action(target,"selected","action.permission",true,[](void* p,const vcf_event*)->vcf_status{
        auto&s=*static_cast<State*>(p);++*s.calls;return *s.result;
    },&state);
    auto menu=[&]{Session s;s.is_menu=true;s.player="player";s.mode=VCF_TRANSIENT;s.permission="menu.permission";
        s.buttons.push_back({"Run","target:selected",""});auto id=e.prepare(caller,s);e.open(caller,id);e.tick();return id;};
    for(int n=0;n<5000;++n){
        auto id=menu();e.selected(id,"player",0);
        refused(VCF_STALE,[&]{e.selected(id,"player",0);});
        check(e.session_count()==1);e.tick();
        check(calls==n+1&&e.session_count()==1&&e.session(caller,id).state==VCF_TERMINAL);
        e.forget(caller,id);check(e.session_count()==0);
    }
    auto id=menu();e.selected(id,"player",0);permitted=false;e.tick();
    check(calls==5000&&e.session(caller,id).result==VCF_DENIED);e.forget(caller,id);permitted=true;
    id=menu();e.selected(id,"player",0);e.close(caller,id);e.tick();
    check(calls==5000&&e.session(caller,id).state==VCF_TERMINAL);e.forget(caller,id);
    action_result=VCF_INTERNAL;id=menu();e.selected(id,"player",0);e.tick();
    check(calls==5001&&e.session(caller,id).result==VCF_INTERNAL);e.forget(caller,id);
    id=e.invoke(caller,"player","target:selected");e.tick();
    check(calls==5002&&e.session(caller,id).result==VCF_INTERNAL);e.forget(caller,id);
    check(e.session_count()==0);
}
void committed_callback_lifecycle(){
    Fixture f;
    struct State {Fixture* fixture;unsigned calls=0;uint64_t revision=0;bool close=false;} state{&f};
    Session s;s.kind="shulker";s.player="player";s.mode=VCF_REAL_SOURCE;s.context=&state;
    s.callback=[](void* context,const vcf_event* event)->vcf_status{
        auto& state=*static_cast<State*>(context);
        if(event->kind==VCF_EVENT_COMMIT){
            ++state.calls;state.revision=event->revision;
            if(state.close)state.fixture->engine.close(state.fixture->owner,event->ticket);
        }
        return VCF_OK;
    };
    auto id=f.engine.prepare(f.owner,s);f.engine.open(f.owner,id);f.engine.tick();
    refused(VCF_STALE,[&]{f.engine.committed(f.owner,id,1);});
    f.engine.opened(f.owner,id,VCF_OK);f.engine.committed(f.owner,id,2);
    check(state.calls==1&&state.revision==2&&f.engine.session(f.owner,id).inventory.revision==2);
    refused(VCF_STALE,[&]{f.engine.committed(f.owner,id,2);});
    refused(VCF_STALE,[&]{f.engine.committed(f.owner,id,1);});check(state.calls==1);
    state.close=true;f.engine.committed(f.owner,id,4);
    check(state.calls==2&&state.revision==4&&f.engine.session(f.owner,id).state==VCF_CLOSING);
    refused(VCF_STALE,[&]{f.engine.committed(f.owner,id,5);});
    f.engine.tick();f.engine.retired(f.owner,id,VCF_CLOSED);check(f.closes==1);
    refused(VCF_STALE,[&]{f.engine.committed(f.owner,id,6);});check(state.calls==2);
}
}
int main() {
    try { pending_and_cancel();permissions_and_release();disable_inside_callback();retired_window_closes();synchronous_host_completion();menu_action_ticket_lifecycle();committed_callback_lifecycle();
        std::cout<<"Asynchronous opens, closes, revocation and ownership passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    catch(const Error& e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}
}
