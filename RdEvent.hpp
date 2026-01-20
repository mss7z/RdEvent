#pragma once

#ifdef RDEVENT_DEBUG
    #include <iostream>
    namespace{
        auto& dbg = std::cout;
        auto& endl = std::endl;
    }
#else
    namespace{
        struct NullOutputStream {
            template<typename T>
            constexpr NullOutputStream& operator<<(T&&) noexcept {
                return *this;
            }
        };
        NullOutputStream dbg;
        constexpr auto endl = 0;
    }
#endif


#include <cstddef>
#include <new>
#include <utility>

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
            dbg<<"45648 detach bef r"<<&ra<<
                " r.preP"<<r.preP<<" r.nextP"<<r.nextP<<
                " r.nextP->preP"<<r.nextP->preP<<" r.preP->nextP"<<r.preP->nextP<<endl;
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
            dbg<<"6877 selfSize"<<sizeof(*this)<<" this" <<this<<endl;
            dbg<<"6877 &(this->handle)"<<&(this->handle)<<"handlea"<<&handlea<<endl;
            handle.nextP=&(this->handle);
            handle.preP=&(this->handle);
            dbg<<"6878 handle,nextP"<<handle.nextP<<"@"<<&(handle.nextP)<<endl;
            dbg<<"6878 handle,preP"<<handle.preP<<"@"<<&(handle.preP)<<endl;
        }
        Error append(ElemType *p){
            auto p2=this->convToHandle(p);
            if(p2->nextP != nullptr){
                return Error::ALRADY_IN_LIST;
            }

            dbg<<"p "<<p<<endl;

            dbg<<"handle pre "<<handle.preP<<" next "<<handle.nextP<<endl;
            
            
            // 前の人に次が自分であることを設定する
            // つまり、最後尾のelemへpが次であることを設定する
            this->handle.preP->nextP=p2;
            p2->nextP=&(this->handle);

            // 新たな最後尾であるpの前は、Handleが持つ最後尾を示すポインタである
            p2->preP=this->handle.preP;
            // 末尾はもちろんp
            this->handle.preP=p2;


            dbg<<"handle pre"<<handle.preP<<" next "<<handle.nextP<<endl;

            return Error::OK;
        }
        
        auto begin()noexcept{
            return this->genIter(handle.nextP);
        }
        auto end()noexcept{
            return this->genIter(&handle);
        }
        bool isEmpty()const noexcept{
            dbg<<"12324 list handler isEmpty called ret"<<(this->handle.nextP == &(this->handle))<<endl;
            dbg<<"12324 list handler isEmpty called handleNext"<<(this->handle.nextP)<<" handleSelf"<<&(this->handle)<<endl;
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
        #ifdef RDEVENT_DEBUG
        int64_t newCount=0;
        #endif

        ElemType *postAppendObj(ElemType *p)noexcept{
            
            if(p == nullptr){
                this->lastError=Error::BAD_ALLOC;
                return nullptr;
            }
            this->lastError= this->append(p);
            #ifdef RDEVENT_DEBUG
            ++newCount;
            dbg<<"4848 appendObj called append"<<p<<" count"<<newCount<<" @"<<this<<endl;
            #endif
            return p;
        }

        public:
        ListHandleDynamic(){
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
            
            #ifdef RDEVENT_DEBUG
            dbg<<"8874 deleteObj called del"<<p<<" count"<<newCount<<" @"<<this<<endl;
            --newCount;
            #endif
            delete p;
        }

        ~ListHandleDynamic(){
            #ifdef RDEVENT_DEBUG
            dbg<<"3425 ListHandleDynamic delete newCount"<<newCount<<" @"<<this<<endl;
            #endif
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
            dbg<<"4679 EventQueueTemplatte que addr"<<&que<<endl;
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
            dbg<<"37942 getFirst "<<firstP<<endl;
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
            dbg<<"2342 broadcasterMyListener addr"<<&broadcasterMyListener<<endl;

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
                dbg<<"listener"<<endl;
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
            dbg<<"3234 Gila delete"<<endl;
            dbg<<"3234 eventQue isEmpty="<<eventQue.isEmpty()<<endl;
        }
    };
    struct GilaFunc{
        static Error checkPeer(BroadcasterCore &b,ListenerCore &l){
            dbg<<"88741 peer called"<<endl;
            if(l.isWantEvent(b.getEventPreInfo())){
                dbg<<"88742 peer know!"<<endl;
                ListenerChainElem *p;
                if( p=l.getChainElem() ){
                    return b.addMyListener(p);
                }
                dbg<<"55484 error"<<endl;
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
            dbg<<"5547 ListenerCore chainP addr"<<this->chainP<<endl;
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
            dbg<<"62398 listener delete starrt"<<endl;
            this->unjoinNetwork();

            dbg<<"62398 listener delete OK"<<endl;
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
            dbg<<"4882 core ptr"<<&core<<endl;
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
            dbg<<"874115 addMyListener Called"<<endl;
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
            dbg<<"796732 stone"<<endl;
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
            dbg<<"3462 broadcaster delete start"<<endl;
            this->unjoinNetwork();

            dbg<<"3462 broadcaster delete OK"<<endl;
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
            dbg<<"12341 called procEvent"<<endl;
            auto& eventQue=this->gila.refEventQue();
            if(eventQue.isEmpty()){
                dbg<<"97723 isEmpty"<<endl;
                return Error::OK;
            }
            const InternalEvent &intEv = eventQue.refNextPop();
            dbg<<"321234 stone"<<endl;

            if(intEv.type == IntervalEventType::NORMAL_EVENT){
                dbg<<"4203 NORMAL_EVENT"<<endl;
                auto &reader = intEv.listener->myListener;
                for(const ListenerChainElem &chainElem: reader){
                    chainElem.mother->callProcer(intEv.event);
                }
            }else if(intEv.type == IntervalEventType::DEL_BROADCASTER){
                dbg<<"4203 DEL_BROADCASTER"<<endl;
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
            dbg<<"46878 stone"<<endl;
            eventQue.popNoRet();
            return Error::OK;
        }
        ~Ctrl(){
            dbg<<"4665 ctrl delete start"<<endl;

            this->gila.disbandNetwork();
            while(!this->gila.refEventQue().isEmpty()){
                this->procEvent();
            }

            dbg<<"46868 ctrl delete OK"<<endl;
        }
    };
};