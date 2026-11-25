#include "physics.hpp"
#include "contact_trace.hpp"
#include <cstdlib>
#include <cmath>
#include <limits>
using namespace kineticle;
void check(bool x){ if(!x) std::abort(); }
bool eq(float a,float b){return std::abs(a-b)<0.0001f;}
int main(){
    Vec a{20,0,2}, b{2,3,-1};
    check(eq(forwardLoss(a,b),18));
    auto x=recover(a,b,true,1);
    check(eq(x.x,20)&&eq(x.y,3)&&eq(x.z,-1));
    check(eq(recover(a,b,true,.5f).x,11));
    check(eq(recover(a,b,false,1).x,2));
    check(eq(recover(a,b,true,0).x,2));
    check(eq(recover(a,{25,0,0},true,1).x,25));
    check(eq(recover(a,b,true,std::numeric_limits<float>::quiet_NaN()).x,2));
    ContactTrace trace;
    trace.push({1,1,90000,0,{},{},{},{},1,1});
    trace.push({2,2,95000,0,{},{},{},{},0,2});
    trace.push({3,3,99000,0,{},{},{},{},0,99});
    trace.event(1,2,100000,4,1);
    trace.push({1,5,110000,0,{},{},{},{},3,3});
    trace.push({2,6,260000,0,{},{},{},{},0,99});
    trace.push({2,7,120000,0,{},{},{},{},0,4});
    const auto& e=trace.events[0];
    check(e.driverPre.sequence==1&&e.targetPre.sequence==2);
    check(e.driverPost.sequence==5&&e.targetPost.sequence==7);
    trace.push({1,8,130000,0,{},{},{},{},1,1});
    check(e.driverPost.sequence==5);
    trace={};
    check(trace.events[0].driver==0);
}
