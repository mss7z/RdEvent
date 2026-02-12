/*

g++  UpVariantTester.cpp -pedantic-errors -std=c++20 -o prog -fno-omit-frame-pointer -g && \
 echo === && valgrind --leak-check=full  --show-leak-kinds=all ./prog
*/

#include <iostream>
#include <vector>
#include <cassert>

#define UPVARIANT_DEBUG
#include "UpVariant.hpp"

using namespace UpVariantNamespace;

#include <vector>


struct A{
    int i=18;
};
struct A2:public A{
    int b=12;
    std::vector<int> test;
};
struct C{
    int c=445;
};


int testUpVariant(){
    // VariantMod<int,float,double> val{456.0};
    // std::cout<<val.get<float>()<<std::endl;
    using Type=UpVariant<A,A2,C>;
    Type val{C{}};

    Type val2{A2{}};
    assert(val2.get<A2>().b==12);

    val.get<C>().c=43;
    val2.get<A2>().test.push_back(44687);

    Type val3=val2;
    val2=val;

    val2=C{887987};
    assert(val3.get<A2>().test[0]==44687);

    assert(val.get<C>().c==43);
    assert(val2.get<C>().c==887987);

    return 0;
}

void checkPtrVec(std::vector<int*>& ptrVec){
    for(int i=0;i<ptrVec.size();i++){
        if(*(ptrVec[i])!=i){
            dbg<<"error"<<endl;
        }
    }
}
void testMemoryPool(){
    UpVariantDebug::MemoryPool<int> memory{5};

    std::vector<int*> ptrVec;
    

    for(int i=0;;i++){
        int* valP=memory.newMem(i);
        ptrVec.push_back(valP);
        *valP=i;

        
        if(memory.getFree()==1){
            break;
        }
        checkPtrVec(ptrVec);
    }

    checkPtrVec(ptrVec);

    for(int i=0;i<1000;i++){
        int* valP=memory.newMem(769864);
        if(nullptr!=memory.newMem(0)){
            dbg<<"error alloc not null"<<endl;
        }
        if(0!=memory.getFree()){
            dbg<<"error free not 0"<<endl;
        }
        checkPtrVec(ptrVec);
        memory.freeMem(valP);
        checkPtrVec(ptrVec);
        if(1!=memory.getFree()){
            dbg<<"error free not 1"<<endl;
        }
    }

    for(auto valP: ptrVec){
        memory.freeMem(valP);
        checkPtrVec(ptrVec);
    }
}

void testEventElemGenerator(){
    EventHandleGenerator<A,A2,C> gen{10};
    std::cout<<"free: "<<gen.getFree()<<std::endl; 
    {
        EventHandle h=gen.genEvent(A2{187,89});

        std::cout<<"free: "<<gen.getFree()<<std::endl; 
        
        std::cout<<"val: "<<h.get<A>().i<<std::endl; 
    }

    std::cout<<"out of scoope free: "<<gen.getFree()<<std::endl; 

}

int main(){
    std::cout << "UpVariantTester"<<std::endl;

    testUpVariant();
    // testMemoryPool();
    // testEventElemGenerator();
    return 0;
}