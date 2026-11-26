// Included in plugin.cpp's anonymous namespace after the proven event decoder.
// Every RTTI invocation checks the live signature. No generated property
// offsets, persistent entity handles, or mutations of global records.
using H=RED4ext::Handle<IScriptable>;
using W=RED4ext::WeakHandle<IScriptable>;
using Fn=RED4ext::CBaseFunction;
using RttiType=RED4ext::CBaseRTTIType;
constexpr const char* scopeNames[]={"allowed","not-road","not-driver","remote","quest","destroyed",
    "locked","player","mounted","blackboard","tags"};
static_assert(std::size(scopeNames)==static_cast<unsigned>(kineticle::Scope::Count));
std::atomic<std::uint64_t> scopeCounts[std::size(scopeNames)]{};
std::atomic<std::uint64_t> damageCalls{0},damageVehicle{0},damageReject{0},damageZeroed{0},damageVerified{0},
    damageAlreadyZero{0},damageApiFail{0},gridCalls{0},gridDecodeFail{0},gridResets{0},glassResets{0},visualApiFail{0},
    gridPairs{0},gridRammed{0},gridWasRammed{0};
std::atomic<std::uint64_t> lastProtectedId{0},lastGridVehicle{0},lastGridOther{0};
std::atomic<float> lastDamageBefore{0},lastDamageAfter{0},lastGridMultiplier{0};
std::atomic<int> lastPsm{-1};
bool typeNamed(RttiType* type,const char* name) {return type&&type->GetName()==RED4ext::CName(name);}
bool handleType(RttiType* type,const char* inner,bool weak=false) {
    auto* rtti=RED4ext::CRTTISystem::Get();
    auto* expected=rtti?rtti->GetClass(inner):nullptr;
    if(!expected||!type||type->GetSize()!=sizeof(H))return false;
    if(weak)return type->GetType()==RED4ext::ERTTIType::WeakHandle&&
        static_cast<RED4ext::CRTTIWeakHandleType*>(type)->GetInnerType()==expected;
    return type->GetType()==RED4ext::ERTTIType::Handle&&
        static_cast<RED4ext::CRTTIHandleType*>(type)->GetInnerType()==expected;
}
bool isObject(IScriptable* obj,const char* name) {
    auto* rtti=RED4ext::CRTTISystem::Get();auto* expected=rtti?rtti->GetClass(name):nullptr;
    return expected&&obj&&readable(obj,sizeof(IScriptable))&&obj->GetType()&&obj->GetType()->IsA(expected);
}
Fn* method(IScriptable* obj,const char* name) {
    return obj&&obj->GetType()?obj->GetType()->GetFunction(RED4ext::CName(name)):nullptr;
}
RttiType* retType(Fn* f) {return f&&f->returnType?f->returnType->type:nullptr;}
RttiType* argType(Fn* f,unsigned i) {return f&&i<f->params.size&&f->params[i]?f->params[i]->type:nullptr;}
bool signature(Fn* f,unsigned n,bool isStatic,bool requireNative=false) {
    if(!f||f->params.size!=n||f->flags.isStatic!=isStatic||(requireNative&&!f->flags.isNative))return false;
    for(unsigned i=0;i<n;++i)if(!argType(f,i)||f->params[i]->flags.isOut)return false;
    return true;
}
bool getHandle(IScriptable* obj,const char* name,const char* inner,H& out) {
    auto* f=method(obj,name);
    return signature(f,0,false)&&handleType(retType(f),inner)&&
        RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(obj),f,&out)&&isObject(out.GetPtr(),inner);
}
template<class T> const T* prop(IScriptable* obj,const char* name,RttiType* expected) {
    auto* cls=obj?obj->GetType():nullptr;auto* pr=cls?cls->GetProperty(RED4ext::CName(name)):nullptr;
    if(!pr||!expected||pr->type!=expected||expected->GetSize()!=sizeof(T))return nullptr;
    auto* ptr=pr->GetValuePtr<T>(static_cast<RED4ext::ScriptInstance>(obj));
    return readable(ptr,sizeof(T))?ptr:nullptr;
}
RttiType* propertyType(IScriptable* obj,const char* name) {
    auto* cls=obj?obj->GetType():nullptr;auto* pr=cls?cls->GetProperty(RED4ext::CName(name)):nullptr;
    return pr?pr->type:nullptr;
}
H strongProperty(IScriptable* obj,const char* name,const char* inner) {
    auto* type=propertyType(obj,name);
    if(!handleType(type,inner))return {};
    auto* value=prop<H>(obj,name,type);
    return value?*value:H{};
}
H weakProperty(IScriptable* obj,const char* name,const char* inner) {
    auto* type=propertyType(obj,name);
    if(!handleType(type,inner,true))return {};
    auto* value=prop<W>(obj,name,type);
    return value?value->Lock():H{};
}
template<class T> const T* namedProp(IScriptable* obj,const char* name,const char* typeName) {
    auto* type=propertyType(obj,name);
    return typeNamed(type,typeName)?prop<T>(obj,name,type):nullptr;
}
H componentVehicle(IScriptable* component) {
    if(!isObject(component,"VehicleComponent"))return {};
    auto* f=method(component,"GetVehicle");
    W weak;
    // The class has three static GetVehicle overloads; use the exact
    // instance full name, not a short-name lookup that can select a static.
    auto* cls=component->GetType();
    f=nullptr;
    for(auto* c=cls;c&&!f;c=c->parent)for(auto* candidate:c->funcs)
        if(candidate&&candidate->fullName==RED4ext::CName("GetVehicle;")){f=candidate;break;}
    if(!signature(f,0,false)||!handleType(retType(f),"vehicleBaseObject",true)||
       !RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(component),f,&weak))return {};
    auto out=weak.Lock();return isObject(out.GetPtr(),"vehicleBaseObject")?out:H{};
}
kineticle::Scope scope(RED4ext::vehicle::BaseObject* v) {
    using S=kineticle::Scope;
    auto* rtti=RED4ext::CRTTISystem::Get();
    if(!v||!rtti||!roadVehicle(v,rtti))return S::NotRoad;
    if(queryBool(v,"IsPlayerDriver").value!=1||queryBool(v,"IsPlayerMounted").value!=1)return S::Driver;
    if(queryBool(v,"IsVehicleRemoteControlled").value!=0)return S::Remote;
    if(queryBool(v,"IsInQuestOrScenePhysicsState").value!=0)return S::Quest;
    if(queryBool(v,"IsDestroyed",true).value!=0)return S::Destroyed;
    if(queryBool(v,"IsVehicleOnStateLocked").value!=0)return S::Locked;
    // Standalone scope: resolve the live local player through the SDK's
    // game-system API, then verify the exact mounted vehicle, PSM and tags.
    auto* engine=RED4ext::CGameEngine::Get();
    auto* playerClass=rtti->GetClass("cpPlayerSystem");
    auto* game=engine&&engine->framework?engine->framework->gameInstance:nullptr;
    auto* ps=game&&playerClass?game->GetSystem(playerClass):nullptr;
    H player;
    auto* pf=method(ps,"GetLocalPlayerMainGameObject");
    if(!signature(pf,0,false,true)||!handleType(retType(pf),"gameObject")||
       !RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(ps),pf,&player)||
       !isObject(player.GetPtr(),"PlayerPuppet"))return S::Player;
    H mounted;
    if(!getHandle(player.GetPtr(),"GetMountedVehicle","vehicleBaseObject",mounted)||mounted.GetPtr()!=v)return S::Mounted;
    H bb,defs;
    if(!getHandle(player.GetPtr(),"GetPlayerStateMachineBlackboard","gameIBlackboard",bb))return S::Blackboard;
    auto* gf=rtti->GetFunction(RED4ext::CName("GetAllBlackboardDefs"));
    if(!signature(gf,0,true,true)||!handleType(retType(gf),"gamebbAllScriptDefinitions")||
       !RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(ps),gf,&defs)||
       !isObject(defs.GetPtr(),"gamebbAllScriptDefinitions"))return S::Blackboard;
    auto psmDef=strongProperty(defs.GetPtr(),"PlayerStateMachine","PlayerStateMachineDef");
    auto* stateType=propertyType(psmDef.GetPtr(),"Vehicle");
    auto* stateProp=psmDef.GetPtr()&&psmDef.GetPtr()->GetType()?
        psmDef.GetPtr()->GetType()->GetProperty(RED4ext::CName("Vehicle")):nullptr;
    auto* getInt=method(bb.GetPtr(),"GetInt");
    if(!typeNamed(stateType,"gamebbScriptID_Int32")||!stateProp||!signature(getInt,1,false,true)||
        argType(getInt,0)!=stateType||!typeNamed(retType(getInt),"Int32"))return S::Blackboard;
    auto* stateArg=stateProp->GetValuePtr<void>(static_cast<RED4ext::ScriptInstance>(psmDef.GetPtr()));
    if(!readable(stateArg,stateType->GetSize()))return S::Blackboard;
    std::int32_t state=-1;
    RED4ext::StackArgs_t args;args.emplace_back(stateType,stateArg);
    if(!RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(bb.GetPtr()),getInt,&state,args))return S::Blackboard;
    lastPsm.store(state);
    if(state!=1&&state!=6)return S::Blackboard;
    auto* statusClass=rtti->GetClass("gameStatusEffectSystem");
    auto* tagFn=statusClass?statusClass->GetFunction(RED4ext::CName("ObjectHasStatusEffectWithTag")):nullptr;
    if(!signature(tagFn,2,true,true)||!handleType(argType(tagFn,0),"gameObject",true)||
       !typeNamed(argType(tagFn,1),"CName")||!typeNamed(retType(tagFn),"Bool"))return S::Tags;
    W playerWeak(player);
    for(const char* name:{"NoDriving","VehicleOnlyForward","VehicleScene"}) {
        RED4ext::CName tag(name);bool has=true;
        RED4ext::StackArgs_t tagArgs;tagArgs.emplace_back(argType(tagFn,0),&playerWeak);tagArgs.emplace_back(argType(tagFn,1),&tag);
        if(!RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(ps),tagFn,&has,tagArgs)||has)return S::Tags;
    }
    return S::Allowed;
}
bool eligible(RED4ext::vehicle::BaseObject* v) {
    const auto result=scope(v);scopeCounts[static_cast<unsigned>(result)].fetch_add(1,std::memory_order_relaxed);
    return result==kineticle::Scope::Allowed;
}
IScriptable* eventArg(Fn* f,RED4ext::CScriptStack* stack,const char* context,const char* event) {
    if(!readable(stack,sizeof(RED4ext::CScriptStack))||!isObject(stack->GetContext(),context)||
       !signature(f,1,false)||!handleType(argType(f,0),event))return nullptr;
    DecodeSample decode;
    auto* h=bumpArgument(f,stack,decode);auto* o=h?h->GetPtr():nullptr;
    return isObject(o,event)?o:nullptr;
}
bool zeroDamage(IScriptable* computed) {
    if(!isObject(computed,"gameAttackComputed"))return false;
    auto* get=method(computed,"GetAttackValues");auto* set=method(computed,"SetAttackValues");
    auto* arrayType=retType(get);
    if(!signature(get,0,false,true)||!signature(set,1,false,true)||set->returnType||
        !typeNamed(arrayType,"array:Float")||arrayType->GetSize()!=sizeof(RED4ext::DynArray<float>)||
        argType(set,0)!=arrayType)return false;
    RED4ext::DynArray<float> values;
    if(!RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(computed),get,&values)||
        !values.size||values.size>32)return false;
    float total=0;bool any=false;
    for(unsigned i=0;i<values.size;++i) {
        if(!std::isfinite(values[i]))return false;
        total+=values[i];any=any||values[i]!=0;
    }
    if(!std::isfinite(total))return false;
    lastDamageBefore.store(total);
    if(!any){damageAlreadyZero++;return true;}
    for(unsigned i=0;i<values.size;++i)values[i]=0;
    RED4ext::StackArgs_t args;args.emplace_back(arrayType,&values);
    if(!RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(computed),set,nullptr,args))return false;
    damageZeroed++;
    RED4ext::DynArray<float> after;
    if(!RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(computed),get,&after)||after.size!=values.size)return false;
    float sum=0;bool zero=true;
    for(unsigned i=0;i<after.size;++i){sum+=after[i];zero=zero&&after[i]==0;}
    lastDamageAfter.store(sum);
    if(zero)damageVerified++;
    return zero;
}
void protectDamage(Fn* fn,RED4ext::CScriptStack* stack) {
    damageCalls++;
    auto* event=eventArg(fn,stack,"gameDamageSystem","gameeventsHitEvent");
    if(!event){damageReject++;return;}
    auto target=weakProperty(event,"target","gameObject");
    if(!isObject(target.GetPtr(),"vehicleBaseObject"))return;
    damageVehicle++;
    auto* v=static_cast<RED4ext::vehicle::BaseObject*>(target.GetPtr());
    if(!eligible(v))return;
    auto computed=strongProperty(event,"attackComputed","gameAttackComputed");
    if(!(protectionFlags.load()&kineticle::Mechanical))return;
    if(zeroDamage(computed.GetPtr()))lastProtectedId.store(v->entityID.hash);
    else damageApiFail++;
}
bool voidNative(IScriptable* obj,const char* name) {
    auto* f=method(obj,name);
    return signature(f,0,false,true)&&!f->returnType&&
        RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(obj),f,nullptr);
}
H observeGrid(Fn* fn,RED4ext::CScriptStack* stack) {
    gridCalls++;
    auto* event=eventArg(fn,stack,"VehicleComponent","vehicleGridDestructionEvent");
    if(!event){gridDecodeFail++;return {};}
    auto vehicle=componentVehicle(stack->GetContext());
    if(!vehicle.GetPtr()){gridDecodeFail++;return {};}
    auto* v=static_cast<RED4ext::vehicle::BaseObject*>(vehicle.GetPtr());
    lastGridVehicle.store(v->entityID.hash);
    auto other=weakProperty(event,"otherVehicle","gameObject");
    if(isObject(other.GetPtr(),"vehicleBaseObject")) {
        gridPairs++;lastGridOther.store(static_cast<RED4ext::vehicle::BaseObject*>(other.GetPtr())->entityID.hash);
    } else lastGridOther.store(0);
    auto* rammed=namedProp<bool>(event,"rammedOtherVehicle","Bool");
    auto* wasRammed=namedProp<bool>(event,"otherVehicleRammed","Bool");
    auto* multiplier=namedProp<float>(event,"damageMultiplier","Float");
    if(rammed&&*rammed)gridRammed++;
    if(wasRammed&&*wasRammed)gridWasRammed++;
    if(multiplier&&std::isfinite(*multiplier))lastGridMultiplier.store(*multiplier);
    return vehicle;
}
bool protectDispatch(Fn* fn,RED4ext::CScriptStack* stack,void* a3) {
    if(fn->fullName==RED4ext::CName("DealDamages;gameHitEvent")) {
        if(protectionFlags.load()&kineticle::Mechanical)protectDamage(fn,stack);
        return originalScripted(fn,stack,a3);
    }
    auto vehicle=observeGrid(fn,stack);
    const bool result=originalScripted(fn,stack,a3);
    const auto flags=protectionFlags.load();
    if(result&&vehicle.GetPtr()&&(flags&(kineticle::Grid|kineticle::Glass))&&
        eligible(static_cast<RED4ext::vehicle::BaseObject*>(vehicle.GetPtr()))) {
        if(flags&kineticle::Grid){if(voidNative(vehicle.GetPtr(),"DestructionResetGrid"))gridResets++;else visualApiFail++;}
        if(flags&kineticle::Glass){if(voidNative(vehicle.GetPtr(),"DestructionResetGlass"))glassResets++;else visualApiFail++;}
    }
    return result;
}
void resetProtectionDiagnostics() {
    for(auto& n:scopeCounts)n.store(0);
    for(auto* n:{&damageCalls,&damageVehicle,&damageReject,&damageZeroed,&damageVerified,&damageAlreadyZero,
        &damageApiFail,&gridCalls,&gridDecodeFail,&gridResets,&glassResets,&visualApiFail,&gridPairs,&gridRammed,
        &gridWasRammed,&lastProtectedId,&lastGridVehicle,&lastGridOther})n->store(0);
    lastDamageBefore.store(0);lastDamageAfter.store(0);lastGridMultiplier.store(0);lastPsm.store(-1);
}
void writeProtectionDiagnostics(std::ostringstream& s) {
    s<<"; PROTECTION flags="<<protectionFlags.load()<<" damage_calls="<<damageCalls.load()
     <<" vehicle_hits="<<damageVehicle.load()<<" decode_failed="<<damageReject.load()<<" zeroed="<<damageZeroed.load()
     <<" verified_zero="<<damageVerified.load()<<" already_zero="<<damageAlreadyZero.load()<<" api_failed="<<damageApiFail.load()
     <<" last_vehicle="<<lastProtectedId.load()<<" last_before="<<lastDamageBefore.load()<<" last_after="<<lastDamageAfter.load()
     <<" last_psm="<<lastPsm.load();
    for(unsigned i=0;i<std::size(scopeNames);++i)s<<" scope_"<<scopeNames[i]<<"="<<scopeCounts[i].load();
    s<<"; GRID callbacks="<<gridCalls.load()<<" decode_failed="<<gridDecodeFail.load()<<" vehicle_pairs="<<gridPairs.load()
     <<" rammed="<<gridRammed.load()<<" was_rammed="<<gridWasRammed.load()<<" last_vehicle="<<lastGridVehicle.load()
     <<" last_other="<<lastGridOther.load()<<" last_damage_multiplier="<<lastGridMultiplier.load()
     <<" grid_resets="<<gridResets.load()<<" glass_resets="<<glassResets.load()<<" api_failed="<<visualApiFail.load();
}
