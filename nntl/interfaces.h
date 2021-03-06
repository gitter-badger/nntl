/*
This file is a part of NNTL project (https://github.com/Arech/nntl)

Copyright (c) 2015-2016, Arech (aradvert@gmail.com; https://github.com/Arech)
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
#pragma once

#include "interface/threads/workers.h"
#include "interface/math/mathn_mt.h"
#include "interface/rng/afrand_mt.h"
#include "interface/inspectors/dummy.h"

namespace nntl {

	template<typename RealT>
	struct d_int_nI {
		typedef RealT real_t;

// #ifdef NNTL_THREADS_WINQDU_AVAILABLE
// 		typedef threads::WinQDU<real_t, math::smatrix_td::numel_cnt_t> iThreads_t;
// #else
// 		typedef threads::Std<real_t, math::smatrix_td::numel_cnt_t> iThreads_t;
// #endif
		//WinQDU+forwarder is about 1.5/22 ~ 7% faster than Std+std::function in one sample setup (everything else remains fixed)
		
		typedef threads::Workers<real_t, math::smatrix_td::numel_cnt_t> iThreads_t;

		//_mt is deprecated. Going to make run-time profiler sometime
		//typedef math::MathN_mt<real_t, iThreads_t> iMath_t;
		typedef math::MathN<real_t, iThreads_t> iMath_t;

		typedef rng::AFRand_mt<real_t, AFog::CRandomSFMT0, iThreads_t> iRng_t;
	};

	//default interfaces definition
	struct d_interfaces :public d_int_nI<math::d_real_t> {
		typedef inspector::dummy<real_t> iInspect_t;
	};

	template<typename RealT>//, typename InspectorT = inspector::dummy<RealT>>
	struct dt_interfaces :public d_int_nI<RealT> {
		typedef inspector::dummy<real_t> iInspect_t;
		//typedef InspectorT iInspect_t;
	};

	template<typename InterfacesT>
	struct interfaces_td {
		typedef InterfacesT interfaces_t;

		typedef typename interfaces_t::real_t real_t;
		typedef typename interfaces_t::iMath_t iMath_t;
		typedef typename interfaces_t::iRng_t iRng_t;
		typedef typename interfaces_t::iThreads_t iThreads_t;
		typedef typename interfaces_t::iInspect_t iInspect_t;

		static_assert(::std::is_base_of<math::_i_math<real_t>, iMath_t>::value, "iMath_t type should be derived from _i_math");
		static_assert(::std::is_base_of<rng::_i_rng<real_t>, iRng_t>::value, "iRng_t type should be derived from _i_rng");
		static_assert(::std::is_base_of<threads::_i_threads<real_t, typename iThreads_t::range_t>, iThreads_t>::value, "iThreads_t type should be derived from _i_threads");
		static_assert(::std::is_base_of<inspector::_i_inspector<real_t>, iInspect_t>::value, "iInspect_t type should be derived from i_inspector");

		static_assert(::std::is_same<iThreads_t, typename iMath_t::iThreads_t>::value, "Math interface must use the same iThreadsT as specified in InterfacesT!");
		static_assert(::std::is_same<iThreads_t, typename iRng_t::iThreads_t>::value, "Math interface must use the same iThreadsT as specified in InterfacesT!");
		
		static_assert(::std::is_same<real_t, typename iThreads_t::real_t>::value, "real_t must resolve to the same type!");
		static_assert(::std::is_same<real_t, typename iMath_t::real_t>::value, "real_t must resolve to the same type!");
		static_assert(::std::is_same<real_t, typename iRng_t::real_t>::value, "real_t must resolve to the same type!");
		static_assert(::std::is_same<real_t, typename iInspect_t::real_t>::value, "real_t must resolve to the same type!");

		static_assert(::std::is_same<typename iMath_t::numel_cnt_t, typename iThreads_t::range_t>::value, "iThreads_t::range_t must be the same as iMath_t::numel_cnt_t!");
	};

}