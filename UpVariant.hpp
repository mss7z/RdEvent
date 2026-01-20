#pragma once

#include <iostream>
#include <cstdint>
#include <tuple>
#include <type_traits>

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
        constexpr ParentInfo<sizeof...(Types)> parentInfo = GetParentInfo<T>::getParentInfo();
        if(parentInfo.check(currentIdx)){
            return reinterpret_cast<T*>(&(data.data));
        }else{
            return static_cast<T*>(nullptr);
        }
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
};