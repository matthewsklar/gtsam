/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010-2019, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file   ShonanAveraging.cpp
 * @date   March 2019
 * @author Frank Dellaert
 * @brief  Shonan Averaging algorithm
 */

#include <gtsam_unstable/slam/ShonanAveraging.h>

#include <gtsam/linear/PCGSolver.h>
#include <gtsam/linear/SubgraphPreconditioner.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/slam/KarcherMeanFactor-inl.h>
#include <gtsam_unstable/slam/FrobeniusFactor.h>

#include <iostream>
#include <map>

namespace gtsam {

ShonanAveraging::ShonanAveraging(const string& g2oFile) {
  factors_ = parse3DFactors(g2oFile);
  poses_ = parse3DPoses(g2oFile);
}

NonlinearFactorGraph ShonanAveraging::buildGraphAt(size_t p) const {
  NonlinearFactorGraph graph;
  for (const auto& factor : factors_) {
    const auto& keys = factor->keys();
    const auto& Tij = factor->measured();
    const auto& model = factor->noiseModel();
    graph.emplace_shared<FrobeniusWormholeFactorTL>(
        keys[0], keys[1], SO3(Tij.rotation().matrix()), p, model);
  }
  const size_t d = p * (p - 1) / 2;
  graph.emplace_shared<KarcherMeanFactor<SOn>>(graph.keys(), d);
  return graph;
}

Values ShonanAveraging::initializeRandomlyAt(size_t p) const {
  static boost::mt19937 rng(42);
  Values initial;
  for (size_t j = 0; j < poses_.size(); j++) {
    initial.insert(j, SOn::Random(rng, p));
  }
  return initial;
}

Values ShonanAveraging::optimize(const NonlinearFactorGraph& graph,
                                 const Values& initial) const {
  // Set parameters to be similar to ceres
  LevenbergMarquardtParams params;
  LevenbergMarquardtParams::SetCeresDefaults(&params);
  //   params.setLinearSolverType("MULTIFRONTAL_QR");
  // params.setVerbosityLM("SUMMARY");
  params.linearSolverType = LevenbergMarquardtParams::Iterative;

  SubgraphBuilderParameters builderParameters;
  builderParameters.skeletonType = SubgraphBuilderParameters::KRUSKAL;
  builderParameters.skeletonWeight = SubgraphBuilderParameters::EQUAL;
  builderParameters.augmentationWeight = SubgraphBuilderParameters::SKELETON;
  builderParameters.augmentationFactor = 0.0;
#ifdef SUBGRAPH_PC
  auto pcg = boost::make_shared<PCGSolverParameters>();
  pcg->preconditioner_ =
      boost::make_shared<SubgraphPreconditionerParameters>(builderParameters);
  // boost::make_shared<BlockJacobiPreconditionerParameters>();
  params.iterativeParams = pcg;
#else
  params.iterativeParams = boost::make_shared<SubgraphSolverParameters>(builderParameters);
#endif
  // Optimize
  LevenbergMarquardtOptimizer lm(graph, initial, params);
  Values result = lm.optimize();
  return result;
}

double ShonanAveraging::cost(const Values& values) const {
  NonlinearFactorGraph graph;
  for (const auto& factor : factors_) {
    const auto& keys = factor->keys();
    const auto& Tij = factor->measured();
    const auto& model = factor->noiseModel();
    graph.emplace_shared<FrobeniusBetweenFactor<SO3>>(
        keys[0], keys[1], SO3(Tij.rotation().matrix()), model);
  }
  return graph.error(values);
}

Values ShonanAveraging::tryOptimizingAt(size_t p) const {
  // Build graph
  auto graph = buildGraphAt(p);

  // Initialize
  auto initial = initializeRandomlyAt(p);

  // Optimize
  auto result = optimize(graph, initial);

  // Project down to SO(3)
  Values SO3_values;
  for (size_t j = 0; j < poses_.size(); j++) {
    const SOn Q = result.at<SOn>(j);
    const SO3 R = SO3::ClosestTo(Q.topLeftCorner(3, 3));
    SO3_values.insert(j, R);
  }
  return SO3_values;
}

void ShonanAveraging::run() const {
  for (const size_t p : {3, 4, 5}) {
    tryOptimizingAt(p);
  }
}

}  // namespace gtsam
