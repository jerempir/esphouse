#include <Arduino.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <iostream>
#include <connect.h>
#include <json.h>
#include <string>

unsigned int ii = 0; // Зачем оно?

bool SPESHIAL_FLAG_FOR_13 = false,
     SPESHIAL_FLAG_FOR_START = false,
     SPESHIAL_FLAG_FOR_21 = false,
     SPESHIAL_FLAG_FOR_22 = false,
     SPESHIAL_FLAG_FOR_31 = false;

bool ALLGOOD = false;
bool check_IamInitiator = false;      // нужен для приема правильного сообщения в пункте 2.2
id check_LastSendId = 0;
id From21 = 0;



uint8_t LastMessageId;

std::vector<std::vector<uint8_t>> AllNodeMacAddr = {{44, 244, 50, 18, 214, 245}, {44, 244, 50, 18, 197, 196}, {188, 221, 194, 38, 116, 151}};

std::vector<uint64_t> AllNodeID(3,0);// = {(((((uint64_t)(44 * 256) + 244) * 256 + 50) * 256 + 18) * 256 + 214) * 256 + 245,
                                   //(((((uint64_t)(44 * 256) + 244) * 256 + 50) * 256 + 18) * 256 + 197) * 256 + 196};
std::vector<std::pair<uint64_t, uint8_t>> localRssi; // Массив смежных узлов + вес ребра
std::vector<id> AVALIABLE;  // черт чтобы рассылка работала, иначе при отправке на неподключенную плату словишь прикол

// struct_message receiveMesage;
connect *My_con; // Теперь уровень сигнала можно настроить при создании

void   sendToArraySS(id to);

// uint8_t ( &idToArray (uint64_t id)) [6] {         // сложная конструкция для возврата массива из 6 uint8_t
std::vector<uint8_t> idToArray(uint64_t id)
{
  std::vector<uint8_t> MacAddress(6);
  for (int i = 0; i < 6; ++i)
  {
    MacAddress[5 - i] = id % 256;
    id /= 256;
  }
  return MacAddress;
}

uint64_t ArrayToID(uint8_t MacAddress[6])
{
  return (((((uint64_t)(MacAddress[0] * 256) + MacAddress[1]) * 256 + MacAddress[2]) * 256 + MacAddress[3]) * 256 + MacAddress[4]) * 256 + MacAddress[5];
}

String ArrayMACToString(uint8_t MacAddress[6])
{
  return String(MacAddress[0]) + "." + String(MacAddress[1]) + "." + String(MacAddress[2]) + "." + String(MacAddress[3]) + "." + String(MacAddress[4]) + "." + String(MacAddress[5]);
}

typedef struct struct_message
{
  uint64_t from;        // id
  uint64_t to;          // id
  uint8_t transid;      // message_id ( random(256) )
  uint8_t message_type; // search in Miro
  char buff[100];       // message
  //  std::vector<uint64_t> allnode;  // all node id
  //  std::vector<std::pair<uint64, uint8_t>> nodeRssi;
} struct_message;

struct_message *MessageNotToMe = nullptr;

// Возвращает позицию id в массиве VectorID
// Если такого нет, то вернет -1
int8 getIndex(std::vector<uint64_t> VectorID, uint64_t id)
{
  auto it = std::find(VectorID.begin(), VectorID.end(), id);

  // If element was found
  if (it != VectorID.end())
  {
    // calculating the index
    // of K
    uint8_t index = it - VectorID.begin();
    return index;
  }
  else
  {
    // If the element is not
    // present in the vector
    return -1;
  }
}


// Заполняет массив localRssi - массив узлов
// и уровней сигналов к ним, доступный напрямую
void scanNetwork(uint8_t networksFound)
{
  Serial.print("<---- Сканирование всех доступных сетей: ");
  Serial.print(networksFound);
  Serial.println(" ---->");
  localRssi.clear();
  AVALIABLE.clear();

  for (int i = 0; i < networksFound; i++)
  {
    std::vector<uint8_t> netmac(0);
    netmac.assign(WiFi.BSSID(i), WiFi.BSSID(i) + 6);
    netmac[0] -= 2; // softap мак адрес наших плат на два больше чем классический station mac

    int indexInAll = getIndex(AllNodeID, ArrayToID(netmac.data()));

    if (indexInAll != -1)
    {
//      Serial.println("Добавляю: ");
//      Serial.print(ArrayMACToString(netmac.data()));
      localRssi.push_back(std::make_pair(ArrayToID(netmac.data()), abs(WiFi.RSSI(i))));
      AVALIABLE.push_back(ArrayToID(netmac.data()));
    }
//    else
//    {
//      Serial.println("Не добавляю: ");
//      Serial.print(ArrayMACToString(netmac.data()));
//    }
  }
}

// Возвращает оптимальный id
// для начального подключения
uint64_t foundnode(int networksFound)
{
//  Serial.print("   Сканирование всех доступных сетей: ");
//  Serial.print(networksFound);
//  Serial.println("штук");

  uint8_t minRSSI = 255;
  uint64_t minID = 0;

  for (int i = 0; i < networksFound; i++)
  {
    std::vector<uint8_t> netmac;
    netmac.assign(WiFi.BSSID(i), WiFi.BSSID(i) + 6);
    netmac[0] -= 2; // softap мак адрес наших плат на два больше чем классический station mac
                    //    Serial.println(ArrayToID(netmac.data()));
                    //    Serial.print(AllNodeID[0]);
                    //    Serial.print("<---->");
                    //    Serial.println(AllNodeID[1]);

    int indexInAll = getIndex(AllNodeID, ArrayToID(netmac.data()));
    //    int indexInAval = getIndex(AllAvaliableNodeID, ArrayToID(netmac.data()));
    // for (int j=0; j<6; ++j){
    //  Serial.print(netmac.data()[j]);
    //  Serial.print(".");
    //}
    //    Serial.println();
    //    Serial.println(indexInAll);

    if (indexInAll != -1 and abs(WiFi.RSSI(i)) < minRSSI)
    {

      minID = ArrayToID(netmac.data());
      minRSSI = abs(WiFi.RSSI(i));
    }
  }
  if (minID == 0){
    Serial.println("    Кандидатов для подключения нет");
  }
  else {
    Serial.print("    Оптимальный: ");
    Serial.println(ArrayMACToString(idToArray(minID).data()));
  }
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
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus)
{

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
  //int n = WiFi.scanNetworks(false, true); // TODO Разобраться как оно работает
  scanNetwork(WiFi.scanNetworks(false, true));

  uint8_t minrssi = 255;
  uint64_t currentid = 0;

  for (auto i : localRssi) {
    if (i.second < minrssi)
    {
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
// void add(){ }

// ########################################################################################################
// ########################################################################################################
// ########################################################################################################
// ########################################################################################################
// Посмотреть когда стоит менять receiveMesage.transid

// Допустим, парашная параша
// (функция вызывается при получении, для каждого типа смс свой сценарий)
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len)
{
  struct_message receiveMesage;
  memcpy(&receiveMesage, incomingData, sizeof(receiveMesage));

  Serial.println("########################################");

  if (receiveMesage.to != My_con->getId() and LastMessageId != receiveMesage.transid) {
    LastMessageId = receiveMesage.transid;

    Serial.println("И я НЕ конечный пункт назначения");
    //while(1) ;
    Serial.print("Отправляю ответ на ");
    Serial.println(ArrayMACToString(idToArray(receiveMesage.to).data()));
    Serial.println("    Через: ");
    MessageNotToMe = new struct_message(receiveMesage);                         ////TODO - проверить работоспособность
    return;
  }

  // Miro тип 1.1, пришел запрос на добавление в сеть.
  if (receiveMesage.message_type == 11)
  {
    ALLGOOD = false;
    SPESHIAL_FLAG_FOR_START = true;
    IamInitiator = true;                  
    //    My_con->clean();
    uint64_t incomingID = receiveMesage.from;
    //Этап 1.1, ко мне хочет добавиться 44.244.50.18.214.245
    Serial.println("<-------------------- Этап 1.1 ------------------------>");
    Serial.print("    ко мне хочет добавиться ");
    Serial.println(ArrayMACToString(idToArray(incomingID).data()));
    Serial.println();
    My_con->addNode(incomingID); // Добавляем новый узел
  }

  // Miro тип 1.2, Узел принимает сообщение о начале переконфигурации отправляет ответ, подтверждающий его живость.
  else if (receiveMesage.message_type == 12 and receiveMesage.transid != LastMessageId)
  {
    LastMessageId = receiveMesage.transid;
    Serial.println("Этап 1.2, мне пришла лавинная рассылка о 'живости' узлов.");

    struct_message send;
    My_con->clean();
    send.from = My_con->getId();
    send.to = receiveMesage.from;
    send.transid = random(256);
          {     LastMessageId = send.transid;     }

    send.message_type = 13;

    Serial.print("Отправляю ответ на ");
    Serial.println(ArrayMACToString(idToArray(send.to).data()));
    Serial.println("    Через: ");

    //int n = WiFi.scanNetworks(false, true); // TODO Разобраться как оно работает
    //scanNetwork(WiFi.scanNetworks(false, true));

    for (auto neibour_node : AVALIABLE) // TODO Уточнить, кому должны рассылать
    {                                   // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
      if (neibour_node == My_con->getId()) continue;
      Serial.print("    ");
      Serial.println(ArrayMACToString(idToArray(neibour_node).data()));
      esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&send, sizeof(send));
    }
    //Serial.println("ВСЕ ОК");
    //delay(100);
    
    //localRssi.clear();
  }

  // Miro тип 1.3. Плата-инициатор получает сообщение от живого узла (узел жив)
  else if (receiveMesage.message_type == 13 and LastMessageId != receiveMesage.transid) {
    LastMessageId = receiveMesage.transid;
    Serial.println("Miro тип 1.3, мне пришел ответ.");

    Serial.print("    Узел "); Serial.print(ArrayMACToString(idToArray(receiveMesage.from).data()));
    Serial.println(" жив!!!");

    SPESHIAL_FLAG_FOR_13 = true; // TODO разобраться с этой Сатанщиной
  }

  // Miro тип 2.1. Все узлы в сети, от которых был получен ответ,
  //   получают массив из всех узлов в сети по принципу этапа 1,
  //   взамен отсылают всем (Лавинно) массив пар содержащий id и
  //   уровень сигнала до всех смежных узлов.
  else if (receiveMesage.message_type == 21 and receiveMesage.transid != LastMessageId) { // ТИП 21
    LastMessageId = receiveMesage.transid;

    Serial.println("Miro тип 2.1");
    Serial.println("Я получил массив со всеми живыми узлами!!");

    StaticJsonDocument<200> listNodeNet;
    DeserializationError error = deserializeJson(listNodeNet, receiveMesage.buff);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    auto AllNodeInNet = listNodeNet["live_node"];

    for (unsigned int i = 0; i < AllNodeInNet.size(); ++i)
    {
      if (AllNodeInNet[i] != My_con->getId()) My_con->putAnswer(AllNodeInNet[i]);

      Serial.print("    ");
      Serial.println(ArrayMACToString(idToArray(AllNodeInNet[i]).data()));
    }

    SPESHIAL_FLAG_FOR_21 = true;
    From21 = receiveMesage.from;
  }
  // Miro тип 2.2. Получение массива с уровнем сигнала
  else if (receiveMesage.message_type == 22 and receiveMesage.transid != LastMessageId) {
    LastMessageId = receiveMesage.transid;
    ii++;
    SPESHIAL_FLAG_FOR_22 = true;

    Serial.println("Я получил массива силы сигнала");
    Serial.print("     "); Serial.println(ArrayMACToString(idToArray(receiveMesage.from).data()));
    Serial.print("     "); Serial.println(String(receiveMesage.buff));

    StaticJsonDocument<200> listNodeNet;
    DeserializationError error = deserializeJson(listNodeNet, String(receiveMesage.buff));

    if (error) {
      Serial.println(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    auto IDs = listNodeNet["IDs"];
    auto SingStr = listNodeNet["SingStr"];
    std::vector<std::pair<id, sgnlstr>> Pretty_vector(IDs.size(), std::pair<id, sgnlstr>(0, 0)); // = listNodeNet["SingStr"];

    for (unsigned int i = 0; i < IDs.size(); ++i) {
      Pretty_vector[i].first = IDs[i];
      Pretty_vector[i].second = SingStr[i];
    }

    My_con->setSignStren(&Pretty_vector, receiveMesage.from); // TODO разбраться в этом мракобесие

    if (ii == My_con->getNodeInNet().size()) {
      SPESHIAL_FLAG_FOR_31 = true;
    }
    Serial.println("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");

  } 
   else {
    Serial.println("Случилась кракозябра: ");
    Serial.print("receiveMesage.message_type = ");
    Serial.println(receiveMesage.message_type);
    Serial.print("receiveMesage.transid = ");
    Serial.println(receiveMesage.transid);
    Serial.print("LastMessageId = ");
    Serial.println(LastMessageId);
    Serial.println("---------------------");
  }
}

void setup() {
  AllNodeID[0] = ArrayToID(AllNodeMacAddr[0].data());
  AllNodeID[1] = ArrayToID(AllNodeMacAddr[1].data());
  AllNodeID[2] = ArrayToID(AllNodeMacAddr[2].data());

  pinMode(0, OUTPUT);
  pinMode(2, OUTPUT);

  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("########################################");
  Serial.print("HELLO FROM NODE ttyUSB0 with ID: ");

  /******************************************************************/
  uint8_t mmac[6];
  WiFi.mode(WIFI_AP_STA);
  WiFi.macAddress(mmac); // получение своего MAC-Адреса

  My_con = new connect(ArrayToID(mmac), 6);

  Serial.println(ArrayMACToString(mmac));
  /******************************************************************/

  if (esp_now_init() != 0)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  //localRssi
  scanNetwork(WiFi.scanNetworks(false, true));

  for (auto &nodeId: AVALIABLE)
    if (nodeId != My_con->getId())
      esp_now_add_peer(idToArray(nodeId).data(), ESP_NOW_ROLE_COMBO, 0, NULL, 0);   //chanel 1

  uint64_t ConnectNodeID = foundnode(WiFi.scanNetworks());

  if (ConnectNodeID != 0)
  {
    struct_message setupmessage;
    setupmessage.from = My_con->getId();
    setupmessage.message_type = 11;

    setupmessage.to = ConnectNodeID;
    setupmessage.transid = random(256);
              {     LastMessageId = setupmessage.transid;     }

    delay(500);
    esp_now_send(idToArray(ConnectNodeID).data(), (uint8_t *)&setupmessage, sizeof(setupmessage));
    // AllAvaliableNodeID.push_back(ConnectNodeID);
  }
}


void loop() {
  if (MessageNotToMe != nullptr){
    Serial.print("    Сейчас мы как отправим:");
    Serial.println(String(MessageNotToMe->buff));
    scanNetwork(WiFi.scanNetworks(false, true));
    //localRssi.clear();

    Serial.println("    Через: ");
    for (auto neibour_node : AVALIABLE) // TODO Уточнить, кому должны рассылать
    {                                   // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
      if (neibour_node != My_con->getId()) {
        Serial.print("    ");
        Serial.print(ArrayMACToString(idToArray(neibour_node).data()));
        //esp_now_add_peer(idToArray(neibour_node).data(), ESP_NOW_ROLE_COMBO, 1, NULL, 0);
        esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&MessageNotToMe, sizeof(MessageNotToMe)); // Если сообщение перешлётся на конечную, та вернет сообщение тип 13.
        delay(500);
      }
    }
    Serial.println("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
    delete MessageNotToMe;
    MessageNotToMe = nullptr;
  }





  if (SPESHIAL_FLAG_FOR_START) {
    scanNetwork(WiFi.scanNetworks(false, true));

    Serial.println("DS:1");
    auto all_node = My_con->getNodeInNet();

    My_con->clean(); // Потенциально все узлы мертвы(отключились)
    Serial.println("########################################");
    Serial.println("<------ Этап 1.2, рассылка всем сообщения ------>");
    
    for (auto iter_node : all_node)
    {
      // 1.2. на миро
      // отправляем сообщение на узел i
      // если есть ответ, то добавляем его в список живых узлов
      struct_message message_to_start;
      //      recvBool = false;
      message_to_start.from = My_con->getId();
      message_to_start.to = iter_node;
      message_to_start.transid = random(256);
      {     LastMessageId = message_to_start.transid;     }
      message_to_start.message_type = 12;

      Serial.print("    Отсылаем сообщение узлу ");
      Serial.println(ArrayMACToString(idToArray(iter_node).data()));

      Serial.println("        Отправляем через:");
      for (auto neibour_node : all_node)
      { // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
        bool ulala=false;for (auto ids:AVALIABLE) if (ids==neibour_node) ulala=true; 
        if (neibour_node == My_con->getId() or ulala == false) {Serial.println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@отлов1"); continue; }

        Serial.print("        ");
        Serial.println(ArrayMACToString(idToArray(neibour_node).data()));

        esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&message_to_start, sizeof(message_to_start)); // Если сообщение перешлётся на конечную, та вернет сообщение тип 13.
        delay(15000);

        if (SPESHIAL_FLAG_FOR_13 == true)
        { // если наша плата получила сообщение типа 1.3 и является конечной точкой она сделает SPESHIAL_FLAG_FOR_13 = true, мы это увидим тут.
          Serial.println("!!!!!!!!Она действительно жива, добавляем... ");
          My_con->putAnswer(iter_node); // в конце мы знаем все живые точки из массива all_node;
        }
        else
        {
          // если ответа нет, то узел отключился от сети (1.3), поэтому его добавлять не нужно
          Serial.println("!!!!!!!!Ответа нет, плата делает плак-плак( ");
        }
      }
    }
    SPESHIAL_FLAG_FOR_START = false;

    struct_message message_array; // Создаем шаблон сообщения
    message_array.from = My_con->getId();
    message_array.message_type = 21;

    //id iteration_node;
    // int i = 1;
    Serial.println("########################################");

    id iteration_node = My_con->getIdToReconf(); // считываем очередной узел у которого должны узнать уровень сигнала до всех смежных узлов
    while (iteration_node != (id)-1 and iteration_node > 0) {
        message_array.to = iteration_node; // Изменяем шаблон под конкретный узел-получатель
        message_array.transid = random(256);
                      {     LastMessageId = message_array.transid;     }

        Serial.print("    Отправляю массив id для: ");
        Serial.println(ArrayMACToString(idToArray(iteration_node).data()));

        StaticJsonDocument<200> My_Array_of_live_node;
        JsonArray data = My_Array_of_live_node.createNestedArray("live_node");

        Serial.print("    массив "); // (2.1)

        auto all_node = My_con->getNodeInNet();

        for (auto node : all_node) { data.add(node); } data.add(My_con->getId());

        String output;
        serializeJson(My_Array_of_live_node, output);
        Serial.println(output);
        strncpy(message_array.buff, output.c_str(), sizeof(message_array.buff));

        SPESHIAL_FLAG_FOR_22 = false;

        Serial.println("    Отправляю через: "); // Serial.println( iteration_node);
        for (auto neibour_node : all_node) { // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
          bool ulala=false;for (auto ids:AVALIABLE) if (ids==neibour_node) ulala=true; 
          if (neibour_node == My_con->getId() or !ulala) {Serial.println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@отлов2"); continue; }

          Serial.print("    ");
          Serial.println(ArrayMACToString(idToArray(neibour_node).data()));

          delay(500);
          esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&message_array, sizeof(message_array)); // Если сообщение перешлётся на конечную, та вернет сообщение тип 13.
        }

        //        message_array.buff = output.c_str();
        delay(15000);
        if (SPESHIAL_FLAG_FOR_22) {
          Serial.println("!!!!!!!!ОТВЕТ ПОЛУЧЕН");
        }

        // РАСКИДЫВАЕМ СВОЙ УРОВЕНЬ СИГНАЛА ВСЕМ
        Serial.println("###########################################");
        Serial.println("<--РАСКИДЫВАЕМ СВОЙ УРОВЕНЬ СИГНАЛА ВСЕМ-->");
        sendToArraySS(iteration_node);

        iteration_node = My_con->getIdToReconf(); // считываем очередной узел у которого должны узнать уровень сигнала до всех смежных узлов
    }

    SPESHIAL_FLAG_FOR_31 = true;
  }

  if (SPESHIAL_FLAG_FOR_21) {
    Serial.println("DS:2");

    sendToArraySS(From21);
    SPESHIAL_FLAG_FOR_21 = false;
    From21 = 0;
  }

  if (SPESHIAL_FLAG_FOR_31 and ii>0) {
    Serial.println("DS:3");

    Serial.println("ДОПУСТИМ МЫ ЭТО ДОЛЖНЫ ДЕЛАТЬ ТУТ"); // выпилить нафиг это легаси
    My_con->searchOptimal();                             // Надо выполнить лишь один раз в конце. всех 22 сообщений
    ALLGOOD = true;
    SPESHIAL_FLAG_FOR_31 = false;
  } else if (SPESHIAL_FLAG_FOR_31){
    Serial.println("DS:4");

    SPESHIAL_FLAG_FOR_31 = false;
    My_con->clean();
    Serial.println("КАКАЯ-ТО ############################################################################################################################## ПРОИЗОШЛА");
  }

  if (ALLGOOD)
  {
    Serial.println("DS:5");
    digitalWrite(0, HIGH);
    digitalWrite(2, HIGH);
    delay(1000);
    digitalWrite(0, LOW);
    digitalWrite(2, LOW);
    delay(1000);
  }
  else
  {
    Serial.println("DS:6");
    digitalWrite(0, HIGH);
    digitalWrite(2, HIGH);
    delay(250);
    digitalWrite(0, LOW);
    digitalWrite(2, LOW);
    delay(250);
  }
  
  delay(100);
}

String idSSToString(std::vector<std::pair<id, sgnlstr>> MyArray)
{
  StaticJsonDocument<200> listNodeNetAndSignStr;
  JsonArray SingStr = listNodeNetAndSignStr.createNestedArray("SingStr");
  JsonArray IDs = listNodeNetAndSignStr.createNestedArray("IDs");

  for (auto &i : localRssi)
  {
    IDs.add(i.first);
    SingStr.add(i.second);
  }
  localRssi.clear();

  String output;
  serializeJson(listNodeNetAndSignStr, output);
  return output;
}

void sendToArraySS(id to)
{
  struct_message send;
  send.from = My_con->getId();
  //send.transid = random(256);
  send.message_type = 22;

  scanNetwork(WiFi.scanNetworks(false, true));            //Отсылаем всем, заодно записываем себе
  Serial.println("Добавляю в массив смежные RSSI");
  My_con->setSignStren(&localRssi, My_con->getId());
  String output = idSSToString(localRssi);
  localRssi.clear();
  
  Serial.println("Устройства просканированы, начинаю рассылать всем подряд)");
  Serial.println(output);
  output.toCharArray(send.buff, 100);

  std::vector<id> AvaliableNodes = My_con->getNodeInNet(); //  getNodeInNet            // TODO Разобраться с AllAvaliableNodeID

  //  цикл по все  живым нодам, каждая Нода конечная, путь не известен
  for (auto &node : AvaliableNodes)
  {
    // ноде инициатору отправить в последнюю очередь ведь он дальше обратится к следующей ноде
    if ((node != to) and (node != My_con->getId()))
    {
      send.to = node;
      send.transid = random(256);
                            {     LastMessageId = send.transid;     }


      Serial.print("    отсылаю: ");
      Serial.println(ArrayMACToString(idToArray(node).data()));

      for (auto &lavanode : AvaliableNodes)
      { //
        // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
        if (lavanode != My_con->getId()){
          bool ulala=false;for (auto ids:AVALIABLE) if (ids==lavanode) ulala=true; 
          if (lavanode == My_con->getId() or !ulala) {Serial.println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@отлов3"); continue; }


          delay(500);
          esp_now_send(idToArray(lavanode).data(), (uint8_t *)&send, sizeof(send));
        }
      }
    }
    delay(10000);
  }

  Serial.println("Отсылаем последнему");
  
  send.to = to; // обращение к ноде иницатору.
  send.transid = random(256);
  {     LastMessageId = send.transid;     }

  for (auto lavanode : AvaliableNodes)
  {
    // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
    if (lavanode != My_con->getId()){
      delay(500);
      bool ulala=false; for (auto ids:AVALIABLE) if (ids==lavanode) ulala=true; 
      if (lavanode == My_con->getId() or !ulala) {Serial.println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@отлов4"); continue; }
      Serial.print("        ");      Serial.println(ArrayMACToString(idToArray(lavanode).data()));
      esp_now_send(idToArray(lavanode).data(), (uint8_t *)&send, sizeof(send));
    }
  }
}
