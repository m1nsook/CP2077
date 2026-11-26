// Included after the common native helpers. No direct writes to mass,
// inertia, pose or velocity fields; invoke the game's own mass setters only.
// Older SDK patterns are exact opcode sequences except the tail rel32.
// Require unique matches in the executable and a common executable tail.
using AltMassSetter=void(*)(PhysicsData*,float);
using NormalMassSetter=void(*)(PhysicsData*);
AltMassSetter setAltMass=nullptr;
NormalMassSetter setNormalMass=nullptr;
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
unsigned altMatches{},normalMatches{};
std::uintptr_t altRva{},normalRva{},tailRva{};
constexpr unsigned char altPrefix[]={0x48,0x8D,0x81,0xB4,0x01,0,0,0x48,0x8D,0x54,0x24,0x08,
    0x48,0x3B,0xD0,0x74,0x08,0xF3,0x0F,0x11,0x08,0xC6,0x40,0x04,0x01,0xE9};
constexpr unsigned char normalPrefix[]={0xC6,0x81,0xB8,0x01,0,0,0,0xE9};
template<std::size_t N> const unsigned char* findMassPrefix(const unsigned char(&pattern)[N],unsigned& matches) {
    auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    if(!base)return nullptr;
    auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>0x100000)return nullptr;
    auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return nullptr;
    const unsigned char* found=nullptr;
    auto* section=IMAGE_FIRST_SECTION(nt);
    const auto imageSize=nt->OptionalHeader.SizeOfImage;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        if(!(section[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)||section[i].VirtualAddress>=imageSize)continue;
        const auto size=std::min(section[i].Misc.VirtualSize,imageSize-section[i].VirtualAddress);
        auto* start=base+section[i].VirtualAddress;auto* end=start+size;
        for(auto* cursor=start;cursor<end;) {
            MEMORY_BASIC_INFORMATION page{};
            if(!VirtualQuery(cursor,&page,sizeof(page)))break;
            auto* regionEnd=static_cast<const unsigned char*>(page.BaseAddress)+page.RegionSize;
            auto* limit=std::min(end,regionEnd);
            if(limit<=cursor)break;
            const bool read=page.State==MEM_COMMIT&&!(page.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
                (page.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
            if(read)for(auto* scan=cursor;limit-scan>=static_cast<std::ptrdiff_t>(N+4);) {
                auto* hit=static_cast<const unsigned char*>(std::memchr(scan,pattern[0],static_cast<std::size_t>(limit-scan-N-3)));
                if(!hit)break;
                if(std::memcmp(hit,pattern,N)==0){++matches;found=hit;if(matches>1)return nullptr;}
                scan=hit+1;
            }
            cursor=limit;
        }
    }
    return matches==1?found:nullptr;
}
template<std::size_t N> const unsigned char* massTail(const unsigned char* at,const unsigned char(&)[N]){
    std::int32_t rel{};std::memcpy(&rel,at+N,4);
    return reinterpret_cast<const unsigned char*>(reinterpret_cast<std::uintptr_t>(at+N+4)+static_cast<std::intptr_t>(rel));
}
void resolveMass() {
    auto* a=findMassPrefix(altPrefix,altMatches);auto* n=findMassPrefix(normalPrefix,normalMatches);
    if(!a||!n){massStatus="unavailable: SDK mass setter signatures absent or ambiguous";return;}
    auto* at=massTail(a,altPrefix);auto* nt=massTail(n,normalPrefix);
    auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    altRva=reinterpret_cast<std::uintptr_t>(a)-base;normalRva=reinterpret_cast<std::uintptr_t>(n)-base;
    if(at!=nt||!executableAddress(const_cast<unsigned char*>(at))||
       !executableAddress(const_cast<unsigned char*>(a))||!executableAddress(const_cast<unsigned char*>(n))){
        massStatus="unavailable: setters do not share an executable update tail";return;
    }
    tailRva=reinterpret_cast<std::uintptr_t>(at)-base;
    setAltMass=reinterpret_cast<AltMassSetter>(const_cast<unsigned char*>(a));
    setNormalMass=reinterpret_cast<NormalMassSetter>(const_cast<unsigned char*>(n));
    massStatus="SDK opcode signatures matched; runtime setter effects unverified";
}
void massReadback(PhysicsData* rb,MassState& s) {
    s.alternative=rb->alternativeChassisMass;s.flag=rb->usesAlternativeChassisMass;
    s.chassis=rb->chassis_mass;s.total=rb->total_mass;s.inverse=rb->inverseMass;
}
void massStep(PhysicsData* rb) {
    if(!rb||!rb->vehicle||rb->vehicle->physicsData!=rb)return;
    auto* owner=rb->vehicle;const auto id=owner->entityID.hash;
    const auto active=activeMassID.load(std::memory_order_relaxed),selected=selectedMassID.load(std::memory_order_relaxed);
    if(!id||(id!=active&&id!=selected))return;
    if(!active&&requestedMass.load()==1)return;
    if(!setAltMass||!setNormalMass)return;
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
            setNormalMass(rb);massReadback(rb,currentMass);lastMass=currentMass;
            if(currentMass.flag!=0){++massRestoreFailures;requestedMass.store(1);return;}
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
    setAltMass(rb,requested);massReadback(rb,currentMass);lastMass=currentMass;
    if(!currentMass.flag||std::abs(currentMass.alternative-requested)>0.1f){
        ++massReadbackFailures;requestedMass.store(1);
        if(currentMass.flag){setNormalMass(rb);massReadback(rb,currentMass);lastMass=currentMass;}
        if(!currentMass.flag){currentMass={};activeMassID.store(0);}else ++massRestoreFailures;
        return;
    }
    ++massApplies;
}
void massReport(std::ostringstream& s) {
    std::lock_guard lock(massMutex);
    const auto& m=currentMass.id?currentMass:lastMass;
    s<<"; MASS status="<<massStatus<<" available="<<(setAltMass&&setNormalMass)
     <<" alt_matches="<<altMatches<<" normal_matches="<<normalMatches<<" alt_rva="<<altRva<<" normal_rva="<<normalRva<<" tail_rva="<<tailRva
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
    if(!currentMass.id)lastMass={};
}
