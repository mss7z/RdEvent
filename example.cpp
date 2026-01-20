#include <iostream>

// ヘッダーファイルに全ての宣言・定義を入れている。
// マイコンなどで実行バイナリの大きさが問題になる場合、実装部をソースファイルに分離することも今後検討する。
#include "RdEvent.hpp"

// RdEventは任意の型のイベントを扱えるように設計されている。
// イベントの情報を格納するEventElemがある。
// EventElemに加えて、イベントの事前情報を格納するEventPreInfoがある。
// 事前情報があることによって、BroadcasterとListenerを事前に結びつけておくことで、RdEventは低負荷を図る。
// ここでは最小限の例として、イベント本体は int、事前情報も int としてシンプルに構成した。
using EventElem=int;
using EventPreInfo=int;
using RdEvent=RdEventTemplate<EventElem,EventPreInfo>;


// 最小限のイベントモジュールを定義する。
// BroadcasterとListenerを1 つずつ持ち、同じEventPreInfoを放送し、受信している。
// つまり同一モジュールが自分自身のブロードキャストをリッスンしている。
// (実際にはこのようなパターンは少ないと思うが、サンプルの行数が多すぎると分かりにくいので、このようにした。)
class EventModuleMinimumExample: RdEvent::ListenerInterface{
    private:
    RdEvent::Broadcaster broadcaster;
    RdEvent::Listener listener;

    public:
    // EventCtrlを一つ受取り、Listenerを登録し、Broadcasterを登録する。
    // EventCtrlを受け取ることで、どのイベント空間にこのEventModuleが属するかを知る。
    // 与えるEventCtrlを変えれば、別のイベント空間に属すこともできる。
    EventModuleMinimumExample(RdEvent::Ctrl &ctrl){
        listener.setListener(this);
        ctrl.addListener(listener);

        broadcaster.setEventPreInfo(315);
        ctrl.addBroadcaster(broadcaster);
    }

    // 自分がListenerとして新規登録されるか、イベント空間に新しいBroadcasterが登録されたときに、Ctrlからコールバックされるメソッドである。
    // ここでは単純なEventPreInfoとしてintを指定している。
    // PreInfoが315に一致していれば購読したい旨をctrlに返す。
    // 言い換えると、true を返すと Ctrl はこのリスナーを購読対象として扱う。
    bool isWantEvent(EventPreInfo val){
        return val==315;
    }

    // PreInfoが315のイベントが放送されたときに、Ctrlからコールバックされる。
    // 受け取ったイベントの数に+1して、またイベント PreInfo 315の購読者に対して放送する。
    void procEvent(EventElem val){
        std::cout<<"event received"<<val<<std::endl;
        broadcaster.broadcast(val+1);
    }
};

int main(){
    // イベント空間（Ctrl オブジェクト）を定義する。
    RdEvent::Ctrl ctrl;

    // 上で定義したEventModuleをインスタンス化する。
    // ctrlを渡して、そのイベント空間にコンストラクタの中で自分を登録している。
    // コンストラクタの中で登録しなくても良く、どのように登録するのが良いかは要検討。
    EventModuleMinimumExample module{ctrl};

    // 315番のイベントを放送する最初のBroadcasterを定義する。
    RdEvent::Broadcaster startBroadcaster;
    startBroadcaster.setEventPreInfo(315);
    ctrl.addBroadcaster(startBroadcaster);
    startBroadcaster.broadcast(10);

    // Ctrl の procEvent は内部キューから、次の放送されたイベントを取り出してListenerに通知する。
    // 言い換えると、誰かがイベントを放送していた場合、そのイベントを聞きたがっているListenerをcallbackして、イベントを伝える。
    // 1回のコールで、1つのイベントのみの処理が行われる。
    // EventModuleは、自分の放送を自分で受信しているので、procEventを呼び続ければ、10~109までのカウントアップが見れるはず。
    for(int i=0; i<100; i++){
        ctrl.procEvent();
    }

    return 0;
}
