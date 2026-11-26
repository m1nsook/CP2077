// Exact 2.50 PE timestamp 0x6AD55DAF and code recovered from the supplied executable.
// The vanilla toggle inlines assignment to RigidBody optional<float> at +1B4,
// then calls this refresh routine. Refresh locks the chassis handle and issues
// the engine's own mass command. No total_mass/inverseMass/inertia/velocity changes.
using MassRefresh=void(*)(PhysicsData*);
using MassWriter=void*(*)(void*,float,std::uint32_t,bool);
MassRefresh refreshMass=nullptr;
MassWriter originalMassWriter=nullptr;
void* massWriterTarget=nullptr;
std::atomic<bool> massReady{false};
struct MassCommand {unsigned count{};float value{};std::uint32_t actor{};bool flag{};};
thread_local MassCommand* captureMassCommand=nullptr;
void* observeMassWriter(void* writer,float value,std::uint32_t actor,bool flag){
    if(captureMassCommand){
        ++captureMassCommand->count;captureMassCommand->value=value;
        captureMassCommand->actor=actor;captureMassCommand->flag=flag;
    }
    return originalMassWriter(writer,value,actor,flag);
}
std::atomic<int> requestedMass{1};
std::atomic<std::uint64_t> selectedMassID{0},activeMassID{0};
std::mutex massMutex;
struct MassState {
    std::uint64_t id{};
    std::uintptr_t body{};
    float baseChassis{},baseTotal{},baseInverse{},baseAlternative{};
    int multiplier{1};
    float alternative{},chassis{},total{},inverse{};
    unsigned flag{};
};
MassState currentMass,lastMass;
std::uint64_t massApplies{},massRestores{},massRejects{},massReadbackFailures{},massRestoreFailures{},massBlocked{},massCalls{};
std::string massStatus="not resolved";
std::uintptr_t refreshRva{},updateRva{},writerRva{};
std::uint64_t massRefreshes{},massCommands{},massCommandFailures{};
float lastCommandMass{};
std::uint32_t lastCommandActor{};

// Match the exact observed entry and the internal selection/command instructions.
// No scanning heuristic or unverified function invocation.
constexpr unsigned char refreshBytes[]={0x48,0x83,0xec,0x38,0x4c,0x8b,0x81,0x80,0x01,0x00,0x00,0x0f,0x57,0xc0,0xf3,0x0f,0x7f,0x44,0x24,0x20,0x49,0x8b,0x80,0xf8,0x02,0x00,0x00};
constexpr unsigned char selectBytes[]={0x8a,0x82,0xb8,0x01,0x00,0x00,0xf6,0xd8,0x48,0x1b,0xc9,0x48,0x83,0xe1,0xf4,0xf3,0x0f,0x10,0xbc,0x11,0xc0,0x01,0x00,0x00};
constexpr unsigned char writerBytes[]={0x48,0x8b,0xc4,0xf3,0x0f,0x11,0x48,0x10,0x53,0x48,0x83,0xec,0x50,0x48,0x8b,0x11,0x48,0x8d,0x40,0x10,0x41,0x80,0xf1,0x01};
template<std::size_t N> bool massCode(std::uintptr_t at,const unsigned char(&bytes)[N]){
    return executableAddress(reinterpret_cast<void*>(at))&&readable(reinterpret_cast<void*>(at),N)&&
        std::memcmp(reinterpret_cast<void*>(at),bytes,N)==0;
}
void resolveMass(RED4ext::PluginHandle handle,const RED4ext::Sdk* sdk,std::uintptr_t(*resolve)(std::uint32_t)) {
    auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(!base||!readable(dos,sizeof(*dos))||dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>0x100000){
        massStatus="unavailable: executable header";return;
    }
    auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(!readable(nt,sizeof(*nt))||nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.TimeDateStamp!=0x6AD55DAFu){
        massStatus="unavailable: executable timestamp mismatch";return;
    }
    // Address-map offsets + section RVA 0x1000. The executable supplied by the
    // user has matching timestamp; require resolver to agree with every RVA.
    auto refresh=resolve(4183691991u),update=resolve(841094152u),writer=resolve(2719813719u);
    if(refresh!=base+0x81e110||update!=base+0x81d718||writer!=base+0x81d89c){
        massStatus="unavailable: address-map mismatch";return;
    }
    refreshRva=refresh-base;updateRva=update-base;writerRva=writer-base;
    if(!massCode(refresh,refreshBytes)||!massCode(update+0x48,selectBytes)||!massCode(writer,writerBytes)){
        massStatus="unavailable: native opcode mismatch";return;
    }
    auto* target=reinterpret_cast<void*>(writer);
    if(!sdk->hooking->Attach(handle,target,reinterpret_cast<void*>(&observeMassWriter),reinterpret_cast<void**>(&originalMassWriter))){
        massStatus="unavailable: mass command observation hook failed";return;
    }
    massWriterTarget=target;refreshMass=reinterpret_cast<MassRefresh>(refresh);
    massReady.store(true);massStatus="2.50 executable verified; native mass refresh and command hook ready";
}
bool submitMass(PhysicsData* rb,float expected){
    MassCommand c{};
    captureMassCommand=&c;
    refreshMass(rb);
    captureMassCommand=nullptr;
    ++massRefreshes;massCommands+=c.count;
    if(c.count){lastCommandMass=c.value;lastCommandActor=c.actor;}
    if(c.count!=1||c.actor!=0||!std::isfinite(c.value)||std::abs(c.value-expected)>.1f){
        ++massCommandFailures;return false;
    }
    return true;
}
void massReadback(PhysicsData* rb,MassState& s) {
    s.alternative=rb->alternativeChassisMass;s.flag=rb->usesAlternativeChassisMass;
    s.chassis=rb->chassis_mass;s.total=rb->total_mass;s.inverse=rb->inverseMass;
}
void massStep(PhysicsData* rb,void* chassis) {
    if(!rb||!rb->vehicle||rb->vehicle->physicsData!=rb)return;
    auto* owner=rb->vehicle;const auto id=owner->entityID.hash;
    const auto active=activeMassID.load(std::memory_order_relaxed),selected=selectedMassID.load(std::memory_order_relaxed);
    if(!id||(id!=active&&id!=selected))return;
    if(!active&&requestedMass.load()==1)return;
    if(!massReady.load(std::memory_order_acquire)||!refreshMass)return;
    if(!chassis||owner->chassis.GetPtr()!=chassis)return;
    const auto stamp=nowUs();bool permitted=false;
    int wanted=requestedMass.load(std::memory_order_relaxed);
    {
        std::lock_guard lock(sampleMutex);
        permitted=kineticle::massScope(id,controllerContext.vehicle,owner->isPlayerControlled!=0,
            static_cast<unsigned>(owner->physicsState),controllerContext.allowed&&controllerContext.us!=0,
            stamp-controllerContext.us,wanted);
    }
    static thread_local bool inside=false;if(inside)return;
    std::lock_guard lock(massMutex);++massCalls;
    inside=true;
    struct Exit{bool& value;~Exit(){value=false;}} exit{inside};
    const auto body=reinterpret_cast<std::uintptr_t>(rb);
    if(currentMass.id) {
        if(currentMass.id!=id||currentMass.body!=body){++massBlocked;return;}
        massReadback(rb,currentMass);
        // Never overwrite another mod's mass mode or a replaced body's data.
        const auto expected=kineticle::desiredChassisMass(currentMass.baseChassis,currentMass.multiplier);
        if(!currentMass.flag||std::abs(currentMass.alternative-expected)>0.1f){
            ++massRejects;lastMass=currentMass;currentMass={};activeMassID.store(0);requestedMass.store(1);return;
        }
        if(!permitted||wanted!=currentMass.multiplier||requestedMass.load()!=wanted) {
            rb->alternativeChassisMass=currentMass.baseAlternative;
            rb->usesAlternativeChassisMass=0;
            const bool restored=submitMass(rb,currentMass.baseChassis);
            massReadback(rb,currentMass);lastMass=currentMass;
            if(!restored||currentMass.flag!=0){++massRestoreFailures;requestedMass.store(1);currentMass={};activeMassID.store(0);massReady.store(false);return;}
            ++massRestores;currentMass={};activeMassID.store(0);
        }else return;
    }
    if(!permitted||requestedMass.load()!=wanted||selectedMassID.load()!=id)return;
    if(rb->usesAlternativeChassisMass!=0||!std::isfinite(rb->inverseMass)||rb->inverseMass<=0||
        !std::isfinite(rb->total_mass)||rb->total_mass<=0){++massRejects;return;}
    const auto requested=kineticle::desiredChassisMass(rb->chassis_mass,wanted);
    if(requested<=0){++massRejects;return;}
    currentMass={id,body,rb->chassis_mass,rb->total_mass,rb->inverseMass,rb->alternativeChassisMass,wanted};
    activeMassID.store(id);
    rb->alternativeChassisMass=requested;
    rb->usesAlternativeChassisMass=1;
    const bool submitted=submitMass(rb,requested);
    massReadback(rb,currentMass);lastMass=currentMass;
    if(!submitted||!currentMass.flag||std::abs(currentMass.alternative-requested)>0.1f){
        ++massReadbackFailures;requestedMass.store(1);
        rb->alternativeChassisMass=currentMass.baseAlternative;rb->usesAlternativeChassisMass=0;
        if(!submitMass(rb,currentMass.baseChassis))++massRestoreFailures;
        massReadback(rb,currentMass);lastMass=currentMass;
        currentMass={};activeMassID.store(0);massReady.store(false);
        return;
    }
    ++massApplies;
}
void massReport(std::ostringstream& s) {
    std::lock_guard lock(massMutex);
    const auto& m=currentMass.id?currentMass:lastMass;
    s<<"; MASS status="<<massStatus<<" available="<<massReady.load()
     <<" refresh_rva="<<refreshRva<<" update_rva="<<updateRva<<" writer_rva="<<writerRva
     <<" refreshes="<<massRefreshes<<" native_commands="<<massCommands<<" command_failures="<<massCommandFailures
     <<" last_command_mass="<<lastCommandMass<<" last_command_actor="<<lastCommandActor
     <<" requested="<<requestedMass.load()<<" selected_id="<<selectedMassID.load()<<" active_id="<<activeMassID.load()
     <<" applies="<<massApplies<<" restores="<<massRestores<<" rejected="<<massRejects<<" readback_failed="<<massReadbackFailures
     <<" restore_failed="<<massRestoreFailures<<" blocked="<<massBlocked<<" calls="<<massCalls
     <<" last_id="<<m.id<<" multiplier="<<m.multiplier<<" base_chassis="<<m.baseChassis<<" base_total="<<m.baseTotal
     <<" base_inverse="<<m.baseInverse<<" base_alternative="<<m.baseAlternative<<" alt="<<m.alternative
     <<" flag="<<m.flag<<" chassis="<<m.chassis<<" total="<<m.total<<" inverse="<<m.inverse;
}
void resetMassCounters(){
    std::lock_guard lock(massMutex);
    massApplies=massRestores=massRejects=massReadbackFailures=massRestoreFailures=massBlocked=massCalls=0;
    massRefreshes=massCommands=massCommandFailures=0;lastCommandMass=0;lastCommandActor=0;
    if(!currentMass.id)lastMass={};
}
