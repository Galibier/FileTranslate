#include <algorithm>
#include <cassert>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

constexpr uint32_t MaxClusterSize = 1 << 20;

inline bool compareClustersDensity(const Cluster &C1, const Cluster &C2) {
    return C1.density() > C2.density();
}

inline int64_t hashCombine(const int64_t Seed, const int64_t Val) {
    std::hash<int64_t> Hasher;
    return Seed ^ (Hasher(Val) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2));
}

void orderFuncs(const CallGraph &Cg, Cluster *C1, Cluster *C2) {
    auto C1head = C1->targets().front();
    auto C1tail = C1->targets().back();
    auto C2head = C2->targets().front();
    auto C2tail = C2->targets().back();

    double C1headC2head = 0;
    double C1headC2tail = 0;
    double C1tailC2head = 0;
    double C1tailC2tail = 0;

    for (const auto &Arc : Cg.arcs()) {
        if ((Arc.src() == C1head && Arc.dst() == C2head) ||
            (Arc.dst() == C1head && Arc.src() == C2head)) {
            C1headC2head += Arc.weight();
        } else if ((Arc.src() == C1head && Arc.dst() == C2tail) ||
                   (Arc.dst() == C1head && Arc.src() == C2tail)) {
            C1headC2tail += Arc.weight();
        } else if ((Arc.src() == C1tail && Arc.dst() == C2head) ||
                   (Arc.dst() == C1tail && Arc.src() == C2head)) {
            C1tailC2head += Arc.weight();
        } else if ((Arc.src() == C1tail && Arc.dst() == C2tail) ||
                   (Arc.dst() == C1tail && Arc.src() == C2tail)) {
            C1tailC2tail += Arc.weight();
        }
    }

    const double Max = std::max(std::max(C1headC2head, C1headC2tail),
                                std::max(C1tailC2head, C1tailC2tail));

    if (C1headC2head == Max) {
        // flip C1
        C1->reverseTargets();
    } else if (C1headC2tail == Max) {
        // flip C1 C2
        C1->reverseTargets();
        C2->reverseTargets();
    } else if (C1tailC2tail == Max) {
        // flip C2
        C2->reverseTargets();
    }
}

class CallGraph {
   public:
    using NodeId = size_t;
    static constexpr NodeId InvalidId = -1;

    template <typename T>
    class iterator_range {
        T Begin;
        T End;

       public:
        template <typename Container>
        iterator_range(Container &&c) : Begin(c.begin()), End(c.end()) {}
        iterator_range(T Begin, T End)
            : Begin(std::move(Begin)), End(std::move(End)) {}

        T begin() const { return Begin; }
        T end() const { return End; }
    };

    class Arc {
       public:
        struct Hash {
            int64_t operator()(const Arc &Arc) const;
        };

        Arc(NodeId S, NodeId D, double W = 0) : Src(S), Dst(D), Weight(W) {}
        Arc(const Arc &) = delete;

        friend bool operator==(const Arc &Lhs, const Arc &Rhs) {
            return Lhs.Src == Rhs.Src && Lhs.Dst == Rhs.Dst;
        }

        NodeId src() const { return Src; }
        NodeId dst() const { return Dst; }
        double weight() const { return Weight; }
        double avgCallOffset() const { return AvgCallOffset; }
        double normalizedWeight() const { return NormalizedWeight; }

       private:
        friend class CallGraph;
        NodeId Src{InvalidId};
        NodeId Dst{InvalidId};
        mutable double Weight{0};
        mutable double NormalizedWeight{0};
        mutable double AvgCallOffset{0};
    };

    using ArcsType = std::unordered_set<Arc, Arc::Hash>;
    using ArcIterator = ArcsType::iterator;
    using ArcConstIterator = ArcsType::const_iterator;

    class Node {
       public:
        explicit Node(uint32_t Size, uint64_t Samples = 0)
            : Size(Size), Samples(Samples) {}

        uint32_t size() const { return Size; }
        uint64_t samples() const { return Samples; }

        const std::vector<NodeId> &successors() const { return Succs; }
        const std::vector<NodeId> &predecessors() const { return Preds; }

       private:
        friend class CallGraph;
        uint32_t Size;
        uint64_t Samples;

        // preds and succs contain no duplicate elements and self arcs are not
        // allowed
        std::vector<NodeId> Preds;
        std::vector<NodeId> Succs;
    };

    size_t numNodes() const { return Nodes.size(); }
    const Node &getNode(const NodeId Id) const {
        assert(Id < Nodes.size());
        return Nodes[Id];
    }
    uint32_t size(const NodeId Id) const {
        assert(Id < Nodes.size());
        return Nodes[Id].Size;
    }
    uint64_t samples(const NodeId Id) const {
        assert(Id < Nodes.size());
        return Nodes[Id].Samples;
    }
    const std::vector<NodeId> &successors(const NodeId Id) const {
        assert(Id < Nodes.size());
        return Nodes[Id].Succs;
    }
    const std::vector<NodeId> &predecessors(const NodeId Id) const {
        assert(Id < Nodes.size());
        return Nodes[Id].Preds;
    }
    NodeId addNode(uint32_t Size, uint64_t Samples = 0) {
        auto Id = Nodes.size();
        Nodes.emplace_back(Size, Samples);
        return Id;
    }
    const Arc &incArcWeight(NodeId Src, NodeId Dst, double W = 1.0,
                            double Offset = 0.0) {
        assert(Offset <= size(Src) && "Call offset exceeds function size");

        auto Res = Arcs.emplace(Src, Dst, W);
        if (!Res.second) {
            Res.first->Weight += W;
            Res.first->AvgCallOffset += Offset * W;
            return *Res.first;
        }
        Res.first->AvgCallOffset = Offset * W;
        Nodes[Src].Succs.push_back(Dst);
        Nodes[Dst].Preds.push_back(Src);
        return *Res.first;
    }
    ArcIterator findArc(NodeId Src, NodeId Dst) {
        return Arcs.find(Arc(Src, Dst));
    }
    ArcConstIterator findArc(NodeId Src, NodeId Dst) const {
        return Arcs.find(Arc(Src, Dst));
    }
    iterator_range<ArcConstIterator> arcs() const {
        return iterator_range<ArcConstIterator>(Arcs.begin(), Arcs.end());
    }
    iterator_range<std::vector<Node>::const_iterator> nodes() const {
        return iterator_range<std::vector<Node>::const_iterator>(Nodes.begin(),
                                                                 Nodes.end());
    }

    double density() const {
        return double(Arcs.size()) / (Nodes.size() * Nodes.size());
    }

    // Initialize NormalizedWeight field for every arc
    void normalizeArcWeights() {
        for (NodeId FuncId = 0; FuncId < numNodes(); ++FuncId) {
            auto &Func = getNode(FuncId);
            for (auto Caller : Func.predecessors()) {
                auto Arc = findArc(Caller, FuncId);
                Arc->NormalizedWeight = Arc->weight() / Func.samples();
                if (Arc->weight() > 0) Arc->AvgCallOffset /= Arc->weight();
                assert(Arc->AvgCallOffset <= size(Caller) &&
                       "Avg call offset exceeds function size");
            }
        }
    }
    // Make sure that the sum of incoming arc weights is at least the number of
    // samples for every node
    void adjustArcWeights() {
        for (NodeId FuncId = 0; FuncId < numNodes(); ++FuncId) {
            auto &Func = getNode(FuncId);
            uint64_t InWeight = 0;
            for (auto Caller : Func.predecessors()) {
                auto Arc = findArc(Caller, FuncId);
                InWeight += (uint64_t)Arc->weight();
            }
            if (Func.samples() < InWeight) setSamples(FuncId, InWeight);
        }
    }

    // template <typename L>
    // void printDot(char *fileName, L getLabel) const;

   private:
    void setSamples(const NodeId Id, uint64_t Samples) {
        assert(Id < Nodes.size());
        Nodes[Id].Samples = Samples;
    }

    std::vector<Node> Nodes;
    ArcsType Arcs;
};

class Cluster {
   public:
    Cluster(CallGraph::NodeId Id, const CallGraph::Node &F);

    std::string toString() const {
        std::string Str;
        bool PrintComma = false;
        Str += "funcs = [";
        for (auto &Target : Targets) {
            if (PrintComma) Str += ", ";
            Str += std::to_string(Target);
            PrintComma = true;
        }
        Str += "]";
        return Str;
    }
    double density() const { return Density; }
    uint64_t samples() const { return Samples; }
    uint32_t size() const { return Size; }
    bool frozen() const { return Frozen; }
    void freeze() { Frozen = true; }
    void merge(const Cluster &Other, const double Aw = 0) {
        Targets.insert(Targets.end(), Other.Targets.begin(),
                       Other.Targets.end());
        Size += Other.Size;
        Samples += Other.Samples;
        Density = (double)Samples / Size;
    }
    void clear() {
        Id = -1u;
        Size = 0;
        Samples = 0;
        Density = 0.0;
        Targets.clear();
        Frozen = false;
    }
    size_t numTargets() const { return Targets.size(); }
    const std::vector<CallGraph::NodeId> &targets() const { return Targets; }
    CallGraph::NodeId target(size_t N) const { return Targets[N]; }
    void reverseTargets() { std::reverse(Targets.begin(), Targets.end()); }
    bool hasId() const { return Id != -1u; }
    void setId(uint32_t NewId) {
        assert(!hasId());
        Id = NewId;
    }
    uint32_t id() const {
        assert(hasId());
        return Id;
    }

   private:
    uint32_t Id{-1u};
    std::vector<CallGraph::NodeId> Targets;
    uint64_t Samples{0};
    uint32_t Size{0};
    double Density{0.0};
    bool Frozen{false};  // not a candidate for merging
};

class ClusterArc {
   public:
    ClusterArc(Cluster *Ca, Cluster *Cb, double W = 0)
        : C1(std::min(Ca, Cb)), C2(std::max(Ca, Cb)), Weight(W) {}

    friend bool operator==(const ClusterArc &Lhs, const ClusterArc &Rhs) {
        return Lhs.C1 == Rhs.C1 && Lhs.C2 == Rhs.C2;
    }

    Cluster *const C1;
    Cluster *const C2;
    mutable double Weight;
};

class ClusterArcHash {
   public:
    int64_t operator()(const ClusterArc &Arc) const {
        std::hash<int64_t> Hasher;
        return hashCombine(Hasher(int64_t(Arc.C1)), int64_t(Arc.C2));
    }
};

std::vector<Cluster> pettisAndHansen(const CallGraph &Cg) {
    // indexed by NodeId, keeps its current cluster
    std::vector<Cluster *> FuncCluster(Cg.numNodes(), nullptr);
    std::vector<Cluster> Clusters;
    std::vector<CallGraph::NodeId> Funcs;

    Clusters.reserve(Cg.numNodes());

    for (CallGraph::NodeId F = 0; F < Cg.numNodes(); F++) {
        if (Cg.samples(F) == 0) continue;
        Clusters.emplace_back(F, Cg.getNode(F));
        FuncCluster[F] = &Clusters.back();
        Funcs.push_back(F);
    }

    using ClusterArcSet = std::unordered_set<ClusterArc, ClusterArcHash>;
    ClusterArcSet Carcs;

    auto insertOrInc = [&](Cluster *C1, Cluster *C2, double Weight) {
        auto Res = Carcs.emplace(C1, C2, Weight);
        if (!Res.second) {
            Res.first->Weight += Weight;
        }
    };

    // Create a std::vector of cluster arcs

    for (auto &Arc : Cg.arcs()) {
        if (Arc.weight() == 0) continue;

        auto const S = FuncCluster[Arc.src()];
        auto const D = FuncCluster[Arc.dst()];

        // ignore if s or d is nullptr

        if (S == nullptr || D == nullptr) continue;

        // ignore self-edges

        if (S == D) continue;

        insertOrInc(S, D, Arc.weight());
    }

    // Find an arc with max weight and merge its nodes

    while (!Carcs.empty()) {
        auto Maxpos = std::max_element(
            Carcs.begin(), Carcs.end(),
            [&](const ClusterArc &Carc1, const ClusterArc &Carc2) {
                return Carc1.Weight < Carc2.Weight;
            });

        auto Max = *Maxpos;
        Carcs.erase(Maxpos);

        auto const C1 = Max.C1;
        auto const C2 = Max.C2;

        if (C1->size() + C2->size() > MaxClusterSize) continue;

        if (C1->frozen() || C2->frozen()) continue;

        // order functions and merge cluster

        orderFuncs(Cg, C1, C2);

        // DEBUG(dbgs() << format("merging %s -> %s: %.1f\n",
        //                        C2->toString().c_str(),
        //                        C1->toString().c_str(), Max.Weight););

        // update carcs: merge C1arcs to C2arcs

        std::unordered_map<ClusterArc, Cluster *, ClusterArcHash> C2arcs;
        for (auto &Carc : Carcs) {
            if (Carc.C1 == C2) C2arcs.emplace(Carc, Carc.C2);
            if (Carc.C2 == C2) C2arcs.emplace(Carc, Carc.C1);
        }

        for (auto It : C2arcs) {
            auto const C = It.second;
            auto const C2arc = It.first;

            insertOrInc(C, C1, C2arc.Weight);
            Carcs.erase(C2arc);
        }

        // update FuncCluster

        for (auto F : C2->targets()) {
            FuncCluster[F] = C1;
        }
        C1->merge(*C2, Max.Weight);
        C2->clear();
    }

    // Return the set of Clusters that are left, which are the ones that
    // didn't get merged.

    std::set<Cluster *> LiveClusters;
    std::vector<Cluster> OutClusters;

    for (auto Fid : Funcs) {
        LiveClusters.insert(FuncCluster[Fid]);
    }
    for (auto C : LiveClusters) {
        OutClusters.push_back(std::move(*C));
    }

    std::sort(OutClusters.begin(), OutClusters.end(), compareClustersDensity);

    return OutClusters;
}