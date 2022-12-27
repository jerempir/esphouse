//
// Created by user on 27.12.2022.
//

#ifndef UNTITLED255_JSON_H
#define UNTITLED255_JSON_H
#include "ArduinoJson.h"

class json {
public:

    //Конструктор преобразует стороку в объект Json
    json(auto jsonchar) {
        deserializeJson(doc, jsonchar);
        //cout<<doc<<endl;
    }

    // Setter определяет для какого nodeid будут выполняться функции
    void setNodeid(auto s) {
        node_id = s;
    }

    // Getter проверка количества событий
    uint8_t getEventnum() {
        uint8_t num;
        /////DO//////
        return num;
    }

    //возвращают соответсвующее значение для выбраных id и i - номера события
    const char *getName(auto i) {
        return doc[node_id][i]["event_name"];
    }

    const char *getCond(auto i) {
        return doc[node_id][i]["event_condition"];
    }

    const char *getFunc(auto i) {
        return doc[node_id][i]["event_function"];
    }

private:
    StaticJsonDocument<400> doc;
    char *node_id;
    //const char* name;
    //const char* cond;
    //const char* func;
};
#endif //UNTITLED255_JSON_H
