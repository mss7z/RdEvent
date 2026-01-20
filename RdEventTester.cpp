/*
コンパイル方法

 g++ -fsanitize=address RdEventTester.cpp -pedantic-errors -std=c++20 -o prog -fno-omit-frame-pointer -g &&  echo === && ASAN_OPTIONS=symbolize=1:detect_leaks=1 && ./prog


*/

#include "RdEvent.hpp"

#include <iostream>
#include <vector>
#include <functional>
#include <variant>
#include <set>
#include <any>
#include <map>
#include <string>



using EventElem=int;
using EventPreInfo=int;
using RdEvent=RdEventTemplate<EventElem,EventPreInfo>;


namespace RdEventTester{
    void exitError(const std::string &msg){
        std::cerr<<msg<<std::endl;
        exit(1);
    }

    template<typename AnsCode>
    concept AnsCodeTemplate=requires{
        AnsCode::OK;
        AnsCode::LOGIC_ERROR_OVERID;
    };

    template<AnsCodeTemplate AnsCode,typename Gila,typename Id>
    class RdTesterTemplate{
        public:
        using PatternCont=Id;
        using HistoryElem=const char*;
        using HistorySeq=std::vector<HistoryElem>;

        class TestElemInterface{
            public:
            virtual AnsCode procById(Id id,HistorySeq& histSeq,Gila &gila)=0;
            virtual PatternCont getCont()=0;
        };

        using CheckerType=std::function<AnsCode(Gila&)>;

        struct PatternDefine{
            HistoryElem hist="DEFAULT_STR";
            CheckerType checker=[](Gila& g){return AnsCode::OK;};
            TestElemInterface *child=nullptr;
        };

        using PatternDefineSeq=std::vector<PatternDefine>;

        class PatternHub:public TestElemInterface{
            private:
            PatternDefineSeq pattern;
            public:
            template<typename PatternDefineSeqX>
            PatternHub(PatternDefineSeqX &&p,TestElemInterface *child):
                pattern{std::forward<PatternDefineSeqX>(p)}{
                if(child!=nullptr){
                    for(PatternDefine &def:this->pattern){
                        if(def.child==nullptr){
                            def.child=child;
                        }
                    }
                }
            }

            template<typename PatternDefineSeqX>
            PatternHub(PatternDefineSeqX &&p):
                PatternHub{std::forward<PatternDefineSeqX>(p),nullptr}{
            }
            AnsCode procById(Id id,HistorySeq& histSeq,Gila &gila)override{
                for(PatternDefine def:this->pattern){
                    if(def.hist==nullptr){
                        exitError("detect def.hist==nullptr");
                    }
                    
                    if(def.child==nullptr){
                        if(id==0){
                            std::cout<<"48756 push0"<<static_cast<const void*>(def.hist)<<std::endl;
                            //自分の問題であるとき
                            histSeq.push_back(def.hist);
                            return def.checker(gila);
                        }
                        id-=1;
                    }else{
                        const PatternCont childCont=def.child->getCont();
                        if(id-childCont < 0){
                            //自分の問題であるとき
                            std::cout<<"48756 push"<<static_cast<const void*>(def.hist)<<std::endl;
                            histSeq.push_back(def.hist);
                            const AnsCode ret{ def.checker(gila) };
                            if(ret==AnsCode::OK){
                                return def.child->procById(id,histSeq,gila);
                            }
                            return ret;
                        }
                        id-=childCont;
                    }
                }
                return AnsCode::LOGIC_ERROR_OVERID;
            }
            PatternCont getCont()override{
                PatternCont cont{0};
                for(PatternDefine def:this->pattern){
                    if(def.child==nullptr){
                        cont+=1;
                    }else{
                        cont+=def.child->getCont();
                    }
                }
                return cont;
            }
        };
    };
    
    using Id=int;
    enum AnsCode{
        OK,
        ALRADY_EXIST,
        NOT_EXIST,
        INCORRECT_STACK_SIZE,
        INCORRECT_ARG_TYPE,
        LOGIC_ERROR_OVERID,
    };

    class CmdIdContext{
        public:
        using IndexType=size_t;
        private:
        Id id;
        std::vector<std::any> argStack;
        
        template<typename RetType,size_t... Index>
        auto getStackInternal(std::index_sequence<Index...>) const
            -> std::tuple<RetType,AnsCode>{
            auto& stack=this->argStack;
            RetType ret;
            bool isSuccess=true;
            ([&](){
                using TargetType=RemoveRef<decltype(std::get<Index>(ret))>::type;
                if(stack[Index].type() != typeid(TargetType)){
                    isSuccess=false;
                    return;
                }
                std::get<Index>(ret)=std::any_cast<TargetType>(stack[Index]);
            }(),...);
            if(isSuccess){
                return {ret,AnsCode::OK};
            }else{
                return {ret,AnsCode::INCORRECT_ARG_TYPE};
            }
        }
        
        public:
        void reset(Id id){
            this->id=id;
            this->argStack.clear();
        }
        template<typename X>
        void stackArg(X&&);

        const auto &refArgStack()const{
            return this->argStack;
        }
        const Id getId();

        
        template<typename... Types>
        auto getStack() const -> std::tuple<std::tuple<Types...>,AnsCode>{
            using RetType=std::tuple<Types...>;
            auto& stack=this->argStack;
            if(stack.size()!=std::tuple_size_v<RetType>){
                return {RetType{},AnsCode::INCORRECT_STACK_SIZE};
            }
            return getStackInternal<RetType>(std::index_sequence_for<Types...>{});
        }
    };
    template<typename X>
    void CmdIdContext::stackArg(X&& arg){
        this->argStack.emplace_back(std::forward<X>(arg));
    }
    const Id CmdIdContext::getId(){
        return this->id;
    }

    class SeqContext{
        private:
        std::set<Id> mustAppendId;
        public:
        void reset();
        void tryAppendMustAppendId(Id id);
        void tryDelMustAppendId(Id id);
        const auto &refMustAppendId(){
            return this->mustAppendId;
        }
    };
    void SeqContext::reset(){
        this->mustAppendId.clear();
    }
    void SeqContext::tryAppendMustAppendId(Id id){
        this->mustAppendId.insert(id);
    }
    void SeqContext::tryDelMustAppendId(Id id){
        this->mustAppendId.erase(id);
    }
    class Gila{
        private:
        public:
        CmdIdContext cmdIdCtx;
        SeqContext seqCtx;
        Gila(){}
    };
    

    
    using Tester=RdTesterTemplate<AnsCode,Gila,Id>;

    template<size_t prefixN,size_t afterN>
    constexpr auto genHistStr(const char (&prefix)[prefixN],int suffixVal){
        std::array<char,afterN> ret{};
        size_t i=0;
        for(;i<prefixN-1;i++){ //null文字 -1
            ret[i]=prefix[i];
        }
        int digit=1;
        for(int i=1;i<afterN-prefixN;i++){
            digit*=10;
        }
        suffixVal%=digit*10;
        for(;i<afterN-1;i++){
            ret[i]=suffixVal/digit+'0';
            suffixVal%=digit;
            digit/=10;
        }
        ret[afterN-1]='\0';
        return ret;
    }

    template<size_t seqN,size_t prefixN>
    constexpr auto genHistStrSeq(const char (&prefix)[prefixN]){
        constexpr int digitCont=3;
        std::array<std::array<char,prefixN+digitCont>,seqN> ret{};
        for(size_t i=0;i<seqN;i++){
            ret[i]=genHistStr<prefixN,prefixN+digitCont>(prefix,i);
        }
        return ret;
    }
    
    template <size_t N>
    struct HistStr{
        char data[N];
        constexpr HistStr(const char(&s)[N]) {
            for (size_t i=0; i<N; i++){
                data[i]=s[i];
            }
        }
    };

    class TestMemoryLayer{
        private:

        class NumGenDelPatternHub{
            private:
            // Tester::PatternDefineSeq hist;
            // Tester::PatternDefineSeq genDel;

            Tester::PatternHub genDelPatternHub;
            Tester::PatternHub numberPatternHub;
            
            
            public:
            template<typename X>
            NumGenDelPatternHub(X&& numberPattern,X&& genDelPattern):
                genDelPatternHub{genDelPattern},
                numberPatternHub{numberPattern,&(this->genDelPatternHub)}
                {}
            Tester::PatternHub &refPatternHub(){
                return this->numberPatternHub;
            }
        };
        
        template<size_t SeqLen,HistStr Prefix,typename TargetTypeA>
        class MultiObjectHistType:private NoMovable,NoCopyable{
            // メンバ変数をポインタで保持しているため
            public:
            using TargetType=TargetTypeA;
            private:
            const size_t cont;
            static constexpr auto histStrSeq=genHistStrSeq<SeqLen>(Prefix.data);
            std::vector<TargetType*> instanceList;
            inline static HistStr<SeqLen> prefixStatic=Prefix;

            NumGenDelPatternHub numGenDel;

            using IndexType=CmdIdContext::IndexType;

            std::tuple<IndexType,AnsCode> getIndex(Gila &g)const{
                const auto [value,ans]=g.cmdIdCtx.getStack<IndexType>();
                return {std::get<0>(value),ans};
            }
            Tester::PatternDefineSeq genNumberPattern()const{
                // 使用不能な番号も含む　クラス内部(生成ー削除)処理用
                Tester::PatternDefineSeq ret;
                ret.reserve(this->cont);
                for(size_t i=0;i < this->cont;i++){
                    std::cout<<"67967 hist"<< static_cast<const void*> (histStrSeq[i].data())<<std::endl;
                    ret.push_back({
                        .hist = histStrSeq[i].data(),
                        .checker = [=](Gila& g){
                            g.cmdIdCtx.stackArg(i);
                            return AnsCode::OK;
                        }
                    });
                }
                return ret;
            }
            Tester::PatternDefineSeq genGenDelPattern(){
                return {
                    Tester::PatternDefine{
                        .hist = "GEN",
                        .checker = [=,this](Gila& g)mutable{
                            const auto [targetIndex,ret]\
                                =this->getIndex(g);
                            if(ret!=AnsCode::OK){
                                return ret;
                            }

                            std::cout<<"005324 taretIndex"<<targetIndex<<std::endl;
                            if(this->instanceList[targetIndex]!=nullptr){
                                return AnsCode::ALRADY_EXIST;
                            }

                            //正しく木構造を設定できていれば、out of indexは発生しない
                            this->instanceList[targetIndex]=new TargetType{};
                            g.seqCtx.tryAppendMustAppendId(g.cmdIdCtx.getId()+1);
                            return AnsCode::OK;
                        }
                    },
                    Tester::PatternDefine{
                        .hist = "DEL",
                        .checker = [=,this](Gila &g)mutable{
                            std::cout<<"0011 DEL called"<<std::endl;

                            const auto [targetIndex,ret]\
                                =this->getIndex(g);
                            if(ret!=AnsCode::OK){
                                return ret;
                            }

                            if(this->instanceList[targetIndex]==nullptr){
                                return AnsCode::NOT_EXIST;
                            }
                            delete this->instanceList[targetIndex];
                            this->instanceList[targetIndex]=nullptr;
                            g.seqCtx.tryDelMustAppendId(g.cmdIdCtx.getId());
                            std::cout<<"0011 DEL success"<<std::endl;
                            return AnsCode::OK;
                        }
                    }
                };
            }
            const char *refPrefixStrC(){
                return prefixStatic.data;
            }
            
            public:
            MultiObjectHistType(const size_t conta):
                cont{conta>SeqLen? SeqLen: conta},
                instanceList{conta,nullptr},
                numGenDel{
                    genNumberPattern(),
                    genGenDelPattern()
                }{
                    std::cout<<"4548 histStrSeq0"<<histStrSeq[0].data()<<std::endl;
            }
            Tester::PatternHub &refPatternHub(){
                return this->numGenDel.refPatternHub();
            }
            Tester::PatternDefineSeq genUsableNumberPattern()const{
                Tester::PatternDefineSeq ret;
                ret.reserve(this->cont);
                for(size_t i=0;i < this->cont;i++){
                    std::cout<<"4687 hellowwww set hist "<<static_cast<const void*>(histStrSeq[i].data())<<std::endl;
                    ret.push_back({
                        .hist = histStrSeq[i].data(),
                        .checker = [=,this](Gila& g){
                            if(this->instanceList[i]==nullptr){
                                std::cout<<"4876 NOT_EXIST"<<std::endl;
                                return AnsCode::NOT_EXIST;
                            }
                            g.cmdIdCtx.stackArg(i);
                            std::cout<<"4876 OK"<<std::endl;
                            return AnsCode::OK;
                        }
                    });
                }
                return ret;
            }
            TargetType* refIndexP(IndexType i){
                return this->instanceList[i];
            }
        };

        public:
        TestMemoryLayer(const size_t ctrlCont,const size_t listenerCont,const size_t broadcasterCont):
            ctrl{ctrlCont},listener{listenerCont},broadcaster{broadcasterCont}{
                // std::cout<<ctrlHistStrSeq[20].data()<<std::endl;
                // std::cout<<genStr<6,9>("hello",9999).data()<<std::endl;
            }
        using CtrlHistType=MultiObjectHistType<30,"CTRL_",RdEvent::Ctrl>;
        CtrlHistType ctrl;

        using ListenerHistType=MultiObjectHistType<30,"LIST_",RdEvent::Listener>;
        ListenerHistType listener;

        using BroadcasterHistType=MultiObjectHistType<30,"BRAD_",RdEvent::Broadcaster>;
        BroadcasterHistType broadcaster;

        private:
        Tester::PatternHub total{
            Tester::PatternDefineSeq{
                Tester::PatternDefine{
                    .hist="genControl",
                    .child=&(this->ctrl.refPatternHub())
                },
                Tester::PatternDefine{
                    .hist="genListener",
                    .child=&(this->listener.refPatternHub())
                },
                Tester::PatternDefine{
                    .hist="genBroadcaster",
                    .child=&(this->broadcaster.refPatternHub())
                },
            }
        };

        public:
        auto &refPatternHub(){
            return this->total;
        }
    };

    class TestJoinLayer{
        private:
        using IndexType=CmdIdContext::IndexType;
        TestMemoryLayer &memoryLayer;

        template<typename FirstHist,typename SecondHist>
        class TwoCombine{
            public:
            using FirstType=FirstHist::TargetType;
            using SecondType=SecondHist::TargetType;
            using CallbackType=std::function<void(FirstType*,SecondType*,Gila&)>;
            private:
            CallbackType callbackFunc;
            FirstHist &firstHist;
            SecondHist &secondHist;
            
            Tester::PatternHub endHub{
                Tester::PatternDefineSeq{
                    {
                        .hist="twoCombineJoinNetwork",
                        .checker=[=,this](Gila& g)mutable{
                            auto [value,code]=g.cmdIdCtx.getStack<IndexType,IndexType>();
                            if(code!=AnsCode::OK){
                                return code;
                            }
                            auto [firstIndex,secondIndex]=value;
                            FirstType* firstP=firstHist.refIndexP(
                                firstIndex
                            );
                            SecondType* secondP=secondHist.refIndexP(
                                secondIndex
                            );
                            
                            this->callbackFunc(firstP,secondP,g);
                            return AnsCode::OK;
                        }
                    },
                },
            };
            Tester::PatternHub secondHub{
                this->secondHist.genUsableNumberPattern(),
                &(this->endHub)
            };
            Tester::PatternHub firstHub{
                this->firstHist.genUsableNumberPattern(),
                &(this->secondHub)
            };


            public:
            template<typename X>
            TwoCombine(FirstHist &firstHist,SecondHist &secondHist,X&& x):
                callbackFunc{std::forward<X>(x)},
                firstHist{firstHist},secondHist{secondHist}
                {}
            Tester::PatternHub &refPatternHub(){
                return this->firstHub;
            }
        };



        TwoCombine<TestMemoryLayer::BroadcasterHistType,TestMemoryLayer::CtrlHistType>
            broadcasterJoinNetwork{
                this->memoryLayer.broadcaster,
                this->memoryLayer.ctrl,
                [](auto* bp,auto* cp,Gila& g){
                    cp->addBroadcaster(*bp);
                }
            };
        TwoCombine<TestMemoryLayer::ListenerHistType,TestMemoryLayer::CtrlHistType>
            listenerJoinNetwork{
                this->memoryLayer.listener,
                this->memoryLayer.ctrl,
                [](auto* lp,auto* cp,Gila& g){
                    cp->addListener(*lp);
                }
            };
        

        
        Tester::PatternHub total{
            Tester::PatternDefineSeq{
                {
                    .hist="broadcasterJoinNetwork_top",
                    .checker=[](Gila& g){
                        std::cout<<"4875 hellowwww"<<std::endl;
                        return AnsCode::OK;
                    },
                    .child=&(broadcasterJoinNetwork.refPatternHub()),
                },
                {
                    .hist="listenerJoinNetwork_top",
                    .checker=[](Gila& g){
                        return AnsCode::OK;
                    },
                    .child=&(listenerJoinNetwork.refPatternHub()),
                },
            }
        };

        public:
        TestJoinLayer(TestMemoryLayer& memoryLayerA):
            memoryLayer{memoryLayerA}{}
        auto &refPatternHub(){
            return this->total;
        }
    };

    class TestEventLayer{
        private:
        using IndexType=CmdIdContext::IndexType;
        TestMemoryLayer &memoryLayer;
        template<typename T>
        using ListType=std::map<const char *,T>;

        ListType<EventElem> eventList{
            {"EventElem0",0},{"EventElem1",1}
        };
        ListType<EventPreInfo> preInfoList{
            {"preInfoList0",0},{"preInfoList1",1}
        };
        //

        // template<typename T>
        // Tester::CheckerType genChecker(const T &val){
        //     return [val](Gila &g){
        //         g.cmdIdCtx.stackArg(val);
        //         return AnsCode::OK;
        //     }
        // }
        template<typename T>
        Tester::PatternDefineSeq genEventPattern(
            const ListType<T> &list,std::function<Tester::CheckerType(const T&)> genChecker
        ){
            Tester::PatternDefineSeq ret;
            ret.reserve(list.size());
            for(const auto& [key, val]: list){
                ret.push_back({
                    .hist = key,
                    .checker = genChecker(val)
                });
            }
            return ret;
        }
        
        

        Tester::PatternDefineSeq eventPatternDefineSeq;
        // Tester::PatternDefineSeq eventPatternDefineSeq=genEventPattern(eventList);
        // Tester::PatternDefineSeq preInfoPatternDefineSeq=genEventPattern(preInfoList);

        Tester::PatternHub eventElemNumHub{
            this->eventPatternDefineSeq
        };
        Tester::PatternHub total{
            this->memoryLayer.broadcaster.genUsableNumberPattern(),
            &(this->eventElemNumHub)
        };


        std::tuple<RdEvent::Broadcaster*,AnsCode> getBroadcasterP(Gila &g){
            auto [value,code]=g.cmdIdCtx.getStack<IndexType>();
            if(code!=AnsCode::OK){
                return {nullptr,code};
            }
            auto [broadcasterIdx]=value;
            RdEvent::Broadcaster* bp= memoryLayer.broadcaster.refIndexP(broadcasterIdx);
            return {bp,AnsCode::OK};
        }

        public:
        TestEventLayer(TestMemoryLayer& memoryLayerA):
            memoryLayer{memoryLayerA},
            eventPatternDefineSeq{
                genEventPattern<EventElem>(
                    eventList,[this](const EventElem &val){
                    return [val,this](Gila &g){
                        auto [bp,code]=getBroadcasterP(g);
                        bp->broadcast(val);
                        return AnsCode::OK;
                    };
                })
            }
        {
            auto preInfoPatternDefineSeq=genEventPattern<EventPreInfo>(
                preInfoList,[this](const EventElem &val){
                    return [val,this](Gila &g){
                        auto [bp,code]=getBroadcasterP(g);
                        bp->setEventPreInfo(val);
                        return AnsCode::OK;
                    };
                });
            eventPatternDefineSeq.reserve(eventPatternDefineSeq.size()+preInfoPatternDefineSeq.size());
            std::move(preInfoPatternDefineSeq.begin(),preInfoPatternDefineSeq.end(),std::back_inserter(eventPatternDefineSeq));
        }
        auto &refPatternHub(){
            return this->total;
        }
    };

    class TestCtrlLayer{
        private:
        TestMemoryLayer &memoryLayer;
        TestJoinLayer joinLayer;
        TestEventLayer eventLayer;

        Tester::PatternHub total{
            Tester::PatternDefineSeq{
                {
                    .hist="generator",
                    .child=&(this->memoryLayer.refPatternHub()),
                },
                {
                    .hist="join",
                    .child=&(this->joinLayer.refPatternHub()),
                },
                {
                    .hist="event",
                    .child=&(this->eventLayer.refPatternHub()),
                },
                
            }
        };

        public:
        TestCtrlLayer(TestMemoryLayer& memoryLayer):
            memoryLayer{memoryLayer},
            joinLayer{memoryLayer},
            eventLayer{memoryLayer}
            {}
        auto &refPatternHub(){
            return this->total;
        }
        
    };

    class StoryId{
        private:
        using SeqType=std::vector<Id>;
        SeqType idSeq;
        
        public:
        template<class... Args>
            requires std::is_constructible_v<SeqType, Args&&...>
        explicit StoryId(Args&&... args):
            idSeq{std::forward<Args>(args)...}
        {}
        auto& ref()const{
            return idSeq;
        }
        void add(Id id){
            idSeq.push_back(id);
        }
        void del(){
            idSeq.pop_back();
        }
    };
    std::ostream& operator<<(std::ostream& os, const StoryId& storyId){
        // using namespace RdEventTester;
        os<<"StoryId: ";
        for(const Id id: storyId.ref()){
            os<<id<<", ";
        }
        return os;
    }
    void cmdIdTest(Tester::PatternHub& patternHub,const Id id,Gila &gila){
        gila.cmdIdCtx.reset(id);
        Tester::HistorySeq histSeq;
        patternHub.procById(id,histSeq,gila);
        std::cout<<"!! histroySeq ("<<id<<"): ";
        for(Tester::HistoryElem elem: histSeq){
            std::cout<<(elem)<<", ";
        }
        std::cout<<std::endl;
    }
    void storyTest(const StoryId &storyId, Tester::PatternHub& patternHub){
        Gila gila;
        std::cout<<"\n!! storyTest "<<storyId<<"\n";
        for(const Id id: storyId.ref()){
            cmdIdTest(patternHub,id,gila);
        }
        for(const Id id:gila.seqCtx.refMustAppendId()){
            cmdIdTest(patternHub,id,gila);
        }
    }

    void recursiveTest(const std::size_t depth, StoryId &storyId, Tester::PatternHub& patternHub){
        if(depth==0){
            storyTest(storyId,patternHub);
            return;
        }
        for(Id id=0; id<patternHub.getCont(); ++id){
            storyId.add(id);
            recursiveTest(depth-1,storyId,patternHub);
            storyId.del();
        }
        // std::cout<<"4
    }
    
    void testerMain(){
        Gila gila;
        TestMemoryLayer memory{2,2,2};
        TestCtrlLayer test{memory};
        std::cout<<"total cont:"<<test.refPatternHub().getCont()<<std::endl;

        StoryId storyId{};
        recursiveTest(2,storyId,test.refPatternHub());
        return;

        for(Id i=0;i<test.refPatternHub().getCont();i++){
            gila.cmdIdCtx.reset(i);
            Tester::HistorySeq histSeq;
            // std::cout<<"4868 histSeq"<<&histSeq<<std::endl;
            std::cout<<"4877 ID"<<i<<std::endl;
            std::cout<<"4681 histSeq before size="<<histSeq.size()<<": ";
            AnsCode ans= test.refPatternHub().procById(i,histSeq,gila);
            std::cout<<"4682 histSeq ans"<<ans<<" size="<<histSeq.size()<<": ";
            for(Tester::HistoryElem elem: histSeq){
                std::cout<<(elem)<<", ";
            }
            std::cout<<std::endl;
        }

        std::vector<Id> testPattern{0,8,12};
        for(const Id i:testPattern){
            gila.cmdIdCtx.reset(i);
            Tester::HistorySeq histSeq;
            std::cout<<"4877 ID"<<i<<std::endl;
            AnsCode ans= test.refPatternHub().procById(i,histSeq,gila);
            std::cout<<"4682 histSeq ans"<<ans<<" size="<<histSeq.size()<<": ";
            for(Tester::HistoryElem elem: histSeq){
                std::cout<<(elem)<<", ";
                // std::cout<<static_cast<const void*>(elem)<<", ";
            }
            std::cout<<std::endl;
        }

        for(const Id i:gila.seqCtx.refMustAppendId()){
            gila.cmdIdCtx.reset(i);
            Tester::HistorySeq histSeq;
            std::cout<<"4877 ID"<<i<<std::endl;
            AnsCode ans= test.refPatternHub().procById(i,histSeq,gila);
            std::cout<<"4682 histSeq ans"<<ans<<" size="<<histSeq.size()<<": ";
            for(Tester::HistoryElem elem: histSeq){
                std::cout<<(elem)<<", ";
                // std::cout<<static_cast<const void*>(elem)<<", ";
            }
            std::cout<<std::endl;
        }

        return;
        
    }
};



int main(){
    RdEventTester::testerMain();

    return 0;
    
}