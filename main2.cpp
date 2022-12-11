#include <Arduino.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <iostream>
#include <connect.h>
#include <ctime>
#include <stdlib.h>
#include <string>
// #include <ArduinoJson.h>
// #include <main2.h>

uint8_t CHANNEL = 1;

int nodeid; // NewNode?

// StaticJsonDocument<100> jsonMessage;
unsigned long timer;
unsigned long timer2;

char buffer[100];

int ii = 0; // Зачем оно?

bool recvBool; // Боже, зачем оно?

// bool boolsendStatus = true;

bool RandomFuckingVariable = true;    //Добавление своих смежных узлов (сила сигнала) один раз (этап 2.2)

uint8_t LastMessageId;

uint8_t mmac[6];
std::vector<uint8_t> myMac;
// std::string AP_SSID = "node " + std::to_string(myMac[0]);
uint64_t mymacID;

std::vector<std::vector<uint8_t>> AllNodeMacAddr = {{44, 244, 50, 18, 214, 245}, {44, 244, 50, 18, 197, 196}};


std::vector<uint64_t> AllNodeID = {(((((uint64_t)(44 * 256) + 244) * 256 + 50) * 256 + 18) * 256 + 214) * 256 + 245,
                                 (((((uint64_t)(44 * 256) + 244) * 256 + 50) * 256 + 18) * 256 + 197) * 256 + 196};

//uint8_t ( &idToArray (uint64_t id)) [6] {         // сложная конструкция для возврата массива из 6 uint8_t
std::vector <uint8_t> idToArray (uint64_t id){
  std::vector <uint8_t> MacAddress(6);
  for (int i=0; i<6; ++i){
    MacAddress[5-i] = id % 256;
    id/=256;
  }
  return MacAddress;
}

uint64_t ArrayToID(uint8_t MacAddress[6]) {
  return (((((uint64_t)(MacAddress[0] * 256) + MacAddress[1]) * 256 + MacAddress[2]) * 256 + MacAddress[3]) * 256
    + MacAddress[4]) * 256 + MacAddress[5];
}

String ArrayMACToString(uint8_t MacAddress[6]){
  return String(MacAddress[0]) + "." + String(MacAddress[1]) + "." + String(MacAddress[2]) + "."
       + String(MacAddress[3]) + "." + String(MacAddress[4]) + "." + String(MacAddress[5]);
}


const uint8_t len2 = 6;

std::vector<int> IdAllNode(AllNodeMacAddr.size());    // Боже, снова, что это??
std::vector<uint64_t> AllAvaliableNodeID;               // Все живые узлы в сети
std::vector<std::pair<uint64_t, uint8_t>> localRssi;      // Массив смежных узлов + вес ребра

std::vector<std::vector<std::pair<uint64_t, uint8_t>>> SignalStrenghtInNode;

typedef struct struct_message {
  uint64_t from;                  // id
  uint64_t to;                    // id
  uint8_t transid;                // message_id ( rand()%256 )
  uint8_t message_type;           // search in Miro 
  char buff[100];                 // message
  std::vector<uint64_t> allnode;  // all node id
  std::vector<std::pair<uint64, uint8_t>> nodeRssi;
} struct_message;

struct_message recv;
connect *My_con;    // Теперь уровень сигнала можно настроить при создании

// Возвращает позицию id в массиве AllNodeID
// Если такого нет, то вернет -1
//uint8_t macPosFromID(uint64_t id) {
//  for (uint8_t i = 0; i < AllNodeID.size(); i++)
//    if (id == AllNodeID[i])
//      return i;
//  return -1;
//}


// Возвращает позицию id в массиве VectorID
// Если такого нет, то вернет -1
int8 getIndex (std::vector <uint64_t> VectorID, uint64_t id) {
    auto it = std::find(VectorID.begin(), VectorID.end(), id);
  
    // If element was found
    if (it != VectorID.end()) {
        // calculating the index
        // of K
        uint8_t index = it - VectorID.begin();
        return index;
    }
    else {
        // If the element is not
        // present in the vector
        return -1;
    }
}



// Заполняет массив localRssi - массив узлов
// и уровней сигналов к ним, доступный напрямую
void scanNetwork(int networksFound) {
  Serial.print("   Сканирование всех доступных сетей: ");
  Serial.print(networksFound);
  Serial.println("штук");
  
  for (int i = 0; i < networksFound; i++)  {
    std::vector<uint8_t> netmac(0);
    netmac.assign(WiFi.BSSID(i), WiFi.BSSID(i) + 6);
    netmac[0] -= 2; // softap мак адрес наших плат на два больше чем классический station mac
    
    int indexInAll = getIndex(AllNodeID, ArrayToID(netmac.data()));
    int indexInAval = getIndex(AllAvaliableNodeID, ArrayToID(netmac.data()));
    
    if (indexInAll != -1 and indexInAval != -1) {
      Serial.println("Добавляю: " ); Serial.print(ArrayMACToString(netmac.data()));

      localRssi.push_back(std::make_pair(ArrayToID(netmac.data()), abs(WiFi.RSSI(i))));

    } else
      Serial.println("Не добавляю: " ); Serial.print(ArrayMACToString(netmac.data()));      
    
  }
  Serial.println("--------------------------------------");  
}



// Возвращает оптимальный id 
// для начального подключения
uint64_t foundnode(int networksFound) {
  Serial.print("   Сканирование всех доступных сетей: ");
  Serial.print(networksFound);
  Serial.println("штук");

  uint8_t minRSSI = 255;
  uint64_t minID = 0;
  
  for (int i = 0; i < networksFound; i++)  {
    std::vector <uint8_t> netmac;
    netmac.assign(WiFi.BSSID(i), WiFi.BSSID(i) + 6);
    netmac[0] -= 2; // softap мак адрес наших плат на два больше чем классический station mac
//    Serial.println(ArrayToID(netmac.data()));
//    Serial.print(AllNodeID[0]);
//    Serial.print("<---->");
//    Serial.println(AllNodeID[1]);

    int indexInAll = getIndex(AllNodeID, ArrayToID(netmac.data()));
//    int indexInAval = getIndex(AllAvaliableNodeID, ArrayToID(netmac.data()));
    //for (int j=0; j<6; ++j){
    //  Serial.print(netmac.data()[j]);
    //  Serial.print(".");   
    //}
//    Serial.println();
//    Serial.println(indexInAll);
    
    if (indexInAll != -1 and abs(WiFi.RSSI(i)) < minRSSI) {

      minID = ArrayToID(netmac.data());
      minRSSI = abs(WiFi.RSSI(i));
    }
  }

  

  Serial.print("Оптимальный: ");
  Serial.println(ArrayMACToString(idToArray(minID).data()));
  Serial.println("--------------------------------------");  
  delay(1000);
  return minID;
}

/*
void scanNetwork0(int networksFound) // Такая же функция как и выше, но используется в начале для поиска платы среди всех а не живых к кторой подключиться.
{
  std::vector<uint8_t> netmac;
  // Serial.println("found nets");
  // Serial.println(networksFound);
  for (int i = 0; i < networksFound; i++)
  {
    Serial.println(WiFi.SSID(i));
    // Serial.println(WiFi.BSSIDstr(i));
    Serial.println(WiFi.RSSI(i));
    netmac.assign(WiFi.BSSID(i), WiFi.BSSID(i) + 6);
    netmac[0] -= 2;
    
    for (int j = 0; j < IdAllNode.size(); j++) {
      int k = macPosFromID(IdAllNode[j]);
      if (AllNodeMacAdr[k] == netmac)
      {
        localRssi.push_back(std::make_pair(IdAllNode[j], WiFi.RSSI(i)));
      }
    }
  }
}
*/


// Допустим, функция вызывается при каждой отправке,
// может знать дошло смс до получателя или нет
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {

  uint8_t boolsendStatus = sendStatus; // TODO использовать данные

  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0)
    Serial.println("Delivery success");
  else
    Serial.println("Delivery fail, " + boolsendStatus);
}

// Допустим, Функция ищет ноду с наименьшим rssi
// для подключения к ней этап 1.1 findIDMinimalRSSI
uint8_t findIDMinimalRSSI() {
  
  Serial.println("Сканирования точек доступа для добавления в сеть...");
  // WiFi.scanNetworksAsync(scanNetwork0); //scanNEtwork0 вызывается после этой функции.
  int n = WiFi.scanNetworks(false, true);                                         // TODO Разобраться как оно работает
  scanNetwork(n);


  uint8_t minrssi = 255;
  uint64_t currentid = 0;

  for (auto i : localRssi) {
    if (i.second < minrssi) {
      minrssi = i.second;
      currentid = i.first;
    }
  }
  Serial.print("Сеть с лучшим сигналом: ");
  Serial.print(currentid);
  Serial.print(" <---> ");
  Serial.println(minrssi);
  
  localRssi.clear();
  
  Serial.println("--------------------------------------");
  return currentid;
}

// Допустим, Какая-то параша (функция переконфигурации сети)
void add(){ }


//########################################################################################################
//########################################################################################################
//########################################################################################################
//########################################################################################################
//Посмотреть когда стоит менять recv.transid

// Допустим, парашная параша
// (функция вызывается при получении, для каждого типа смс свой сценарий)
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) { 
  
  memcpy(&recv, incomingData, sizeof(recv));

  // Miro тип 1.1, пришел запрос на добавление в сеть.
  if (recv.message_type == 11) {
    uint64_t incomingID = recv.from;
    Serial.print("Этап 1.1, ко мне хочет добавиться ");
    Serial.println(ArrayMACToString(idToArray(incomingID).data()));
    Serial.println("Запускаем прикол с моим кодом ()");
    Serial.println();
    // AllNodeMacAdr.push_back(recv.from);
    // esp_now_add_peer(&recv.from[0],ESP_NOW_ROLE_COMBO,CHANNEL,NULL,0);
    add();
  }

  // Miro тип 1.2, мне пришла лавинная рассылка о живости узлов.
  else if (recv.message_type == 12 and recv.transid != LastMessageId) { // ТИП 12
    LastMessageId = recv.transid;
    Serial.println("Miro тип 1.2, мне пришла лавинная рассылка о живости узлов.");
    if (mymacID == recv.to) {
      Serial.println("И я конечный пункт назначения");

      struct_message send;
      send.from = mymacID;
      send.to = recv.from;
      send.transid = rand() % 256;
      send.message_type = 13;

      Serial.print("Отправляю ответ на ");
      Serial.println(ArrayMACToString(idToArray(send.to).data()));
      Serial.println("    Через: ");

      for (auto neibour_node : AllAvaliableNodeID)          //TODO Уточнить, кому должны рассылать
      { // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
        Serial.print("    ");
        Serial.print(ArrayMACToString(idToArray(neibour_node).data()));
        
        esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&send, sizeof(send));
      }
    } else {
      Serial.println("И я НЕ конечный пункт назначения");
      Serial.print("Отправляю ответ на ");
      Serial.println(ArrayMACToString(idToArray(recv.to).data()));
      Serial.println("    Через: ");

      for (auto neibour_node : AllAvaliableNodeID)          //TODO Уточнить, кому должны рассылать
      { // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
        Serial.print("    ");
        Serial.print(ArrayMACToString(idToArray(neibour_node).data()));

        esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&recv, sizeof(recv)); // Если сообщение перешлётся на конечную, та вернет сообщение тип 13.
      }
    }
  }

  // Miro тип 1.3. Узел жив, пытается связаться с инициатором.
  else if (recv.message_type == 13 and LastMessageId != recv.transid) { // ТИП 13
    LastMessageId = recv.transid;
    Serial.println("Miro тип 1.3, мне пришел ответ.");

    if (mymacID == recv.to) {
      Serial.print("Ответ пришел мне! Узел ");
      Serial.print(ArrayMACToString(idToArray(recv.from).data()));
      Serial.println(" жив!!!");
      
      recvBool = true;
    } else {
      Serial.print("Я лишь посредник(. Пересылаю на узел: ");
      Serial.println(ArrayMACToString(idToArray(recv.to).data()));

      Serial.println("    Через: ");

      for (auto neibour_node : AllAvaliableNodeID)          //TODO Уточнить, кому должны рассылать
      { // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
        Serial.print("    ");
        Serial.print(ArrayMACToString(idToArray(neibour_node).data()));

        esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&recv, sizeof(recv)); // Если сообщение перешлётся на конечную, та вернет сообщение тип 13.
      }
    }
  }

  // Miro тип 2.1. Все узлы в сети, от которых был получен ответ,
  //   получают массив из всех узлов в сети по принципу этапа 1,
  //   взамен отсылают всем (Лавинно) массив пар содержащий id и
  //   уровень сигнала до всех смежных узлов.
  else if (recv.message_type == 21 and recv.transid != LastMessageId) { // ТИП 21
    LastMessageId = recv.transid;
    
    Serial.println("Miro тип 2.1");

    if (mymacID == recv.to) {
      Serial.println("Я получил массив со всеми живыми узлами!!");
      AllAvaliableNodeID = recv.allnode;
      for (auto &i: AllAvaliableNodeID){
        Serial.print("    ");
        Serial.println(ArrayMACToString(idToArray(i).data()));
      }
      
      struct_message send;
      
      send.from = mymacID;
      send.transid = rand() % 256;

      send.message_type = 22;
      
      Serial.println("Сканирую устройства вокруг");

      //WiFi.scanNetworksAsync(scanNetwork);
      int n = WiFi.scanNetworks(false, true);
      scanNetwork(n);
      
      send.nodeRssi = localRssi;
      
      localRssi.clear();

      Serial.println("Устройства просканированы, начинаю рассылать всем подряд)");
      
      //  цикл по все  живым нодам, каждая Нода конечная, путь не известен
      for (auto &node : AllAvaliableNodeID) {
        // ноде инициатору отправить в последнюю очередь ведь он дальше обратится к следующей ноде
        if (node != recv.from) {
          send.to = node;
          
          Serial.print("    отсылаю: ");
          Serial.println(ArrayMACToString(idToArray(node).data()));
          
          for (auto &lavanode : AllAvaliableNodeID) {           // 
             // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
            //uint8_t k = macPosFromID(lavanode);
            //Serial.println("sending 22 with help of node = ");
            //Serial.print(lavanode);
            esp_now_send(idToArray(lavanode).data(), (uint8_t *)&send, sizeof(send));
          }
        }
        delay(2000);
      }
      //timer2 = millis();
      //while (millis() - timer2 < 2000)
      //{
      //}
      Serial.println("Отсылаем инициализатору");
      send.to = recv.from; // обращение к ноде иницатору.
      for (auto lavanode : AllAvaliableNodeID) {
        // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
        //uint8_t k = macPosFromID(lavanode);
        //Serial.println("sending 22 iniziator with help of node = ");
        //Serial.println(lavanode);
        esp_now_send(idToArray(lavanode).data(), (uint8_t *)&send, sizeof(send));
      }
    } else {
      Serial.println("Сообщение с массивом не мне, рассылаю всем");
      for (auto lavanode : AllAvaliableNodeID)
      { // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
        //uint8_t k = macPosFromID(lavanode);
        //Serial.println("send to node = ");
        //Serial.print(lavanode);
        esp_now_send(idToArray(lavanode).data(), (uint8_t *)&recv, sizeof(recv));
      }
    }
  } else if (recv.message_type == 22 and recv.transid != LastMessageId) { // ТИП 22
    LastMessageId = recv.transid;
    Serial.println("Отправка обратно массива силы сигнала");

    if (mymacID == recv.to) {
      Serial.println("Я конечный, я принял");
      if (RandomFuckingVariable != false) { // Добавляем свои значения, только один раз
        //WiFi.scanNetworksAsync(scanNetwork);
        int number_of_node_around = WiFi.scanNetworks(false, true);
        scanNetwork(number_of_node_around);

        Serial.println("Добавляю в массив смежные RSSI");

        //SignalStrenghtInNode.push_back(localRssi);     //recv.nodeRssi
        My_con->setSignStren(&localRssi);
        localRssi.clear();

        RandomFuckingVariable = false;
      }
      Serial.println("Pushback recived pair to SignalStrengthInNode");
      
      ii++;
      
      //SignalStrenghtInNode.push_back(recv.nodeRssi); // принимаем пары от платы, заполняем в класс My con
      Serial.print("Добавляю в массив RSSI от ");
      Serial.println(ArrayMACToString(idToArray(recv.from).data()));

      My_con->setSignStren(&recv.nodeRssi, recv.from);
      
      if (ii = AllAvaliableNodeID.size()) {
        Serial.println("ДОПУСТИМ МЫ ЭТО ДОЛЖНЫ ДЕЛАТЬ ТУТ");          // выпилить нафиг это легаси
        My_con->searchOptimal(); // Надо выполнить лишь один раз в конце. всех 22 сообщений
      }
    } else {
      Serial.println("Отправка массива уровней сигнала не мне, пересылаю");
      for (auto lavanode : AllAvaliableNodeID) { // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
      //  uint8_t k = macPosFromID(lavanode);
      //  Serial.println("Send 22 mess to: ");
      //  Serial.print(lavanode);
        esp_now_send(idToArray(lavanode).data(), (uint8_t *)&recv, sizeof(recv));
      }
    }
  }
}

void setup()
{
  srand(time(0)); // автоматическая рандомизация

  delay(100);
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("HELLO FROM NODE ttyUSB0 with ID: ");
  delay(100);

  /******************************************************************/
  WiFi.mode(WIFI_AP_STA);

  WiFi.macAddress(mmac); // получение своего MAC-Адреса

  myMac.assign(mmac, mmac + 6); //  Запись его в массив myMac

  mymacID = myMac[0];

  My_con = new connect(mymacID, 6);

  for (int i = 0; i < myMac.size(); ++i)
    Serial.print(String(myMac[i]) + ":");

  /******************************************************************/

  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  for (int i = 0; i < AllNodeID.size(); i++)
    if (AllNodeID[i] != mymacID)
      esp_now_add_peer(idToArray(AllNodeID[i]).data(), ESP_NOW_ROLE_COMBO, CHANNEL, NULL, 0);

  struct_message setupmessage;
  setupmessage.from = mymacID;
  setupmessage.message_type = 11;
  
  //uint64_t sendnode = 0;
  
  int num_of_device = WiFi.scanNetworks();
  uint64_t sendnode = foundnode(num_of_device);
  //while ( sendnode == 0) {
  //  num_of_device = WiFi.scanNetworks();
  //  sendnode = foundnode(num_of_device);
  //}

  setupmessage.to = sendnode;
  
  esp_now_send(idToArray(sendnode).data(), (uint8_t *)&setupmessage, sizeof(setupmessage));
}

void loop()
{
  // Serial.print('fuck');
  // for (int &node_id : arrayAllNode) {                                             // Чтобы отправить в узел n нужно предварительно отправить в узел ...
  //         std::cout << "To send a message to " << node_id<< " I have to send a message to "
  //                   << My_con->getNextNode(node_id) <<std::endl;            // -1 там где пути нет
  //     }
}
