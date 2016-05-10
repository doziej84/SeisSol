/******************************************************************************
** Copyright (c) 2015, Intel Corporation                                     **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Alexander Heinecke (Intel Corp.)
******************************************************************************/
/**
 * @file
 * This file is part of SeisSol.
 *
 * @author Carsten Uphoff (c.uphoff AT tum.de, http://www5.in.tum.de/wiki/index.php/Carsten_Uphoff,_M.Sc.)
 *
 * @section LICENSE
 * Copyright (c) 2015, SeisSol Group
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 **/
 
#include "Manager.h"
#include "NRFReader.h"
#include "PointSource.h"

#include <Solver/Interoperability.h>
#include <utils/logger.h>
#include <cstring>

#ifdef USE_MPI
#include <mpi.h>
#endif

#if defined(__AVX__)
#include <immintrin.h>
#endif

extern seissol::Interoperability e_interoperability;

template<typename T>
class index_sort_by_value
{
private:
    T const* value;
public:
    index_sort_by_value(T const* value) : value(value) {}
    inline bool operator()(unsigned i, unsigned j) const {
        return value[i] < value[j];
    }
};

void seissol::sourceterm::findMeshIds(Vector3 const* centres, MeshReader const& mesh, unsigned numSources, short* contained, unsigned* meshIds)
{
  std::vector<Vertex> const& vertices = mesh.getVertices();
  std::vector<Element> const& elements = mesh.getElements();
  
  memset(contained, 0, numSources * sizeof(short));
  
  double (*planeEquations)[4][4] = static_cast<double(*)[4][4]>(seissol::memory::allocate(elements.size() * sizeof(double[4][4]), ALIGNMENT));
  for (unsigned elem = 0; elem < elements.size(); ++elem) {
    for (int face = 0; face < 4; ++face) {
      VrtxCoords n, p;
      MeshTools::pointOnPlane(elements[elem], face, vertices, p);
      MeshTools::normal(elements[elem], face, vertices, n);
      
      for (unsigned i = 0; i < 3; ++i) {
        planeEquations[elem][i][face] = n[i];
      }
      planeEquations[elem][3][face] = - MeshTools::dot(n, p);
    }
  }
  
  double (*centres1)[4] = new double[numSources][4];
  for (unsigned source = 0; source < numSources; ++source) {
    centres1[source][0] = centres[source].x;
    centres1[source][1] = centres[source].y;
    centres1[source][2] = centres[source].z;
    centres1[source][3] = 1.0;
  }

/// @TODO Could use the code generator for the following
#ifdef _OPENMP
    #pragma omp parallel for schedule(static)
#endif
  for (unsigned elem = 0; elem < elements.size(); ++elem) {
#if 0 //defined(__AVX__)
      __m256d zero = _mm256_setzero_pd();
      __m256d planeDims[4];
      for (unsigned i = 0; i < 4; ++i) {
        planeDims[i] = _mm256_load_pd(&planeEquations[elem][i][0]);
      }
#endif
    for (unsigned source = 0; source < numSources; ++source) {
      int l_notInside = 0;
#if 0 //defined(__AVX__)
      // Not working because <0 => 0 should actually be  <=0 => 0
      /*__m256d result = _mm256_setzero_pd();
      for (unsigned dim = 0; dim < 4; ++dim) {
        result = _mm256_add_pd(result, _mm256_mul_pd(planeDims[dim], _mm256_broadcast_sd(&centres1[source][dim])) );
      }
      // >0 => (2^64)-1 ; <0 = 0
      __m256d inside4 = _mm256_cmp_pd(result, zero, _CMP_GE_OQ);
      l_notInside = _mm256_movemask_pd(inside4);*/
#else
      double result[4] = { 0.0, 0.0, 0.0, 0.0 };
      for (unsigned dim = 0; dim < 4; ++dim) {
        for (unsigned face = 0; face < 4; ++face) {
          result[face] += planeEquations[elem][dim][face] * centres1[source][dim];
        }
      }
      for (unsigned face = 0; face < 4; ++face) {
        l_notInside += (result[face] > 0.0) ? 1 : 0;
      }
#endif
      if (l_notInside == 0) {
#ifdef _OPENMP
        #pragma omp critical
        {
#endif
          /* It might actually happen that a source is found in two tetrahedrons
           * if it lies on the boundary. In this case we arbitrarily assign
           * it to the one with the higher meshId.
           * @todo Check if this is a problem with the numerical scheme. */
          /*if (contained[source] != 0) {
             logError() << "source with id " << source << " was already found in a different element!";
          }*/
          contained[source] = 1;
          meshIds[source] = elem;
#ifdef _OPENMP
        }
#endif
      }
    }
  }
  
  seissol::memory::free(planeEquations);
  delete[] centres1;
}

#ifdef USE_MPI
void seissol::sourceterm::cleanDoubles(short* contained, unsigned numSources)
{
  int myrank;
  int size;
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  short* globalContained = new short[size * numSources];
  MPI_Allgather(contained, numSources, MPI_SHORT, globalContained, numSources, MPI_SHORT, MPI_COMM_WORLD);
  
  unsigned cleaned = 0;
  for (unsigned source = 0; source < numSources; ++source) {
    if (contained[source] == 1) {
      for (int rank = 0; rank < myrank; ++rank) {
        if (globalContained[rank * numSources + source] == 1) {
          contained[source] = 0;
          ++cleaned;
          break;
        }
      }
    }
  }
  
  if (cleaned > 0) {
    logInfo(myrank) << "Cleaned " << cleaned << " double occurring sources on rank " << myrank << ".";
  }
  
  delete[] globalContained;
}
#endif

void seissol::sourceterm::transformNRFSourceToInternalSource( Vector3 const&            centre,
                                                              unsigned                  element,
                                                              Subfault const&           subfault,
                                                              Offsets const&            offsets,
                                                              Offsets const&            nextOffsets,
                                                              double *const             sliprates[3],
                                                              seissol::model::Material  material,
                                                              PointSources&             pointSources,
                                                              unsigned                  index )
{
  e_interoperability.computeMInvJInvPhisAtSources( centre.x,
                                                   centre.y,
                                                   centre.z,
                                                   element,
                                                   pointSources.mInvJInvPhisAtSources[index] );
  
  real* faultBasis = pointSources.tensor[index];
  faultBasis[0] = subfault.tan1.x;
  faultBasis[1] = subfault.tan1.y;
  faultBasis[2] = subfault.tan1.z;
  faultBasis[3] = subfault.tan2.x;
  faultBasis[4] = subfault.tan2.y;
  faultBasis[5] = subfault.tan2.z;
  faultBasis[6] = subfault.normal.x;
  faultBasis[7] = subfault.normal.y;
  faultBasis[8] = subfault.normal.z;
  
  double mu = (subfault.mu == 0.0) ? material.mu : subfault.mu;  
  pointSources.muA[index] = mu * subfault.area;
  pointSources.lambdaA[index] = material.lambda * subfault.area;
  
  for (unsigned sr = 0; sr < 3; ++sr) {
    unsigned numSamples = nextOffsets[sr] - offsets[sr];
    double const* samples = (numSamples > 0) ? &sliprates[sr][ offsets[sr] ] : NULL;
    samplesToPiecewiseLinearFunction1D( samples,
                                        numSamples,
                                        subfault.tinit,
                                        subfault.timestep,
                                        &pointSources.slipRates[index][sr] );
  }
}

void seissol::sourceterm::Manager::freeSources()
{
  delete[] cmps;
  delete[] sources;
  cmps = NULL;
  sources = NULL;
}

void seissol::sourceterm::Manager::mapPointSourcesToClusters( unsigned const*                 meshIds,
                                                              unsigned                        numberOfSources,
                                                              seissol::initializers::LTSTree* ltsTree,
                                                              seissol::initializers::LTS*     lts,
                                                              seissol::initializers::Lut*     ltsLut )
{
  std::vector<std::vector<unsigned> > clusterToPointSources(ltsTree->numChildren());
  std::vector<unsigned> clusterToNumberOfMappings(ltsTree->numChildren(), 0);
  
  for (unsigned source = 0; source < numberOfSources; ++source) {
    unsigned meshId = meshIds[source];
    unsigned (&ltsIds)[seissol::initializers::Lut::MaxDuplicates] = ltsLut->ltsIds(meshId);
    unsigned cluster = ltsTree->findTimeClusterId(ltsIds[0]);
    clusterToPointSources[cluster].push_back(source);
    for (unsigned dup = 0; dup < seissol::initializers::Lut::MaxDuplicates && ltsIds[dup] != std::numeric_limits<unsigned>::max(); ++dup) {
      assert(cluster == ltsTree->findTimeClusterId(ltsIds[dup]));
      ++clusterToNumberOfMappings[cluster];
    }
  }
  
  cmps = new ClusterMapping[ltsTree->numChildren()];
  for (unsigned cluster = 0; cluster < ltsTree->numChildren(); ++cluster) {
    cmps[cluster].sources           = new unsigned[ clusterToPointSources[cluster].size() ];
    cmps[cluster].numberOfSources   = clusterToPointSources[cluster].size();
    cmps[cluster].cellToSources     = new CellToPointSourcesMapping[ clusterToNumberOfMappings[cluster] ];
    cmps[cluster].numberOfMappings  = clusterToNumberOfMappings[cluster];
    
    for (unsigned source = 0; source < clusterToPointSources[cluster].size(); ++source) {
      cmps[cluster].sources[source] = clusterToPointSources[cluster][source];
    }
    std::sort(cmps[cluster].sources, cmps[cluster].sources + cmps[cluster].numberOfSources, index_sort_by_value<unsigned>(meshIds));
    
    unsigned clusterSource = 0;
    unsigned mapping = 0;
    while (clusterSource < cmps[cluster].numberOfSources) {
      unsigned meshId = meshIds[ cmps[cluster].sources[clusterSource] ];
      unsigned next = clusterSource + 1;
      while (meshIds[next] == meshId && next < cmps[cluster].numberOfSources) {
        ++next;
      }
      
      unsigned (&ltsIds)[seissol::initializers::Lut::MaxDuplicates] = ltsLut->ltsIds(meshId);
      for (unsigned dup = 0; dup < seissol::initializers::Lut::MaxDuplicates && ltsIds[dup] != std::numeric_limits<unsigned>::max(); ++dup) {
        seissol::initializers::Layer* layer = ltsTree->findLayer(ltsIds[dup]);
        assert(layer != NULL && layer->var(lts->dofs) != NULL);
        cmps[cluster].cellToSources[mapping].dofs = &layer->var(lts->dofs)[ltsIds[dup] - layer->getLtsIdStart()];
        cmps[cluster].cellToSources[mapping].pointSourcesOffset = clusterSource;
        cmps[cluster].cellToSources[mapping].numberOfPointSources = next - clusterSource;
      }      
      
      clusterSource = next;
    }
    assert(mapping == cmps[cluster].numberOfMappings);
  }
}

void seissol::sourceterm::Manager::loadSourcesFromFSRM( double const*                   momentTensor,
                                                        int                             numberOfSources,
                                                        double const*                   centres,
                                                        double const*                   strikes,
                                                        double const*                   dips,
                                                        double const*                   rakes,
                                                        double const*                   onsets,
                                                        double const*                   areas,
                                                        double                          timestep,
                                                        int                             numberOfSamples,
                                                        double const*                   timeHistories,
                                                        MeshReader const&               mesh,
                                                        seissol::initializers::LTSTree* ltsTree,
                                                        seissol::initializers::LTS*     lts,
                                                        seissol::initializers::Lut*     ltsLut,
                                                        time_stepping::TimeManager&     timeManager )
{
  freeSources();
  
  int rank;
#ifdef USE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#else
  rank = 0;
#endif
  
  logInfo(rank) << "<--------------------------------------------------------->";
  logInfo(rank) << "<                      Point sources                      >";
  logInfo(rank) << "<--------------------------------------------------------->";

  short* contained = new short[numberOfSources];
  unsigned* meshIds = new unsigned[numberOfSources];
  Vector3* centres3 = new Vector3[numberOfSources];
  
  for (int source = 0; source < numberOfSources; ++source) {
    centres3[source].x = centres[3*source];
    centres3[source].y = centres[3*source + 1];
    centres3[source].z = centres[3*source + 2];
  }
  
  logInfo(rank) << "Finding meshIds for point sources...";
  
  findMeshIds(centres3, mesh, numberOfSources, contained, meshIds);

#ifdef USE_MPI
  logInfo(rank) << "Cleaning possible double occurring point sources for MPI...";
  cleanDoubles(contained, numberOfSources);
#endif

  unsigned* originalIndex = new unsigned[numberOfSources];
  unsigned numSources = 0;
  for (int source = 0; source < numberOfSources; ++source) {
    originalIndex[numSources] = source;
    meshIds[numSources] = meshIds[source];
    numSources += contained[source];
  }
  delete[] contained;

  logInfo(rank) << "Mapping point sources to LTS cells...";
  mapPointSourcesToClusters(meshIds, numSources, ltsTree, lts, ltsLut);
  
  real localMomentTensor[3][3];
  for (unsigned i = 0; i < 9; ++i) {
    *(&localMomentTensor[0][0] + i) = momentTensor[i];
  }
  
  sources = new PointSources[ltsTree->numChildren()];
  for (unsigned cluster = 0; cluster < ltsTree->numChildren(); ++cluster) {
    sources[cluster].mode                  = PointSources::FSRM;
    sources[cluster].numberOfSources       = cmps[cluster].numberOfSources;
    /// \todo allocate aligned or remove ALIGNED_
    sources[cluster].mInvJInvPhisAtSources = new real[cmps[cluster].numberOfSources][NUMBER_OF_ALIGNED_BASIS_FUNCTIONS];
    sources[cluster].tensor                = new real[cmps[cluster].numberOfSources][9];
    sources[cluster].slipRates             = new PiecewiseLinearFunction1D[cmps[cluster].numberOfSources][3];

    for (unsigned clusterSource = 0; clusterSource < cmps[cluster].numberOfSources; ++clusterSource) {
      unsigned sourceIndex = cmps[cluster].sources[clusterSource];
      unsigned fsrmIndex = originalIndex[sourceIndex];
      
      e_interoperability.computeMInvJInvPhisAtSources( centres3[fsrmIndex].x,
                                                       centres3[fsrmIndex].y,
                                                       centres3[fsrmIndex].z,
                                                       meshIds[sourceIndex],
                                                       sources[cluster].mInvJInvPhisAtSources[clusterSource] );

      transformMomentTensor( localMomentTensor,
                             strikes[fsrmIndex],
                             dips[fsrmIndex],
                             rakes[fsrmIndex],
                             sources[cluster].tensor[clusterSource] );
      for (unsigned i = 0; i < 9; ++i) {
        sources[cluster].tensor[clusterSource][i] *= areas[fsrmIndex];
      }
      
      samplesToPiecewiseLinearFunction1D( &timeHistories[fsrmIndex * numberOfSamples],
                                          numberOfSamples,
                                          onsets[fsrmIndex],
                                          timestep,
                                          &sources[cluster].slipRates[clusterSource][0] );
    }
  }
  delete[] originalIndex;
  delete[] meshIds;
  delete[] centres3;
  
  timeManager.setPointSourcesForClusters(cmps, sources);
  
  logInfo(rank) << ".. finished point source initialization.";
}

#ifdef USE_NETCDF
void seissol::sourceterm::Manager::loadSourcesFromNRF(  char const*                     fileName,
                                                        MeshReader const&               mesh,
                                                        seissol::initializers::LTSTree* ltsTree,
                                                        seissol::initializers::LTS*     lts,
                                                        seissol::initializers::Lut*     ltsLut,
                                                        time_stepping::TimeManager&     timeManager )
{
  freeSources();
  
  int rank;
#ifdef USE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#else
  rank = 0;
#endif

  logInfo(rank) << "<--------------------------------------------------------->";
  logInfo(rank) << "<                      Point sources                      >";
  logInfo(rank) << "<--------------------------------------------------------->";
  
  logInfo(rank) << "Reading" << fileName;
  NRF nrf;
  readNRF(fileName, nrf);
  
  short* contained = new short[nrf.source];
  unsigned* meshIds = new unsigned[nrf.source];
  
  logInfo(rank) << "Finding meshIds for point sources...";
  findMeshIds(nrf.centres, mesh, nrf.source, contained, meshIds);

#ifdef USE_MPI
  logInfo(rank) << "Cleaning possible double occurring point sources for MPI...";
  cleanDoubles(contained, nrf.source);
#endif

  unsigned* originalIndex = new unsigned[nrf.source];
  unsigned numSources = 0;
  for (unsigned source = 0; source < nrf.source; ++source) {
    originalIndex[numSources] = source;
    meshIds[numSources] = meshIds[source];
    numSources += contained[source];
  }
  delete[] contained;

  logInfo(rank) << "Mapping point sources to LTS cells...";
  mapPointSourcesToClusters(meshIds, numSources, ltsTree, lts, ltsLut);
  
  sources = new PointSources[ltsTree->numChildren()];
  for (unsigned cluster = 0; cluster < ltsTree->numChildren(); ++cluster) {
    sources[cluster].mode                  = PointSources::NRF;
    sources[cluster].numberOfSources       = cmps[cluster].numberOfSources;
    /// \todo allocate aligned or remove ALIGNED_
    sources[cluster].mInvJInvPhisAtSources = new real[cmps[cluster].numberOfSources][NUMBER_OF_ALIGNED_BASIS_FUNCTIONS];
    sources[cluster].tensor                = new real[cmps[cluster].numberOfSources][9];
    sources[cluster].muA                   = new real[cmps[cluster].numberOfSources];
    sources[cluster].lambdaA               = new real[cmps[cluster].numberOfSources];
    sources[cluster].slipRates             = new PiecewiseLinearFunction1D[cmps[cluster].numberOfSources][3];

    for (unsigned clusterSource = 0; clusterSource < cmps[cluster].numberOfSources; ++clusterSource) {
      unsigned sourceIndex = cmps[cluster].sources[clusterSource];
      unsigned nrfIndex = originalIndex[sourceIndex];
      transformNRFSourceToInternalSource( nrf.centres[nrfIndex],
                                          meshIds[sourceIndex],
                                          nrf.subfaults[nrfIndex],
                                          nrf.sroffsets[nrfIndex],
                                          nrf.sroffsets[nrfIndex+1],
                                          nrf.sliprates,
                                          ltsLut->lookup(lts->material, meshIds[sourceIndex]).local,
                                          sources[cluster],
                                          clusterSource );
    }
  }
  delete[] originalIndex;
  delete[] meshIds;
  
  timeManager.setPointSourcesForClusters(cmps, sources);
  
  logInfo(rank) << ".. finished point source initialization.";
}
#endif
