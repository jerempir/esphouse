#include <Arduino.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <iostream>
#include <connect.h>
#include <json.h>
#include <string>
#define MESSAGE_BUFFER_SIZE 230

//###############################################################################################
//###############################################################################################
//###############################################################################################












//###############################################################################################
//###############################################################################################
//###############################################################################################



unsigned int ii = 0; // Зачем оно?


bool FLAG_11 = false,      // Мне пришел запрос на подключение
    FLAG_12 = false,       // Мне пришло сообщение-запрос о моей "живости"
    FLAG_13 = false,       // Мне пришел ответ на запрос о живости (узел жив!)
    FLAG_21 = false,       // Мне пришел массив всех узлов, сообщение-запрос о моём окружении
    FLAG_22 = false,       // Мне пришел ответ, массив окружения от i-ого узла
    FLAG_31 = false,       // Мне прислали большой массив (все силы сигналов)
    FLAG_32 = false,       // Мне прислали ответ на большой массив (ок)
    FLAG_all_good = false, // Все подключено, все работает, все ок
    FLAG_not_for_me = false,
    FLAG_i_am_main = false;
uint8_t LastMessageId = 0; // Уникальный id сообщения, по которому проверяется пришло ли эхо или информативное сообщение
id LastSendNodeId = 0;     // id соотвествующий последней плате кому отправили сообщение (от других приниматься сообщения не будут в определенных условиях)
id LastReceivedNodeId = 0; // id последнего узла, от которого было получено сообщение
uint32_t counter_of_good=0;
std::vector<std::vector<uint8_t>> AllNodeMacAddr = {{44, 244, 50, 18, 214, 245}, {44, 244, 50, 18, 197, 196}, {188, 221, 194, 38, 116, 151}};

std::vector<uint64_t> AllNodeID(3, 0);               // = {(((((uint64_t)(44 * 256) + 244) * 256 + 50) * 256 + 18) * 256 + 214) * 256 + 245,
                                                     //(((((uint64_t)(44 * 256) + 244) * 256 + 50) * 256 + 18) * 256 + 197) * 256 + 196};
std::vector<std::pair<uint64_t, uint8_t>> localRssi; // Массив смежных узлов + вес ребра
std::vector<id> AVALIABLE;                           // черт чтобы рассылка работала, иначе при отправке на неподключенную плату словишь прикол

connect *My_con; // Теперь уровень сигнала можно настроить при создании


typedef struct struct_message {
  uint64_t from;                      // id
  uint64_t to;                        // id
  uint8_t transid;                    // message_id ( random(256) )
  uint8_t message_type;               // search in Miro
  char buff[MESSAGE_BUFFER_SIZE];     // message
} struct_message;

struct_message *LastMessageArray = nullptr; // последнее принятое сообщение


void sendToArraySS(id to);
void scanAddMy();

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

uint64_t ArrayToID(uint8_t MacAddress[6]) {
  return (((((uint64_t)(MacAddress[0] * 256) + MacAddress[1]) * 256 + MacAddress[2]) * 256 + MacAddress[3]) * 256 + MacAddress[4]) * 256 + MacAddress[5];
}

String ArrayMACToString(uint8_t MacAddress[6])
{
  return String(MacAddress[0]) + "." + String(MacAddress[1]) + "." + String(MacAddress[2]) + "." + String(MacAddress[3]) + "." + String(MacAddress[4]) + "." + String(MacAddress[5]);
}

// Возвращает позицию id в массиве VectorID
// Если такого нет, то вернет -1
int8 getIndex(std::vector<uint64_t> VectorID, uint64_t id) {
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
  if (minID == 0)
  {
    Serial.println("    Кандидатов для подключения нет");
  }
  else
  {
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
uint8_t findIDMinimalRSSI()
{

  Serial.println("Сканирования точек доступа для добавления в сеть...");
  // WiFi.scanNetworksAsync(scanNetwork0); //scanNEtwork0 вызывается после этой функции.
  // int n = WiFi.scanNetworks(false, true); // TODO Разобраться как оно работает
  scanNetwork(WiFi.scanNetworks(false, true));

  uint8_t minrssi = 255;
  uint64_t currentid = 0;

  for (auto i : localRssi)
  {
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
  bool my_checker_not_crack=false;

  for (auto &i:AllNodeID)
    if (receiveMesage.to == i) my_checker_not_crack=true;

  if (!my_checker_not_crack){
    Serial.println("Партия гордится тобой! Ты поймать враг! +150 баллов социального рейтинга");
    return;
  }

  if (receiveMesage.to != My_con->getId() and LastMessageId != receiveMesage.transid) {
    if (FLAG_i_am_main) {
      Serial.println("Я главный, меня пересылка не интересует");
      return;
    }
    FLAG_not_for_me = true;
    LastReceivedNodeId = receiveMesage.from;
    LastMessageId = receiveMesage.transid;
//    Serial.println("ulala");
    LastMessageArray = new struct_message(receiveMesage);
    Serial.println(String(receiveMesage.buff));
    
    Serial.print("И я НЕ конечный пункт назначения, конечный - ");
    Serial.println(ArrayMACToString(idToArray(receiveMesage.to).data()));
    
    return;
  }
  else if (LastMessageId == receiveMesage.transid)
  {
    Serial.print("Уже было!");
    return;
  }

  // Miro тип 1.1, пришел запрос на добавление в сеть.
  if (receiveMesage.message_type == 11)
  {
    FLAG_all_good = false; // запрос на переконфигурацию - все плохо
    FLAG_11 = true;
    LastReceivedNodeId = receiveMesage.from;
    LastMessageId = receiveMesage.transid;
  }

  // Miro тип 1.2, Узел принимает сообщение о начале переконфигурации отправляет ответ, подтверждающий его живость.
  else if (receiveMesage.message_type == 12 and receiveMesage.transid != LastMessageId)
  {
    FLAG_all_good = false; // Сообщение получают все платы, всем становится плохо
    FLAG_12 = true;
    LastMessageId = receiveMesage.transid;
    LastReceivedNodeId = receiveMesage.from;
  }

  // Miro тип 1.3. Плата-инициатор получает сообщение от живого узла (узел жив)
  else if (receiveMesage.message_type == 13 and LastMessageId != receiveMesage.transid)
  {
    FLAG_13 = true;
    LastMessageId = receiveMesage.transid;
    LastReceivedNodeId = receiveMesage.from;

    Serial.println("Miro тип 1.3, мне пришел ответ.");

    Serial.print("    Узел ");
    Serial.print(ArrayMACToString(idToArray(receiveMesage.from).data()));
    Serial.println(" жив!!!");
  }

  // Miro тип 2.1. Все узлы в сети, от которых был получен ответ,
  //   получают массив из всех узлов в сети по принципу этапа 1,
  //   взамен отсылают всем (Лавинно) массив пар содержащий id и
  //   уровень сигнала до всех смежных узлов.
  else if (receiveMesage.message_type == 21 and receiveMesage.transid != LastMessageId)
  { // ТИП 21
    FLAG_21 = true;
    LastMessageId = receiveMesage.transid;
    LastReceivedNodeId = receiveMesage.from;
    LastMessageArray = new struct_message(receiveMesage);
  }

  // Miro тип 2.2. Получение массива с уровнем сигнала
  else if (receiveMesage.message_type == 22 and receiveMesage.transid != LastMessageId)
  {
    FLAG_22 = true;
    LastMessageId = receiveMesage.transid;
    LastReceivedNodeId = receiveMesage.from;
    LastMessageArray = new struct_message(receiveMesage);

    // LastMessageId = receiveMesage.transid;
    // ii++;
    // SPESHIAL_FLAG_FOR_22 = true;
  }
  else if (receiveMesage.message_type == 31 and receiveMesage.transid != LastMessageId)
  {
    FLAG_31 = true;
    LastMessageId = receiveMesage.transid;
    LastReceivedNodeId = receiveMesage.from;
    LastMessageArray = new struct_message(receiveMesage);
  }
  else if (receiveMesage.message_type == 32 and receiveMesage.transid != LastMessageId)
  {
    FLAG_32 = true;
    LastMessageId = receiveMesage.transid;
    LastReceivedNodeId = receiveMesage.from;
    //    LastMessageArray = new struct_message(receiveMesage);
  }
  else
  {
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

void setup()
{
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

  // localRssi
  scanNetwork(WiFi.scanNetworks(false, true));

  for (auto &nodeId : AVALIABLE)
    if (nodeId != My_con->getId())
      esp_now_add_peer(idToArray(nodeId).data(), ESP_NOW_ROLE_COMBO, 0, NULL, 0); // chanel 1

  uint64_t ConnectNodeID = foundnode(WiFi.scanNetworks());

  if (ConnectNodeID != 0)
  {
    struct_message setupmessage;
    setupmessage.from = My_con->getId();
    setupmessage.message_type = 11;

    setupmessage.to = ConnectNodeID;
    setupmessage.transid = random(256);
    {
      LastMessageId = setupmessage.transid;
    }

    delay(500);
    esp_now_send(idToArray(ConnectNodeID).data(), (uint8_t *)&setupmessage, sizeof(setupmessage));
    delay(500);
    
    // AllAvaliableNodeID.push_back(ConnectNodeID);
  }
}

void loop() {
//  Serial.println(ESP_NOW_MAX_DATA_LEN);
  if (FLAG_not_for_me)
  {
    FLAG_not_for_me = false;

    Serial.print("    Сейчас мы как отправим:");
    Serial.println(String(LastMessageArray->buff));
    scanNetwork(WiFi.scanNetworks(false, true));
    // localRssi.clear();

    Serial.println("    Через: ");
    for (auto neibour_node : AVALIABLE) // TODO Уточнить, кому должны рассылать
    {                                   // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
      if (neibour_node != My_con->getId())
      {
        Serial.print("    ");
        Serial.print(ArrayMACToString(idToArray(neibour_node).data()));
        // esp_now_add_peer(idToArray(neibour_node).data(), ESP_NOW_ROLE_COMBO, 1, NULL, 0);
        delay(500);
        esp_now_send(idToArray(neibour_node).data(), (uint8_t *)LastMessageArray, sizeof(*LastMessageArray)); // Если сообщение перешлётся на конечную, та вернет сообщение тип 13.
        //delay(500);
      }
    }
    Serial.println("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
    delete LastMessageArray;
    LastMessageArray = nullptr;
  }

  // ###########################################################################
  if (FLAG_12) {
    FLAG_12 = false;
    Serial.println("<-------------------- Этап 1.2 ------------------------>");
    Serial.println("    мне пришла лавинная запрос о 'живости' узлов.");

    struct_message send;
    My_con->clean();
    send.from = My_con->getId();
    send.to = LastReceivedNodeId;
    send.transid = random(256); {
      LastMessageId = send.transid;
    }

    send.message_type = 13;

    Serial.print("    Отправляю ответ на ");
    Serial.println(ArrayMACToString(idToArray(send.to).data()));
    Serial.println("    Через: ");

    for (auto neibour_node : AVALIABLE) // TODO Уточнить, кому должны рассылать
    {                                   // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
      if (neibour_node == My_con->getId()) continue;
      Serial.print("      ");
      Serial.println(ArrayMACToString(idToArray(neibour_node).data()));
      delay(500);
      esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&send, sizeof(send));
      delay(500);
    }
  }
  if (FLAG_21)
  {
    FLAG_21 = false;

    Serial.println("<-------------------- Этап 2.1 ------------------------>");
    Serial.println("    Я получил массив со всеми живыми узлами!!");
    Serial.print("     ");
    Serial.println(String(LastMessageArray->buff));

    StaticJsonDocument<MESSAGE_BUFFER_SIZE> listNodeNet;
    DeserializationError error = deserializeJson(listNodeNet, LastMessageArray->buff);

    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    auto AllNodeInNet = listNodeNet["live_node"];

    for (unsigned int i = 0; i < AllNodeInNet.size(); ++i)
    {
      if (AllNodeInNet[i] != My_con->getId())
        My_con->putAnswer(AllNodeInNet[i]);

      //      Serial.print("      ");
      //      Serial.println(ArrayMACToString(idToArray(AllNodeInNet[i]).data()));
    }

    sendToArraySS(LastReceivedNodeId); // send array of signal streight
  }

  if (FLAG_31)
  {
    FLAG_31 = false;
    Serial.println("<-------------------- Этап 3.1 ------------------------>");
    Serial.println("    Я получил массивЫ СИЛ сигналОВ");
    Serial.print("     ");
    Serial.println(String(LastMessageArray->buff));

    struct_message ok_message;
    ok_message.from = My_con->getId();
    ok_message.to = LastReceivedNodeId;
    ok_message.transid = random(256);
    ok_message.message_type = 32;

    // TODO сделать добавление и поиск оптимального пути

    StaticJsonDocument<MESSAGE_BUFFER_SIZE> NodeConectArray;
    DeserializationError error = deserializeJson(NodeConectArray, LastMessageArray->buff);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    //auto AllNodeInNet = listNodeNet["live_node"];
    for (JsonPair iNode : NodeConectArray.as<JsonObject>()) {
      Serial.println(iNode.key().c_str());
      auto NodeStrArray = iNode.value();
      //for (auto imessage: NodeStrArray.as<JsonObject>()) {
      //  Serial.println("      " + String(imessage.key().c_str()));
      //}
      auto IDs = NodeStrArray.as<JsonObject>()["IDs"];//NodeStrArray.as<JsonObject>()[0].value();
      auto SingStr = NodeStrArray.as<JsonObject>()["SingStr"];
      

      std::vector<std::pair<id, sgnlstr>> Pretty_vector(IDs.size(), std::pair<id, sgnlstr>(0, 0)); // = listNodeNet["SingStr"];

      for (unsigned int i = 0; i < IDs.size(); ++i) {
        Pretty_vector[i].first = IDs[i];
        Pretty_vector[i].second = SingStr[i];
      }

      My_con->setSignStren(&Pretty_vector, String(iNode.key().c_str()).toInt()); // TODO разбраться в этом мракобесие
      
      
      //Serial.println();
      //Serial.println();
    }

//    for (unsigned int i = 0; i < AllNodeInNet.size(); ++i)
//    {
//      if (AllNodeInNet[i] != My_con->getId())
//        My_con->putAnswer(AllNodeInNet[i]);

      //      Serial.print("      ");
      //      Serial.println(ArrayMACToString(idToArray(AllNodeInNet[i]).data()));
//    }



    delay(500);
    esp_now_send(idToArray(LastReceivedNodeId).data(), (uint8_t *)&ok_message, sizeof(ok_message));
    delay(500);
    FLAG_all_good = true;
    My_con->searchOptimal();
  }

  if (FLAG_11) {
    FLAG_11 = false;
    FLAG_i_am_main = true;
    scanNetwork(WiFi.scanNetworks(false, true));
    Serial.println("<-------------------- Этап 1.1 ------------------------>");
    Serial.print("    ко мне хочет добавиться ");
    Serial.println(ArrayMACToString(idToArray(LastReceivedNodeId).data()));
    Serial.println();

    My_con->addNode(LastReceivedNodeId);
    std::vector<id> all_node = My_con->getNodeInNet();

    My_con->clean(); // Потенциально все узлы мертвы(отключились), проверяем массив

    Serial.println("########################################################");
    Serial.println("###########Этап 1.2, рассылка всем сообщения############");

    // 1.2. на миро
    // отправляем сообщение на узел i
    // если есть ответ, то добавляем его в список живых узлов
    struct_message message_to_start;

    for (auto iter_node : all_node) {
      if (iter_node == My_con->getId())
        continue; // TODO в правильной реализации - костыль

      message_to_start.from = My_con->getId();
      message_to_start.to = iter_node;
      message_to_start.transid = random(256);
      message_to_start.message_type = 12;

      LastMessageId = message_to_start.transid;

      Serial.print("    Отсылаем сообщение узлу ");
      Serial.println(ArrayMACToString(idToArray(iter_node).data()));

      Serial.println("        Отправляем через:");
      for (auto neibour_node : all_node) { // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
        bool ulala = false;
        for (auto ids : AVALIABLE)
          if (ids == neibour_node)
            ulala = true;
        if (neibour_node == My_con->getId() or ulala == false) {
          Serial.println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@отлов1");
          continue;
        }

        Serial.print("        ");
        Serial.println(ArrayMACToString(idToArray(neibour_node).data()));
        
        delay(500);
        esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&message_to_start, sizeof(message_to_start)); // Если сообщение перешлётся на конечную, та вернет сообщение тип 13.
        delay(15000);

        if (FLAG_13)
        { // если наша плата получила сообщение типа 1.3 и является конечной точкой она сделает SPESHIAL_FLAG_FOR_13 = true, мы это увидим тут.
          FLAG_13 = false;
          Serial.println("!!!!!!!!Она действительно жива, добавляем... ");
          My_con->putAnswer(iter_node); // в конце мы знаем все живые точки из массива all_node;
          //break;
        }
        else
        {
          // если ответа нет, то узел отключился от сети (1.3), поэтому его добавлять не нужно
          Serial.println("!!!!!!!!Ответа нет, плата делает плак-плак( ");
        }
      }
    }

    Serial.println("########################################################");
    Serial.println("###########Этап 2.1, рассылка всем сообщения############");

    struct_message message_array; // Создаем шаблон сообщения
    message_array.from = My_con->getId();
    message_array.message_type = 21;

    std::vector<id> live_nodes = My_con->getNodeInNet(); // считываем очередной узел у которого должны узнать уровень сигнала до всех смежных узлов

    /// while (send_node != (id)-1 and send_node > 0) {
    for (auto &iter_node : live_nodes)
    {
      message_array.to = iter_node; // Изменяем шаблон под конкретный узел-получатель
      message_array.transid = random(256);
      LastMessageId = message_array.transid;

      // auto neibour_node = My_con->getNodeInNet();

      Serial.print("    Отправляю массив id для: ");
      Serial.println(ArrayMACToString(idToArray(iter_node).data()));

      StaticJsonDocument<MESSAGE_BUFFER_SIZE> My_Array_of_live_node;
      JsonArray data = My_Array_of_live_node.createNestedArray("live_node");

      for (auto node_to_data : live_nodes) {
        data.add(node_to_data);
      }
      data.add(My_con->getId());

      String output;
      serializeJson(My_Array_of_live_node, output);
      //Serial.println(output);
      strncpy(message_array.buff, output.c_str(), MESSAGE_BUFFER_SIZE); //sizeof(message_array.buff)

      Serial.print("    ");
      Serial.println(output);

      Serial.println("    Отправляю через: ");
      for (auto neibour_node : live_nodes)
      { // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
        bool ulala = false;
        for (auto ids : AVALIABLE)
          if (ids == neibour_node)
            ulala = true;
        if (neibour_node == My_con->getId() or !ulala)
        {
          Serial.println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@отлов2");
          continue;
        }

        Serial.print("      ");
        Serial.println(ArrayMACToString(idToArray(neibour_node).data()));

        delay(500);
        if (FLAG_22 == true)
          break;                                                                                        // Сообщение уже получено, смысла слать больше нет!
        esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&message_array, sizeof(message_array)); // Если сообщение перешлётся на конечную, та вернет сообщение тип 13.
        delay(500);
        
      }

      delay(15000);

      if (FLAG_22) {
        FLAG_22 = false;
        Serial.println("<-------------------- Этап 2.2 ------------------------>");
        Serial.println("    Я получил массива силы сигнала");
        //    Serial.print("     "); Serial.println(ArrayMACToString(idToArray(receiveMesage.from).data()));
        Serial.print("     ");
        Serial.println(String(LastMessageArray->buff));

        StaticJsonDocument<MESSAGE_BUFFER_SIZE> listNodeNet;
        DeserializationError error = deserializeJson(listNodeNet, String(LastMessageArray->buff));

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

        My_con->setSignStren(&Pretty_vector, LastReceivedNodeId); // TODO разбраться в этом мракобесие

        //    if (ii == My_con->getNodeInNet().size()) {
        //      SPESHIAL_FLAG_FOR_31 = true;
        //    }
        //    Serial.println("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
      } else {
        Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
        Serial.println("ответ 2.2 не получен");
        while (1)
          delay(1);
      }
    }

    scanAddMy();

    Serial.println("########################################################");
    Serial.println("###########Этап 3.1, рассылка всем сообщения############");
    
    uint16_t counter_of_good=0;
    live_nodes = My_con->getNodeInNet(); // считываем очередной узел у которого должны узнать уровень сигнала до всех смежных узлов
    live_nodes.push_back(My_con->getId());

    StaticJsonDocument <MESSAGE_BUFFER_SIZE> listG;                                           // TODO разбраться в этом мракобесие
    //JsonArray myGindex = listG.createNestedArray("G");

    for (auto &iter_node : live_nodes) {
      //Serial.println(iter_node);
      JsonArray IDs = listG[String(iter_node)].createNestedArray("IDs");
      JsonArray SingStr = listG[String(iter_node)].createNestedArray("SingStr");

      std::vector<std::pair<id, sgnlstr>> My_i = My_con->getSignStren(iter_node);

      for (auto &i : My_i) {
        IDs.add(i.first);
        SingStr.add(i.second);
      }
    }

    String output;
    serializeJson(listG, output);
    
    Serial.println(output);
    struct_message sendG;
    strncpy(sendG.buff, output.c_str(), MESSAGE_BUFFER_SIZE);  //sizeof(sendG.buff)
    sendG.from = My_con->getId();
    sendG.message_type = 31;

    live_nodes = My_con->getNodeInNet(); // считываем очередной узел у которого должны узнать уровень сигнала до всех смежных узлов
    // прошлый не годится там есть myID
    for (auto &iter_node : live_nodes) {                                                        // TODO сок бесобесия, концентрат
      sendG.transid = random(256);
      LastMessageId = sendG.transid;
      sendG.to = iter_node;

      for (auto neibour_node : live_nodes) { // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная.
        bool ulala = false; for (auto ids : AVALIABLE) if (ids == neibour_node) ulala = true;
        if (neibour_node == My_con->getId() or !ulala){ Serial.println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@отлов666"); continue; }

        Serial.print("      ");
        Serial.println(ArrayMACToString(idToArray(neibour_node).data()));

        delay(500);
        if (FLAG_32 == true) break;                                                                     // Сообщение уже получено, смысла слать больше нет!
        esp_now_send(idToArray(neibour_node).data(), (uint8_t *)&sendG, sizeof(sendG)); // Если сообщение перешлётся на конечную, та вернет сообщение тип 13.
        delay(500);
        
      }
      delay(10000);
      if (FLAG_32 == true) {
        Serial.println("ВСЁ ОКЕЕЕЕЕЕЕЕЕЕЙ");
        counter_of_good++;
        FLAG_32=false;
      } else {
        Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
        Serial.println("ответ 3.2 не получен");
        while (1) delay(1);
      }
    }
    
    if (counter_of_good == My_con->getNodeInNet().size()){
      FLAG_all_good = true;
      FLAG_i_am_main = false;
      My_con->searchOptimal();
    }
  }

  if (FLAG_all_good)
  {
    Serial.println("ALLGOOD");
    digitalWrite(0, HIGH);
    digitalWrite(2, HIGH);
    delay(1000);
    digitalWrite(0, LOW);
    digitalWrite(2, LOW);
    delay(1000);
  }
  else
  {
    Serial.print(".");
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
  StaticJsonDocument<MESSAGE_BUFFER_SIZE> listNodeNetAndSignStr;
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

void scanAddMy(){
  scanNetwork(WiFi.scanNetworks(false, true)); // Отсылаем всем, заодно записываем себе
  Serial.println("      Добавляю в массив смежные RSSI");
  My_con->setSignStren(&localRssi, My_con->getId());
  //localRssi.clear();
}


void sendToArraySS (id to) {
  struct_message send;
  send.from = My_con->getId();
  send.message_type = 22;
  // send.transid = random(256);
  scanAddMy();

  String output = idSSToString(My_con->getSignStren(My_con->getId())); //output = idSSToString(localRssi);              //насколько правильно?             фывпфывпфпфыпвфыпвыпвывпфыпфывфыафафыавфвафвпфвпфвпф
                //насколько правильно?             фывпфывпфпфыпвфыпвыпвывпфыпфывфыафафыавфвафвпфвпфвпф
  
  Serial.println("Устройства просканированы, начинаю рассылать ПРАВИЛЬНОМУ через всех доступных)");
  Serial.println(output);
  output.toCharArray(send.buff, MESSAGE_BUFFER_SIZE);

  std::vector<id> AvaliableNodes = My_con->getNodeInNet(); //  getNodeInNet            // TODO Разобраться с AllAvaliableNodeID
  /*
  //  цикл по все  живым нодам, каждая Нода конечная, путь не известен
  for (auto &node : AvaliableNodes) {
    // ноде инициатору отправить в последнюю очередь ведь он дальше обратится к следующей ноде
    if ((node != to) and (node != My_con->getId()))
    {
      send.to = node;
      send.transid = random(256);
      {
        LastMessageId = send.transid;
      }

      Serial.print("    отсылаю: ");
      Serial.println(ArrayMACToString(idToArray(node).data()));

      for (auto &lavanode : AvaliableNodes)
      { //
        // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
        if (lavanode != My_con->getId())
        {
          bool ulala = false;
          for (auto ids : AVALIABLE)
            if (ids == lavanode)
              ulala = true;
          if (lavanode == My_con->getId() or !ulala)
          {
            Serial.println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@отлов3");
            continue;
          }

          delay(500);
          esp_now_send(idToArray(lavanode).data(), (uint8_t *)&send, sizeof(send));
          delay(500);
        
        }
      }
    }
    delay(10000);
  }
  */
  //Serial.println("Отсылаем ПРАВИЛЬНОМУ");

  send.to = to; // обращение к ноде иницатору.
  send.transid = random(256);
  LastMessageId = send.transid;
  
  for (auto lavanode : AvaliableNodes) {
    // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
    if (lavanode != My_con->getId()) {
      bool ulala = false; for (auto ids : AVALIABLE) if (ids == lavanode) ulala = true;
      
      if (lavanode == My_con->getId() or !ulala) { Serial.println("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@отлов4"); continue; }

      Serial.print("        ");
      Serial.println(ArrayMACToString(idToArray(lavanode).data()));

      delay(500);
      esp_now_send(idToArray(lavanode).data(), (uint8_t *)&send, sizeof(send));
      delay(500);
    }
  }
}
