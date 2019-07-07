#include <fstream>
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

int main() {
    ifstream in("../trace.txt");
    string tmp, caller, callee;
    char flag;
    stack<string> stk;
    map<pair<string, string>, int> callGraph;
    while (in >> tmp >> flag >> caller >> callee) {
        cout << tmp << flag << caller << callee;
        if (flag == 'E') {
            stk.push(callee);
            if (callGraph.find(make_pair(caller, callee)) != callGraph.end()) {
                callGraph[make_pair(caller, callee)] = 1;
            } else {
                callGraph[make_pair(caller, callee)] += 1;
            }
        } else {
            stk.pop();
        }
    }
}

class FuncCallGraph{


    private:
    vector<string> _node;
    map<pair<string, string>, int> _edge;
};

class ClusterCallGraph{

    private:
    
}