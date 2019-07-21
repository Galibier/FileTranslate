#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>

using namespace std;

const long long MaxClusterSize = INT32_MAX;
const int MinThreshold = 10;
const double CallerDegradeFactor = 0.01;

class Cluster {
public:
    Cluster() = default;

    Cluster(string str, double time, int size) : timeSample(time), size(size) {
        funcNames.push_back(str);
        density = timeSample / size;
    }

    void merge(const Cluster &rhs) {
        timeSample += rhs.timeSample;
        size += rhs.size;
        density = timeSample / size;
        for (auto func:rhs.funcNames) {
            funcNames.push_back(func);
        }
    }

    void clear() {
        funcNames.resize(0);
        timeSample = 0;
        size = 0;
        density = 0;
        valid = false;
    }

    vector<string> funcNames;
    double timeSample;
    int size;
    double density;

    bool frozen = false;
    bool valid = true;
};

class funcNode {
public:
    string funcName;
    int size;
    double time;

    funcNode(string name, int size, double time) : funcName(name), size(size), time(time) {}
};

class edge {
public:
    string src;
    string dst;
    int cnt;

    edge(string caller, string callee, int cnt) : src(caller), dst(callee), cnt(cnt) {}
};

vector<Cluster> C3(vector<edge> call_graph, vector<funcNode> nodes) {
    sort(nodes.begin(), nodes.end(), [&](const funcNode lhs, const funcNode rhs) {
        return lhs.time * rhs.size > rhs.time * lhs.size;
    });
    map<string, Cluster *> funcToCluster;
    int cnt = nodes.size();
    vector<Cluster> clusters;
    clusters.resize(cnt);
    for (auto node:nodes) {
        Cluster tmp(node.funcName, node.time, node.size);
        clusters.push_back(tmp);
        funcToCluster[node.funcName] = &clusters.back();
    }
//    while (call_graph.size() != 0) {
//        auto cur = call_graph.back();
//        call_graph.pop_back();
//        if (funcToCluster[cur.dst] = funcToCluster[cur.src]) {
//            continue;
//        } else if (funcToCluster[cur.dst]->size + funcToCluster[cur.src]->size >= MaxClusterSize) {
//            continue;
//        }
//        funcToCluster[cur.src]->merge(*funcToCluster[cur.dst]);
//    }

    for (auto node:nodes) {
        auto cluster = funcToCluster[node.funcName];
        if (cluster->frozen)
            continue;

        vector<edge> pre;
        for (auto e:call_graph) {
            if (e.dst == node.funcName)
                pre.push_back(e);
        }

        string best_pre_func;
        double best_weight = 0;
        if (pre.size() == 0)
            continue;
        else if (pre.size() == 1) {
            best_pre_func = pre.back().src;
            best_weight = pre.back().cnt;
        } else {
            for (auto p:pre) {
                if (p.cnt > best_weight) {
                    best_pre_func = p.src;
                    best_weight = p.cnt;
                }
            }
        }

        if (best_weight < MinThreshold)
            continue;

        auto predCluster = funcToCluster[best_pre_func];
        if (predCluster == nullptr || predCluster == cluster || predCluster->frozen) {
            continue;
        }

        if (cluster->size + predCluster->size > MaxClusterSize)
            continue;

        const double NewDensity =
                ((double) predCluster->timeSample + cluster->timeSample) /
                (predCluster->size + cluster->size);
        if (predCluster->density > NewDensity * CallerDegradeFactor) {
            continue;
        }

        for (auto F : cluster->funcNames) {
            funcToCluster[F] = predCluster;
        }

        predCluster->merge(*cluster);
        cluster->clear();
    }

    vector<Cluster> ret;
    for (auto c:clusters) {
        if (c.valid)
            ret.push_back(c);
    }
    return ret;
}

int main() {
    ifstream funcIn("funcNode");
    string funcName;
    double time;
    int size;
    vector<funcNode> nodes;
    while (funcIn >> funcName >> time >> size) {
        nodes.push_back({funcName, size, time});
    }

    ifstream callIn("call_graph");
    string caller, callee;
    int cnt;
    vector<edge> call_graph;
    while (callIn >> caller >> callee >> cnt) {
        call_graph.push_back({caller, callee, cnt});
    }
    C3(call_graph, nodes);
}