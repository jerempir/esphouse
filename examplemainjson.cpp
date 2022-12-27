#include <iostream>
#include "json.h"

using namespace std;

DynamicJsonDocument doc(1024);
char jsontxt[1024];

char id1[] = "123321522";
char id2[] = "222222222";
uint8_t num_event;




int main() {
    //несколько способов задать json документ
    //JsonObject node1 = doc.createNestedObject(id1);
    //JsonObject node2 = doc.createNestedObject(id2);
    //JsonObject event11 = node1.createNestedObject("1");
    //JsonObject event12 = node1.createNestedObject("2");
    //JsonObject event21 = node2.createNestedObject("1");
    //event11["event_name"]="key-switch1";
    //event11["event_condition"]= "change";
    //event11["event_function"] = "turn_lamp";
    //event12["event_name"]="key-switch2";
    //event12["event_condition"]= "up";
    //event12["event_function"] = "turn_sound";
    //event21["event_name"] = "key-switch1";
    //event21["event_condition"] = "down";
    //event21["event_function"] = "turn_heating";
    doc[id1]["1"]["event_name"] = "key-switch1";
    doc[id1]["1"]["event_condition"] = "Change";
    doc[id1]["1"]["event_function"] = "turn_lamp";
    doc[id1]["2"]["event_name"] = "key-switch2";
    doc[id1]["2"]["event_condition"] = "Up";
    doc[id1]["2"]["event_function"] = "turn_sound";
    doc[id2]["1"]["event_name"] = "key-switch1";
    doc[id2]["1"]["event_condition"] = "Down";
    doc[id2]["1"]["event_function"] = "turn_heating";
    doc["2342"]["1"]["event_function"] = "turn_heating";
    serializeJson(doc, jsontxt);  //преобразует json в char jsontxt


    json myjson(jsontxt);
    myjson.setNodeid(id2);  //Указываем какой id нас интересует
    if (true) { //Проверка на существование событий myjson.getEventnum()
        auto i = "1";
        cout << myjson.getName(i) << endl;
        cout << myjson.getCond(i) << endl;
        cout << myjson.getFunc(i) << endl;
    }

    return 0;
}