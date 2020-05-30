#ifndef PGM_DATASET_SCORES_HPP
#define PGM_DATASET_SCORES_HPP

#include <factors/continuous/LinearGaussianCPD.hpp>
#include <dataset/dataset.hpp>
#include <models/BayesianNetwork.hpp>
#include <learning/parameter/mle.hpp>
#include <util/math_constants.hpp>
#include <util/bn_traits.hpp>

using factors::continuous::LinearGaussianCPD;
using namespace dataset;
using models::GaussianNetwork, models::GaussianNetwork_M, models::GaussianNetwork_L;
using learning::parameter::MLE;

namespace learning::scores {

    // template<typename Score>
    // struct score_traits;


    class BIC {
    public:
        inline static constexpr bool is_decomposable = true;

        BIC(DataFrame& df) : m_df(df) {}

        template<typename Model>
        double score(const Model& model);

        template<typename Model, typename VarType, typename EvidenceType, std::enable_if_t<is_decomposable, int> = 0>
        double local_score(const Model& model,
                            const VarType& variable, 
                            const EvidenceType& evidence) const
        {
            return local_score(model, variable, evidence.begin(), evidence.end());
        }


        template<typename Model, typename VarType, typename EvidenceIter, std::enable_if_t<is_decomposable, int> = 0>
        double local_score(const Model& model,
                           const VarType& variable, 
                           EvidenceIter evidence_begin,
                                  EvidenceIter evidence_end) const;
    
    private:
        DataFrame& m_df;        
    };

    template<typename Model, typename VarType, typename EvidenceIter, std::enable_if_t<BIC::is_decomposable, int> = 0>
    double BIC::local_score(const Model&,
                            const VarType& variable, 
                            EvidenceIter evidence_begin,
                            EvidenceIter evidence_end) const {

        MLE<LinearGaussianCPD> mle;

        auto mle_params = mle.estimate(m_df, variable, evidence_begin, evidence_end);

        auto rows = m_df->num_rows();
        auto loglik = (1-rows) / 2 - (rows / 2)*std::log(2*util::pi<double>) - rows * std::log(std::sqrt(mle_params.variance));

        return loglik - std::log(rows) * 0.5 * (std::distance(evidence_begin, evidence_end) + 2);
    }
}

#endif //PGM_DATASET_SCORES_HPP