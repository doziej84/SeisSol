#ifndef SEISSOL_LTS_PARAMETERS_H
#define SEISSOL_LTS_PARAMETERS_H

#include "ParameterReader.h"

namespace seissol::initializer::parameters {

enum class LtsWeightsTypes : int {
  ExponentialWeights = 0,
  ExponentialBalancedWeights,
  EncodedBalancedWeights,
  Count
};

struct VertexWeightParameters {
  int weightElement;
  int weightDynamicRupture;
  int weightFreeSurfaceWithGravity;
};

enum class AutoMergeCostBaseline {
  // Use cost without wiggle and cluster merge as baseline
  MaxWiggleFactor,
  // First find best wiggle factor (without merge) and use this as baseline
  BestWiggleFactor,
};

AutoMergeCostBaseline parseAutoMergeCostBaseline(std::string str);

class LtsParameters {
  private:
  unsigned int rate;
  double wiggleFactorMinimum;
  double wiggleFactorStepsize;
  bool wiggleFactorEnforceMaximumDifference;
  bool autoMergeClusters;
  double allowedPerformanceLossRatioAutoMerge;
  AutoMergeCostBaseline autoMergeCostBaseline = AutoMergeCostBaseline::BestWiggleFactor;
  LtsWeightsTypes ltsWeightsType;
  double finalWiggleFactor = 1.0;
  int maxNumberOfClusters = std::numeric_limits<int>::max() - 1;

  public:
  [[nodiscard]] unsigned int getRate() const;
  [[nodiscard]] bool isWiggleFactorUsed() const;
  [[nodiscard]] double getWiggleFactorMinimum() const;
  [[nodiscard]] double getWiggleFactorStepsize() const;
  [[nodiscard]] bool getWiggleFactorEnforceMaximumDifference() const;
  [[nodiscard]] int getMaxNumberOfClusters() const;
  [[nodiscard]] bool isAutoMergeUsed() const;
  [[nodiscard]] double getAllowedPerformanceLossRatioAutoMerge() const;
  [[nodiscard]] AutoMergeCostBaseline getAutoMergeCostBaseline() const;
  [[nodiscard]] double getWiggleFactor() const;
  [[nodiscard]] LtsWeightsTypes getLtsWeightsType() const;
  void setWiggleFactor(double factor);
  void setMaxNumberOfClusters(int numClusters);

  LtsParameters() = default;

  LtsParameters(unsigned int rate,
                double wiggleFactorMinimum,
                double wiggleFactorStepsize,
                bool wigleFactorEnforceMaximumDifference,
                int maxNumberOfClusters,
                bool ltsAutoMergeClusters,
                double allowedPerformanceLossRatioAutoMerge,
                AutoMergeCostBaseline autoMergeCostBaseline,
                LtsWeightsTypes ltsWeightsType);
};

struct TimeSteppingParameters {
  VertexWeightParameters vertexWeight;
  double cfl;
  double maxTimestepWidth;
  double endTime;
  LtsParameters lts;

  TimeSteppingParameters() = default;

  TimeSteppingParameters(VertexWeightParameters vertexWeight,
                         double cfl,
                         double maxTimestepWidth,
                         double endTime,
                         LtsParameters lts);
};

LtsParameters readLtsParameters(ParameterReader* baseReader);
TimeSteppingParameters readTimeSteppingParameters(ParameterReader* baseReader);

} // namespace seissol::initializer::parameters

#endif // SEISSOL_LTSCONFIGURATION_H
