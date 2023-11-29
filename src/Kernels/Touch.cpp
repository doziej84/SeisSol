// Copyright (C) 2013-2023 SeisSol group
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause

#include "Touch.h"

#include <generated_code/tensor.h>
#include <yateto.h>

#ifdef ACL_DEVICE
#include "Parallel/AcceleratorDevice.h"
#endif

namespace seissol::kernels {

void touchBuffersDerivatives(real** buffers, real** derivatives, unsigned numberOfCells) {
#ifdef ACL_DEVICE
  constexpr auto qSize = tensor::Q::size();
  constexpr auto dQSize = yateto::computeFamilySize<tensor::dQ>();

  if (numberOfCells > 0) {
  
    void* stream = device::DeviceInstance::getInstance().api->getDefaultStream();

    device::DeviceInstance::getInstance().algorithms.setToValue(buffers, 0, qSize, numberOfCells, stream);
    device::DeviceInstance::getInstance().algorithms.setToValue(derivatives, 0, dQSize, numberOfCells, stream);

    device::DeviceInstance::getInstance().api->syncDefaultStreamWithHost();

  }
#else
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (unsigned cell = 0; cell < numberOfCells; ++cell) {
    // touch buffers
    real* buffer = buffers[cell];
    if (buffer != NULL) {
      for (unsigned dof = 0; dof < tensor::Q::size(); ++dof) {
        // zero time integration buffers
        buffer[dof] = (real)0;
      }
    }

    // touch derivatives
    real* derivative = derivatives[cell];
    if (derivative != NULL) {
      for (unsigned dof = 0; dof < yateto::computeFamilySize<tensor::dQ>(); ++dof) {
        derivative[dof] = (real)0;
      }
    }
  }
#endif
}

void fillWithStuff(real* buffer, unsigned nValues, [[maybe_unused]] bool onDevice) {
  // No real point for these numbers. Should be just something != 0 and != NaN and != Inf
  auto const stuff = [](unsigned n) { return static_cast<real>((214013 * n + 2531011) / 65536); };
#ifdef ACL_DEVICE
  if (onDevice) {
    void* stream = device::DeviceInstance::getInstance().api->getDefaultStream();

    device::DeviceInstance::getInstance().algorithms.fillArray(buffer, 2531011.0 / 65536, nValues, stream);

    device::DeviceInstance::getInstance().api->syncDefaultStreamWithHost();
    return;
  }
#endif
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (unsigned n = 0; n < nValues; ++n) {
    buffer[n] = stuff(n);
  }
}

} // namespace seissol::kernels
