/* g++ -fsanitize=address  RdEvent_Cpp20_v4.cpp -pedantic-errors -std=c++20 -o prog -fno-omit-frame-pointer -g && \
 echo === && ASAN_OPTIONS=symbolize=1:detect_leaks=1 && ./prog


 g++  RdEvent_Cpp20_v4.cpp -pedantic-errors -std=c++20 -o prog -fno-omit-frame-pointer -g && \
 echo === && valgrind --leak-check=full  --show-leak-kinds=all ./prog
*/


#include <iostream>
#include <list>
#include <deque>

enum RdEventError{
    OK = 0,
    ALRADY_IN_LIST,
    ALRADY_JOINED,
    ALRADY_UNJOINED,
    ALRADY_SET,
    BAD_ALLOC,
    NOT_JOINED,
};

namespace{

template<int N>
struct ListElemCoreTemplate{
    ListElemCoreTemplate<N-1> list;
    ListElemCoreTemplate<N> *nextP=nullptr;
    ListElemCoreTemplate<N> *preP=nullptr;

    static constexpr int depth = N;
};

template<>
struct ListElemCoreTemplate<0>{
    ListElemCoreTemplate<0> *nextP=nullptr;
    ListElemCoreTemplate<0> *preP=nullptr;

    static constexpr int depth = 0;
};

template<int N>
using ListElem=ListElemCoreTemplate<N>;

//layerの定義
constexpr int L0=0;
constexpr int L1=1;


template<typename After,typename Before=After>
struct ListElemAdvTemplate{
    static void detachFromList(Before &ra){
        auto r = reinterpret_cast<After&>(ra);
        if(r.nextP != nullptr){
            std::cout<<"45648 detach bef r"<<&ra<<
                " r.preP"<<r.preP<<" r.nextP"<<r.nextP<<
                " r.nextP->preP"<<r.nextP->preP<<" r.preP->nextP"<<r.preP->nextP<<std::endl;
            r.nextP->preP = r.preP;
            r.preP->nextP = r.nextP;
            r.nextP = nullptr;
            r.preP = nullptr;
        }
    }
};

//Layer Mを Layer Nとして扱う
template<int N,int M=N>
using ListElemAdv=ListElemAdvTemplate<ListElemCoreTemplate<N>,ListElemCoreTemplate<M>>;


template<typename Type>
struct RemoveRef{
    using type = Type;
};
template<typename Type>
struct RemoveRef<Type&>{
    using type = Type;
};

class NoCopyable{
    protected:
    NoCopyable() = default;
    ~NoCopyable() = default;

    NoCopyable(const NoCopyable&) = delete;
    NoCopyable& operator=(const NoCopyable&) = delete;
};

class NoMovable{
    protected:
    NoMovable() = default;
    ~NoMovable() = default;

    NoMovable(const NoMovable&&) = delete;
    NoMovable& operator=(const NoMovable&&) = delete;
};

};//namespace


template<typename EventElem,typename EventPreInfo>
class RdEventTemplate{
    public:
    using Error=RdEventError;

    private:
    class ListenerCore;
    class BroadcasterCore;

    
    template<typename ElemType,typename HandleType>
    class ListIterable: private NoCopyable, private NoMovable{
        private:
        using HandleRawType = RemoveRef<HandleType>::type;
        template<typename ElemClass, typename MemberType>
        static constexpr MemberType getMemberType(MemberType ElemClass::*list);
        static constexpr bool isUsable(){
            using ListType = decltype( getMemberType(&ElemType::list) );
            return std::is_standard_layout_v<ElemType> && 
                // std::is_same_v< ListType,ListElemTemplate<ListType::depth> > && 
                ListType::depth >= HandleRawType::depth &&
                offsetof(ElemType,list)==0;
        }
        static_assert(isUsable());

        public:
        class Iter{
            friend ListIterable;
            private:
            HandleRawType *target;
            Iter(HandleRawType *t): target{t}{}

            public:
            ElemType *getPtr()const noexcept{
                //ここで強制的にダウンキャストする仕様にすることで1elemあたりsizeof(*Type)Byteのメモリ削減と場合分け処理をなくすことができる
                return reinterpret_cast<ElemType*>(target);
            }
            ElemType& operator*()noexcept{ 
                //nullptrの時、stdの仕様に倣い未定義動作
                return *reinterpret_cast<ElemType*>(target);
            }
            Iter& operator++()noexcept{
                target=target->nextP;
                return *this;
            }
            Iter operator++(int)noexcept{
                Iter temp{*this};
                ++(*this);
                return temp;
            }
            bool operator!=(const Iter& other)const noexcept{
                return reinterpret_cast<HandleRawType*>(other.getPtr()) != this->target;
            }
        };

        protected:
        ListIterable(){}
        template<typename T>
        static Iter genIter(T&& t){
            return Iter{ std::forward<T>(t) };
        }
        static HandleRawType *convToHandle(ElemType *p){
            return reinterpret_cast<HandleRawType*>(p);
        }
        static ElemType *convToElem(HandleRawType *p){
            return reinterpret_cast<ElemType*>(p);
        }
    };

    template<typename ElemType,typename HandleType>
    class ListReader:public ListIterable<ElemType,HandleType>{
        private:
        HandleType *firstElem;

        public:
        ListReader(ElemType *firstElem):
            firstElem{ this->convToHandle(firstElem) }{}

        auto begin()noexcept{
            return this->genIter(firstElem);
        }
        auto end()noexcept{
            return this->genIter(firstElem->preP);
        }
    };

    template<typename ElemType,typename HandleType>
    class ListHandler:public ListIterable<ElemType,HandleType>{
        private:
        HandleType handle;

        public:
        using Iter=ListIterable<ElemType,HandleType>::Iter;
        ListHandler():
            handle{.nextP=&(this->handle), .preP=&(this->handle)}
        {}
        ListHandler(HandleType handlea):
            handle{handlea}
        {
            std::cout<<"6877 selfSize"<<sizeof(*this)<<" this" <<this<<std::endl;
            std::cout<<"6877 &(this->handle)"<<&(this->handle)<<"handlea"<<&handlea<<std::endl;
            handle.nextP=&(this->handle);
            handle.preP=&(this->handle);
            std::cout<<"6878 handle,nextP"<<handle.nextP<<"@"<<&(handle.nextP)<<std::endl;
            std::cout<<"6878 handle,preP"<<handle.preP<<"@"<<&(handle.preP)<<std::endl;
        }
        Error append(ElemType *p){
            auto p2=this->convToHandle(p);
            if(p2->nextP != nullptr){
                return Error::ALRADY_IN_LIST;
            }

            std::cout<<"p "<<p<<std::endl;

            std::cout<<"handle pre "<<handle.preP<<" next "<<handle.nextP<<std::endl;
            
            
            // 前の人に次が自分であることを設定する
            // つまり、最後尾のelemへpが次であることを設定する
            this->handle.preP->nextP=p2;
            p2->nextP=&(this->handle);

            // 新たな最後尾であるpの前は、Handleが持つ最後尾を示すポインタである
            p2->preP=this->handle.preP;
            // 末尾はもちろんp
            this->handle.preP=p2;


            std::cout<<"handle pre"<<handle.preP<<" next "<<handle.nextP<<std::endl;

            return Error::OK;
        }
        
        auto begin()noexcept{
            return this->genIter(handle.nextP);
        }
        auto end()noexcept{
            return this->genIter(&handle);
        }
        bool isEmpty()const noexcept{
            std::cout<<"12324 list handler isEmpty called ret"<<(this->handle.nextP == &(this->handle))<<std::endl;
            std::cout<<"12324 list handler isEmpty called handleNext"<<(this->handle.nextP)<<" handleSelf"<<&(this->handle)<<std::endl;
            return this->handle.nextP == &(this->handle);
        }
        ElemType *getFirst()const noexcept{
            if(this->isEmpty()){
                return nullptr;
            }
            return this->convToElem(this->handle.nextP);
        }
    };
    template<typename ElemType,typename HandleType>
    class ListHandleDynamic:public ListHandler<ElemType,HandleType>{
        private:
        Error lastError=Error::OK;
        int64_t newCount=0;

        ElemType *postAppendObj(ElemType *p)noexcept{
            
            if(p == nullptr){
                this->lastError=Error::BAD_ALLOC;
                return nullptr;
            }
            this->lastError= this->append(p);
            ++newCount;
            std::cout<<"4848 appendObj called append"<<p<<" count"<<newCount<<" @"<<this<<std::endl;
            return p;
        }

        public:
        ListHandleDynamic(){
            std::cout<<"5557 new Instance"<<" count"<<newCount<<" @"<<this<<std::endl;
        }
        ListHandleDynamic(HandleType handle):ListHandler<ElemType,HandleType>{handle}{}

        template<typename T>
        ElemType *appendObj(T &&x)noexcept{
            ElemType *p=new(std::nothrow) ElemType{ std::forward<ElemType>(x) };
            return this->postAppendObj(p);
        }
        ElemType *appendObj()noexcept{
            ElemType *p=new(std::nothrow) ElemType{ };
            return this->postAppendObj(p);
        }


        Error getLastError()const noexcept{
            return this->lastError;
        }
        void deleteObj(ElemType *p){
            std::cout<<"8874 deleteObj called del"<<p<<" count"<<newCount<<" @"<<this<<std::endl;
            --newCount;
            delete p;
        }

        ~ListHandleDynamic(){
            std::cout<<"3425 ListHandleDynamic delete newCount"<<newCount<<" @"<<this<<std::endl;
        }
    };

    template<int N,typename Type>
    struct ListElemInChain{
        ListElem<N> list;
        Type *mother = nullptr;
    };

    using ListenerChainElem = ListElemInChain<1,ListenerCore>;
    struct BroadcasterMyListener{
        ListElem<L0> list;
        ListHandler<ListenerChainElem,ListElem<L0>> myListener;
    };

    enum IntervalEventType{
        NORMAL_EVENT,
        DEL_BROADCASTER,
    };

    struct InternalEvent{
        ListElem<L0> list;
        IntervalEventType type;
        BroadcasterMyListener *listener;
        EventElem event;
    };

    template<typename Type>
    class EventQueueTemplate{
        private:
        ListHandleDynamic<Type,ListElem<L0>> que;
        public:
        EventQueueTemplate(){
            std::cout<<"4679 EventQueueTemplatte que addr"<<&que<<std::endl;
            que.isEmpty();
        }
        template<typename T>
        Type *push(T &&x)noexcept{
            return this->que.appendObj( std::forward<T>(x) );
        }
        Error getLastPushError()const noexcept{
            return this->que.getLastError();
        }
        bool isEmpty()const noexcept{
            return this->que.isEmpty();
        }
        Type &refNextPop(){
            Type *firstP=this->que.getFirst();
            std::cout<<"37942 getFirst "<<firstP<<std::endl;
            return *(this->que.getFirst());
        }
        void popNoRet()noexcept{
            Type *targetP = this->que.getFirst();
            ListElemAdv<L0>::detachFromList(targetP->list);
            this->que.deleteObj(targetP);
        }
    };

    

    class Gila{
        private:
        ListHandler<ListenerCore,ListElem<L0>> inUseListener;
        ListHandler<BroadcasterCore,ListElem<L0>> inUseBroadcaster;
        ListHandleDynamic<BroadcasterMyListener,ListElem<L0>> broadcasterMyListener;
        EventQueueTemplate<InternalEvent> eventQue;

        public:
        Gila(){
            std::cout<<"2342 broadcasterMyListener addr"<<&broadcasterMyListener<<std::endl;

            // inUseListener.isEmpty();
            // inUseBroadcaster.isEmpty();
        }
        Error addListener(ListenerCore* listener){
            return this->inUseListener.append(listener);
        }
        ListHandler<ListenerCore,ListElem<L0>> &refListenerList(){
            return inUseListener;
        }
        Error addBroadcaster(BroadcasterCore* broadcaster){
            return this->inUseBroadcaster.append(broadcaster);
        }
        ListHandler<BroadcasterCore,ListElem<L0>> &refBroadcasterList(){
            return inUseBroadcaster;
        }
        EventQueueTemplate<InternalEvent> &refEventQue(){
            return eventQue;
        }
        auto &refBroadcasterMyListener(){
            return broadcasterMyListener;
        }
        void printListener(){
            for(const ListenerCore &lis : inUseListener){
                std::cout<<"listener"<<std::endl;
            }
        }
        void disbandNetwork(){
            {
                const auto end=inUseListener.end();
                for(
                    auto it=inUseListener.begin();
                    end!=it;
                ){
                    (*it++).unjoinNetwork();
                }
            }
            {
                const auto end=inUseBroadcaster.end();
                for(
                    auto it=inUseBroadcaster.begin();
                    end!=it;
                ){
                    (*it++).unjoinNetwork();
                }
            }
            
        }
        ~Gila(){
            std::cout<<"3234 Gila delete"<<std::endl;
            std::cout<<"3234 eventQue isEmpty="<<eventQue.isEmpty()<<std::endl;
        }
    };
    struct GilaFunc{
        static Error checkPeer(BroadcasterCore &b,ListenerCore &l){
            std::cout<<"88741 peer called"<<std::endl;
            if(l.isWantEvent(b.getEventPreInfo())){
                std::cout<<"88742 peer know!"<<std::endl;
                ListenerChainElem *p;
                if( p=l.getChainElem() ){
                    return b.addMyListener(p);
                }
                std::cout<<"55484 error"<<std::endl;
                return l.getLastChainElemError();
            }
            return Error::OK;
        }
    };
    

    

    // iteratorが無効化されないコンテナである必要がある
    using ListenerChain = ListHandleDynamic<ListenerChainElem,ListElem<L1>&>;

    public:
    class Ctrl;
    class ListenerInterface{
        public:
        virtual void procEvent(EventElem) = 0;
        virtual bool isWantEvent(EventPreInfo) = 0;
    };

    private:
    class ListenerCore final{
        friend ListIterable<ListenerCore,ListElem<L0>>;
        friend BroadcasterCore;
        friend GilaFunc;
        friend Ctrl;

        private:
        ListElem<L1> list;
        Gila *gila=nullptr;
        // std_layoutである必要がある
        ListenerChain *chainP;
        ListenerInterface *procer=nullptr;

        bool isStarted() const noexcept{
            return this->gila!=nullptr && this->procer!=nullptr;
        }

        Error tryStart(){
            if(!this->isStarted()){
                return Error::OK;
            }
            Error ret;
            if(ret = this->gila->addListener(this)){
                return ret;
            }
            for(BroadcasterCore &b: this->gila->refBroadcasterList()){
                if(ret = GilaFunc::checkPeer(b,*this)){
                    return ret;
                }
            }
            return Error::OK;
        }
        bool isWantEvent(EventPreInfo info)noexcept{
            return this->procer->isWantEvent(info);
        }

        Error getLastChainElemError()const noexcept{
            return this->chainP->getLastError();
        }
        ListenerChainElem *getChainElem()noexcept{
            // return &(this->chainP->emplace_back(ListenerChainElem{.mother=this}));
            return this->chainP->appendObj(ListenerChainElem{ .mother = this });
        }
        Error unjoinNetworkUnsafe(){
            ListElemAdv<L0,L1>::detachFromList(this->list);
            const auto end=chainP->end();
            for(
                auto it=chainP->begin();
                end!=it;
            ){
                //横方向のLayer0を削除
                ListElemAdv<0>::detachFromList((*it).list.list);
                this->deleteChainElem( &* it++ );
            }
            return Error::OK;
        }
        void deleteChainElem(ListenerChainElem *p){
            chainP->deleteObj(p);
        }
        void unjoinChainElem(ListenerChainElem *p){
            ListElemAdv<L0>::detachFromList(p->list.list);
            ListElemAdv<L1>::detachFromList(p->list);
            chainP->deleteObj(p);
        }
        void callProcer(EventElem ev){
            this->procer->procEvent(ev);
        }

        public:
        ListenerCore(ListenerChain *const chainP):chainP{chainP}{
            std::cout<<"5547 ListenerCore chainP addr"<<this->chainP<<std::endl;
        }

        Error setListener(ListenerInterface *procer){
            if(this->procer != nullptr){
                return Error::ALRADY_SET;
            }
            this->procer = procer;
            return this->tryStart();
        }

        Error joinNetwork(Gila *gila){
            if(gila == nullptr){
                return this->unjoinNetwork();
            }
            if(this->gila != nullptr){
                return Error::ALRADY_JOINED;
            }
            this->gila = gila;
            return this->tryStart();
        }
        Error unjoinNetwork(){
            if(!this->isStarted()){
                return Error::ALRADY_UNJOINED;
            }
            const Error ret=this->unjoinNetworkUnsafe();
            this->gila = nullptr;
            return ret;
        }
        ~ListenerCore(){
            std::cout<<"62398 listener delete starrt"<<std::endl;
            this->unjoinNetwork();

            std::cout<<"62398 listener delete OK"<<std::endl;
            //ネットワークからの分離作業
        }
    };

    public:
    class Listener{
        private:
        //この順番である必要あり（初期化順が重要）
        ListenerCore core;
        ListenerChain chain;

        public:
        Listener():
            core{&chain},
            chain{*(reinterpret_cast<ListElem<L1>*>(&core))}
        {
            std::cout<<"4882 core ptr"<<&core<<std::endl;
        }
        Error setListener(ListenerInterface *procer){
            return core.setListener(procer);
        }
        Error joinNetwork(Gila *gila){
            return core.joinNetwork(gila);
        }
    };

    private:
    
    class BroadcasterCore final{
        friend ListIterable<BroadcasterCore,ListElem<L0>>;
        friend ListenerCore;
        friend GilaFunc;

        private:
        ListElem<L0> list;
        Gila *gila=nullptr;
        EventPreInfo preInfo;
        bool isSetPreInfo=false;
        BroadcasterMyListener *myListener=nullptr;
        // ListHandler<ListenerChainElem,ListElem<L0>> myListener;

        bool isStarted() const noexcept{
            return this->gila!=nullptr && isSetPreInfo;
        }

        Error addMyListener(ListenerChainElem *p){
            std::cout<<"874115 addMyListener Called"<<std::endl;
            return this->myListener->myListener.append(p);
        }
        Error tryStart(){
            if(!this->isStarted()){
                return Error::OK;
            }
            Error ret;
            if(ret = this->gila->addBroadcaster(this)){
                return ret;
            }

            decltype(this->myListener) p;
            if(nullptr == (p = this->gila->refBroadcasterMyListener().appendObj())){
                return this->gila->refBroadcasterMyListener().getLastError();
            }
            this->myListener = p;
            
            for(ListenerCore &l: this->gila->refListenerList()){
                if(ret = GilaFunc::checkPeer(*this,l)){
                    return ret;
                }
            }
            return Error::OK;
        }
        Error unjoinNetworkUnsafe(){
            ListElemAdv<L0>::detachFromList(this->list);
            
            // if(myListener->myListener.isEmpty()){
            //     return Error::OK;
            // }
            std::cout<<"796732 stone"<<std::endl;
            this->gila->refEventQue().push(InternalEvent{
                .type = DEL_BROADCASTER,
                .listener = this->myListener
            });
            return this->gila->refEventQue().getLastPushError();
        }

        public:

        Error joinNetwork(Gila *gila){
            if(gila == nullptr){
                return this->unjoinNetwork();
            }
            if(this->gila != nullptr){
                return Error::ALRADY_JOINED;
            }
            this->gila = gila;
            return this->tryStart();
        }
        Error setEventPreInfo(EventPreInfo preInfo){
            if(this->isSetPreInfo){
                return Error::ALRADY_SET;
            }
            this->preInfo = preInfo;
            this->isSetPreInfo = true;
            // preInfo再設定時の再検索
            return this->tryStart();

        }
        EventPreInfo getEventPreInfo(){
            return this->preInfo;
        }
        Error broadcast(EventElem event){
            if(!this->isStarted()){
                return Error::NOT_JOINED;
            }
            if(myListener->myListener.isEmpty()){
                // 聞き手がいないときにErrorとするかは、仕様次第
                return Error::OK;
            }
            this->gila->refEventQue().push(InternalEvent{
                .type = NORMAL_EVENT,
                .listener = this->myListener,
                .event = event
            });
            return this->gila->refEventQue().getLastPushError();
        }
        Error unjoinNetwork(){
            if(!this->isStarted()){
                return Error::ALRADY_UNJOINED;
            }
            const Error ret=this->unjoinNetworkUnsafe();
            this->gila = nullptr;
            return ret;
        }
        ~BroadcasterCore(){
            std::cout<<"3462 broadcaster delete start"<<std::endl;
            this->unjoinNetwork();

            std::cout<<"3462 broadcaster delete OK"<<std::endl;
            //ネットワークからの分離作業
        }
    };

    public:
    class Broadcaster{
        private:
        BroadcasterCore core;

        public:
        Error joinNetwork(Gila *gila){
            return core.joinNetwork(gila);
        }
        Error setEventPreInfo(EventPreInfo preInfo){
            return core.setEventPreInfo(preInfo);
        }
        Error broadcast(EventElem event){
            return core.broadcast(event);
        }

    };

    class Ctrl{
        private:
        Gila gila;

        public:
        Error addListener(Listener &listener){
            return listener.joinNetwork(&this->gila);
        }
        Error addBroadcaster(Broadcaster &broadcaster){
            return broadcaster.joinNetwork(&this->gila);
        }
        void printListener(){
            this->gila.printListener();
        }
        Error procEvent(){
            std::cout<<"12341 called procEvent"<<std::endl;
            auto& eventQue=this->gila.refEventQue();
            if(eventQue.isEmpty()){
                std::cout<<"97723 isEmpty"<<std::endl;
                return Error::OK;
            }
            const InternalEvent &intEv = eventQue.refNextPop();
            std::cout<<"321234 stone"<<std::endl;

            if(intEv.type == IntervalEventType::NORMAL_EVENT){
                std::cout<<"4203 NORMAL_EVENT"<<std::endl;
                auto &reader = intEv.listener->myListener;
                for(const ListenerChainElem &chainElem: reader){
                    chainElem.mother->callProcer(intEv.event);
                }
            }else if(intEv.type == IntervalEventType::DEL_BROADCASTER){
                std::cout<<"4203 DEL_BROADCASTER"<<std::endl;
                auto &reader = intEv.listener->myListener;
                const auto end=reader.end();
                for(
                    auto it=reader.begin();
                    end!=it;
                ){
                    (*it).mother->unjoinChainElem( &* it++ );
                }
                this->gila.refBroadcasterMyListener().deleteObj(intEv.listener);
            }
            std::cout<<"46878 stone"<<std::endl;
            eventQue.popNoRet();
            return Error::OK;
        }
        ~Ctrl(){
            std::cout<<"4665 ctrl delete start"<<std::endl;

            this->gila.disbandNetwork();
            while(!this->gila.refEventQue().isEmpty()){
                this->procEvent();
            }

            std::cout<<"46868 ctrl delete OK"<<std::endl;
        }
    };
};



using EventElem=int;
using EventPreInfo=int;
using RdEvent=RdEventTemplate<EventElem,EventPreInfo>;

class PrintListener: public RdEvent::ListenerInterface{
    public:
    void procEvent(EventElem x)override{
        std::cout<<"event! "<<x<<std::endl;
    }
    bool isWantEvent(EventPreInfo x)override{
        return true;
    }
};

void watchCode(std::string msg,int val){
    std::cout<<"245234 "<<msg<<" : "<<val<<std::endl;
}
// RdEvent::Ctrl ctrl;

#include <vector>
#include <functional>
#include <variant>
#include <set>
#include <any>

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

        struct PatternDefine{
            HistoryElem hist="DEFAULT_STR";
            std::function<AnsCode(Gila&)> checker=[](Gila& g){return AnsCode::OK;};
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
            // std::tuple<IndexType,AnsCode> getIndex(Gila &g)const{
            //     const auto stack=g.cmdIdCtx.refArgStack();
            //     if(stack.size()!=1){
            //         return {0,AnsCode::INCORRECT_STACK_SIZE};
            //     }
            //     const auto targetArg=stack[0];
            //     if(! std::holds_alternative<IndexType>(targetArg)){
            //         return {0,AnsCode::INCORRECT_ARG_TYPE};
            //     }
            //     const auto targetIndex=std::get<IndexType>(targetArg);
            //     return {targetIndex,AnsCode::OK};
            // }
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

    class TestCtrlLayer{
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
                        .hist="broadcasterJoinNetwork",
                        .checker=[=,this](Gila& g)mutable{
                            auto [value,code]=g.cmdIdCtx.getStack<IndexType,IndexType>();
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

        
        Tester::PatternHub total{
            Tester::PatternDefineSeq{
                {
                    .hist="generator",
                    .child=&(this->memoryLayer.refPatternHub()),
                },
                {
                    .hist="broadcasterJoinNetwork_top",
                    .checker=[](Gila& g){
                        std::cout<<"4875 hellowwww"<<std::endl;
                        return AnsCode::OK;
                    },
                    .child=&(broadcasterJoinNetwork.refPatternHub()),
                    
                },
            }
        };

        public:
        TestCtrlLayer(TestMemoryLayer& memoryLayer):
            memoryLayer{memoryLayer}{}
        auto &refPatternHub(){
            return this->total;
        }
        
    };

    
    void testerMain(){
        Gila gila;
        TestMemoryLayer memory{2,2,2};
        TestCtrlLayer test{memory};
        std::cout<<"total cont:"<<test.refPatternHub().getCont()<<std::endl;

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

    // PrintListener printListener{};
    
    // // {
    //     RdEvent::Broadcaster broadcaster;
        
    //     {
    //         RdEvent::Listener listener;
    //         constexpr int offset=2+4;
    //         std::cout<<"4875 l1 next ptr"<<*(int**)(((int64_t*)&listener)+offset)<<"@"<<(int**)(((int64_t*)&listener)+offset)<<std::endl;

    //         std::cout<<"\n addListener"<<std::endl;
    //         ctrl.addListener(listener);

    //         std::cout<<"\n addBroadcaster"<<std::endl;
    //         ctrl.addBroadcaster(broadcaster);

    //         std::cout<<"\n setListener"<<std::endl;
    //         listener.setListener(&printListener);

    //         std::cout<<"\n setEventPreInfo"<<std::endl;
    //         watchCode("setEventPreInfo",broadcaster.setEventPreInfo(10));

    //         // std::cout<<"\n broadcast"<<std::endl;
    //         // watchCode("broadcast",broadcaster.broadcast(80));

    //         // std::cout<<"\n broadcast2"<<std::endl;
    //         // watchCode("broadcast2",broadcaster.broadcast(805));

    //         // std::cout<<"\n procEvent0"<<std::endl;
    //         // ctrl.procEvent();

    //         // std::cout<<"\n procEvent1"<<std::endl;
    //         // ctrl.procEvent();
            
    //         // std::cout<<"\n procEvent2"<<std::endl;
    //         // ctrl.procEvent();

    //         // ctrl.printListener();
    //     }
    //     // int *p=new int{1};

    //     // RdEvent::Broadcaster *leakCaster=new RdEvent::Broadcaster{};
    //     // leakCaster->setEventPreInfo(404);
    //     // ctrl.addBroadcaster(*leakCaster);

    // // }
    // std::cout<<"\n procEvent3"<<std::endl;
    // ctrl.procEvent();

    // std::cout<<"hello"<<std::endl;

    

    return 0;
    
}