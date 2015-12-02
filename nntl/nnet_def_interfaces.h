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
#pragma once

/*
#include "interface/threads/winqduc2.h"
#ifndef NNTL_THREADS_WINQDU_AVAILABLE
#include "interface/threads/stdc2.h"
#endif
*/
#include "interface/threads/winqdu.h"
#ifndef NNTL_THREADS_WINQDU_AVAILABLE
#include "interface/threads/std.h"
#endif

#include "interface/math/i_yeppp_openblas.h"

#include "interface/rng/std.h"
//#include "interface/rng/AFRandom.h"
#include "interface/rng/AFRandom_mt.h"


namespace nntl {

	//default nnet template parameters definition
	struct nnet_def_interfaces {

#ifdef NNTL_THREADS_WINQDU_AVAILABLE
		typedef threads::WinQDU<math_types::floatmtx_ty::numel_cnt_t> iThreads_t;
		//typedef threads::Std<math_types::floatmtx_ty::numel_cnt_t> iThreads_t;
#else
		typedef threads::Std<math_types::floatmtx_ty::numel_cnt_t> iThreads_t;
#endif // NNTL_THREADS_WINQDU_AVAILABLE

		typedef math::iMath_basic<iThreads_t> iMath_t;

		//typedef rng::Std iRng_t;
		//typedef rng::AFRandom<Agner_Fog::CRandomSFMT0> iRng_t;
		typedef rng::AFRandom_mt<Agner_Fog::CRandomSFMT0, iThreads_t> iRng_t;
	};

}