// g++ -std=c++20 -o prog UpVariantExample.cpp -pedantic-errors && echo === && ./prog

#include <iostream>

// ヘッダーファイルに全ての宣言・定義を入れている。
// マイコンなどで実行バイナリの大きさが問題になる場合、実装部をソースファイルに分離することも今後検討する。
#include "RdEvent.hpp"
#include "UpVariant.hpp"

// RdEventは任意の型のイベントを扱えるように設計されている。
// イベントの情報を格納するEventElemがある。
// EventElemに加えて、イベントの事前情報を格納するEventPreInfoがある。
// 事前情報があることによって、BroadcasterとListenerを事前に結びつけておくことで、RdEventは低負荷を図る。

// イベントとして流すデータの型を定義する。
struct Ev_Base{
};

struct Ev_Int:public Ev_Base{
    int i;
    Ev_Int(const int a=0):i{a}{}
};

struct Ev_Float:public Ev_Base{
    float f;
    Ev_Float(const float a=0.0):f{a}{}
};

// 実際にイベントとしてやり取りするためのハンドルを用意するジェネレータを用意する。
using EventGen=UpVariantNamespace::EventHandleGenerator<
    Ev_Base,
    Ev_Int,
    Ev_Float
>;
// ジェネレータのハンドルには内部で確保するメモリ長を計算するためのbit数を指定する。
// 同時に存在できる最大イベント数 = ( 1<<bit数 ) - 2
EventGen gen{8};

// 今回の例では、EventElemとEventPreInfoは同一とする。
using EventElem=EventGen::Handle;
using EventPreInfo=EventGen::Handle;
using RdEvent=RdEventNamespace::RdEventTemplate<EventElem,EventPreInfo>;

// すべてのイベントの親であるEv_Baseを購読し、受信したらEv_Baseを受信した旨を標準ストリームに表示するイベントモジュール。
class Em_PrintEvent: RdEvent::ListenerInterface{
    private:
    RdEvent::Listener listener;

    public:
    Em_PrintEvent(RdEvent::Ctrl &ctrl){
        listener.setListener(this);
        ctrl.addListener(listener);
    }

    bool isWantEvent(EventPreInfo val){
        return val.isHold<Ev_Base>();
    }

    void procEvent(EventElem val){
        std::cout<<"Em_PrintEvent received Ev_Base "<<std::endl;
    }
};

// Ev_Intを受信し、受信したらその数値を標準ストリームに表示するイベントモジュール。
class Em_PrintInt: RdEvent::ListenerInterface{
    private:
    RdEvent::Listener listener;

    public:
    Em_PrintInt(RdEvent::Ctrl &ctrl){
        listener.setListener(this);
        ctrl.addListener(listener);
    }

    bool isWantEvent(EventPreInfo val){
        return val.isHold<Ev_Int>();
    }

    void procEvent(EventElem val){
        std::cout<<"Em_PrintInt received Ev_Int "<<val.get<Ev_Int>().i<<std::endl;
    }
};

// Ev_Floatを受信し、受信したらその数値を標準ストリームに表示するイベントモジュール。
class Em_PrintFloat: RdEvent::ListenerInterface{
    private:
    RdEvent::Listener listener;

    public:
    Em_PrintFloat(RdEvent::Ctrl &ctrl){
        listener.setListener(this);
        ctrl.addListener(listener);
    }

    bool isWantEvent(EventPreInfo val){
        return val.isHold<Ev_Float>();
    }

    void procEvent(EventElem val){
        std::cout<<"Em_PrintFloat received Ev_Float "<<val.get<Ev_Float>().f<<std::endl;
    }
};

// Ev_Intを受信し、、受信したらその数値を2で割った値をEv_Floatとして放送するイベントモジュール
class Em_CalcHalf: RdEvent::ListenerInterface{
    private:
    RdEvent::Broadcaster broadcaster;
    RdEvent::Listener listener;

    public:
    Em_CalcHalf(RdEvent::Ctrl &ctrl){
        listener.setListener(this);
        ctrl.addListener(listener);

        UpVariantNamespace::EventHandle h=gen.genEvent(Ev_Float{});
        broadcaster.setEventPreInfo(h);
        ctrl.addBroadcaster(broadcaster);
    }

    bool isWantEvent(EventPreInfo val){
        return val.isHold<Ev_Int>();
    }

    void procEvent(EventElem val){
        const float broadcastVal=(val.get<Ev_Int>().i)/2.0f;
        std::cout<<"Em_CalcHalf broadcast "<<broadcastVal<<std::endl;
        broadcaster.broadcast(
            gen.genEvent(Ev_Float{ broadcastVal })
        );
    }
};

// limitとして与えられた値までの整数値を、Ev_Intとして放送するイベントモジュール
class Em_GenInt{
    private:
    RdEvent::Broadcaster broadcaster;
    int nextInt=0;
    int limit;

    public:
    Em_GenInt(RdEvent::Ctrl &ctrl,int limita=10):
        limit{limita}
    {
        UpVariantNamespace::EventHandle h=gen.genEvent(Ev_Int{});
        broadcaster.setEventPreInfo(h);
        ctrl.addBroadcaster(broadcaster);
    }
    void genNextInt(){
        if(nextInt>=limit){
            return;
        }
        std::cout<<"Em_GenInt broadcast "<<nextInt<<std::endl;
        broadcaster.broadcast(
            gen.genEvent(Ev_Int{ nextInt })
        );
        ++nextInt;
    }
};

class Em_MultiListener{
    private:
    RdEvent::ListenerAsFunc listener1;
    RdEvent::ListenerAsFunc listener2;

    void setup(){
        listener1.setFunc(
            [](EventPreInfo val){
                return val.isHold<Ev_Int>();
            },
            [](EventElem val){
                std::cout<<"Em_MultiListener received Ev_Int "<<val.get<Ev_Int>().i<<std::endl;
            }
        );
        listener2.setFunc(
            [](EventPreInfo val){
                return val.isHold<Ev_Float>();
            },
            [](EventElem val){
                std::cout<<"Em_MultiListener received Ev_Float "<<val.get<Ev_Float>().f<<std::endl;
            }
        );
        
    }
    public:
    Em_MultiListener(RdEvent::Ctrl &ctrl){
        setup();
        ctrl.addListener(listener1);
        ctrl.addListener(listener2);
    }


};

int main(){
    // イベント空間（Ctrl オブジェクト）を定義する。
    RdEvent::Ctrl ctrl;

    Em_PrintEvent printEvent{ctrl};
    Em_PrintInt printInt{ctrl};
    Em_PrintFloat printFloat{ctrl};
    Em_CalcHalf calcHalf{ctrl};
    Em_GenInt genInt{ctrl,10};
    Em_MultiListener multiListener{ctrl};

    while(true){
        genInt.genNextInt();
        if(!ctrl.isExistEvent()){
            break;
        }
        ctrl.procEvent();
    }

    return 0;
}
