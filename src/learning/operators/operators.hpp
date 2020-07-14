#ifndef PGM_DATASET_OPERATORS_HPP
#define PGM_DATASET_OPERATORS_HPP

#include <queue>
#include <Eigen/Dense>
#include <models/BayesianNetwork.hpp>
#include <util/util_types.hpp>
#include <util/bn_traits.hpp>

using Eigen::MatrixXd, Eigen::VectorXd, Eigen::Matrix, Eigen::Dynamic;
using MatrixXb = Matrix<bool, Dynamic, Dynamic>;
using VectorXb = Matrix<bool, Dynamic, 1>;

using graph::AdjListDag;
using models::BayesianNetwork, models::BayesianNetworkBase;
using factors::FactorType;
using util::ArcVector, util::FactorTypeVector;

namespace learning::operators {


    class OperatorType {
    public:
        enum Value : uint8_t
        {
            ADD_ARC,
            REMOVE_ARC,
            FLIP_ARC,
            CHANGE_NODE_TYPE
        };

        struct Hash
        {
            inline std::size_t operator ()(OperatorType const opset_type) const
            {
                return static_cast<std::size_t>(opset_type.value);
            }
        };

        using HashType = Hash;

        OperatorType() = default;
        constexpr OperatorType(Value opset_type) : value(opset_type) { }

        operator Value() const { return value; }  
        explicit operator bool() = delete;

        constexpr bool operator==(OperatorType a) const { return value == a.value; }
        constexpr bool operator==(Value v) const { return value == v; }
        constexpr bool operator!=(OperatorType a) const { return value != a.value; }
        constexpr bool operator!=(Value v) const { return value != v; }

        std::string ToString() const { 
            switch(value) {
                case Value::ADD_ARC:
                    return "AddArc";
                case Value::REMOVE_ARC:
                    return "RemoveArc";
                case Value::FLIP_ARC:
                    return "FlipArc";
                case Value::CHANGE_NODE_TYPE:
                    return "ChangeNodeType";
                default:
                    throw std::invalid_argument("Unreachable code in OperatorSetType.");
            }
        }

    private:
        Value value;
    };
    
    class Operator {
    public:
        Operator(double delta, OperatorType type) : m_delta(delta), m_type(type) {}

        virtual void apply(BayesianNetworkBase& m) = 0;
        virtual std::shared_ptr<Operator> opposite() = 0;
        double delta() const { return m_delta; }
        OperatorType type() const { return m_type; }
        virtual std::shared_ptr<Operator> copy() const = 0;

        virtual std::string ToString() const = 0;
    private:
        double m_delta;
        OperatorType m_type;
    };

    class ArcOperator : public Operator {
    public:
        ArcOperator(std::string source, 
                    std::string target,
                    double delta,
                    OperatorType type) : Operator(delta, type), m_source(source), m_target(target) {}

        const std::string& source() const { return m_source; }
        const std::string& target() const { return m_target; }

    private:
        std::string m_source;
        std::string m_target;
    };

    class AddArc : public ArcOperator {
    public:
        AddArc(std::string source, 
               std::string target,
               double delta) :  ArcOperator(source, target, delta, OperatorType::ADD_ARC) {}
    
        void apply(BayesianNetworkBase& m) override {
            m.add_edge(this->source(), this->target());
        }
        std::shared_ptr<Operator> opposite() override;
        std::shared_ptr<Operator> copy() const override {
            return std::make_shared<AddArc>(this->source(), this->target(), this->delta());
        }
        std::string ToString() const override {
            return "AddArc(" + this->source() + " -> " + this->target() + "; " + std::to_string(this->delta()) + ")";
        }
    };

    class RemoveArc : public ArcOperator {
    public:
        RemoveArc(std::string source, 
                  std::string target,
                  double delta) : ArcOperator(source, target, delta, OperatorType::REMOVE_ARC) {}
        
        void apply(BayesianNetworkBase& m) override {
            m.remove_edge(this->source(), this->target());
        }
        std::shared_ptr<Operator> opposite() override {
            return std::make_shared<AddArc>(this->source(), this->target(), -this->delta());
        }
        std::shared_ptr<Operator> copy() const override {
            return std::make_shared<RemoveArc>(this->source(), this->target(), this->delta());
        }
        std::string ToString() const override {
            return "RemoveArc(" + this->source() + " -> " + this->target() + "; " + std::to_string(this->delta()) + ")";
        }
    };

    class FlipArc : public ArcOperator {
    public:
        FlipArc(std::string source, 
                std::string target,
                double delta) : ArcOperator(source, target, delta, OperatorType::FLIP_ARC) {}

        void apply(BayesianNetworkBase& m) override {
            m.remove_edge(this->source(), this->target());
            m.add_edge(this->target(), this->source());
        }
        std::shared_ptr<Operator> opposite() override {
            return std::make_shared<FlipArc>(this->target(), this->source(), -this->delta());
        }
        std::shared_ptr<Operator> copy() const override {
            return std::make_shared<FlipArc>(this->source(), this->target(), this->delta());
        }
        std::string ToString() const override {
            return "FlipArc(" + this->source() + " -> " + this->target() + "; " + std::to_string(this->delta()) + ")";
        }
    };

    class ChangeNodeType : public Operator {
    public:

        // static_assert(util::is_semiparametricbn_v<Model>, "ChangeNodeType operator can only be used with a SemiparametricBN.")
        ChangeNodeType(std::string node,
                       FactorType new_node_type,
                       double delta) : Operator(delta, OperatorType::CHANGE_NODE_TYPE),
                                       m_node(node),
                                       m_new_node_type(new_node_type) {}

        const std::string& node() const { return m_node; }
        FactorType node_type() const { return m_new_node_type; }
        void apply(BayesianNetworkBase& m) override {
            auto& spbn = dynamic_cast<SemiparametricBNBase&>(m);
            spbn.set_node_type(m_node, m_new_node_type);
        }
        std::shared_ptr<Operator> opposite() override {
            return std::make_shared<ChangeNodeType>(m_node, m_new_node_type.opposite(), -this->delta());
        }
        std::shared_ptr<Operator> copy() const override {
            return std::make_shared<ChangeNodeType>(m_node, m_new_node_type, this->delta());
        }
        std::string ToString() const override {
            return "ChangeNodeType(" + node() + " -> " + m_new_node_type.ToString() + "; " + std::to_string(this->delta()) + ")";
        }
    private:
        std::string m_node;
        FactorType m_new_node_type;
    };

    class HashOperator {
    public:
        inline std::size_t operator()(Operator* const op) const {
            switch(op->type()) {
                case OperatorType::ADD_ARC:
                case OperatorType::REMOVE_ARC:
                case OperatorType::FLIP_ARC: {
                    auto dwn_op = dynamic_cast<ArcOperator*>(op);
                    auto hasher = std::hash<std::string>{};
                    return (hasher(dwn_op->source()) << (op->type()+1)) ^ hasher(dwn_op->target());
                }
                break;
                case OperatorType::CHANGE_NODE_TYPE: {
                    auto dwn_op = dynamic_cast<ChangeNodeType*>(op);
                    auto hasher = std::hash<std::string>{};

                    return hasher(dwn_op->node()) * dwn_op->node_type();
                }
                break;
                default:
                    throw std::invalid_argument("[HashOperator] Wrong Operator.");
            }
        }
    };

    class OperatorPtrEqual {
    public:
        inline bool operator()(const Operator* lhs, const Operator* rhs) const {
            bool eq = (lhs->type() == rhs->type());
            if (!eq)
                return false; 


            switch(lhs->type()) {
                case OperatorType::ADD_ARC:
                case OperatorType::REMOVE_ARC:
                case OperatorType::FLIP_ARC: {
                    auto dwn_lhs = dynamic_cast<const ArcOperator*>(lhs);
                    auto dwn_rhs = dynamic_cast<const ArcOperator*>(rhs);
                    if ((dwn_lhs->source() == dwn_rhs->source()) && (dwn_lhs->target() == dwn_rhs->target()))
                        return true;
                    else
                        return false;
                }
                case OperatorType::CHANGE_NODE_TYPE: {
                    auto dwn_lhs = dynamic_cast<const ChangeNodeType*>(lhs);
                    auto dwn_rhs = dynamic_cast<const ChangeNodeType*>(rhs);
                    if ((dwn_lhs->node() == dwn_rhs->node()) && (dwn_lhs->node_type() == dwn_rhs->node_type()))
                        return true;
                    else
                        return false;
                }
                default:
                    throw std::invalid_argument("Unreachable code");
            }
        }
    };

    class OperatorTabuSet {
    public:
        OperatorTabuSet() : m_map() { }

        OperatorTabuSet(const OperatorTabuSet& other) : m_map() {
            for (auto& pair : other.m_map) {
                // auto copy = pair.second->copy();
                m_map.insert({pair.first, pair.second});
            }
        }

        OperatorTabuSet& operator=(const OperatorTabuSet& other) {
            clear();
            for (auto& pair : other.m_map) {
                // auto copy = pair.second->copy();
                m_map.insert({pair.first, pair.second});
            }

            return *this;
        }

        OperatorTabuSet(OperatorTabuSet&& other) : m_map(std::move(other.m_map)) {}
        OperatorTabuSet& operator=(OperatorTabuSet&& other) { m_map = std::move(other.m_map); return *this; }

        void insert(std::shared_ptr<Operator> op) {
            m_map.insert({op.get(), op});
        }
        bool contains(std::shared_ptr<Operator>& op) const {
            return m_map.count(op.get()) > 0;
        }
        void clear() {
            m_map.clear();
        }
        bool empty() const {
            return m_map.empty();
        }
    private:
        using MapType = std::unordered_map<Operator*, 
                                           std::shared_ptr<Operator>, 
                                           HashOperator, 
                                           OperatorPtrEqual>;

        MapType m_map;
    };

    class OperatorSetType
    {
    public:
        enum Value : uint8_t
        {
            ARCS,
            NODE_TYPE
        };

        struct Hash
        {
            inline std::size_t operator ()(OperatorSetType const opset_type) const
            {
                return static_cast<std::size_t>(opset_type.value);
            }
        };

        using HashType = Hash;

        OperatorSetType() = default;
        constexpr OperatorSetType(Value opset_type) : value(opset_type) { }

        operator Value() const { return value; }  
        explicit operator bool() = delete;

        constexpr bool operator==(OperatorSetType a) const { return value == a.value; }
        constexpr bool operator==(Value v) const { return value == v; }
        constexpr bool operator!=(OperatorSetType a) const { return value != a.value; }
        constexpr bool operator!=(Value v) const { return value != v; }

        std::string ToString() const { 
            switch(value) {
                case Value::ARCS:
                    return "arcs";
                case Value::NODE_TYPE:
                    return "node_type";
                default:
                    throw std::invalid_argument("Unreachable code in OperatorSetType.");
            }
        }

    private:
        Value value;
    };

    class LocalScoreCache {
    public:

        template<typename Model>
        LocalScoreCache(Model& m) : m_local_score(m.num_nodes()) {}


        template<typename Model, typename Score>
        void cache_local_scores(Model& model, Score& score) {
            for (int i = 0; i < model.num_nodes(); ++i) {
                m_local_score(i) = score.local_score(model, i);
            }
        }

        template<typename Model, typename Score>
        void update_local_score(Model& model, Score& score, int index) {
            m_local_score(index) = score.local_score(model, index);
        }

        template<typename Model, typename Score>
        void update_local_score(Model& model, Score& score, Operator& op) {
            switch(op.type()) {
                case OperatorType::ADD_ARC:
                case OperatorType::REMOVE_ARC: {
                    auto& dwn_op = dynamic_cast<ArcOperator&>(op);
                    update_local_score(model, score, model.index(dwn_op.target()));
                }
                    break;
                case OperatorType::FLIP_ARC: {
                    auto& dwn_op = dynamic_cast<ArcOperator&>(op);
                    update_local_score(model, score, model.index(dwn_op.source()));
                    update_local_score(model, score, model.index(dwn_op.target()));
                }
                    break;
                case OperatorType::CHANGE_NODE_TYPE: {
                    auto& dwn_op = dynamic_cast<ChangeNodeType&>(op);
                    update_local_score(model, score, model.index(dwn_op.node()));
                }
                    break;
            }
        }

        double sum() {
            return m_local_score.sum();
        }

        double local_score(int index) {
            return m_local_score(index);
        }

    private:
        VectorXd m_local_score;
    };
    
    template<typename... Types>
    class OperatorSetInterface {};

    template<typename Type>
    class OperatorSetInterface<Type> {
    public:
        virtual void cache_scores(Type&) {
            throw std::invalid_argument("OperatorSet::cache_scores() not implemented." );
        }
        virtual std::shared_ptr<Operator> find_max(Type&) {
            throw std::invalid_argument("OperatorSet::find_max() not implemented.");
        }
        virtual std::shared_ptr<Operator> find_max(Type&, OperatorTabuSet&) {
            throw std::invalid_argument("OperatorSet::find_max() not implemented.");
        }
        virtual void update_scores(Type&, Operator&) {
            throw std::invalid_argument("OperatorSet::update_scores() not implemented.");
        }
    };

    template<typename Type, typename... Types>
    class OperatorSetInterface<Type, Types...> : public OperatorSetInterface<Types...> {
    public:
        using Base = OperatorSetInterface<Types...>;
        using Base::cache_scores;
        using Base::find_max;
        using Base::update_scores;
        virtual void cache_scores(Type&) {
            throw std::invalid_argument("OperatorSet::cache_scores() not implemented.");
        }
        virtual std::shared_ptr<Operator> find_max(Type&) {
            throw std::invalid_argument("OperatorSet::find_max() not implemented.");
        }
        virtual std::shared_ptr<Operator> find_max(Type&, OperatorTabuSet&) {
            throw std::invalid_argument("OperatorSet::find_max() not implemented.");
        }
        virtual void update_scores(Type&, Operator&) {
            throw std::invalid_argument("OperatorSet::update_scores() not implemented.");
        }
    };

    class OperatorSet : public OperatorSetInterface<GaussianNetwork<>, 
                                                    GaussianNetwork<AdjListDag>,
                                                    SemiparametricBN<>,
                                                    SemiparametricBN<AdjListDag>>
    {
    public:
        void set_local_score_cache(std::shared_ptr<LocalScoreCache> local_cache) {
            m_local_cache = local_cache;
        }

        std::shared_ptr<LocalScoreCache>& local_cache() { return m_local_cache; }
    protected:
        std::shared_ptr<LocalScoreCache> m_local_cache;
    };

    template<typename Derived, bool EnableOverride, typename... Types>
    class OperatorSetImplDetail {};

    template<template<typename> typename Derived, typename Score, typename Type>
    class OperatorSetImplDetail<Derived<Score>, true, Type>  : public OperatorSet {
    public:
        void cache_scores(Type& m) override {
            static_cast<Derived<Score>*>(this)->cache_scores(m);
        }
        std::shared_ptr<Operator> find_max(Type& m) override {
            return static_cast<Derived<Score>*>(this)->find_max(m);
        }
        std::shared_ptr<Operator> find_max(Type& m, OperatorTabuSet& tabu) override {
            return static_cast<Derived<Score>*>(this)->find_max(m, tabu);
        }
        void update_scores(Type& m, Operator& op) override {
            static_cast<Derived<Score>*>(this)->update_scores(m, op);
        }
    };

    template<template<typename> typename Derived, typename Score, typename Type>
    class OperatorSetImplDetail<Derived<Score>, false, Type>  : public OperatorSet {};

    template<template<typename> typename Derived, typename Score, typename Type, typename... Types>
    class OperatorSetImplDetail<Derived<Score>, true, Type, Types...> : 
                public OperatorSetImplDetail<Derived<Score>, 
                                             util::is_compatible_score_v<typename std::tuple_element<0, std::tuple<Types...>>::type,
                                                                         Score>,
                                             Types...> {
    public:
        void cache_scores(Type& m) override {
            static_cast<Derived<Score>*>(this)->cache_scores(m);
        }
        std::shared_ptr<Operator> find_max(Type& m) override {
            return static_cast<Derived<Score>*>(this)->find_max(m);
        }
        std::shared_ptr<Operator> find_max(Type& m, OperatorTabuSet& tabu) override {
            return static_cast<Derived<Score>*>(this)->find_max(m, tabu);
        }
        void update_scores(Type& m, Operator& op) override {
            static_cast<Derived<Score>*>(this)->update_scores(m, op);
        }
    };

    template<template<typename> typename Derived, typename Score, typename Type, typename... Types>
    class OperatorSetImplDetail<Derived<Score>, false, Type, Types...> : 
                public OperatorSetImplDetail<Derived<Score>, 
                                             util::is_compatible_score_v<typename std::tuple_element<0, std::tuple<Types...>>::type,
                                                                         Score>,
                                             Types...> { };

    template<typename Derived, typename Type, typename... Types>
    class OperatorSetImpl {};

    template<template<typename> typename Derived, typename Score, typename Type, typename... Types>
    class OperatorSetImpl<Derived<Score>, Type, Types...> 
            : public OperatorSetImplDetail<Derived<Score>, 
                                           util::is_compatible_score_v<typename std::tuple_element<0, std::tuple<Types...>>::type,
                                                                        Score>,
                                           Type,
                                           Types...> { };

    template<typename Score>
    class ArcOperatorSet : public OperatorSetImpl<ArcOperatorSet<Score>,
                                                  GaussianNetwork<>,
                                                  GaussianNetwork<AdjListDag>,
                                                  SemiparametricBN<>,
                                                  SemiparametricBN<AdjListDag>>
    {
    public:

        template<typename Model>
        ArcOperatorSet(Model& model, const Score score, ArcVector& whitelist, ArcVector& blacklist,
                       int max_indegree);

        template<typename Model>
        void cache_scores(Model& model);

        template<typename Model>
        std::shared_ptr<Operator> find_max(Model& model);
        template<typename Model>
        std::shared_ptr<Operator> find_max(Model& model, OperatorTabuSet& tabu_set);
        template<typename Model, bool limited_indigree>
        std::shared_ptr<Operator> find_max_indegree(Model& model);
        template<typename Model, bool limited_indigree>
        std::shared_ptr<Operator> find_max_indegree(Model& model, OperatorTabuSet& tabu_set);
        template<typename Model>
        void update_scores(Model& model, Operator& op);
        template<typename Model>
        void update_node_arcs_scores(Model& model, const std::string& dest_node);
    private:
        const Score m_score;
        MatrixXd delta;
        MatrixXb valid_op;
        std::vector<int> sorted_idx;
        int max_indegree;
    };


    template<typename Score>
    template<typename Model>
    ArcOperatorSet<Score>::ArcOperatorSet(Model& model,
                                          const Score score,
                                          ArcVector& blacklist,
                                          ArcVector& whitelist,
                                          int max_indegree) : m_score(score),
                                                                     delta(model.num_nodes(), model.num_nodes()),
                                                                     valid_op(model.num_nodes(), model.num_nodes()), 
                                                                     sorted_idx(),
                                                                     max_indegree(max_indegree)
    {
        auto num_nodes = model.num_nodes();
        auto val_ptr = valid_op.data();

        std::fill(val_ptr, val_ptr + num_nodes*num_nodes, true);

        auto indices = model.indices();
        auto valid_ops = (num_nodes * num_nodes) - 2*whitelist.size() - blacklist.size() - num_nodes;

        for(auto whitelist_edge : whitelist) {
            auto source_index = indices[whitelist_edge.first];
            auto dest_index = indices[whitelist_edge.second];

            valid_op(source_index, dest_index) = false;
            valid_op(dest_index, source_index) = false;
            delta(source_index, dest_index) = std::numeric_limits<double>::lowest();
            delta(dest_index, source_index) = std::numeric_limits<double>::lowest();
        }
        
        for(auto blacklist_edge : blacklist) {
            auto source_index = indices[blacklist_edge.first];
            auto dest_index = indices[blacklist_edge.second];

            valid_op(source_index, dest_index) = false;
            delta(source_index, dest_index) = std::numeric_limits<double>::lowest();
        }

        for (int i = 0; i < num_nodes; ++i) {
            valid_op(i, i) = false;
            delta(i, i) = std::numeric_limits<double>::lowest();
        }

        sorted_idx.reserve(valid_ops);

        for (int i = 0; i < num_nodes; ++i) {
            for (int j = 0; j < num_nodes; ++j) {
                if (valid_op(i, j)) {
                    sorted_idx.push_back(i + j * num_nodes);
                }
            }
        }
    }

    template<typename Score>
    template<typename Model>
    void ArcOperatorSet<Score>::cache_scores(Model& model) {
        for (auto dest = 0; dest < model.num_nodes(); ++dest) {
            std::vector<int> new_parents_dest = model.parent_indices(dest);
            
            for (auto source = 0; source < model.num_nodes(); ++source) {
                if(valid_op(source, dest)) {
                    if (model.has_edge(source, dest)) {            
                        std::iter_swap(std::find(new_parents_dest.begin(), new_parents_dest.end(), source), new_parents_dest.end() - 1);
                        double d = m_score.local_score(model, dest, new_parents_dest.begin(), new_parents_dest.end() - 1) - 
                                    this->m_local_cache->local_score(dest);
                        delta(source, dest) = d;
                    } else if (model.has_edge(dest, source)) {
                        auto new_parents_source = model.parent_indices(source);
                        std::iter_swap(std::find(new_parents_source.begin(), new_parents_source.end(), dest), new_parents_source.end() - 1);
                        
                        new_parents_dest.push_back(source);
                        double d = m_score.local_score(model, source, new_parents_source.begin(), new_parents_source.end() - 1) + 
                                   m_score.local_score(model, dest, new_parents_dest.begin(), new_parents_dest.end()) 
                                   - this->m_local_cache->local_score(source) - this->m_local_cache->local_score(dest);
                        new_parents_dest.pop_back();
                        delta(dest, source) = d;
                    } else {
                        new_parents_dest.push_back(source);
                        double d = m_score.local_score(model, dest, new_parents_dest.begin(), new_parents_dest.end()) 
                                    - this->m_local_cache->local_score(dest);
                        new_parents_dest.pop_back();
                        delta(source, dest) = d;
                    }
                }
            }
        }
    }

    template<typename Score>
    template<typename Model>
    std::shared_ptr<Operator> ArcOperatorSet<Score>::find_max(Model& model) {
        if (max_indegree > 0)
            return find_max_indegree<Model, true>(model);
        else
            return find_max_indegree<Model, false>(model);
    }

    template<typename Score>
    template<typename Model>
    std::shared_ptr<Operator> ArcOperatorSet<Score>::find_max(Model& model, OperatorTabuSet& tabu_set) {
        if (max_indegree > 0)
            return find_max_indegree<Model, true>(model, tabu_set);
        else
            return find_max_indegree<Model, false>(model, tabu_set);
    }


    template<typename Score>
    template<typename Model, bool limited_indegree>
    std::shared_ptr<Operator> ArcOperatorSet<Score>::find_max_indegree(Model& model) {

        auto delta_ptr = delta.data();

        // TODO: Not checking sorted_idx empty
        std::sort(sorted_idx.begin(), sorted_idx.end(), [&delta_ptr](auto i1, auto i2) {
            return delta_ptr[i1] >= delta_ptr[i2];
        });

        for(auto it = sorted_idx.begin(); it != sorted_idx.end(); ++it) {
            auto idx = *it;
            auto source = idx % model.num_nodes();
            auto dest = idx / model.num_nodes();

            if(model.has_edge(source, dest)) {
                return std::make_shared<RemoveArc>(model.name(source), model.name(dest), delta(source, dest));
            } else if (model.has_edge(dest, source) && model.can_flip_edge(dest, source)) {
                if constexpr (limited_indegree) {
                    if (model.num_parents(dest) >= max_indegree) {
                        continue;
                    }
                }
                return std::make_shared<FlipArc>(model.name(dest), model.name(source), delta(dest, source));
            } else if (model.can_add_edge(source, dest)) {
                if constexpr (limited_indegree) {
                    if (model.num_parents(dest) >= max_indegree) {
                        continue;
                    }
                }
                return std::make_shared<AddArc>(model.name(source), model.name(dest), delta(source, dest));
            }
        }

        return nullptr;
    }

    template<typename Score>
    template<typename Model, bool limited_indegree>
    std::shared_ptr<Operator> ArcOperatorSet<Score>::find_max_indegree(Model& model,  OperatorTabuSet& tabu_set) {
        auto delta_ptr = delta.data();

        // TODO: Not checking sorted_idx empty
        std::sort(sorted_idx.begin(), sorted_idx.end(), [&delta_ptr](auto i1, auto i2) {
            return delta_ptr[i1] >= delta_ptr[i2];
        });

        for(auto it = sorted_idx.begin(); it != sorted_idx.end(); ++it) {
            auto idx = *it;
            auto source = idx % model.num_nodes();
            auto dest = idx / model.num_nodes();

            if(model.has_edge(source, dest)) {
                std::shared_ptr<Operator> op = std::make_shared<RemoveArc>(model.name(source), model.name(dest), delta(source, dest));
                if (!tabu_set.contains(op))
                    return std::move(op);
            } else if (model.has_edge(dest, source) && model.can_flip_edge(dest, source)) {
                if constexpr (limited_indegree) {
                    if (model.num_parents(dest) >= max_indegree) {
                        continue;
                    }
                }
                std::shared_ptr<Operator> op = std::make_shared<FlipArc>(model.name(dest), model.name(source), delta(dest, source));
                if (!tabu_set.contains(op))
                    return std::move(op);
            } else if (model.can_add_edge(source, dest)) {
                if constexpr (limited_indegree) {
                    if (model.num_parents(dest) >= max_indegree) {
                        continue;
                    }
                }
                std::shared_ptr<Operator> op = std::make_shared<AddArc>(model.name(source), model.name(dest), delta(source, dest));
                if (!tabu_set.contains(op))
                    return std::move(op);
            }
        }

        return nullptr;
    }

    template<typename Score>
    template<typename Model>
    void ArcOperatorSet<Score>::update_scores(Model& model, Operator& op) {
        switch(op.type()) {
            case OperatorType::ADD_ARC:
            case OperatorType::REMOVE_ARC: {
                auto& dwn_op = dynamic_cast<ArcOperator&>(op);
                update_node_arcs_scores(model, dwn_op.target());
            }
                break;
            case OperatorType::FLIP_ARC: {
                auto& dwn_op = dynamic_cast<ArcOperator&>(op);
                update_node_arcs_scores(model, dwn_op.source());
                update_node_arcs_scores(model, dwn_op.target());
            }
                break;
            case OperatorType::CHANGE_NODE_TYPE: {
                auto& dwn_op = dynamic_cast<ChangeNodeType&>(op);
                update_node_arcs_scores(model, dwn_op.node());
            }
                break;
        }
    }

    template<typename Score>
    template<typename Model>
    void ArcOperatorSet<Score>::update_node_arcs_scores(Model& model, const std::string& dest_node) {

        auto dest_idx = model.index(dest_node);
        auto parents = model.parent_indices(dest_idx);
        
        for (int i = 0; i < model.num_nodes(); ++i) {
            if (valid_op(i, dest_idx)) {

                if (model.has_edge(i, dest_idx)) {
                    std::iter_swap(std::find(parents.begin(), parents.end(), i), parents.end() - 1);
                    double d = m_score.local_score(model, dest_idx, parents.begin(), parents.end() - 1) - 
                               this->m_local_cache->local_score(dest_idx);
                    delta(i, dest_idx) = d;

                    auto new_parents_i = model.parent_indices(i);
                    new_parents_i.push_back(dest_idx);

                    delta(dest_idx, i) = d + m_score.local_score(model, i, new_parents_i.begin(), new_parents_i.end())
                                            - this->m_local_cache->local_score(i);
                } else if (model.has_edge(dest_idx, i)) {
                    auto new_parents_i = model.parent_indices(i);
                    std::iter_swap(std::find(new_parents_i.begin(), new_parents_i.end(), dest_idx), new_parents_i.end() - 1);
                        
                    parents.push_back(i);
                    double d = m_score.local_score(model, i, new_parents_i.begin(), new_parents_i.end() - 1) + 
                                m_score.local_score(model, dest_idx, parents.begin(), parents.end()) 
                                - this->m_local_cache->local_score(i) - this->m_local_cache->local_score(dest_idx);
                    parents.pop_back();
                    delta(dest_idx, i) = d;
                } else {
                    parents.push_back(i);
                    double d = m_score.local_score(model, dest_idx, parents.begin(), parents.end()) - this->m_local_cache->local_score(dest_idx);
                    parents.pop_back();
                    delta(i, dest_idx) = d;
                }
            }
        }
    }

    template<typename Score>
    class ChangeNodeTypeSet : public OperatorSetImpl<ChangeNodeTypeSet<Score>,
                                                     SemiparametricBN<>,
                                                     SemiparametricBN<AdjListDag>> {
    public:
        template<typename Model>
        ChangeNodeTypeSet(Model& model, 
                          const Score score, 
                          FactorTypeVector& type_whitelist) : m_score(score),
                                                              delta(model.num_nodes()),
                                                              valid_op(model.num_nodes()),
                                                              sorted_idx()
        {
            auto val_ptr = valid_op.data();
            std::fill(val_ptr, val_ptr + model.num_nodes(), true);

            auto indices = model.indices();

            for (auto &node : type_whitelist) {
                delta(indices[node.first]) = std::numeric_limits<double>::lowest();;
                valid_op(indices[node.first]) = false;
            }

            auto valid_ops = model.num_nodes() - type_whitelist.size();
            sorted_idx.reserve(valid_ops);
            for (auto i = 0; i < model.num_nodes(); ++i) {
                if(valid_op(i))
                    sorted_idx.push_back(i);
            }
        }

        template<typename Model>
        void cache_scores(Model& model);
        template<typename Model>
        std::shared_ptr<Operator> find_max(Model& model);
        template<typename Model>
        std::shared_ptr<Operator> find_max(Model& model, OperatorTabuSet& tabu_set);
        template<typename Model>
        void update_scores(Model& model, Operator& op);

        template<typename Model>
        void update_local_delta(Model& model, const std::string& node) {
            update_local_delta(model, model.index(node));
        }

        template<typename Model>
        void update_local_delta(Model& model, int node_index) {
            FactorType type = model.node_type(node_index);
            auto parents = model.parent_indices(node_index);
            delta(node_index) = m_score.local_score(type.opposite(), node_index, parents.begin(), parents.end()) 
                                - this->m_local_cache->local_score(node_index);
        }

    private:
        const Score m_score;
        VectorXd delta;
        VectorXb valid_op;
        std::vector<int> sorted_idx;
    };

    template<typename Score>
    template<typename Model>
    void ChangeNodeTypeSet<Score>::cache_scores(Model& model) {
        for(auto i = 0; i < model.num_nodes(); ++i) {
            if(valid_op(i)) {
                update_local_delta(model, i);
            }
        }
    }

    template<typename Score>
    template<typename Model>
    std::shared_ptr<Operator> ChangeNodeTypeSet<Score>::find_max(Model& model) {
        auto delta_ptr = delta.data();
        auto max_element = std::max_element(delta_ptr, delta_ptr + model.num_nodes());
        int idx_max = std::distance(delta_ptr, max_element);
        auto node_type = model.node_type(idx_max);

        if(valid_op(idx_max))
            return std::make_shared<ChangeNodeType>(model.name(idx_max), node_type.opposite(), *max_element);
        else
            return nullptr;
    }

    template<typename Score>
    template<typename Model>
    std::shared_ptr<Operator> ChangeNodeTypeSet<Score>::find_max(Model& model, OperatorTabuSet& tabu_set) {
        auto delta_ptr = delta.data();
        // TODO: Not checking sorted_idx empty
        std::sort(sorted_idx.begin(), sorted_idx.end(), [&delta_ptr](auto i1, auto i2) {
            return delta_ptr[i1] >= delta_ptr[i2];
        });

        for(auto it = sorted_idx.begin(); it != sorted_idx.end(); ++it) {
            int idx_max = *it;
            auto node_type = model.node_type(idx_max);
            std::shared_ptr<Operator> op = std::make_shared<ChangeNodeType>(model.name(idx_max), node_type.opposite(), delta(idx_max));
            if (tabu_set.contains(op))
                return std::move(op);

        }

        return nullptr;
    }

    template<typename Score>
    template<typename Model>
    void ChangeNodeTypeSet<Score>::update_scores(Model& model, Operator& op) {
        switch(op.type()) {
            case OperatorType::ADD_ARC:
            case OperatorType::REMOVE_ARC: {
                auto& dwn_op = dynamic_cast<ArcOperator&>(op);
                update_local_delta(model, dwn_op.target());
            }
                break;
            case OperatorType::FLIP_ARC: {
                auto& dwn_op = dynamic_cast<ArcOperator&>(op);
                update_local_delta(model, dwn_op.source());
                update_local_delta(model, dwn_op.target());
            }
                break;
            case OperatorType::CHANGE_NODE_TYPE: {
                auto& dwn_op = dynamic_cast<ChangeNodeType&>(op);
                int index = model.index(dwn_op.node());
                delta(index) = -dwn_op.delta();
            }
                break;
        }
    }

    template<typename Score>
    class OperatorPool {
    public:
        template<typename Model>
        OperatorPool(Model& model, 
                     const Score score, 
                     std::vector<std::shared_ptr<OperatorSet>> op_sets) : m_score(score),
                                                                          local_cache(std::make_shared<LocalScoreCache>(model)),
                                                                          m_op_sets(std::move(op_sets)) {
            
            for (auto& op_set : m_op_sets) {
                op_set->set_local_score_cache(local_cache);
            }
        }
        
        template<typename Model>
        void cache_scores(Model& model);
        template<typename Model>
        std::shared_ptr<Operator> find_max(Model& model);
        template<typename Model>
        std::shared_ptr<Operator> find_max(Model& model, OperatorTabuSet& tabu_set);
        template<typename Model>
        void update_scores(Model& model, Operator& op);
               
        double score() {
            return local_cache->sum();
        }

        template<typename Model>
        double score(Model& model) {
            double s = 0;
            for (int i = 0; i < model.num_nodes(); ++i) {
                s += m_score.local_score(model, i);
            }
            return s;
        }
    private:
        const Score m_score;
        std::shared_ptr<LocalScoreCache> local_cache;
        std::vector<std::shared_ptr<OperatorSet>> m_op_sets;
    };

    template<typename Score>
    template<typename Model>
    void OperatorPool<Score>::cache_scores(Model& model) {
        local_cache->cache_local_scores(model, m_score);

        for (auto& op_set : m_op_sets) {
            op_set->cache_scores(model);
        }
    }

    template<typename Score>
    template<typename Model>
    std::shared_ptr<Operator> OperatorPool<Score>::find_max(Model& model) {

        double max_delta = std::numeric_limits<double>::lowest();
        std::shared_ptr<Operator> max_op = nullptr;

        for (auto it = m_op_sets.begin(); it != m_op_sets.end(); ++it) {
            auto new_op = (*it)->find_max(model);
            if (new_op && new_op->delta() > max_delta) {
                max_op = std::move(new_op);
                max_delta = max_op->delta();
            }
        }

        return max_op;
    }

    template<typename Score>
    template<typename Model>
    std::shared_ptr<Operator> OperatorPool<Score>::find_max(Model& model, OperatorTabuSet& tabu_set) {
        if (tabu_set.empty())
            return find_max(model);
        
        double max_delta = std::numeric_limits<double>::lowest();
        std::shared_ptr<Operator> max_op = nullptr;

        for (auto it = m_op_sets.begin(); it != m_op_sets.end(); ++it) {
            auto new_op = (*it)->find_max(model, tabu_set);
            if (new_op && new_op->delta() > max_delta) {
                max_op = std::move(new_op);
                max_delta = max_op->delta();
            }
        }

        return max_op;
    }

    template<typename Score>
    template<typename Model>
    void OperatorPool<Score>::update_scores(Model& model, Operator& op) {
        local_cache->update_local_score(model, m_score, op);
        for (auto& op_set : m_op_sets) {
            op_set->update_scores(model, op);
        }
    }

}

#endif //PGM_DATASET_OPERATORS_HPP