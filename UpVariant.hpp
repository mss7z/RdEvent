#pragma once

#ifdef UPVARIANT_DEBUG
    #include <iostream>
#endif

#include <cstdint>
#include <tuple>
#include <type_traits>

namespace UpVariantNamespace{

#ifdef UPVARIANT_DEBUG
    #include <iostream>
    namespace{
        auto& dbg = std::cout;
        using endl_t = std::ostream& (*)(std::ostream&);
        constexpr endl_t endl = std::endl<char, std::char_traits<char>>;
        void dbgAssert(bool b,const char* msg="assertion failed"){
            if(!b) {
                dbg << "assertion failed:"<<msg<<endl;
                std::abort();
            }
        }
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

        void dbgAssert(bool,const char*){
        }
    }
#endif

namespace{

template<typename Target, typename First, typename... Rest>
constexpr std::size_t calcIndex(){
    if constexpr(std::is_same_v<Target,First>){
        return 0;
    }else{
        return 1+calcIndex<Target,Rest...>();
    }
}

template<std::size_t Len>
class ParentInfo{
    private:
    using ElemType=uint64_t;

    static constexpr std::size_t elemSize=sizeof(ElemType);
    static constexpr std::size_t dataLen=Len/elemSize+1;

    ElemType data[dataLen]={};

    template<std::size_t... Idx>
    constexpr bool isEmptyCore(std::index_sequence<Idx...>)const noexcept{
        return ((data[Idx] | ... | 0) == 0);
    }

    template<std::size_t... Idx>
    constexpr bool isHitCore(std::index_sequence<Idx...>,const ParentInfo<Len> &other)const noexcept{
        return (((data[Idx] & other.data[Idx]) | ... | 0) != 0);
    }

    public:
    constexpr bool isEmpty()const noexcept{
        return isEmptyCore(std::make_index_sequence<dataLen>());
    }
    constexpr bool isHit(const ParentInfo<Len> &other)const noexcept{
        return isHitCore(std::make_index_sequence<dataLen>(),other);
    }

    constexpr bool check(const std::size_t pos)const noexcept{
        return (data[pos/elemSize] >> (pos%elemSize)) & 1;
    }
    constexpr void set(const std::size_t pos)noexcept{
        data[pos/elemSize] |= (static_cast<ElemType>(1) << (pos%elemSize));
    }

};

template<typename TargetType, typename ... Rest>
union RecursiveUnion{
    TargetType data;
    RecursiveUnion<Rest...> rest;

    RecursiveUnion()=delete;
    RecursiveUnion(const RecursiveUnion&)=default;
    RecursiveUnion(RecursiveUnion&&)=default;
    RecursiveUnion& operator=(const RecursiveUnion&)=default;
    RecursiveUnion& operator=(RecursiveUnion&&)=default;

    template<std::size_t Idx,typename T>
    constexpr RecursiveUnion(std::integral_constant<std::size_t,Idx>,T&& dataA):
        rest(std::integral_constant<std::size_t,Idx-1>{},std::forward<T>(dataA))
    {    
    }

    template<typename T>
    constexpr RecursiveUnion(std::integral_constant<std::size_t,0>,T&& dataA):
        data(std::forward<T>(dataA))
    {    
    }

    template<std::size_t Idx>
    constexpr auto& get()const{
        if constexpr(Idx==0){
            return data;
        }else{
            return rest.template get<Idx-1>();
        }
    }

    template<typename ConvedType>
    ConvedType *get_ifConv(const std::size_t idx){
        if constexpr(std::is_base_of_v<ConvedType, TargetType>){
            dbg<<"start proc at base ctx"<<idx<<endl;
            if( idx==0 ){
                return static_cast<ConvedType*>(&data);
            }else{
                return rest.template get_ifConv<ConvedType>(idx-1);
            }
        }else{
            dbg<<"start proc at NObase ctx"<<idx<<endl;
            if( idx==0 ){
                dbg<<"ret nullptr at Idx(!=)0"<<endl;
                return nullptr;
            }else{
                return rest.template get_ifConv<ConvedType>(idx-1);
            }
        }
    }

    constexpr void del(const std::size_t idx){
        if ( idx==0 ){
            if constexpr( !(std::is_trivially_destructible_v<TargetType>)){
                data.~TargetType();
            }
        }else{
            rest.del(idx-1);
        }
    }
};

template<typename TargetType>
union RecursiveUnion<TargetType>{
    TargetType data;

    RecursiveUnion()=delete;
    RecursiveUnion(const RecursiveUnion&)=default;
    RecursiveUnion(RecursiveUnion&&)=default;
    RecursiveUnion& operator=(const RecursiveUnion&)=default;
    RecursiveUnion& operator=(RecursiveUnion&&)=default;

    template<typename T>
    constexpr RecursiveUnion(std::integral_constant<std::size_t,0>,T&& dataA):
        data(std::forward<T>(dataA))
    {    
    }

    template<std::size_t Idx>
    constexpr auto& get()const{
        static_assert(Idx==0,"logic error");
        return data;
    }
    template<typename ConvedType>
    ConvedType *get_ifConv(const std::size_t idx){
        if constexpr(std::is_base_of_v<ConvedType, TargetType>){
            return static_cast<ConvedType*>(&data);
        }else{
            dbg<<"ret nullptr at Idx==0"<<endl;
            return nullptr;
        }
    }

    constexpr void del(const std::size_t idx){
        if constexpr( !(std::is_trivially_destructible_v<TargetType>)){
            data.~TargetType();
        }
    }
};

}

template<typename ... Types>
class UpVariant{
    private:
    std::size_t currentIdx;
    RecursiveUnion<Types...> data;


    template<std::size_t Idx>
    using CalcType=typename std::tuple_element_t<Idx, std::tuple<Types...>>; 

    template<typename ChildType>
    struct GetParentInfo{
        template<std::size_t... Idx>
        static constexpr auto getParentInfoCore(std::index_sequence<Idx...>){
            ParentInfo<sizeof...(Types)> parentInfo;
            (( (std::is_base_of_v<ChildType, CalcType<Idx>> )?
                parentInfo.set(Idx) : (void)0), ...);
            return parentInfo;
        }
        static constexpr auto getParentInfo(){
            return getParentInfoCore( std::make_index_sequence<sizeof...(Types)>() );
        }
    };

    public:

    UpVariant()=delete;
    UpVariant(const UpVariant&)=default;
    UpVariant(UpVariant&&)=default;
    UpVariant& operator=(const UpVariant&)=default;
    UpVariant& operator=(UpVariant&&)=default;


    template<typename T>
        requires (!std::same_as<std::remove_cvref_t<T>, UpVariant>)
    constexpr UpVariant(T&& t):
        currentIdx{ calcIndex<T,Types...>() },
        data( std::integral_constant<std::size_t, calcIndex<T,Types...>() >{}, std::forward<T>(t) )
    {
    }

    template<typename T>
        requires (!std::same_as<std::remove_cvref_t<T>, UpVariant>)
    constexpr UpVariant& operator=(T&& t){
        currentIdx=calcIndex<T,Types...>();
        data=RecursiveUnion<Types...>(std::integral_constant<std::size_t, calcIndex<T,Types...>() >{}, std::forward<T>(t));
        return *this;
    }

    constexpr std::size_t index()const noexcept{
        return currentIdx;
    }

    template<std::size_t Idx>
    constexpr auto* getDirect_if(){
        static_assert(Idx < sizeof...(Types));
        if(Idx==currentIdx){
            return &(data.template get<Idx>());
        }else{
            return nullptr;
        }
    }
    template<typename T>
    constexpr auto* getDirect_if(){
        return getDirect_if<calcIndex<T,Types...>>();
    }
    template<std::size_t Idx>
    constexpr auto& getDirect()const{
        auto* const ptr = getDirect_if<Idx>();
        if(ptr==nullptr){
            throw std::runtime_error("type mismatch");
        }else{
            return *ptr;
        }
    }

    template<typename T>
    constexpr auto& getDirect()const{
        return get<calcIndex<T,Types...>()>();
    }

    template<typename T>
    constexpr auto* get_if(){
        // constexpr ParentInfo<sizeof...(Types)> parentInfo = GetParentInfo<T>::getParentInfo();
        // if(parentInfo.check(currentIdx)){
        //     // 処理は早いが、std_layoutではないとき（特にダイヤモンド継承があったとき）に、未定義となる
        //     return reinterpret_cast<T*>(&(data.data));
        // }else{
        //     return static_cast<T*>(nullptr);
        // }
        return (data.template get_ifConv<T>(currentIdx));
    }
    template<typename T>
    constexpr auto& get(){
        auto* const ptr = get_if<T>();
        if(ptr==nullptr){
            throw std::runtime_error("type mismatch");
        }else{
            return *ptr;
        }
    }

    template<std::size_t Idx>
    constexpr bool isHoldDirect() const{
        return Idx == currentIdx;
    }

    template<typename T>
    constexpr bool isHoldDirect() const{
        return isHold<calcIndex<T,Types...>()>();
    }

    template<typename T>
    constexpr bool isHold() const{
        constexpr ParentInfo<sizeof...(Types)> parentInfo = GetParentInfo<T>::getParentInfo();
        return parentInfo.check(currentIdx);
    }
};

namespace{

template<typename Type>
class MemoryPool{

    private:
    Type* memoryP;
    Type** freeRingPP;
    size_t nextAllocIdx=0;
    size_t lastFreeIdx;

    const size_t mask;

    public:
    MemoryPool(const size_t bitlen):
        mask{ (1<<bitlen)- static_cast<decltype(bitlen)>(1) }
    {
        const size_t len=1<<bitlen;
        const size_t memLen=len-1;
        memoryP=static_cast<Type*>(
            ::operator new(sizeof(Type)*memLen, std::align_val_t{alignof(Type)})
        );
        freeRingPP=new Type*[len];
        for(size_t i=0;i<memLen;i++){
            freeRingPP[i]=&memoryP[i];
        }
        lastFreeIdx=memLen-1;
    }


    // Type* alloc(){
    //     if(nextAllocIdx==lastFreeIdx){
    //         return nullptr;
    //     }
    //     const size_t retIdx=nextAllocIdx;
    //     nextAllocIdx= nextAllocIdx+1 & mask;
    //     dbg<<"allcoc nextAllocIdx:"<<nextAllocIdx<<" lastFreeIdx:"<<lastFreeIdx<<endl;
    //     return freeRingPP[retIdx];
    // }


    template<typename T>
    Type* newMem(T&& val){
        if(nextAllocIdx==lastFreeIdx){
            return nullptr;
        }
        const size_t retIdx=nextAllocIdx;
        nextAllocIdx= nextAllocIdx+1 & mask;
        dbg<<"allcoc nextAllocIdx:"<<nextAllocIdx<<" lastFreeIdx:"<<lastFreeIdx<<endl;
        Type *const ret=freeRingPP[retIdx];
        new(ret) Type{ std::forward<T>(val) };
        return ret;
    }
    void freeMem(Type* ptr){
        dbg<<"free before nextAllocIdx:"<<nextAllocIdx<<" lastFreeIdx:"<<lastFreeIdx<<endl;
        dbgAssert(ptr>=memoryP && ptr<(memoryP+mask), "not self ram");
        dbgAssert((lastFreeIdx+1 & mask)!=nextAllocIdx, "not free ring");

        if constexpr(!(std::is_trivially_destructible_v<Type>)){
            ptr->~Type();
        }

        const size_t retIdx=lastFreeIdx+1 & mask;
        freeRingPP[retIdx]=ptr;
        lastFreeIdx=retIdx;

        dbg<<"free nextAllocIdx:"<<nextAllocIdx<<" lastFreeIdx:"<<lastFreeIdx<<endl;
    }
    size_t getFree(){
        return (lastFreeIdx-nextAllocIdx) & mask;
    }

    ~MemoryPool(){
        ::operator delete(static_cast<void*>(memoryP), std::align_val_t{alignof(Type)});
        delete [](freeRingPP);
    }
};

}

#ifdef UPVARIANT_DEBUG
namespace UpVariantDebug{
    template<typename Type>
    using MemoryPool = MemoryPool<Type>;

}
#endif

namespace{

template<typename... Types>
struct MemoryElemCore{
    size_t count=0;
    UpVariant<Types...> data;
    //todo: 依存関係が相互依存。設計改良
    MemoryPool<MemoryElemCore<Types...>> &pool;

    template<typename T>
    MemoryElemCore(T&& val,MemoryPool<MemoryElemCore<Types...>> &poola):
        data{std::forward<T>(val)},
        pool{poola}
    {}
};

template<typename... Types>
class EventHandle{
    private:
    using MemoryElem=MemoryElemCore<Types...>;
    MemoryElem *memoryElemP;

    public:
    EventHandle():memoryElemP{nullptr}{}

    EventHandle(MemoryElem* const memoryElemPa):
    memoryElemP{memoryElemPa}{
        memoryElemP->count++;
    }

    EventHandle(const EventHandle& other):
    memoryElemP{other.memoryElemP}{
        memoryElemP->count++;
    }
    EventHandle(EventHandle&& other):
    memoryElemP{other.memoryElemP}{
        other.memoryElemP=nullptr;
    }

    EventHandle& operator=(const EventHandle& other){
        if(this!=&other){
            if(isEnable()){
                memoryElemP->count--;
            }
            memoryElemP=other.memoryElemP;
            memoryElemP->count++;
        }
        return *this;
    }

    EventHandle& operator=(EventHandle&& other){
        if(this!=&other){
            if(isEnable()){
                memoryElemP->count--;
            }
            memoryElemP=other.memoryElemP;
            other.memoryElemP=nullptr;
        }
        return *this;
    }

    template<typename Type>
    Type &get(){
        if(memoryElemP==nullptr){
            throw std::runtime_error("disabled Event");
        }
        return memoryElemP->data.template get<Type>();
    }

    template<typename Type>
    bool isHold(){
        if(memoryElemP==nullptr){
            throw std::runtime_error("disabled Event");
        }
        return memoryElemP->data.template isHold<Type>();
    }

    bool isEnable(){
        return memoryElemP!=nullptr;
    }

    ~EventHandle(){
        if(memoryElemP){
            memoryElemP->count--;
            if(memoryElemP->count==0){
                memoryElemP->pool.freeMem(memoryElemP);
            }
        }
    }
};

}

template<typename... Types>
class EventHandleGenerator{
    public:
    using Handle=EventHandle<Types...>;
    
    private:
    using MemCore=MemoryElemCore<Types...>;
    MemoryPool<MemCore> memoryPool;

    public:


    EventHandleGenerator(const size_t bitlen):
        memoryPool{bitlen}
    {
    }

    template<typename T>
    Handle genEvent(T&& val){
        // 返却値はisEnableをチェックして使うこと
        MemCore *mem=memoryPool.newMem(MemCore{
            std::forward<T>(val),memoryPool
        });
        dbg<<"hello>"<<endl;
        if(mem==nullptr){
            dbg<<"EventHandleGenerator::genEvent: memoryPool is full"<<endl;
            throw std::runtime_error("memoryPool is full");
        }
        return EventHandle<Types...>{
            mem
        };
    }

    size_t getFree(){
        return memoryPool.getFree();
    }
};

}//namespace UpVariantNamespace