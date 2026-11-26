#include "mass_policy.hpp"
#include "protection_policy.hpp"
#include "report_pages.hpp"
#include <cstdlib>
#include <cmath>
using namespace kineticle;
void check(bool v){if(!v)std::abort();}
int main(){
 check(validMultiplier(1)&&validMultiplier(25)&&validMultiplier(100)&&!validMultiplier(0)&&!validMultiplier(101));
 check(desiredChassisMass(1390,25)==34750.f);
 check(desiredChassisMass(1390,100)==100000.f);
 check(desiredChassisMass(8000,25)==100000.f&&desiredChassisMass(-1,5)==0);
 check(desiredChassisMass(NAN,5)==0);
 check(massScope(1,1,true,0,true,100000,5));
 check(!massScope(2,1,true,0,true,100000,5));
 check(!massScope(1,1,false,0,true,100000,5));
 check(!massScope(1,1,true,1,true,100000,5));
 check(!massScope(1,1,true,0,false,100000,5));
 check(!massScope(1,1,true,0,true,250001,5));
 check(!massScope(1,1,true,0,true,100000,1));
 check(validProtectionFlags(7)&&!validProtectionFlags(8));
 check(allowedScope(true,1,0,0,0,0,true,1,0,0,0));
 check(allowedScope(true,1,0,0,0,0,true,6,0,0,0));
 for(int i=-1;i<=7;++i)if(i!=1&&i!=6)check(!allowedScope(true,1,0,0,0,0,true,i,0,0,0));
 check(!allowedScope(true,0,0,0,0,0,true,1,0,0,0));
 check(!allowedScope(true,1,0,0,0,0,false,1,0,0,0));
 check(!allowedScope(true,1,1,0,0,0,true,1,0,0,0));
 check(!allowedScope(true,1,0,1,0,0,true,1,0,0,0));
 check(!allowedScope(true,1,0,0,1,0,true,1,0,0,0));
 check(!allowedScope(true,1,0,0,0,1,true,1,0,0,0));
 check(!allowedScope(true,1,0,0,0,0,true,1,1,0,0));
 check(!allowedScope(true,1,0,0,0,0,true,1,0,1,0));
 check(!allowedScope(true,1,0,0,0,0,true,1,0,0,1));
 check(!allowedScope(true,1,-1,0,0,0,true,1,0,0,0));
 check(!reportPages("test").empty());
}
