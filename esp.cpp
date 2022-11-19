#include <Arduino.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <iostream>
#include <connect.h>
#include <ctime>
#include <stdlib.h>
#include <string>
//#include <ArduinoJson.h>
//#include <main2.h>

uint8_t CHANNEL = 1;
int i;
uint8_t ii=1;
int nodeid;
//StaticJsonDocument<100> jsonMessage;
unsigned long timer;
unsigned long timer2;
char buffer[100];
bool FoundNodes = false;
bool recvBool;
bool boolsendStatus = true;
bool test = true;
uint8_t lastTransid12;
uint8_t lastTransid13;
uint8_t lastTransid21;
uint8_t lastTransid22;
uint8_t mmac[6];
std::vector<uint8_t> myMac;
//std::string AP_SSID = "node " + std::to_string(myMac[0]);
int mymacID;
std::vector<std::vector<uint8_t>> macadrrs = {{0xBC, 0xDD, 0xC2, 0x26, 0x74, 0x97},{0xCC, 0x50, 0xE3, 0x35, 0x19, 0x09}};
const uint8_t len1 = macadrrs.size();
const uint8_t len2 = 6;
std::vector<int> arrayAllNode(len1);
std::vector<int>aliveAllnode;
std::vector<std::pair<id, int>> localRssi;
std::vector<std::vector<std::pair<id, int>>> SignalStrenghtInNode;
typedef struct struct_message {
    std::vector<uint8_t> from;
    int to;
    int transid;
    int message_type;
    char buff[100];
    std::vector<int> alnode;
    std::vector<std::pair<id, int>> nodeRssi;
} struct_message;

struct_message setmessage;
struct_message recv;
connect My_con(mymacID, 6);//Теперь уровень сигнала можно настроить при создании

uint8_t macFromid(int id){  //из id выдаёт порядковый номер в списке мак адресов всех нод.
  for (uint8_t i=0;i<len1;i++){
    if (id == macadrrs[i][0]){
      return i;
    }
  }
  return -1;
}

uint8_t* macFromid2(int id){
  for (uint8_t i=0;i<len1;i++){
    if (id == macadrrs[i][0]){
      return &macadrrs[i][0];
    }
  }
  return 0;
}


void scanNetwork(int networksFound)   // Вызывается из функции WiFi.scanNetworkAsync(), цикл в цикле сравнений мак адресов всех сетей с мак адресами всех нод 
{
  std::vector<uint8_t> netmac;
  Serial.println("found nets");
  Serial.println(networksFound);
  for (int i = 0; i < networksFound; i++)
  {
    netmac.assign(WiFi.BSSID(i), WiFi.BSSID(i) + 6);
    netmac[0]-=2; // softap мак адрес наших плат на два больше чем классический station mac
    for (int j=0;j<aliveAllnode.size();j++){
      int k = macFromid(aliveAllnode[j]);
      if (macadrrs[k] == netmac){
        localRssi.push_back(std::make_pair(aliveAllnode[j],WiFi.RSSI(i)));
    }
    }
  }
}

void scanNetwork0(int networksFound) // Такая же функция как и выше, но используется в начале для поиска платы среди всех а не живых к кторой подключиться.
{
  std::vector<uint8_t> netmac;
  //Serial.println("found nets");
  //Serial.println(networksFound);
  for (int i = 0; i < networksFound; i++)
  {
    Serial.println(WiFi.SSID(i));
    //Serial.println(WiFi.BSSIDstr(i));
    Serial.println(WiFi.RSSI(i));
    netmac.assign(WiFi.BSSID(i), WiFi.BSSID(i) + 6);
    netmac[0]-=2;
    for (int j=0;j<arrayAllNode.size();j++){
      int k = macFromid(arrayAllNode[j]);
      if (macadrrs[k] == netmac){
        localRssi.push_back(std::make_pair(arrayAllNode[j],WiFi.RSSI(i)));
    }
    }
  }
}

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) { //функция вызывается при каждой отправке, может знать дошло смс до получателя или нет
  boolsendStatus = sendStatus;
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0){
    Serial.println("Delivery success");
  }
  else{
    Serial.println("Delivery fail");
  }
}

uint8_t foundnode(){ //Функция ищет ноду с наименьшим rssi для подключения к ней этап 1.1
  uint8_t minrssi = 255;
  int currentid = 0;
  Serial.println("starting scaning network");
  WiFi.scanNetworksAsync(scanNetwork0); //scanNEtwork0 вызывается после этой функции.
  for (auto i:localRssi){
    if(abs(i.second) < minrssi){
        minrssi = i.second;
        currentid = i.first;
}
}
 //for (auto i:localRssi){
 //  Serial.println("pair: ");
 //  Serial.print(i.first);
 //  Serial.print(i.second);
 //}
 Serial.println("ending scaning network");
  localRssi.clear();
  return currentid;
}


int main() {      //функция переконфигурации сети                         
    struct_message send;
    std::cout << "Hello, I am " + std::to_string(My_con.getId()) + "!" << std::endl << std::endl;

    ///********** Этап 1 *************
    //1.1 на миро
    My_con.addNode(nodeid);                                                // Добавляем новый узел ******Done

    auto all_node = My_con.getNodeInNet();

    My_con.clean();                                                             //Потенциально все узлы мертвы(отключились)

//1.2. на миро 
    Serial.println("Starting sending all nodes message 12");
    for (auto iter_node:all_node) { //желаемая конечная точка.
      recvBool = false;
      send.from = myMac;
      send.to = iter_node;
      send.transid = rand() % 254;
      send.message_type = 12;
      Serial.println("Send to final node = ");
      Serial.print(iter_node);
      for (auto neibour_node:macadrrs){ // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная. 
        Serial.println("send with help of node= ");
        Serial.print((int)neibour_node[0]);
        timer = millis();
        esp_now_send(&neibour_node[0], (uint8_t *) &send, sizeof(send)); //Если сообщение перешлётся на конечную, та вернет сообщение тип 3.
        while (millis() - timer < 5000){}
        if (recvBool == true){ //если наша плата получила сообщение типа 3 и является конечной точкой она сделает bool = true, мы это увидим тут.
            Serial.print("recvbool = True, alive node = ");
            Serial.print(iter_node);
            My_con.putAnswer(iter_node); //в конце мы знаем все живые точки из массива all_node;
        }
      }
        //относится к верхнему:
        //отправляем сообщение на узел и id = i
        //если есть ответ, то добавляем его в список живых уZлOV
        //если ответа нет, то узел отключился от сети (1.3), поэтому его добавлять не нужно
    }
    std::cout << "#######################################" << std::endl<< std::endl;


    std::cout << "############# starting 2 part ##########################" << std::endl;

    ///********** Этап 2 *************
    //WiFi.scanNetworksAsync(scanNetwork);
    //SignalStrenghtInNode.push_back(localRssi);
    //My_con.setSignStren(&SignalStrenghtInNode[0]);                              // наш узел, со смежными вершинами
    id iteration_node;
    //int i=1;
    do {
        iteration_node = My_con.getIdToReconf();          //считываем очередной узел у которого должны узнать уровень сигнала до всех смежных узлов
        if (iteration_node != -1) {
          
            //std::cout << "Send message about reconfiguration to: " << iteration_node << std::endl <<
            //          "       with array: ";                                            // (2.1)
          Serial.println("sending array of all alive nodes to all alive nodes x_x");
          all_node = My_con.getNodeInNet();
          send.to = iteration_node;
           Serial.println("sending messae 21 with array of all alive nodes to node = ");
           Serial.print(iteration_node);
          send.transid = rand() % 254;
          send.message_type = 21;
          send.alnode = all_node;
            //for (auto node : all_node) std::cout << node << ' '; std::cout << My_con.getId() << std::endl;
            //....                                                                     
          for (auto node:all_node){ // Идём по всем живым платам. Есть начальная точка и конечная. 
            Serial.println("send message 21 with help of node = ");
            Serial.print(node);
            auto k = macFromid(node);
            timer = millis();
            esp_now_send(&macadrrs[k][0], (uint8_t *) &send, sizeof(send)); 
            while (millis() - timer < 6000){}
          }

            //std::cout << "Get signal strenghts from node: " << iteration_node << std::endl;
            //My_con.setSignStren(&SignalStrenghtInNode[i++], iteration_node);   // Сила сигнала смежных узлов, полученная от узла i

           // std::cout<<"GET OK STATUS, sending messages from node " << iteration_node << " is done, we can change node" << std::endl<< std::endl;
        }
    } while (iteration_node != -1);
    std::cout << "#######################################" << std::endl<< std::endl;



   // ///********** Этап 3 *************
   // My_con.searchOptimal();// Поиск отпимального пути
//
   // std::cout << "#######################################" << std::endl;
//
   // for (int i = 0; i < len1; i++){
   //     int node_id = arrayAllNode[i];
   //     std::cout << "To send a message to " << node_id<< " I have to send a message to "
   //               << My_con.getNextNode(node_id) <<std::endl;            // -1 там где пути нет
   // }
//
    return 0;
}


void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {  // функция вызывается при получении, для каждого типа смс свой сценарий
  memcpy(&recv, incomingData, sizeof(recv));
  if (recv.message_type == 11){      //ТИП 11
    nodeid = recv.from[0];
    Serial.print("recived message 11 from ");
    Serial.print((int)recv.from[0]);
    Serial.println("starting main() process");
    Serial.println();
    //macadrrs.push_back(recv.from);
    //esp_now_add_peer(&recv.from[0],ESP_NOW_ROLE_COMBO,CHANNEL,NULL,0);
    main();
  }
  if (recv.message_type == 12 and recv.transid != lastTransid12){      //ТИП 12
    lastTransid12 = recv.transid;
    if (mymacID == recv.to){
      Serial.println("message 12 send to me");
      struct_message send;
      send.from = myMac;
      send.to = recv.from[0];
      send.transid = rand() % 254;
      send.message_type = 13;
      Serial.println("send message 13 back to node = ");
      Serial.print(recv.from[0]);
      for (auto neibour_node:macadrrs){ // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная. 
        Serial.println("send with help of node = ");
        Serial.print((int)neibour_node[0]);
        esp_now_send(&neibour_node[0], (uint8_t *) &send, sizeof(send));
      }
    }
    else {
      Serial.println("message 12 send not to me, start sending to others");
      for (auto neibour_node:macadrrs){ // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная. 
      Serial.println("send to node = ");
      Serial.print((int)neibour_node[0]);
        esp_now_send(&neibour_node[0], (uint8_t *) &recv, sizeof(recv)); //Если сообщение перешлётся на конечную, та вернет сообщение тип 13.
      }
    }
  }
  if (recv.message_type == 13 and lastTransid13 != recv.transid){      //ТИП 13
  lastTransid13 = recv.transid;
    if (mymacID == recv.to){
      Serial.println("recieved message 13 sent to me, making recvbool True");
      recvBool = true;
    }
    else{
      Serial.println("recieved message 13 sent not to me, sending to others");
      for (auto neibour_node:macadrrs){ // Идём по всем платам ибо не знаем сигнал соседей. Есть начальная точка и конечная. 
      Serial.println("send to node=");
      Serial.println((int)neibour_node[0]);
        esp_now_send(&neibour_node[0], (uint8_t *) &recv, sizeof(recv));
      }
    }

  }
  if (recv.message_type == 21 and recv.transid != lastTransid21){      //ТИП 21
    lastTransid21 = recv.transid;
    if (mymacID == recv.to){
      Serial.println("recived message 21 sent to me");
      aliveAllnode = recv.alnode;
      struct_message send;
      send.from = myMac;
      send.transid = rand() % 254;
      send.message_type = 22;
      Serial.println("starting scaning network");
      WiFi.scanNetworksAsync(scanNetwork);
      send.nodeRssi = localRssi;
      Serial.println("LocalRssi is found, sending it to all alive nodes:");
      for (auto node:aliveAllnode){ //  цикл по все  живым нодам, каждая Нода конечная, путь не известен 
        if (node != recv.from[0]){ // ноде инициатору отправить в последнюю очередь ведь он дальше обратится к следующей ноде
          send.to = node;
          Serial.println("Sending mes 22 to final node = ");
          Serial.print(node);
          for (auto lavanode:aliveAllnode){ // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
            uint8_t k = macFromid(lavanode);
            Serial.println("sending 22 with help of node = ");
            Serial.print(lavanode);
            esp_now_send(&macadrrs[k][0],(uint8_t *) &send,sizeof(send));
          }
        }
      }
      timer2 = millis();
      while (millis() - timer2 < 2000){}
      Serial.println("Now its time to send mess 22 to iniziator");
      send.to = recv.from[0]; //обращение к ноде иницатору.
      for (auto lavanode:aliveAllnode){ // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
            uint8_t k = macFromid(lavanode);
            Serial.println("sending 22 iniziator with help of node = ");
            Serial.println(lavanode);
            esp_now_send(&macadrrs[k][0],(uint8_t *) &send,sizeof(send));
          }
    }
    else {
      Serial.println("message 21 is not for me, sending others:");
      for (auto lavanode:aliveAllnode){ // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
            uint8_t k = macFromid(lavanode);
            Serial.println("send to node = ");
            Serial.print(lavanode);
            esp_now_send(&macadrrs[k][0],(uint8_t *) &recv,sizeof(recv));
          }
    }
  }
  if (recv.message_type == 22 and recv.transid != lastTransid22){      //ТИП 22
    lastTransid22 = recv.transid;
    if (mymacID == recv.to){
      Serial.println("recived mess 22 is for me");
      if (test !=false){ // Добавляем свои значения, только один раз
      WiFi.scanNetworksAsync(scanNetwork);
      Serial.println("Adding my own network partners rssi");
      SignalStrenghtInNode.push_back(recv.nodeRssi);
      My_con.setSignStren(&SignalStrenghtInNode[0]);
      test = false;
      }
      Serial.println("Pushback recived pair to SignalStrengthInNode");
      ii++;
      SignalStrenghtInNode.push_back(recv.nodeRssi); //принимаем пары от платы, заполняем в класс My con
      Serial.println("doing SetSignalStren");
      My_con.setSignStren(&recv.nodeRssi, recv.from[0]);
      if (ii = aliveAllnode.size()){
        Serial.println("Now all nodes have already send pairs,so start optomal searching");
      My_con.searchOptimal(); // Надо выполнить лишь один раз в конце. всех 22 сообщений
      }
    }
    else {
      Serial.println("22 mess sent not to me, sending to others:");
      for (auto lavanode:aliveAllnode){ // Лавинный проход по всем нодам в поисках Конечной ноды код сообщения 22
            uint8_t k = macFromid(lavanode);
            Serial.println("Send 22 mess to: ");
            Serial.print(lavanode);
            esp_now_send(&macadrrs[k][0],(uint8_t *) &recv,sizeof(recv));
          }
    }
  }
}



void setup() {
  Serial.begin(115200);
  srand( time( 0 ) ); // автоматическая рандомизация
  WiFi.mode(WIFI_AP_STA);
  uint8_t sendnode = 0;
  WiFi.macAddress(mmac);
  myMac.assign(mmac,mmac+6);
  mymacID = myMac[1];
  for (i=0; i<len1;i++){ // Заполняем массив ID плат
  arrayAllNode[i] = macadrrs[i][0];
  }

  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  for (i=1;i<len1;i++) {
    esp_now_add_peer(&macadrrs[i][0],ESP_NOW_ROLE_COMBO, CHANNEL,NULL,0);
  }

  setmessage.from = myMac;
  setmessage.message_type = 11;
  Serial.println();
  Serial.println("Trying to find some nodes to connect");
//  do{
//  sendnode = foundnode();
//  Serial.println();
//  if (sendnode != 0){
//  Serial.println("found some node,id: ");
//  Serial.println(sendnode);
//  auto k = macFromid(sendnode);
//  while (boolsendStatus == true){
//  esp_now_send(&macadrrs[k][0],(uint8_t *) &setmessage, sizeof(setmessage));
//  delay(3000);
//  }
//  }
//delay(3000);
//}
//while(sendnode==0);
//}
  sendnode = foundnode();
  Serial.println("first");
  Serial.println(sendnode);
  sendnode = foundnode();
  Serial.println("second");
  Serial.println(sendnode);
  sendnode = foundnode();
  delay(8000);
  Serial.println("third");
  Serial.println(sendnode);
  if (sendnode != 0){
  Serial.println("found some node,id: ");
  Serial.println(sendnode);
  auto k = macFromid(sendnode);
  esp_now_send(&macadrrs[k][0],(uint8_t *) &setmessage, sizeof(setmessage));
}
 Serial.print(sendnode);

 }



void loop() {
  //Serial.print('fuck');
//for (int &node_id : arrayAllNode) {                                             // Чтобы отправить в узел n нужно предварительно отправить в узел ...
//        std::cout << "To send a message to " << node_id<< " I have to send a message to "
//                  << My_con.getNextNode(node_id) <<std::endl;            // -1 там где пути нет
//    }

}