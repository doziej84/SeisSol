#ifndef SEISSOL_IMPOSEDSLIPRATES_H
#define SEISSOL_IMPOSEDSLIPRATES_H

#include "BaseFrictionLaw.h"

namespace seissol::dr::friction_law {
/**
 * Slip rates are set fixed values
 */
template <typename STF>
class ImposedSlipRates : public BaseFrictionLaw<ImposedSlipRates<STF>> {
  public:
  using BaseFrictionLaw<ImposedSlipRates>::BaseFrictionLaw;

  void copyLtsTreeToLocal(seissol::initializer::Layer& layerData,
                          seissol::initializer::DynamicRupture const* const dynRup,
                          real fullUpdateTime) {
    auto* concreteLts =
        dynamic_cast<seissol::initializer::LTSImposedSlipRates const* const>(dynRup);
    imposedSlipDirection1 = layerData.var(concreteLts->imposedSlipDirection1);
    imposedSlipDirection2 = layerData.var(concreteLts->imposedSlipDirection2);
    stf.copyLtsTreeToLocal(layerData, dynRup, fullUpdateTime);
  }

  void updateFrictionAndSlip(FaultStresses const& faultStresses,
                             TractionResults& tractionResults,
                             std::array<real, misc::numPaddedPoints>& stateVariableBuffer,
                             std::array<real, misc::numPaddedPoints>& strengthBuffer,
                             unsigned ltsFace,
                             unsigned timeIndex) {
    const real timeIncrement = this->deltaT[timeIndex];
    real currentTime = this->mFullUpdateTime;
    for (unsigned i = 0; i <= timeIndex; i++) {
      currentTime += this->deltaT[i];
    }

#pragma omp simd
    for (unsigned pointIndex = 0; pointIndex < misc::numPaddedPoints; pointIndex++) {
      const real stfEvaluated = stf.evaluate(currentTime, timeIncrement, ltsFace, pointIndex);

      this->traction1[ltsFace][pointIndex] =
          faultStresses.traction1[timeIndex][pointIndex] -
          this->impAndEta[ltsFace].etaS * imposedSlipDirection1[ltsFace][pointIndex] * stfEvaluated;
      this->traction2[ltsFace][pointIndex] =
          faultStresses.traction2[timeIndex][pointIndex] -
          this->impAndEta[ltsFace].etaS * imposedSlipDirection2[ltsFace][pointIndex] * stfEvaluated;

      this->slipRate1[ltsFace][pointIndex] =
          this->imposedSlipDirection1[ltsFace][pointIndex] * stfEvaluated;
      this->slipRate2[ltsFace][pointIndex] =
          this->imposedSlipDirection2[ltsFace][pointIndex] * stfEvaluated;
      this->slipRateMagnitude[ltsFace][pointIndex] = misc::magnitude(
          this->slipRate1[ltsFace][pointIndex], this->slipRate2[ltsFace][pointIndex]);

      // Update slip
      this->slip1[ltsFace][pointIndex] += this->slipRate1[ltsFace][pointIndex] * timeIncrement;
      this->slip2[ltsFace][pointIndex] += this->slipRate2[ltsFace][pointIndex] * timeIncrement;
      this->accumulatedSlipMagnitude[ltsFace][pointIndex] +=
          this->slipRateMagnitude[ltsFace][pointIndex] * timeIncrement;

      tractionResults.traction1[timeIndex][pointIndex] = this->traction1[ltsFace][pointIndex];
      tractionResults.traction2[timeIndex][pointIndex] = this->traction2[ltsFace][pointIndex];
    }
  }

  void preHook(std::array<real, misc::numPaddedPoints>& stateVariableBuffer, unsigned ltsFace) {}
  void postHook(std::array<real, misc::numPaddedPoints>& stateVariableBuffer, unsigned ltsFace) {}
  void saveDynamicStressOutput(unsigned int ltsFace) {}

  protected:
  real (*imposedSlipDirection1)[misc::numPaddedPoints];
  real (*imposedSlipDirection2)[misc::numPaddedPoints];
  STF stf{};
};

} // namespace seissol::dr::friction_law
#endif // SEISSOL_IMPOSEDSLIPRATES_H
