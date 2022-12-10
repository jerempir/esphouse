/*
 * ☠☠☠ ACHTUNG MINES ☠☠☠
 *
 * C Nuestro que estas en la Memoria,
 * Compilado sea tu código,
 * venga a nosotros tu software,
 * carguense tus punteros.
 * así en la RAM como en el Disco Duro,
 * Danos hoy nuestro Array de cada día,
 * Perdona nuestros Warnings,
 * así como nosotros también los eliminamos,
 * no nos dejes caer en Bucles,
 * y libranos del Windows, Enter.
 *
 */


#ifndef ALG_CONNECT_H
#define ALG_CONNECT_H

#include <queue>
#include <vector>
#include <map>
#include <vector>
#include <algorithm>

using id = int;
using vertex = int;

//Ассоциативный контейнер для id и номера вершины для поиска в глубину далее dict

class connect {
    int INT_MAX_SIGNAL = 70;
    int INT_MAX = 1000000;

    id my_id;                                                       // Задание id
    std::queue <id> all_stren;                                      // Все узлы, доступные в сети
    //std::queue <id> all_stren_help;                               // Все узлы, доступные в сети
    std::map <id, vertex> dict;                                     // Ассоциативный контейнер для id и номера вершины для поиска в глубину
    std::map <id, id> next_node;                                    // Ассоциативный контейнер для конечной вершины и вершины, следующей в пути(самый полезный массив)

    id indicator = -1;

    int index = 0;
    id first_node = -1;                                             // Вспомонательная пеменная, обозгначающая первый элемент в очереди (флаг остановки)
    bool check_first_node = false;

    //std::vector <int> visited;

    std::vector< std::vector <std::pair<vertex, int>> *> G;         // (id, sig_stren)
    bool check_resize = false;

    id search_map(std::map<int, int> *my_map, int value){
        for (auto it = my_map->begin(); it != my_map->end(); ++it)
            if (it->second == value)
                return it->first;
        return -1;
    }

public:
    connect(id _my_id, int sign_threshold): my_id(_my_id), INT_MAX_SIGNAL(sign_threshold) {  }

    id getId() {                                                    // Возвращает id данного узла
        return my_id;
    }

    std::vector<id> getNodeInNet() {                                // Возвращает очередь <id, уровень сигнала для узлов, которые он видит>
        std::vector<id> ulala;

        id start = all_stren.front();

        do {
           ulala.push_back( all_stren.front());
           all_stren.push(all_stren.front());
           all_stren.pop();
        } while (start != all_stren.front());

        return ulala;
    }

    void clean(){
        while (!all_stren.empty()) all_stren.pop();
        dict.clear();
        next_node.clear();
        G.clear();
        index==0;
        indicator = -1;
        first_node = -1;
        check_first_node = false;
        check_resize = false;
    }

    id getIdToReconf() {                                            //Если была замечена отвалившаяся/добавившаяся нода, вызываем
        id start = all_stren.front();

        if (indicator != all_stren.front() and indicator != -1) {
            id hlp = all_stren.front();
            all_stren.push(all_stren.front());
            all_stren.pop();
            return hlp;                                             //то разослать информацию о переконфигурации сети всем нодам в базе
        } else if (indicator == all_stren.front()){
            indicator = -1;
        } else{
            indicator = all_stren.front();
            all_stren.push(indicator);
            all_stren.pop();
            return indicator;
        }

        if (all_stren.empty()) {
            id hlp = all_stren.front();
            all_stren.pop();
            return hlp;                                             //то разослать информацию о переконфигурации сети всем нодам в базе
        } else return -1;
    }

    void addNode(id new_node){
//        if (all_stren == nullptr) all_stren = new std::queue<id>;

        all_stren.push(new_node);
    }

    void rmNode(id node){
        id hlp = all_stren.front();
        id hlp2= hlp;
        do {
            if (hlp2 != node) {
                all_stren.push(hlp2);
            }
            all_stren.pop();
            hlp2 = all_stren.front();
        } while (hlp != hlp2);
    }

    void putAnswer(id last_node) {                                  // После рассылки придет ответ от каждой "Живой ноды"
//        //std::cout<<last_id<<" "<<std::endl;
        //if (all_stren == nullptr) all_stren = new std::queue<int>;
        all_stren.push(last_node);                               // Записываем порядок

        if (index==0) {dict[my_id] = 0; index++;}
        dict[last_node] = index++;                                  // Заносим в dict новую ассоциацию
        // [[id005: 0], [id012: 1], [id003: 2], [id001: 3]]         // Для дальнейшей удобной работой в поиске в глубину
        check_first_node = false;                                   // Первая не выбрана, процесс get_priority не запущен
        //std::cout<<last_id<<" "<<std::endl;
    }

    id getNextToSendRequest() {                                     //Те ноды, что прислали ответ на переконфигурацию в порядке получения их ответов получают запрос на отправку уровеня сигнала
        // Узел n, получил запрос, померил уровень сигнала(set_sig_stren)
        // Отослал в синхронном режиме до всех узлов, Master-узел(узел затеявший переконфигурацию) получает список последним

        if (!check_first_node) {
            first_node = all_stren.front();
            all_stren.push(all_stren.front());
            all_stren.pop();
            check_first_node= true;
            return first_node;
        }

        id help_node = all_stren.front();
        if (first_node != help_node){
            all_stren.push(help_node);
            all_stren.pop();
            return help_node;
        } else return -1;
    }

    void setSignStren(std::vector<std::pair<id, int>> *idAndSignStren,
                      id node_id = -1) {                            //заполняет (очередь id, уровень сигнала)  для узлов, от
        if (-1 == node_id) node_id = my_id;                         //которых пришло соотвествуюшее сообщение, в том числе и от нашего узла(my_id)
        if (!check_resize) {
            G.resize(index);
            check_resize = true;
        }

        for (auto &i: *idAndSignStren) {
            i.first = dict[i.first];
        }
        G[dict[node_id]] = idAndSignStren;
    }

    void searchOptimal() {
        int st=dict[my_id];
        int n = G.size();

//        for(auto i: G){
//            for(auto j:*i){
//                std::cout <<"( "+ std::to_string(j.first) + " : "+ std::to_string(j.second)+" ) ";
//            }
//            std::cout <<    std::endl;
//        }

        std::vector<int> D(n, INT_MAX),  p(n);
        D[st] = 0;
        std::vector<char> u(n);
        for (int i=0; i<n; ++i) {
            int v = -1;
            for (int j=0; j<n; ++j)
                if (!u[j] && (v == -1 || D[j] < D[v]))
                    v = j;
            if (D[v] == INT_MAX)
                break;
            u[v] = true;

            for (size_t j=0; j<G[v]->size(); ++j) {
                /*
                int to = G[v]->at(j).first,
                        len = G[v]->at(j).second;
                if (D[v] + len < D[to]) {
                    D[to] = D[v] + len;
                    p[to] = v;
                }*/
                int to = G[v]->at(j).first,
                    len = G[v]->at(j).second;
                if (len>=INT_MAX_SIGNAL) continue;
                if (D[v] + len < D[to]) {
                    D[to] = D[v] + len;
                    p[to] = v;
                }
            }
        }

        std::vector<int> path;
        for(int i=1; i<n; ++i) {
            path.clear();
            for (int v = i; v != st; v = p[v])
                path.push_back(v);
            path.push_back(st);

            std::reverse(path.begin(), path.end());
            //for (int j = 0; j < path.size(); ++j) std::cout << search_map(&dict, path[j]) << " -> ";                  //todo DEBUG
//            std::cout << "#" << D[i] << std::endl;
            if (D[i]<INT_MAX) {
                next_node[search_map(&dict, i)] = search_map(&dict, path[1]);
            } else {
                next_node[search_map(&dict, i)] = -1;
            }
            //std::cout << search_map(&dict, i) << "--->" <<next_node[search_map(&dict, i)]  <<std::endl<<std::endl;    //todo DEBUG
        }
//        for(int i=0; i<n; ++i) {
//            std::cout << search_map(&dict, i) << "--->" << i << std::endl;
//
//            std::cout << "########" << all_stren.front() << "--->" << dict[all_stren.front()] << std::endl;
//            all_stren.push(all_stren.front());
//            all_stren.pop();
//            std::map<int,char> example = {{1,'a'},{2,'b'}};
//        }
    }

    id getNextNode(id node){
        //id nn = next_node[node];
        return next_node[node];
    }
};

#endif //ALG_CONNECT_H
