#include "physics.hpp"
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
}
