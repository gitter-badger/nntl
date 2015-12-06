/*
This file is a part of NNTL project (https://github.com/Arech/nntl)

Copyright (c) 2015, Arech (aradvert@gmail.com; https://github.com/Arech)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of NNTL nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// this file provides definitions of weights initialization algorithms

#pragma once

#include "interface/rng/distr_normal_naive.h"

namespace nntl {
namespace weights_init {

	// According to Xavier et al. "Understanding the difficulty of training deep feedforward neural networks" 2010
	// for symmetric activation function (probably with unit derivative at 0) it's a 
	// sqrt(6/(prevLayerNeurons+thisLayerNeurons))  - best for Tanh. Probably could fit SoftSign and etc.
	//
	// According to http://deeplearning.net/tutorial/mlp.html for sigmoid it's a
	// 4*sqrt(6/(prevLayerNeurons+thisLayerNeurons))
	// 
	// And by the way, according to mentioned work it looks like this formula works best for same-sized adjacent layers. If their
	// sizes are different, it's kind of compromise. Which means, that there might be other more suitable initialization schemes.
	template<size_t scalingCoeff1e6 = 1000000>
	struct Xavier {
		template <typename iRng_t>
		static bool init(typename iRng_t::realmtx_t& W, iRng_t& iR)noexcept {
			typedef typename iRng_t::real_t real_t;
			NNTL_ASSERT(!W.empty() && W.numel() > 0);

			const real_t scalingCoeff = real_t(scalingCoeff1e6) / real_t(1000000);

			//probably we should count a bias unit as incoming too, so no -1 here
			const real_t weightsScale = scalingCoeff*sqrt(real_t(6.0) / (W.rows() + W.cols()));
			iR.gen_matrix(W, weightsScale);
			return true;
		}
	};

	// for sigm
	typedef Xavier<4000000> XavierFour;

	//////////////////////////////////////////////////////////////////////////
	// For ReLU:
	// according to He, Zhang et al. "Delving Deep into Rectifiers: Surpassing Human-Level Performance on 
	// ImageNet Classification" 2015 the formula (sqrt(2/prevLayerNeurons) should define _standard_ deviation
	// of gaussian zero-mean noise. Bias weights set to zero.
	template<size_t scalingCoeff1e6 = 1000000>
	struct He_Zhang {
		template <typename iRng_t>
		static bool init(typename iRng_t::realmtx_t& W, iRng_t& iR)noexcept {
			typedef typename iRng_t::real_t real_t;
			typedef typename iRng_t::realmtx_t realmtx_t;

			NNTL_ASSERT(!W.empty() && W.numel() > 0);

			const real_t scalingCoeff = real_t(scalingCoeff1e6) / real_t(1000000);
			const auto prevLayerNeuronsCnt = W.cols() - 1;
			const real_t stdDev = scalingCoeff*sqrt(real_t(2.0) / prevLayerNeuronsCnt);

			rng::distr_normal_naive<iRng_t> d(iR, real_t(0.0), stdDev);
			d.gen_vector(W.dataAsVec(), realmtx_t::sNumel(W.rows(), prevLayerNeuronsCnt));

			auto pBiases = W.colDataAsVec(prevLayerNeuronsCnt);
			std::fill(pBiases, pBiases + W.rows(), real_t(0.0));
			return true;
		}
	};


	//////////////////////////////////////////////////////////////////////////
	// According to Martens "Deep learning via Hessian-free optimization" 2010 and
	// Sutskever, Martens et al. "On the importance of initialization and momentum in deep learning" 2013,
	// there is another very effective scheme of weights initialization they call "Sparse initialization (ST)"
	// Sigm: "In this scheme, each random unit is connected to 15 randomly chosen units in  the previous layer,
	// whose weights are drawn from a unit Gaussian, and the biases are set to zero."
	// Tanh: "When using tanh units, we transform the weights to simulate sigmoid units by setting 
	// the biases to 0.5 and rescaling the weights by 0.25."
	template<int Biases1e6 = 0, size_t StdDev1e6=1000000, unsigned int NonZeroUnitsCount = 15>
	struct Martens_SI {
		template <typename iRng_t>
		static bool init(typename iRng_t::realmtx_t& W, iRng_t& iR)noexcept {
			typedef typename iRng_t::real_t real_t;
			typedef typename iRng_t::realmtx_t realmtx_t;
			typedef realmtx_t::vec_len_t vec_len_t;

			NNTL_ASSERT(!W.empty() && W.numel() > 0);
			const auto prevLayerNeuronsCnt = W.cols() - 1;
			const auto thisLayerNeuronsCnt = W.rows();
			NNTL_ASSERT(prevLayerNeuronsCnt >= NonZeroUnitsCount);

			W.zeros();

			const real_t biases = real_t(Biases1e6) / real_t(1000000);
			const real_t stdDev = real_t(StdDev1e6) / real_t(1000000);

			//making random indexes of weights to set
			std::unique_ptr<vec_len_t[]> columnIdxs(new(std::nothrow) vec_len_t[prevLayerNeuronsCnt]);
			auto pIdxs = columnIdxs.get();
			if (nullptr == pIdxs) return false;
			const auto pIdxsE = pIdxs + prevLayerNeuronsCnt;
			for (vec_len_t i = 0; i < prevLayerNeuronsCnt; ++i) pIdxs[i] = i;

			//generating weights
			realmtx_t src(NonZeroUnitsCount, thisLayerNeuronsCnt);
			if (src.isAllocationFailed())return false;
			rng::distr_normal_naive<iRng_t> d(iR, real_t(0.0), stdDev);
			d.gen_matrix(src);

			auto pS = src.dataAsVec();
			//setting rowwise (I don't think we can significantly simply this code and leave the same quality of randomness)
			for (vec_len_t r = 0; r < thisLayerNeuronsCnt; ++r) {
				std::random_shuffle(pIdxs, pIdxsE, iR);
				for (vec_len_t cIdx = 0; cIdx < NonZeroUnitsCount; ++cIdx) {
					W.set(r, pIdxs[cIdx], *pS++);
				}
			}

			if (biases != real_t(0.0)) {
				auto pBiases = W.colDataAsVec(prevLayerNeuronsCnt);
				std::fill(pBiases, pBiases + thisLayerNeuronsCnt, biases);
			}
			return true;
		}
	};

	template <unsigned int NonZeroUnitsCount = 15>
	using Martens_SI_sigm = Martens_SI<0, 1000000, NonZeroUnitsCount>;

	template <unsigned int NonZeroUnitsCount = 15>
	using Martens_SI_tanh = Martens_SI<500000, 250000, NonZeroUnitsCount>;

}
}