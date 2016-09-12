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
#include "stdafx.h"

//TODO: cleanup all this mess. Move all performance testing code into separate module and leave only correctness tests.

#include "../nntl/math.h"
#include "../nntl/common.h"

#include "../nntl/interface/math/mathn.h"
#include "../nntl/interfaces.h"

#include "../nntl/_supp/io/jsonreader.h"

#include <array>
#include <numeric>

#include "../nntl/utils/prioritize_workers.h"
#include "../nntl/utils/tictoc.h"
#include "imath_etalons.h"

using namespace nntl;
using namespace nntl::utils;

typedef d_interfaces::iThreads_t iThreads_t;
typedef math::MathN<real_t, iThreads_t> imath_basic_t;

static imath_basic_t iM;
const vec_len_t g_MinDataSizeDelta = 2 * iM.ithreads().workers_count() + 2;


using namespace std::chrono;

#ifdef TESTS_SKIP_LONGRUNNING
constexpr unsigned TEST_PERF_REPEATS_COUNT = 10;
//constexpr unsigned TEST_CORRECTN_REPEATS_COUNT = 100;
constexpr unsigned TEST_CORRECTN_REPEATS_COUNT = 30, _baseRowsCnt = 30;
#else
constexpr unsigned TEST_PERF_REPEATS_COUNT = 500;
//constexpr unsigned TEST_CORRECTN_REPEATS_COUNT = 50;
constexpr unsigned TEST_CORRECTN_REPEATS_COUNT = 60, _baseRowsCnt = 300;
#endif // NNTL_DEBUG

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


template<typename base_t> struct loss_sigm_xentropy_EPS {};
template<> struct loss_sigm_xentropy_EPS<double> { static constexpr double eps = 1e-10; };
template<> struct loss_sigm_xentropy_EPS<float> { static constexpr float eps = 7e-5f; };
void test_loss_sigm_xentropy(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "loss_sigm_xentropy");
	constexpr unsigned testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;
	const real_t frac = .5;
	realmtx_t A(rowsCnt, colsCnt), Y(rowsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed() && !Y.isAllocationFailed());

	iM.preinit(A.numel());
	ASSERT_TRUE(iM.init());
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_norm(A);
		rg.gen_matrix_norm(Y);
		iM.ewBinarize_ip(Y, frac);

		real_t loss, etLoss = loss_sigm_xentropy_ET(A, Y);

		loss = iM.loss_sigm_xentropy_st(A, Y);
		ASSERT_NEAR(etLoss, loss, loss_sigm_xentropy_EPS<real_t>::eps);

		loss = iM.loss_sigm_xentropy_mt(A, Y);
		ASSERT_NEAR(etLoss, loss, loss_sigm_xentropy_EPS<real_t>::eps);

		loss = iM.loss_sigm_xentropy(A, Y);
		ASSERT_NEAR(etLoss, loss, loss_sigm_xentropy_EPS<real_t>::eps);
	}
}

TEST(TestMathN, lossSigmXentropy) {
	const numel_cnt_t elmsMax = g_MinDataSizeDelta;
	for (numel_cnt_t e = 1; e < elmsMax; ++e) {
		ASSERT_NO_FATAL_FAILURE(test_loss_sigm_xentropy(static_cast<vec_len_t>(e), 1));
	}
	constexpr unsigned rowsCnt = _baseRowsCnt;
	const vec_len_t maxCols = g_MinDataSizeDelta, maxRows = rowsCnt + g_MinDataSizeDelta;
	for (vec_len_t r = rowsCnt; r < maxRows; ++r) {
		for (vec_len_t c = 1; c < maxCols; ++c) ASSERT_NO_FATAL_FAILURE(test_loss_sigm_xentropy(r, c));
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void test_ewBinarize_ip_corr(vec_len_t rowsCnt, vec_len_t colsCnt = 10, const real_t frac = .5) {
	MTXSIZE_SCOPED_TRACE1(rowsCnt, colsCnt, "ewBinarize_ip, frac=", frac);
	constexpr unsigned testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t A(rowsCnt, colsCnt), A_orig(rowsCnt, colsCnt), A_ET(rowsCnt, colsCnt);
	ASSERT_TRUE(!A_orig.isAllocationFailed() && !A.isAllocationFailed() && !A_ET.isAllocationFailed());
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_norm(A_orig);

		A_orig.cloneTo(A_ET);
		ewBinarize_ip_ET(A_ET, frac);

		A_orig.cloneTo(A);
		iM.ewBinarize_ip_st(A, frac);
		ASSERT_MTX_EQ(A_ET, A, "st() failed correctness test");

		A_orig.cloneTo(A);
		iM.ex_ewBinarize_ip_st(A, frac);
		ASSERT_MTX_EQ(A_ET, A, "ex_st() failed correctness test");

		A_orig.cloneTo(A);
		iM.ex2_ewBinarize_ip_st(A, frac);
		ASSERT_MTX_EQ(A_ET, A, "ex2_st() failed correctness test");



		A_orig.cloneTo(A);
		iM.ewBinarize_ip_mt(A, frac);
		ASSERT_MTX_EQ(A_ET, A, "mt() failed correctness test");

		A_orig.cloneTo(A);
		iM.ewBinarize_ip(A, frac);
		ASSERT_MTX_EQ(A_ET, A, "() failed correctness test");
	}

}

TEST(TestMathN, ewBinarizeIp) {
	const numel_cnt_t elmsMax = g_MinDataSizeDelta;
	for (numel_cnt_t e = 1; e < elmsMax; ++e) {
		ASSERT_NO_FATAL_FAILURE(test_ewBinarize_ip_corr(static_cast<vec_len_t>(e), 1, real_t(.5)));
		ASSERT_NO_FATAL_FAILURE(test_ewBinarize_ip_corr(static_cast<vec_len_t>(e), 1, real_t(.1)));
		ASSERT_NO_FATAL_FAILURE(test_ewBinarize_ip_corr(static_cast<vec_len_t>(e), 1, real_t(.9)));
	}

	constexpr unsigned rowsCnt = _baseRowsCnt;
	const vec_len_t maxCols = g_MinDataSizeDelta, maxRows = rowsCnt + g_MinDataSizeDelta;
	for (vec_len_t r = rowsCnt; r < maxRows; ++r) {
		for (vec_len_t c = 1; c < maxCols; ++c) ASSERT_NO_FATAL_FAILURE(test_ewBinarize_ip_corr(r, c, real_t(.5)));
	}
}

void test_ewBinarize_corr(vec_len_t rowsCnt, vec_len_t colsCnt = 10, const real_t frac = real_t(.5)) {
	MTXSIZE_SCOPED_TRACE1(rowsCnt, colsCnt, "ewBinarize, frac=", frac);
	constexpr unsigned testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	typedef math::smatrix<char> binmtx_t;

	realmtx_t A(rowsCnt, colsCnt);
	binmtx_t DestET(rowsCnt, colsCnt), Dest(rowsCnt, colsCnt);

	ASSERT_TRUE(!A.isAllocationFailed() && !DestET.isAllocationFailed() && !Dest.isAllocationFailed());
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_norm(A);

		ewBinarize_ET(DestET, A, frac);

		std::fill(Dest.begin(), Dest.end(), binmtx_t::value_type(-1));
		iM.ewBinarize_st(Dest, A, frac);
		ASSERT_MTX_EQ(DestET, Dest, "st() failed correctness test");

		std::fill(Dest.begin(), Dest.end(), binmtx_t::value_type(-1));
		iM.ewBinarize_mt(Dest, A, frac);
		ASSERT_MTX_EQ(DestET, Dest, "mt() failed correctness test");

		std::fill(Dest.begin(), Dest.end(), binmtx_t::value_type(-1));
		iM.ewBinarize(Dest, A, frac);
		ASSERT_MTX_EQ(DestET, Dest, "() failed correctness test");
	}
}

TEST(TestMathN, ewBinarize) {
	const numel_cnt_t elmsMax = g_MinDataSizeDelta;
	for (numel_cnt_t e = 1; e < elmsMax; ++e) {
		ASSERT_NO_FATAL_FAILURE(test_ewBinarize_corr(static_cast<vec_len_t>(e), 1, real_t(.5)));
		ASSERT_NO_FATAL_FAILURE(test_ewBinarize_corr(static_cast<vec_len_t>(e), 1, real_t(.1)));
		ASSERT_NO_FATAL_FAILURE(test_ewBinarize_corr(static_cast<vec_len_t>(e), 1, real_t(.9)));
	}

	constexpr unsigned rowsCnt = _baseRowsCnt;
	const vec_len_t maxCols = g_MinDataSizeDelta, maxRows = rowsCnt + g_MinDataSizeDelta;
	for (vec_len_t r = rowsCnt; r < maxRows; ++r) {
		for (vec_len_t c = 1; c < maxCols; ++c) ASSERT_NO_FATAL_FAILURE(test_ewBinarize_corr(r, c, real_t(.5)));
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename base_t> struct softmax_parts_EPS {};
template<> struct softmax_parts_EPS<double> { static constexpr double eps = 1e-10; };
template<> struct softmax_parts_EPS<float> { static constexpr float eps = 1e-5f; };

void test_softmax_parts(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "softmax_parts");
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);

	constexpr unsigned testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t A(rowsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed());
	const auto denominatorElmsMax = realmtx_t::sNumel(rowsCnt, iM.ithreads().workers_count());
	std::vector<real_t> vec_max(rowsCnt), vec_den(denominatorElmsMax), vec_num(dataSize), vec_den2(denominatorElmsMax), vec_num2(dataSize);

	iM.preinit(dataSize);
	ASSERT_TRUE(iM.init());
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	for (unsigned rr = 0; rr < testCorrRepCnt; ++rr) {
		rg.gen_matrix(A, 2);
		mrwMax_ET(A, &vec_max[0]);

		softmax_parts_ET(A, &vec_max[0], &vec_den[0], &vec_num[0]);

		std::fill(vec_den2.begin(), vec_den2.end(), real_t(-1));
		std::fill(vec_num2.begin(), vec_num2.end(), real_t(-1));
		iM.softmax_parts_st_rw(A, &vec_max[0], &vec_den2[0], &vec_num2[0]);
		//real denominator takes only a first row of vec_den
		for (unsigned i = 0; i < rowsCnt; ++i) ASSERT_NEAR(vec_den[i], vec_den2[i], softmax_parts_EPS<real_t>::eps) << "st_rw() failed denominator vector comparision @ " << i;
		ASSERT_VECTOR_NEAR(vec_num, vec_num2, "st_rw() failed numerator matrix comparision", softmax_parts_EPS<real_t>::eps);

		std::fill(vec_den2.begin(), vec_den2.end(), real_t(-1));
		std::fill(vec_num2.begin(), vec_num2.end(), real_t(-1));
		iM.softmax_parts_st_cw(A, &vec_max[0], &vec_den2[0], &vec_num2[0]);
		//real denominator takes only a first row of vec_den
		for (unsigned i = 0; i < rowsCnt; ++i) ASSERT_NEAR(vec_den[i], vec_den2[i], softmax_parts_EPS<real_t>::eps) << "st_cw() failed denominator vector comparision @ " << i;
		ASSERT_VECTOR_NEAR(vec_num, vec_num2, "st_cw() failed numerator matrix comparision", softmax_parts_EPS<real_t>::eps);

		std::fill(vec_den2.begin(), vec_den2.end(), real_t(-1));
		std::fill(vec_num2.begin(), vec_num2.end(), real_t(-1));
		iM.softmax_parts_st(A, &vec_max[0], &vec_den2[0], &vec_num2[0]);
		//real denominator takes only a first row of vec_den
		for (unsigned i = 0; i < rowsCnt; ++i) ASSERT_NEAR(vec_den[i], vec_den2[i], softmax_parts_EPS<real_t>::eps) << "st() failed denominator vector comparision @ " << i;
		ASSERT_VECTOR_NEAR(vec_num, vec_num2, "st() failed numerator matrix comparision", softmax_parts_EPS<real_t>::eps);

		if (colsCnt > imath_basic_t::Thresholds_t::softmax_parts_mt_cw_ColsPerThread) {
			std::fill(vec_den2.begin(), vec_den2.end(), real_t(-1));
			std::fill(vec_num2.begin(), vec_num2.end(), real_t(-1));
			iM.softmax_parts_mt_cw(A, &vec_max[0], &vec_den2[0], &vec_num2[0]);
			//real denominator takes only a first row of vec_den
			for (unsigned i = 0; i < rowsCnt; ++i) ASSERT_NEAR(vec_den[i], vec_den2[i], softmax_parts_EPS<real_t>::eps) << "mt_cw() failed denominator vector comparision @ " << i;
			ASSERT_VECTOR_NEAR(vec_num, vec_num2, "mt_cw() failed numerator matrix comparision", softmax_parts_EPS<real_t>::eps);
		}
		
		std::fill(vec_den2.begin(), vec_den2.end(), real_t(-1));
		std::fill(vec_num2.begin(), vec_num2.end(), real_t(-1));
		iM.softmax_parts_mt_rw(A, &vec_max[0], &vec_den2[0], &vec_num2[0]);
		//real denominator takes only a first row of vec_den
		for (unsigned i = 0; i < rowsCnt; ++i) ASSERT_NEAR(vec_den[i], vec_den2[i], softmax_parts_EPS<real_t>::eps) << "mt_rw() failed denominator vector comparision @ " << i;
		ASSERT_VECTOR_NEAR(vec_num, vec_num2, "mt_rw() failed numerator matrix comparision", softmax_parts_EPS<real_t>::eps);
				
		std::fill(vec_den2.begin(), vec_den2.end(), real_t(-1));
		std::fill(vec_num2.begin(), vec_num2.end(), real_t(-1));
		iM.softmax_parts_mt(A, &vec_max[0], &vec_den2[0], &vec_num2[0]);
		//real denominator takes only a first row of vec_den
		for (unsigned i = 0; i < rowsCnt; ++i) ASSERT_NEAR(vec_den[i], vec_den2[i], softmax_parts_EPS<real_t>::eps) << "mt() failed denominator vector comparision @ " << i;
		ASSERT_VECTOR_NEAR(vec_num, vec_num2, "mt() failed numerator matrix comparision", softmax_parts_EPS<real_t>::eps);

		std::fill(vec_den2.begin(), vec_den2.end(), real_t(-1));
		std::fill(vec_num2.begin(), vec_num2.end(), real_t(-1));
		iM.softmax_parts(A, &vec_max[0], &vec_den2[0], &vec_num2[0]);
		//real denominator takes only a first row of vec_den
		for (unsigned i = 0; i < rowsCnt; ++i) ASSERT_NEAR(vec_den[i], vec_den2[i], softmax_parts_EPS<real_t>::eps) << "() failed denominator vector comparision @ " << i;
		ASSERT_VECTOR_NEAR(vec_num, vec_num2, "() failed numerator matrix comparision", softmax_parts_EPS<real_t>::eps);
	}
}
TEST(TestMathN, SoftmaxParts) {
	constexpr unsigned rowsCnt = _baseRowsCnt;
	const vec_len_t maxCols = g_MinDataSizeDelta, maxRows = rowsCnt + g_MinDataSizeDelta;
	for (vec_len_t r = rowsCnt; r < maxRows; ++r) {
		for (vec_len_t c = 1; c < maxCols; ++c) ASSERT_NO_FATAL_FAILURE(test_softmax_parts(r, c));
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename base_t> struct softmax_EPS {};
template<> struct softmax_EPS<double> { static constexpr double eps = 1e-10; };
template<> struct softmax_EPS<float> { static constexpr double eps = 1e-5; };

template<bool bHasBiases>
void test_softmax(vec_len_t rowsCnt, vec_len_t colsCnt) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, bHasBiases ? "softmax with biases" : "softmax");
	constexpr unsigned testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtxdef_t A(rowsCnt, colsCnt, bHasBiases), A_ET(rowsCnt, colsCnt, bHasBiases), A_orig(rowsCnt, colsCnt, bHasBiases);
	ASSERT_TRUE(!A.isAllocationFailed() && !A_ET.isAllocationFailed() && !A_orig.isAllocationFailed());

	const auto maxSoftmaxMemSize = iM.softmax_needTempMem(A);
	iM.preinit(maxSoftmaxMemSize);
	ASSERT_TRUE(iM.init());
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	for (unsigned rr = 0; rr < testCorrRepCnt; ++rr) {
		if (bHasBiases) rg.gen_matrix_no_bias(A_orig, 5);
		else rg.gen_matrix(A_orig, 5);

		A_orig.cloneTo(A_ET);
		softmax_ET(A_ET, iM._get_thread_temp_raw_storage(maxSoftmaxMemSize));
		
		A_orig.cloneTo(A);
		iM.softmax_st(A);
		ASSERT_REALMTX_NEAR(A_ET, A, "st() failed", softmax_EPS<real_t>::eps);

		A_orig.cloneTo(A);
		iM.softmax_mt(A);
		ASSERT_REALMTX_NEAR(A_ET, A, "mt() failed", softmax_EPS<real_t>::eps);

		A_orig.cloneTo(A);
		iM.softmax(A);
		ASSERT_REALMTX_NEAR(A_ET, A, "() failed", softmax_EPS<real_t>::eps);
	}
}
TEST(TestMathN, Softmax) {
	constexpr unsigned rowsCnt = _baseRowsCnt;
	const vec_len_t maxCols = g_MinDataSizeDelta, maxRows = rowsCnt + g_MinDataSizeDelta;
	for (vec_len_t r = rowsCnt; r < maxRows; ++r) {
		for (vec_len_t c = 1; c < maxCols; ++c) {
			ASSERT_NO_FATAL_FAILURE(test_softmax<false>(r, c));
			ASSERT_NO_FATAL_FAILURE(test_softmax<true>(r, c));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename base_t> struct loss_softmax_xentropy_EPS {};
template<> struct loss_softmax_xentropy_EPS<double> { static constexpr double eps = 1e-10; };
template<> struct loss_softmax_xentropy_EPS<float> { static constexpr float eps = 4e-5f; };
void test_loss_softmax_xentropy(vec_len_t rowsCnt, vec_len_t colsCnt) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "loss_softmax_xentropy");
	constexpr unsigned testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;
	realmtx_t A(rowsCnt, colsCnt), Y(rowsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed() && !Y.isAllocationFailed());
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	real_t et, l;
	for (unsigned rr = 0; rr < testCorrRepCnt; ++rr) {
		rg.gen_matrix_norm(A);
		rg.gen_matrix_norm(Y);

		et = loss_softmax_xentropy_ET(A, Y);

		l = iM.loss_softmax_xentropy_st(A, Y);
		ASSERT_NEAR(et, l, loss_softmax_xentropy_EPS<real_t>::eps) << "st failed";

		l = iM.loss_softmax_xentropy_mt(A, Y);
		ASSERT_NEAR(et, l, loss_softmax_xentropy_EPS<real_t>::eps) << "mt failed";

		l = iM.loss_softmax_xentropy(A, Y);
		ASSERT_NEAR(et, l, loss_softmax_xentropy_EPS<real_t>::eps) << "() failed";
	}
}
TEST(TestMathN, LossSoftmaxXentropy) {
	constexpr unsigned rowsCnt = _baseRowsCnt;
	const vec_len_t maxCols = g_MinDataSizeDelta, maxRows = rowsCnt + g_MinDataSizeDelta;
	for (vec_len_t r = rowsCnt; r < maxRows; ++r) {
		for (vec_len_t c = 1; c < maxCols; ++c) ASSERT_NO_FATAL_FAILURE(test_loss_softmax_xentropy(r, c));
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


template<typename base_t> struct vSumAbs_EPS {};
template<> struct vSumAbs_EPS<double> { static constexpr double eps = 3e-10; };
template<> struct vSumAbs_EPS<float> { static constexpr float eps = .2f; };
template<typename iMath>
void test_vSumAbs(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing vSumAbs() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t A(rowsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix(A, 1);

		const auto vss = vSumAbs_ET(A);

		auto v = iM.vSumAbs_st(A);
		ASSERT_NEAR(vss, v, vSumAbs_EPS<real_t>::eps) << "vSumAbs_st failed correctness test";

		v = iM.vSumAbs_mt(A);
		ASSERT_NEAR(vss, v, vSumAbs_EPS<real_t>::eps) << "vSumAbs_mt failed correctness test";

		v = iM.vSumAbs(A);
		ASSERT_NEAR(vss, v, vSumAbs_EPS<real_t>::eps) << "vSumAbs failed correctness test";
	}

	tictoc tst, tmt, tb;
	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	//FFFFfffffffff... don't ever think about removing rg. calls that randomizes data...
	real_t vv = 0;
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(A, 2);
		tst.tic();
		vv += iM.vSumAbs_st(A);
		tst.toc();

		rg.gen_matrix(A, 2);
		tmt.tic();
		vv += iM.vSumAbs_mt(A);
		tmt.toc();

		rg.gen_matrix(A, 2);
		tb.tic();
		vv += iM.vSumAbs(A);
		tb.toc();
	}
	tst.say("st");
	tmt.say("mt");
	tb.say("best");
	STDCOUTL(vv);
}
TEST(TestMathN, vSumAbs) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::vSumAbs, 100) test_vSumAbs(iM, i, 100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename base_t> struct vSumSquares_EPS {};
template<> struct vSumSquares_EPS<double> { static constexpr double eps = 1e-10; };
template<> struct vSumSquares_EPS<float> { static constexpr float eps = .2f; };
template<typename iMath>
void test_vSumSquares(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing vSumSquares() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t A(rowsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix(A, 1);

		const auto vss = vSumSquares_ET(A);
		
		ASSERT_NEAR(vss, iM.vSumSquares_st(A), vSumSquares_EPS<real_t>::eps) << "vSumSquares_st failed correctness test";
		ASSERT_NEAR(vss, iM.vSumSquares_mt(A), vSumSquares_EPS<real_t>::eps) << "vSumSquares_mt failed correctness test";
		ASSERT_NEAR(vss, iM.vSumSquares(A), vSumSquares_EPS<real_t>::eps) << "vSumSquares failed correctness test";
	}

	tictoc tst, tmt, tb;
	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	//FFFFfffffffff... don't ever think about removing rg. calls that randomizes data...
	real_t vv = 0;
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(A, 2);
		tst.tic();
		vv+=iM.vSumSquares_st(A);
		tst.toc();

		rg.gen_matrix(A, 2);
		tmt.tic();
		vv += iM.vSumSquares_mt(A);
		tmt.toc();

		rg.gen_matrix(A, 2);
		tb.tic();
		vv += iM.vSumSquares(A);
		tb.toc();
	}
	tst.say("st");
	tmt.say("mt");
	tb.say("best");
	STDCOUTL(vv);
}
TEST(TestMathN, vSumSquares) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::vSumSquares, 100) test_vSumSquares(iM, i, 100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_evAddScaledSign_ip(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing evAddScaledSign_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t B(rowsCnt, colsCnt), A(rowsCnt, colsCnt);
	ASSERT_TRUE(!B.isAllocationFailed() && !A.isAllocationFailed());

	real_t scaleCoeff = .5;

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(B, 2);

	{
		realmtx_t A2(rowsCnt, colsCnt), A3(rowsCnt, colsCnt);
		ASSERT_TRUE(!A2.isAllocationFailed() && !A3.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(A, 2);
			A.cloneTo(A2);
			A.cloneTo(A3);

			evAddScaledSign_ip_ET(A2, scaleCoeff, B);

			iM.evAddScaledSign_ip_st(A, scaleCoeff, B);
			ASSERT_MTX_EQ(A2, A, "evAddScaledSign_ip_st failed correctness test");

			A3.cloneTo(A);
			iM.evAddScaledSign_ip_mt(A, scaleCoeff, B);
			ASSERT_MTX_EQ(A2, A, "evAddScaledSign_ip_mt failed correctness test");

			A3.cloneTo(A);
			iM.evAddScaledSign_ip(A, scaleCoeff, B);
			ASSERT_MTX_EQ(A2, A, "evAddScaledSign_ip failed correctness test");
		}
	}

	tictoc tst, tmt, tb;
	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	//FFFFfffffffff... don't ever think about removing rg. calls that randomizes data...
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(A, 2); rg.gen_matrix(B, 2);
		tst.tic();
		iM.evAddScaledSign_ip_st(A, scaleCoeff, B);
		tst.toc();

		rg.gen_matrix(A, 2); rg.gen_matrix(B, 2);
		tmt.tic();
		iM.evAddScaledSign_ip_mt(A, scaleCoeff, B);
		tmt.toc();

		rg.gen_matrix(A, 2); rg.gen_matrix(B, 2);
		tb.tic();
		iM.evAddScaledSign_ip(A, scaleCoeff, B);
		tb.toc();
	}
	tst.say("st");
	tmt.say("mt");
	tb.say("best");
}
TEST(TestMathN, evAddScaledSign_ip) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evAddScaledSign_ip, 100) test_evAddScaledSign_ip(iM, i, 100);
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_evAddScaled_ip(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing evAddScaled_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t B(rowsCnt, colsCnt), A(rowsCnt, colsCnt);
	ASSERT_TRUE(!B.isAllocationFailed() && !A.isAllocationFailed());

	real_t scaleCoeff = .5;

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(B, 2);

	{
		realmtx_t A2(rowsCnt, colsCnt), A3(rowsCnt, colsCnt);
		ASSERT_TRUE(!A2.isAllocationFailed() && !A3.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(A, 2);
			A.cloneTo(A2);
			A.cloneTo(A3);

			evAddScaled_ip_ET(A2, scaleCoeff, B);

			iM.evAddScaled_ip_st(A, scaleCoeff, B);
			ASSERT_MTX_EQ(A2, A, "evAddScaled_ip_st failed correctness test");

			A3.cloneTo(A);
			iM.evAddScaled_ip_mt(A, scaleCoeff, B);
			ASSERT_MTX_EQ(A2, A, "evAddScaled_ip_mt failed correctness test");

			A3.cloneTo(A);
			iM.evAddScaled_ip(A, scaleCoeff, B);
			ASSERT_MTX_EQ(A2, A, "evAddScaled_ip failed correctness test");
		}
	}
	
	tictoc tst, tmt, tb;
	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	//FFFFfffffffff... don't ever think about removing rg. calls that randomizes data...
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(A, 2); rg.gen_matrix(B, 2);
		tst.tic();
		iM.evAddScaled_ip_st(A, scaleCoeff, B);
		tst.toc();

		rg.gen_matrix(A, 2); rg.gen_matrix(B, 2);
		tmt.tic();
		iM.evAddScaled_ip_mt(A, scaleCoeff, B);
		tmt.toc();

		rg.gen_matrix(A, 2); rg.gen_matrix(B, 2);
		tb.tic();
		iM.evAddScaled_ip(A, scaleCoeff, B);
		tb.toc();
	}
	tst.say("st");
	tmt.say("mt");
	tb.say("best");
}
TEST(TestMathN, evAddScaled_ip) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evAddScaled_ip, 100) test_evAddScaled_ip(iM, i, 100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_evAdd_ip(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing evAdd_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t B(rowsCnt, colsCnt), A(rowsCnt, colsCnt);
	ASSERT_TRUE(!B.isAllocationFailed() && !A.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(B, 2);

	{
		realmtx_t A2(rowsCnt, colsCnt), A3(rowsCnt, colsCnt);
		ASSERT_TRUE(!A2.isAllocationFailed() && !A3.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(A, 2);
			A.cloneTo(A2);
			A.cloneTo(A3);

			evAdd_ip_ET(A2, B);

			iM.evAdd_ip_st(A, B);
			ASSERT_MTX_EQ(A2, A, "evAdd_ip_st failed correctness test");

			A3.cloneTo(A);
			iM.evAdd_ip_mt(A, B);
			ASSERT_MTX_EQ(A2, A, "evAdd_ip_mt failed correctness test");

			A3.cloneTo(A);
			iM.evAdd_ip(A, B);
			ASSERT_MTX_EQ(A2, A, "evAdd_ip failed correctness test");
		}
	}
	
	tictoc tst, tmt, tb;
	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	//FFFFfffffffff... don't ever think about removing rg. calls that randomizes data...
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(A, 2); rg.gen_matrix(B, 2);
		tst.tic();
		iM.evAdd_ip_st(A, B);
		tst.toc();

		rg.gen_matrix(A, 2); rg.gen_matrix(B, 2);
		tmt.tic();
		iM.evAdd_ip_mt(A, B);
		tmt.toc();

		rg.gen_matrix(A, 2); rg.gen_matrix(B, 2);
		tb.tic();
		iM.evAdd_ip(A, B);
		tb.toc();
	}
	tst.say("st");
	tmt.say("mt");
	tb.say("best");
}
TEST(TestMathN, evAddIp) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evAdd_ip, 100) test_evAdd_ip(iM, i, 100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_evMulCipSubip(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("********* testing evMulC_ip_Sub_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	const real_t momentum(real_t(.9));
	realmtx_t vW(rowsCnt, colsCnt), W(colsCnt, rowsCnt), vW2(colsCnt, rowsCnt), W2(colsCnt, rowsCnt), vW3(colsCnt, rowsCnt), W3(colsCnt, rowsCnt);
	ASSERT_TRUE(!vW.isAllocationFailed() && !W.isAllocationFailed() && !vW2.isAllocationFailed()
		&& !W2.isAllocationFailed() && !vW3.isAllocationFailed() && !W3.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(vW2, 2);
	rg.gen_matrix(W2, 2);
	vW2.cloneTo(vW);
	W2.cloneTo(W);
	vW2.cloneTo(vW3);
	W2.cloneTo(W3);

	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		evCMulSub_ET(iM, vW3, momentum, W3);
			
		iM.evMulC_ip_Sub_ip_st(vW, momentum, W);
		ASSERT_MTX_EQ(vW3, vW, "evMulC_ip_Sub_ip_st failed correctness test on vW");
		ASSERT_MTX_EQ(W3, W, "evMulC_ip_Sub_ip_st failed correctness test on W");

		iM.evMulC_ip_Sub_ip_mt(vW2, momentum, W2);
		ASSERT_MTX_EQ(vW3, vW2, "evMulC_ip_Sub_ip_mt failed correctness test on vW");
		ASSERT_MTX_EQ(W3, W2, "evMulC_ip_Sub_ip_mt failed correctness test on W");
	}

// 	rg.gen_matrix(vW2, 2);
// 	rg.gen_matrix(W2, 2);
// 	vW2.cloneTo(vW);
// 	W2.cloneTo(W);
// 	vW2.cloneTo(vW3);
// 	W2.cloneTo(W3);

	tictoc tst, tmt, tb;
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(vW, 2);
		rg.gen_matrix(W, 2);
		tst.tic();
		iM.evMulC_ip_Sub_ip_st(vW, momentum, W);
		tst.toc();

		rg.gen_matrix(vW2, 2);
		rg.gen_matrix(W2, 2);
		tmt.tic();
		iM.evMulC_ip_Sub_ip_mt(vW2, momentum, W2);
		tmt.toc();

		rg.gen_matrix(vW3, 2);
		rg.gen_matrix(W3, 2);
		tb.tic();
		iM.evMulC_ip_Sub_ip(vW3, momentum, W3);
		tb.toc();
	}
	tst.say("st");
	tmt.say("mt");
	tb.say("best");
}

TEST(TestMathN, evMulCipSubip) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evMulC_ip_Sub_ip, 100) test_evMulCipSubip(iM, i, 100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename base_t> struct mCheck_normalize_rows_EPS {};
template<> struct mCheck_normalize_rows_EPS<double> { static constexpr double eps = 1e-10; };
template<> struct mCheck_normalize_rows_EPS<float> { static constexpr float eps = 8e-5f; };

template<typename iMath>
void test_mCheck_normalize_rows(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing mCheck_normalize_rows() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	const real_t scale = 5;
	real_t renormTo = 0;
	realmtx_t W(rowsCnt, colsCnt), srcW(rowsCnt, colsCnt);
	ASSERT_TRUE(!W.isAllocationFailed() && !srcW.isAllocationFailed());

	iM.preinit(W.numel());
	ASSERT_TRUE(iM.init());
	
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	{
		realmtx_t etW(rowsCnt, colsCnt);
		ASSERT_TRUE(!etW.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(srcW, scale);

			srcW.cloneTo(etW);
			auto renormVal = rowvecs_renorm_ET(etW, iM._get_thread_temp_raw_storage(etW.numel()));
			renormTo += renormVal;

			srcW.cloneTo(W);
			iM.mCheck_normalize_rows_st(W, renormVal);
			ASSERT_REALMTX_NEAR(etW, W, "st failed correctness test", mCheck_normalize_rows_EPS<real_t>::eps);

			srcW.cloneTo(W);
			iM.mCheck_normalize_rows_mt(W, renormVal);
			ASSERT_REALMTX_NEAR(etW, W, "mt failed correctness test", mCheck_normalize_rows_EPS<real_t>::eps);

			srcW.cloneTo(W);
			iM.mCheck_normalize_rows(W, renormVal);
			ASSERT_REALMTX_NEAR(etW, W, "() failed correctness test", mCheck_normalize_rows_EPS<real_t>::eps);
		}
		renormTo /= testCorrRepCnt;
	}

	tictoc tSt, tMt, tB;
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(srcW, scale);
		
		srcW.cloneTo(W);
		tSt.tic();
		iM.mCheck_normalize_rows_st(W, renormTo);
		tSt.toc();

		srcW.cloneTo(W);
		tMt.tic();
		iM.mCheck_normalize_rows_mt(W, renormTo);
		tMt.toc();

		srcW.cloneTo(W);
		tB.tic();
		iM.mCheck_normalize_rows(W, renormTo);
		tB.toc();
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");
}
TEST(TestMathN, mCheckNormalizeRows) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::mCheck_normalize_rows, 100) test_mCheck_normalize_rows(iM, i, 100);

#ifndef TESTS_SKIP_LONGRUNNING
	for (unsigned i = 1400; i <= 1425; i += 5) {
		test_mCheck_normalize_rows(iM, i, i / 16);
		test_mCheck_normalize_rows(iM, i / 4, i / 4);
		test_mCheck_normalize_rows(iM, i / 16, i);
	}
#endif // !TESTS_SKIP_LONGRUNNING
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_evSub(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing evSub() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t B(rowsCnt, colsCnt), A(rowsCnt, colsCnt), C(rowsCnt, colsCnt);
	ASSERT_TRUE(!B.isAllocationFailed() && !A.isAllocationFailed() && !C.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(A, 2);
	rg.gen_matrix(B, 2);

	{
		realmtx_t C2(rowsCnt, colsCnt);
		ASSERT_TRUE(!C2.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			evSub_ET(A, B, C2);

			iM.evSub_st_naive(A, B, C);
			ASSERT_MTX_EQ(C2, C, "evSub_st_naive failed correctness test");

			iM.evSub_mt_naive(A, B, C);
			ASSERT_MTX_EQ(C2, C, "evSub_mt_naive failed correctness test");

			iM.evSub(A, B, C);
			ASSERT_MTX_EQ(C2, C, "evSub failed correctness test");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSub_st_naive(A, B, C);
	diff = steady_clock::now() - bt;
	STDCOUTL("st_naive:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSub_mt_naive(A, B, C);
	diff = steady_clock::now() - bt;
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSub(A, B, C);
	diff = steady_clock::now() - bt;
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, evSub) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evSub, 10) test_evSub(iM, i, 10);
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_evSub_ip(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing evSub_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest; //, tstVec, tmtVec;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t B(rowsCnt, colsCnt), A(rowsCnt, colsCnt);
	ASSERT_TRUE(!B.isAllocationFailed() && !A.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(B, 2);

	{
		realmtx_t A2(rowsCnt, colsCnt), A3(rowsCnt, colsCnt);
		ASSERT_TRUE(!A2.isAllocationFailed() && !A3.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(A, 2);
			A.cloneTo(A2);
			A.cloneTo(A3);

			evSub_ip_ET(A2, B);

			iM.evSub_ip_st_naive(A, B);
			ASSERT_MTX_EQ(A2, A, "evSub_ip_st_naive failed correctness test");

			A3.cloneTo(A);
			iM.evSub_ip_mt_naive(A, B);
			ASSERT_MTX_EQ(A2, A, "evSub_ip_mt_naive failed correctness test");

			/*iM.evSub_ip_st_vec(A, B);
			ASSERT_MTX_EQ(A2, A, "evSub_ip_st_vec failed correctness test");

			A3.cloneTo(A);
			iM.evSub_ip_mt_vec(A, B);
			ASSERT_MTX_EQ(A2, A, "evSub_ip_mt_vec failed correctness test");*/

			A3.cloneTo(A);
			iM.evSub_ip(A, B);
			ASSERT_MTX_EQ(A2, A, "evSub_ip failed correctness test");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());
	
	rg.gen_matrix(A, 2);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSub_ip_st_naive(A, B);
	diff = steady_clock::now() - bt;
	STDCOUTL("st_naive:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	rg.gen_matrix(A, 2);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSub_ip_mt_naive(A, B);
	diff = steady_clock::now() - bt;
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	/*rg.gen_matrix(A, 2);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSub_ip_st_vec(A, B);
	diff = steady_clock::now() - bt;
	STDCOUTL("st_vec:\t" << utils::duration_readable(diff, maxReps, &tstVec));

	rg.gen_matrix(A, 2);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSub_ip_mt_vec(A, B);
	diff = steady_clock::now() - bt;
	STDCOUTL("mt_vec:\t" << utils::duration_readable(diff, maxReps, &tmtVec));*/

	rg.gen_matrix(A, 2);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSub_ip(A, B);
	diff = steady_clock::now() - bt;
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, evSubIp) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evSub_ip, 100) test_evSub_ip(iM, i,100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_apply_momentum(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing apply_momentum() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	const real_t momentum(real_t(0.9));
	realmtx_t dW(rowsCnt, colsCnt), vW(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW.isAllocationFailed() && !vW.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(dW, 2);

	{
		realmtx_t vW2(rowsCnt, colsCnt), vW3(rowsCnt, colsCnt);
		ASSERT_TRUE(!vW2.isAllocationFailed() && !vW3.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(vW, 2);
			vW.cloneTo(vW2);
			vW.cloneTo(vW3);

			apply_momentum_ET(vW2, momentum, dW);

			iM.apply_momentum_st(vW, momentum, dW);
			ASSERT_MTX_EQ(vW2, vW, "apply_momentum_st failed correctness test");

			vW3.cloneTo(vW);
			iM.apply_momentum_mt(vW,momentum, dW);
			ASSERT_MTX_EQ(vW2, vW, "apply_momentum_mt failed correctness test");

			vW3.cloneTo(vW);
			iM.apply_momentum(vW, momentum, dW);
			ASSERT_MTX_EQ(vW2, vW, "apply_momentum failed correctness test");
		}
	}
	rg.gen_matrix(vW, 2);

	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.apply_momentum_st(vW, momentum, dW);
	diff = steady_clock::now() - bt;
	STDCOUTL("st:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.apply_momentum_mt(vW, momentum, dW);
	diff = steady_clock::now() - bt;
	STDCOUTL("mt:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.apply_momentum(vW, momentum, dW);
	diff = steady_clock::now() - bt;
	STDCOUTL("best:\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, applyMomentum) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::apply_momentum, 100) test_apply_momentum(iM, i,100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
template<typename iMath>
void test_applyILR_perf(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing apply_ILR() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tstVec, tmtNaive, tmtVec, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	real_t decr = real_t(.9), incr = real_t(1/0.9), capH = real_t(9.9), capL = real_t(0.1);

	realmtx_t dW(rowsCnt, colsCnt), prevdW(rowsCnt, colsCnt), gain(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW.isAllocationFailed() && !prevdW.isAllocationFailed() && !gain.isAllocationFailed() );

	iM.preinit(dataSize);
	ASSERT_TRUE(iM.init());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(prevdW, 10);

	//////////////////////////////////////////////////////////////////////////
	//testing correctness
	{
		realmtx_t dW2(rowsCnt, colsCnt), dW3(rowsCnt, colsCnt), gain2(rowsCnt, colsCnt), gain3(rowsCnt, colsCnt);
		ASSERT_TRUE(!dW2.isAllocationFailed() && !dW3.isAllocationFailed() && !gain2.isAllocationFailed() && !gain3.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(dW, 10);
			dW.cloneTo(dW2);
			dW.cloneTo(dW3);
			rg.gen_matrix_gtz(gain, 10);
			gain.cloneTo(gain2);
			gain.cloneTo(gain3);

			apply_ILR_ET(dW, prevdW, gain, decr, incr, capL, capH);

			iM.apply_ILR_st_naive(dW2, prevdW, gain2, decr, incr, capL, capH);
			ASSERT_MTX_EQ(dW2, dW, "apply_ILR_st_naive: wrong dLdW matrix content!");
			ASSERT_MTX_EQ(gain2, gain, "apply_ILR_st_naive: wrong ILRGain matrix content!");

			dW3.cloneTo(dW2);
			gain3.cloneTo(gain2);
			iM.apply_ILR_st_vec(dW2, prevdW, gain2, decr, incr, capL, capH);
			ASSERT_MTX_EQ(dW2, dW, "apply_ILR_st_vec: wrong dLdW matrix content!");
			ASSERT_MTX_EQ(gain2, gain, "apply_ILR_st_vec: wrong ILRGain matrix content!");

			dW3.cloneTo(dW2);
			gain3.cloneTo(gain2);
			iM.apply_ILR_mt_naive(dW2, prevdW, gain2, decr, incr, capL, capH);
			ASSERT_MTX_EQ(dW2, dW, "apply_ILR_mt_naive: wrong dLdW matrix content!");
			ASSERT_MTX_EQ(gain2, gain, "apply_ILR_mt_naive: wrong ILRGain matrix content!");

			dW3.cloneTo(dW2);
			gain3.cloneTo(gain2);
			iM.apply_ILR_mt_vec(dW2, prevdW, gain2, decr, incr, capL, capH);
			ASSERT_MTX_EQ(dW2, dW, "apply_ILR_mt_vec: wrong dLdW matrix content!");
			ASSERT_MTX_EQ(gain2, gain, "apply_ILR_mt_vec: wrong ILRGain matrix content!");

			dW3.cloneTo(dW2);
			gain3.cloneTo(gain2);
			iM.apply_ILR(dW2, prevdW, gain2, decr, incr, capL, capH);
			ASSERT_MTX_EQ(dW2, dW, "apply_ILR: wrong dLdW matrix content!");
			ASSERT_MTX_EQ(gain2, gain, "apply_ILR: wrong ILRGain matrix content!");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());
	
	if (dataSize < 180000*4/sizeof(real_t) ) {
		diff = nanoseconds(0);
		for (unsigned r = 0; r < maxReps; ++r) {
			rg.gen_matrix(dW, 10);
			rg.gen_matrix_gtz(gain, 10);
			bt = steady_clock::now();
			iM.apply_ILR_st_naive(dW, prevdW, gain, decr, incr, capL, capH);
			diff += steady_clock::now() - bt;
		}
		STDCOUTL("st_naive:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

		diff = nanoseconds(0);
		for (unsigned r = 0; r < maxReps; ++r) {
			rg.gen_matrix(dW, 10);
			rg.gen_matrix_gtz(gain, 10);
			bt = steady_clock::now();
			iM.apply_ILR_st_vec(dW, prevdW, gain, decr, incr, capL, capH);
			diff += steady_clock::now() - bt;
		}
		STDCOUTL("st_vec:\t\t" << utils::duration_readable(diff, maxReps, &tstVec));
	}

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		rg.gen_matrix_gtz(gain, 10);
		bt = steady_clock::now();
		iM.apply_ILR_mt_naive(dW, prevdW, gain, decr, incr, capL, capH);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		rg.gen_matrix_gtz(gain, 10);
		bt = steady_clock::now();
		iM.apply_ILR_mt_vec(dW, prevdW, gain, decr, incr, capL, capH);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("mt_vec:\t\t" << utils::duration_readable(diff, maxReps, &tmtVec));

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		rg.gen_matrix_gtz(gain, 10);
		bt = steady_clock::now();
		iM.apply_ILR(dW, prevdW, gain, decr, incr, capL, capH);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));

	iM.deinit();
}

TEST(TestMathN, ApplyILRPerf) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::apply_ILR_st, 10) test_applyILR_perf(iM, i, 10);
	//NNTL_RUN_TEST4(iMB::Thresholds_t::apply_ILR_st, 60, 2, 10) test_applyILR_perf(iM, i, 10);
	
	NNTL_RUN_TEST2(iMB::Thresholds_t::apply_ILR_mt_lo, 100) test_applyILR_perf(iM, i, 100);	
	//NNTL_RUN_TEST4(iMB::Thresholds_t::apply_ILR_mt_lo, 4, 1, 100) test_applyILR_perf(iM, i, 100);
	
	NNTL_RUN_TEST2(iMB::Thresholds_t::apply_ILR_mt_hi, 100) test_applyILR_perf(iM, i, 100);
	//NNTL_RUN_TEST4(iMB::Thresholds_t::apply_ILR_mt_hi, 4, 1, 100) test_applyILR_perf(iM, i, 100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_evAbs_perf(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing evAbs() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t src(rowsCnt, colsCnt), dest(rowsCnt, colsCnt);
	ASSERT_TRUE(!src.isAllocationFailed() && !dest.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());	

	{
		realmtx_t dest2(rowsCnt, colsCnt);
		ASSERT_TRUE(!dest2.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(src, 10);
			evAbs_ET(dest2, src);

			iM.evAbs_st(dest, src);
			ASSERT_MTX_EQ(dest2, dest, "evAbs_st failed correctness test");

			iM.evAbs_mt(dest, src);
			ASSERT_MTX_EQ(dest2, dest, "evAbs_mt failed correctness test");

			iM.evAbs(dest, src);
			ASSERT_MTX_EQ(dest2, dest, "evAbs failed correctness test");
		}
	}
	rg.gen_matrix(src, 10);

	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r)  iM.evAbs_st(dest, src);
	diff = steady_clock::now() - bt;
	STDCOUTL("st:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r)  iM.evAbs_mt(dest, src);
	diff = steady_clock::now() - bt;
	STDCOUTL("mt:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evAbs(dest, src);
	diff = steady_clock::now() - bt;
	STDCOUTL("best:\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, evAbsPerf) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evAbs, 100) test_evAbs_perf(iM, i,100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_evSquare_perf(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing evSquare() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	realmtx_t src(rowsCnt, colsCnt), dest(rowsCnt, colsCnt);
	ASSERT_TRUE(!src.isAllocationFailed() && !dest.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	
	{
		realmtx_t dest2(rowsCnt, colsCnt);
		ASSERT_TRUE(!dest2.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(src, 10);
			evSquare_ET(dest2, src);

			iM.evSquare_st(dest, src);
			ASSERT_MTX_EQ(dest2, dest, "evSquare_st failed correctness test");

			iM.evSquare_mt(dest, src);
			ASSERT_MTX_EQ(dest2, dest, "evSquare_mt failed correctness test");

			iM.evSquare(dest, src);
			ASSERT_MTX_EQ(dest2, dest, "evSquare failed correctness test");
		}
	}
	rg.gen_matrix(src, 10);

	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());
	
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSquare_st(dest,src);
	diff = steady_clock::now() - bt;
	STDCOUTL("st:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSquare_mt(dest, src);
	diff = steady_clock::now() - bt;
	STDCOUTL("mt:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.evSquare(dest, src);
	diff = steady_clock::now() - bt;
	STDCOUTL("best:\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, evSquarePerf) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evSquare, 100) test_evSquare_perf(iM, i,100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_modprop_perf(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing ModProp() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	real_t emaCoeff = real_t(.9), lr = real_t(.1), numStab = real_t(.00001);

	realmtx_t dW(rowsCnt, colsCnt), rms(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW.isAllocationFailed() && !rms.isAllocationFailed());

	rms.zeros();

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	//////////////////////////////////////////////////////////////////////////
	//testing correctness
	{
		realmtx_t dW2(rowsCnt, colsCnt), rms2(rowsCnt, colsCnt), dW3(rowsCnt, colsCnt), rms3(rowsCnt, colsCnt);
		ASSERT_TRUE(!dW2.isAllocationFailed() && !rms2.isAllocationFailed() && !dW3.isAllocationFailed() && !rms3.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(dW, 10);
			dW.cloneTo(dW2);
			dW.cloneTo(dW3);
			rg.gen_matrix_gtz(rms, 10);
			rms.cloneTo(rms2);
			rms.cloneTo(rms3);

			ModProp_ET(dW2, rms2, lr, emaCoeff, numStab);

			iM.ModProp_st(dW, rms, lr, emaCoeff, numStab);
			ASSERT_MTX_EQ(dW2, dW, "ModProp_st: wrong dW");
			ASSERT_MTX_EQ(rms2, rms, "ModProp_st: wrong rms");

			dW3.cloneTo(dW);
			rms3.cloneTo(rms);
			iM.ModProp_mt(dW, rms, lr, emaCoeff, numStab);
			ASSERT_MTX_EQ(dW2, dW, "ModProp_mt: wrong dW");
			ASSERT_MTX_EQ(rms2, rms, "ModProp_mt: wrong rms");

			dW3.cloneTo(dW);
			rms3.cloneTo(rms);
			iM.ModProp(dW, rms, lr, emaCoeff, numStab);
			ASSERT_MTX_EQ(dW2, dW, "ModProp: wrong dW");
			ASSERT_MTX_EQ(rms2, rms, "ModProp: wrong rms");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.ModProp_st(dW, rms, lr, emaCoeff, numStab);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("st:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.ModProp_mt(dW, rms, lr, emaCoeff, numStab);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("mt:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.ModProp(dW, rms, lr, emaCoeff, numStab);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("best:\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, ModPropPerf) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::ModProp, 1) test_modprop_perf(iM, i,1);
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_rprop_perf(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing RProp() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	real_t lr = real_t(.1);

	realmtx_t dW(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW.isAllocationFailed());
	
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	{
		realmtx_t dW2(rowsCnt, colsCnt), dW3(rowsCnt, colsCnt);
		ASSERT_TRUE(!dW2.isAllocationFailed() && !dW3.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(dW, 10);
			dW.cloneTo(dW2);
			dW.cloneTo(dW3);

			RProp_ET(dW2, lr);

			iM.RProp_st(dW, lr);
			ASSERT_MTX_EQ(dW2, dW, "RProp_st: wrong dW");

			dW3.cloneTo(dW);
			iM.RProp_mt(dW, lr);
			ASSERT_MTX_EQ(dW2, dW, "RProp_mt: wrong dW");

			dW3.cloneTo(dW);
			iM.RProp(dW, lr);
			ASSERT_MTX_EQ(dW2, dW, "RProp: wrong dW");
		}
	}
	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.RProp_st(dW, lr);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("st:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.RProp_mt(dW, lr);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("mt:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.RProp(dW, lr);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("best:\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, RPropPerf) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::RProp, 1) test_rprop_perf(iM, i,1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_rmspropgraves_perf(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing RMSProp_Graves() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	real_t emaCoeff = real_t(.9), lr = real_t(.1), numStab = real_t(.00001);

	realmtx_t dW(rowsCnt, colsCnt), rms(rowsCnt, colsCnt), rmsG(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW.isAllocationFailed() && !rms.isAllocationFailed() && !rmsG.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	{
		realmtx_t dW2(rowsCnt, colsCnt), rms2(rowsCnt, colsCnt), rmsG2(rowsCnt, colsCnt),
			dW3(rowsCnt, colsCnt), rms3(rowsCnt, colsCnt), rmsG3(rowsCnt, colsCnt);
		ASSERT_TRUE(!dW2.isAllocationFailed() && !rms2.isAllocationFailed() && !dW3.isAllocationFailed() && !rms3.isAllocationFailed()
			&& !rmsG2.isAllocationFailed() && !rmsG3.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(dW, 10);
			dW.cloneTo(dW2);
			dW.cloneTo(dW3);
			evSquare_ET(rms, dW);
			rms.cloneTo(rms2);
			rms.cloneTo(rms3);
			dW.cloneTo(rmsG);
			rmsG.cloneTo(rmsG2);
			rmsG.cloneTo(rmsG3);

			RMSProp_Graves_ET(dW2, rms2, rmsG2, lr, emaCoeff, numStab);

			iM.RMSProp_Graves_st(dW, rms, rmsG, lr, emaCoeff, numStab);
			ASSERT_MTX_EQ(dW2, dW, "RMSProp_Graves_st: wrong dW");
			ASSERT_MTX_EQ(rms2, rms, "RMSProp_Graves_st: wrong rms");
			ASSERT_MTX_EQ(rmsG2, rmsG, "RMSProp_Graves_st: wrong rmsG");

			dW3.cloneTo(dW);
			rms3.cloneTo(rms);
			rmsG3.cloneTo(rmsG);
			iM.RMSProp_Graves_mt(dW, rms, rmsG, lr, emaCoeff, numStab);
			ASSERT_MTX_EQ(dW2, dW, "RMSProp_Graves_mt: wrong dW");
			ASSERT_MTX_EQ(rms2, rms, "RMSProp_Graves_mt: wrong rms");
			ASSERT_MTX_EQ(rmsG2, rmsG, "RMSProp_Graves_mt: wrong rmsG");

			dW3.cloneTo(dW);
			rms3.cloneTo(rms);
			rmsG3.cloneTo(rmsG);
			iM.RMSProp_Graves(dW, rms, rmsG, lr, emaCoeff, numStab);
			ASSERT_MTX_EQ(dW2, dW, "RMSProp_Graves: wrong dW");
			ASSERT_MTX_EQ(rms2, rms, "RMSProp_Graves: wrong rms");
			ASSERT_MTX_EQ(rmsG2, rmsG, "RMSProp_Graves: wrong rmsG");
		}
	}
	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.RMSProp_Graves_st(dW, rms, rmsG, lr, emaCoeff, numStab);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("st:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.RMSProp_Graves_mt(dW, rms, rmsG, lr, emaCoeff, numStab);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("mt:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.RMSProp_Graves(dW, rms, rmsG, lr, emaCoeff, numStab);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("best:\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, RMSProp_Graves) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::RMSProp_Graves, 10) test_rmspropgraves_perf(iM, i,10);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_rmsprophinton_perf(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing RMSProp_Hinton() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	real_t emaCoeff = real_t(.9), lr = real_t(.1), numStab = real_t(.00001);

	realmtx_t dW(rowsCnt, colsCnt), rms(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW.isAllocationFailed() && !rms.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	{
		realmtx_t dW2(rowsCnt, colsCnt), rms2(rowsCnt, colsCnt), dW3(rowsCnt, colsCnt), rms3(rowsCnt, colsCnt);
		ASSERT_TRUE(!dW2.isAllocationFailed() && !rms2.isAllocationFailed() && !dW3.isAllocationFailed() && !rms3.isAllocationFailed());

		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix(dW, 10);
			dW.cloneTo(dW2);
			dW.cloneTo(dW3);
			evSquare_ET(rms, dW);
			rms.cloneTo(rms2);
			rms.cloneTo(rms3);

			RMSProp_Hinton_ET(dW2, rms2, lr, emaCoeff, numStab);

			iM.RMSProp_Hinton_st(dW, rms, lr, emaCoeff, numStab);
			ASSERT_MTX_EQ(dW2, dW, "RMSProp_Hinton_st: wrong dW");
			ASSERT_MTX_EQ(rms2, rms, "RMSProp_Hinton_st: wrong rms");

			dW3.cloneTo(dW);
			rms3.cloneTo(rms);
			iM.RMSProp_Hinton_mt(dW, rms, lr, emaCoeff, numStab);
			ASSERT_MTX_EQ(dW2, dW, "RMSProp_Hinton_mt: wrong dW");
			ASSERT_MTX_EQ(rms2, rms, "RMSProp_Hinton_mt: wrong rms");

			dW3.cloneTo(dW);
			rms3.cloneTo(rms);
			iM.RMSProp_Hinton(dW, rms, lr, emaCoeff, numStab);
			ASSERT_MTX_EQ(dW2, dW, "RMSProp_Hinton: wrong dW");
			ASSERT_MTX_EQ(rms2, rms, "RMSProp_Hinton: wrong rms");
		}
	}
	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.RMSProp_Hinton_st(dW, rms, lr, emaCoeff, numStab);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("st:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.RMSProp_Hinton_mt(dW, rms, lr, emaCoeff, numStab);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("mt:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(dW, 10);
		bt = steady_clock::now();
		iM.RMSProp_Hinton(dW, rms, lr, emaCoeff, numStab);
		diff += steady_clock::now() - bt;
	}
	STDCOUTL("best:\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, RMSProp_Hinton) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::RMSProp_Hinton, 10) test_rmsprophinton_perf(iM, i,10);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void test_Adam_corr(const size_t epochs, const vec_len_t maxRowsCnt, const vec_len_t maxColsCnt = 10) {
	const real_t beta1 = real_t(.9), beta2=real_t(.999), learningRate=real_t(.001), numStab=real_t(1e-8);
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	for (vec_len_t r = 1; r < maxRowsCnt; ++r)	{
		for (vec_len_t c = 1; c < maxColsCnt; ++c) {
			MTXSIZE_SCOPED_TRACE(r, c, "test_Adam_corr");

			realmtx_t dW_ET(r, c), Mt_ET(r, c), Vt_ET(r, c);
			ASSERT_TRUE(!dW_ET.isAllocationFailed() && !Mt_ET.isAllocationFailed() && !Vt_ET.isAllocationFailed());
			realmtx_t dW_st(r, c), Mt_st(r, c), Vt_st(r, c);
			ASSERT_TRUE(!dW_st.isAllocationFailed() && !Mt_st.isAllocationFailed() && !Vt_st.isAllocationFailed());
			realmtx_t dW_mt(r, c), Mt_mt(r, c), Vt_mt(r, c);
			ASSERT_TRUE(!dW_mt.isAllocationFailed() && !Mt_mt.isAllocationFailed() && !Vt_mt.isAllocationFailed());
			realmtx_t dW_(r, c), Mt_(r, c), Vt_(r, c);
			ASSERT_TRUE(!dW_.isAllocationFailed() && !Mt_.isAllocationFailed() && !Vt_.isAllocationFailed());

			Mt_ET.zeros(); Mt_st.zeros(); Mt_mt.zeros(); Mt_.zeros();
			Vt_ET.zeros(); Vt_st.zeros(); Vt_mt.zeros(); Vt_.zeros();

			real_t beta1t_ET = real_t(1.), beta2t_ET = real_t(1.);
			real_t beta1t_st = real_t(1.), beta2t_st = real_t(1.);
			real_t beta1t_mt = real_t(1.), beta2t_mt = real_t(1.);
			real_t beta1t_ = real_t(1.), beta2t_ = real_t(1.);
			
			for (size_t e = 0; e < epochs; ++e) {
				rg.gen_matrix(dW_ET, real_t(3.0));
				ASSERT_TRUE(dW_ET.cloneTo(dW_st)); ASSERT_TRUE(dW_ET.cloneTo(dW_mt)); ASSERT_TRUE(dW_ET.cloneTo(dW_));
				
				Adam_ET(dW_ET, Mt_ET, Vt_ET, beta1t_ET, beta2t_ET, learningRate, beta1, beta2, numStab);

				iM.Adam_st(dW_st, Mt_st, Vt_st, beta1t_st, beta2t_st, learningRate, beta1, beta2, numStab);
				ASSERT_MTX_EQ(dW_ET, dW_st, "dW @ _st");
				ASSERT_MTX_EQ(Mt_ET, Mt_st, "Mt @ _st");
				ASSERT_MTX_EQ(Vt_ET, Vt_st, "Vt @ _st");
				ASSERT_EQ(beta1t_ET, beta1t_st) << "_st";
				ASSERT_EQ(beta2t_ET, beta2t_st) << "_st";

				iM.Adam_mt(dW_mt, Mt_mt, Vt_mt, beta1t_mt, beta2t_mt, learningRate, beta1, beta2, numStab);
				ASSERT_MTX_EQ(dW_ET, dW_mt, "dW @ _mt");
				ASSERT_MTX_EQ(Mt_ET, Mt_mt, "Mt @ _mt");
				ASSERT_MTX_EQ(Vt_ET, Vt_mt, "Vt @ _mt");
				ASSERT_EQ(beta1t_ET, beta1t_mt) << "_mt";
				ASSERT_EQ(beta2t_ET, beta2t_mt) << "_mt";

				iM.Adam(dW_, Mt_, Vt_, beta1t_, beta2t_, learningRate, beta1, beta2, numStab);
				ASSERT_MTX_EQ(dW_ET, dW_, "dW @ _");
				ASSERT_MTX_EQ(Mt_ET, Mt_, "Mt @ _");
				ASSERT_MTX_EQ(Vt_ET, Vt_, "Vt @ _");
				ASSERT_EQ(beta1t_ET, beta1t_) << "_";
				ASSERT_EQ(beta2t_ET, beta2t_) << "_";
			}
		}
	}
}

TEST(TestMathN, Adam) {
	test_Adam_corr(10, g_MinDataSizeDelta * 2, g_MinDataSizeDelta * 2);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void test_AdaMax_corr(const size_t epochs, const vec_len_t maxRowsCnt, const vec_len_t maxColsCnt = 10) {
	const real_t beta1 = real_t(.9), beta2 = real_t(.999), learningRate = real_t(.001), numStab = real_t(1e-8);
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	for (vec_len_t r = 1; r < maxRowsCnt; ++r) {
		for (vec_len_t c = 1; c < maxColsCnt; ++c) {
			MTXSIZE_SCOPED_TRACE(r, c, "test_AdaMax_corr");

			realmtx_t dW_ET(r, c), Mt_ET(r, c), Vt_ET(r, c);
			ASSERT_TRUE(!dW_ET.isAllocationFailed() && !Mt_ET.isAllocationFailed() && !Vt_ET.isAllocationFailed());
			realmtx_t dW_st(r, c), Mt_st(r, c), Vt_st(r, c);
			ASSERT_TRUE(!dW_st.isAllocationFailed() && !Mt_st.isAllocationFailed() && !Vt_st.isAllocationFailed());
			realmtx_t dW_mt(r, c), Mt_mt(r, c), Vt_mt(r, c);
			ASSERT_TRUE(!dW_mt.isAllocationFailed() && !Mt_mt.isAllocationFailed() && !Vt_mt.isAllocationFailed());
			realmtx_t dW_(r, c), Mt_(r, c), Vt_(r, c);
			ASSERT_TRUE(!dW_.isAllocationFailed() && !Mt_.isAllocationFailed() && !Vt_.isAllocationFailed());

			Mt_ET.zeros(); Mt_st.zeros(); Mt_mt.zeros(); Mt_.zeros();
			Vt_ET.zeros(); Vt_st.zeros(); Vt_mt.zeros(); Vt_.zeros();

			real_t beta1t_ET = real_t(1.), beta1t_st = real_t(1.), beta1t_mt = real_t(1.), beta1t_ = real_t(1.);

			for (size_t e = 0; e < epochs; ++e) {
				rg.gen_matrix(dW_ET, real_t(3.0));
				ASSERT_TRUE(dW_ET.cloneTo(dW_st)); ASSERT_TRUE(dW_ET.cloneTo(dW_mt)); ASSERT_TRUE(dW_ET.cloneTo(dW_));

				AdaMax_ET(dW_ET, Mt_ET, Vt_ET, beta1t_ET, learningRate, beta1, beta2, numStab);

				iM.AdaMax_st(dW_st, Mt_st, Vt_st, beta1t_st, learningRate, beta1, beta2, numStab);
				ASSERT_MTX_EQ(dW_ET, dW_st, "dW @ _st");
				ASSERT_MTX_EQ(Mt_ET, Mt_st, "Mt @ _st");
				ASSERT_MTX_EQ(Vt_ET, Vt_st, "Vt @ _st");
				ASSERT_EQ(beta1t_ET, beta1t_st) << "_st";

				iM.AdaMax_mt(dW_mt, Mt_mt, Vt_mt, beta1t_mt, learningRate, beta1, beta2, numStab);
				ASSERT_MTX_EQ(dW_ET, dW_mt, "dW @ _mt");
				ASSERT_MTX_EQ(Mt_ET, Mt_mt, "Mt @ _mt");
				ASSERT_MTX_EQ(Vt_ET, Vt_mt, "Vt @ _mt");
				ASSERT_EQ(beta1t_ET, beta1t_mt) << "_mt";

				iM.AdaMax(dW_, Mt_, Vt_, beta1t_, learningRate, beta1, beta2, numStab);
				ASSERT_MTX_EQ(dW_ET, dW_, "dW @ _");
				ASSERT_MTX_EQ(Mt_ET, Mt_, "Mt @ _");
				ASSERT_MTX_EQ(Vt_ET, Vt_, "Vt @ _");
				ASSERT_EQ(beta1t_ET, beta1t_) << "_";
			}
		}
	}
}

TEST(TestMathN, AdaMax) {
	test_AdaMax_corr(10, g_MinDataSizeDelta * 2, g_MinDataSizeDelta * 2);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
template<typename iMath>
void test_make_dropout_perf(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing make_dropout() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT, testCorrRepCnt = TEST_CORRECTN_REPEATS_COUNT;

	real_t dfrac = .5;

	realmtx_t act(rowsCnt, colsCnt, true), dm(rowsCnt, colsCnt);
	ASSERT_TRUE(!act.isAllocationFailed() && !dm.isAllocationFailed());
	
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	{
		realmtx_t act2(rowsCnt, colsCnt, true), dm2(rowsCnt, colsCnt), act3(rowsCnt, colsCnt, true), dm3(rowsCnt, colsCnt);
		ASSERT_TRUE(!act2.isAllocationFailed() && !dm2.isAllocationFailed()&& !act3.isAllocationFailed() && !dm3.isAllocationFailed());
		for (unsigned r = 0; r < testCorrRepCnt; ++r) {
			rg.gen_matrix_no_bias(act, 5);
			ASSERT_TRUE(act.test_biases_ok());
			act.cloneTo(act2);
			act.cloneTo(act3);
			rg.gen_matrix_norm(dm);
			dm.cloneTo(dm2);
			dm.cloneTo(dm3);

			make_dropout_ET(act2, dfrac, dm2);
			ASSERT_TRUE(act2.test_biases_ok());

			iM.make_dropout_st(act, dfrac, dm);
			ASSERT_MTX_EQ(act2, act, "make_dropout_st: wrong act");
			ASSERT_MTX_EQ(dm2, dm, "make_dropout_st: wrong dm");

			act3.cloneTo(act);
			dm3.cloneTo(dm);
			iM.make_dropout_mt(act, dfrac, dm);
			ASSERT_MTX_EQ(act2, act, "make_dropout_mt: wrong act");
			ASSERT_MTX_EQ(dm2, dm, "make_dropout_mt: wrong dm");

			act3.cloneTo(act);
			dm3.cloneTo(dm);
			iM.make_dropout(act, dfrac, dm);
			ASSERT_MTX_EQ(act2, act, "make_dropout: wrong act");
			ASSERT_MTX_EQ(dm2, dm, "make_dropout: wrong dm");
		}
	}
	//////////////////////////////////////////////////////////////////////////
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	rg.gen_matrix_no_bias(act, 5);
	ASSERT_TRUE(act.test_biases_ok());
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix_norm(dm);
		bt = steady_clock::now();
		iM.make_dropout_st(act, dfrac, dm);
		diff += steady_clock::now() - bt;
		ASSERT_TRUE(act.test_biases_ok());
	}
	STDCOUTL("st:\t\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	rg.gen_matrix_no_bias(act, 5);
	ASSERT_TRUE(act.test_biases_ok());
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix_norm(dm);
		bt = steady_clock::now();
		iM.make_dropout_mt(act, dfrac, dm);
		diff += steady_clock::now() - bt;
		ASSERT_TRUE(act.test_biases_ok());
	}
	STDCOUTL("mt:\t\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	rg.gen_matrix_no_bias(act, 5);
	ASSERT_TRUE(act.test_biases_ok());
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix_norm(dm);
		bt = steady_clock::now();
		iM.make_dropout(act, dfrac, dm);
		diff += steady_clock::now() - bt;
		ASSERT_TRUE(act.test_biases_ok());
	}
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, MakeDropoutPerf) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::make_dropout, 1) test_make_dropout_perf(iM, 1,i);
}

//////////////////////////////////////////////////////////////////////////

TEST(TestMathN, vCountSameNaive) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;

	constexpr unsigned dataCnt = 9;
	const std::array<unsigned, dataCnt> src1 = { 3,55,32, 35,63,5, 2,400,6 };
	const std::array<unsigned, dataCnt> src2 = { 3,55,33, 35,63,5, 4,400,6 };

	iMB iM;
	ASSERT_EQ(iM.vCountSame_st_naive(src1, src2), dataCnt-2);
}

TEST(TestMathN, vCountSameMtCorrectness) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	typedef std::vector<realmtx_t::vec_len_t> vec_t;

#ifdef NNTL_DEBUG
	constexpr unsigned rowsCnt = 100;
#else
	constexpr unsigned rowsCnt = 100000;
#endif

	vec_t v1(rowsCnt), v2(rowsCnt);

	iMB iM;
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	rg.gen_vector_gtz(&v1[0], rowsCnt, (vec_t::value_type)5);
	rg.gen_vector_gtz(&v2[0], rowsCnt, (vec_t::value_type)5);

	ASSERT_EQ(iM.vCountSame_st_naive(v1, v2), iM.vCountSame_mt_naive(v1, v2));
}

template<typename iMath>
void test_vCountSame_perf(iMath& iM, vec_len_t rowsCnt) {
	typedef std::vector<realmtx_t::vec_len_t> vec_t;
	STDCOUTL("******* testing vCountSame() over " << rowsCnt << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	size_t vv;

	vec_t v1(rowsCnt), v2(rowsCnt);
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	rg.gen_vector_gtz(&v1[0], rowsCnt, (vec_t::value_type)5);
	rg.gen_vector_gtz(&v2[0], rowsCnt, (vec_t::value_type)5);

	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	iM.vCountSame_st_naive(v1, v2);
	vv = 0;
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) {
		vv += iM.vCountSame_st_naive(v1, v2);
	}
	diff = steady_clock::now() - bt;
	STDCOUTL("st_naive:\t" << utils::duration_readable(diff, maxReps, &tstNaive) << "\t\tvv=" << vv);

	iM.vCountSame_mt_naive(v1, v2);;
	vv = 0;
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) {
		vv += iM.vCountSame_mt_naive(v1, v2);
	}
	diff = steady_clock::now() - bt;
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps, &tmtNaive) << "\t\tvv=" << vv);

	iM.vCountSame(v1, v2);
	vv = 0;
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) {
		vv += iM.vCountSame(v1, v2);
	}
	diff = steady_clock::now() - bt;
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest) << "\t\tvv=" << vv);
}

TEST(TestMathN, vCountSamePerf) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST4(100000, 75, 25, 1) test_vCountSame_perf(iM, i);
}


//////////////////////////////////////////////////////////////////////////
template<typename iMath>
void test_evClamp_perf(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	typedef std::vector<realmtx_t::vec_len_t> vec_t;

	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing evClamp() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	real_t lo = -50, hi = 50;

	realmtx_t m(rowsCnt, colsCnt);
	ASSERT_TRUE(!m.isAllocationFailed());
	vec_t vec(rowsCnt);

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(m, 100);

	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	iM.evClamp_st(m, lo,hi);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) {
		iM.evClamp_st(m, lo, hi);
	}
	diff = steady_clock::now() - bt;
	STDCOUTL("st:\t\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	iM.evClamp_mt(m, lo, hi);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) {
		iM.evClamp_mt(m, lo, hi);
	}
	diff = steady_clock::now() - bt;
	STDCOUTL("mt:\t\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	iM.evClamp(m, lo, hi);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) {
		iM.evClamp(m, lo, hi);
	}
	diff = steady_clock::now() - bt;
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, evClampPerf) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evClamp, 10) test_evClamp_perf(iM, i,10);
}

//////////////////////////////////////////////////////////////////////////
TEST(TestMathN, mExtractRowsCorrectness) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	
	constexpr vec_len_t rowsCnt = 2000, colsCnt = 50, extrCnt = 1000;

	realmtx_t src(rowsCnt, colsCnt), destSt(extrCnt, colsCnt), destMt(extrCnt, colsCnt);;
	ASSERT_TRUE(!src.isAllocationFailed() && !destSt.isAllocationFailed() && !destMt.isAllocationFailed());
	auto pSrc = src.data();
	for (numel_cnt_t i = 0, im = src.numel(); i < im; ++i) pSrc[i] = static_cast<real_t>(i);

	std::vector<vec_len_t> vec(extrCnt);
	iMB iM;
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());

	rg.gen_vector_gtz(&vec[0], vec.size(), rowsCnt - 1);

	iM.mExtractRows_st_naive(src, vec.begin(), extrCnt, destSt);
	iM.mExtractRows_mt_naive(src, vec.begin(), extrCnt, destMt);

	ASSERT_EQ(destSt, destMt);
	for (vec_len_t r = 0; r < extrCnt; ++r) {
		for (vec_len_t c = 0; c < colsCnt; ++c) {
			ASSERT_DOUBLE_EQ(destSt.get(r, c), src.get(vec[r], c));
			//ASSERT_DOUBLE_EQ(destMt.get(r, c), src.get(vec[r], c));
		}
	}
}

template<typename iMath>
void test_mExtractRows_perf(iMath& iM, vec_len_t rowsCnt, vec_len_t extrCnt, vec_len_t colsCnt = 10) {
	typedef std::vector<realmtx_t::vec_len_t> vec_t;

	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing mExtractRows() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elems) ExtractRows="<< extrCnt 
		<<" -> "<< realmtx_t::sNumel(extrCnt,colsCnt) << " elems *********");

	double tstNaive, tmtNaive, tBest;
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	realmtx_t src(rowsCnt, colsCnt), dest(extrCnt, colsCnt);
	ASSERT_TRUE(!src.isAllocationFailed() && !dest.isAllocationFailed());
	vec_t vec(extrCnt);

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(src, 1000);
	rg.gen_vector_gtz(&vec[0], vec.size(), rowsCnt - 1);

	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	iM.mExtractRows_st_naive(src, vec.begin(), extrCnt, dest);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) {
		iM.mExtractRows_st_naive(src, vec.begin(), extrCnt, dest);
	}
	diff = steady_clock::now() - bt;
	STDCOUTL("st_naive:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	iM.mExtractRows_mt_naive(src, vec.begin(), extrCnt, dest);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) {
		iM.mExtractRows_mt_naive(src, vec.begin(), extrCnt, dest);
	}
	diff = steady_clock::now() - bt;
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	iM.mExtractRows(src, vec.begin(), extrCnt, dest);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) {
		iM.mExtractRows(src, vec.begin(), extrCnt, dest);
	}
	diff = steady_clock::now() - bt;
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, mExtractRowsPerf) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
/*
	for (unsigned r = 8; r <= 64; r *= 2) {
		for (unsigned c = 10; c <= 800; c += 790) {
			for (unsigned e = 1; e <= 8; e *= 2) {
				test_mExtractRows_perf(iM, r * 1000, e * 100, c);
			}
		}
	}*/
	//constexpr unsigned batchSize = 100;
	NNTL_RUN_TEST2(iMB::Thresholds_t::mExtractRows, 100) test_mExtractRows_perf(iM, 60000, i, 100);
	NNTL_RUN_TEST2(iMB::Thresholds_t::mExtractRows, 10) test_mExtractRows_perf(iM, 60000, i, 10);
/*
#ifndef TESTS_SKIP_LONGRUNNING
	test_mExtractRows_perf(iM, 60000, batchSize, 800);
	test_mExtractRows_perf(iM, 60000, batchSize, 10);
	test_mExtractRows_perf(iM, 40000, batchSize, 800);
	test_mExtractRows_perf(iM, 40000, batchSize, 10);
#endif*/
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TEST(TestMathN, mMulABt_Cnb) {
	using namespace nntl_supp;

	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	
	using ErrCode = jsonreader::ErrorCode;
	typedef train_data<real_t> train_data_t;
	typedef train_data_t::mtx_t realmtx_t;
	using mtx_size_t = realmtx_t::mtx_size_t;

	realmtx_t A,B,C, etA, etB, etC;
	jsonreader reader;

	ErrCode ec = reader.read(NNTL_STRING("./test_data/mtx4-2.json"), etA);
	ASSERT_EQ(ErrCode::Success, ec) << "Error code description: " << reader.get_last_error_string();
	ASSERT_TRUE(!etA.empty());
	etA.cloneTo(A);

	ec = reader.read(NNTL_STRING("./test_data/mtx3-2.json"), etB);
	ASSERT_EQ(ErrCode::Success, ec) << "Error code description: " << reader.get_last_error_string();
	ASSERT_TRUE(!etB.empty());
	etB.cloneTo(B);

	ec = reader.read(NNTL_STRING("./test_data/mtx4-3.json"), etC);
	ASSERT_EQ(ErrCode::Success, ec) << "Error code description: " << reader.get_last_error_string();
	ASSERT_TRUE(!etC.empty());

	C.resize(etC.size());
	C.zeros();

	iMB iM;

	iM.mMulABt_Cnb(A, B, C);
	EXPECT_EQ(A, etA);
	EXPECT_EQ(B, etB);
	EXPECT_EQ(C, etC);
}

TEST(TestMathN, mMulABt_Cnb_biased) {
	using namespace nntl_supp;

	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;

	using ErrCode = jsonreader::ErrorCode;

	typedef train_data<real_t> train_data_t;
	typedef train_data_t::mtx_t realmtx_t;
	using mtx_size_t = realmtx_t::mtx_size_t;

	realmtx_t A, B, C, etA, etB, etC;
	jsonreader reader;

	ErrCode ec = reader.read(NNTL_STRING("./test_data/mtx4-2.json"), etA);
	ASSERT_EQ(ErrCode::Success, ec) << "Error code description: " << reader.get_last_error_string();
	ASSERT_TRUE(!etA.empty());
	etA.cloneTo(A);

	ec = reader.read(NNTL_STRING("./test_data/mtx3-2.json"), etB);
	ASSERT_EQ(ErrCode::Success, ec) << "Error code description: " << reader.get_last_error_string();
	ASSERT_TRUE(!etB.empty());
	etB.cloneTo(B);

	ec = reader.read(NNTL_STRING("./test_data/mtx4-3.json"), etC);
	ASSERT_EQ(ErrCode::Success, ec) << "Error code description: " << reader.get_last_error_string();
	ASSERT_TRUE(!etC.empty());

	C.will_emulate_biases();
	C.resize(etC.size());
	C.zeros();

	iMB iM;

	iM.mMulABt_Cnb(A, B, C);
	EXPECT_EQ(A, etA);
	EXPECT_EQ(B, etB);

	auto ptrC = C.data(), ptrEt = etC.data();
	auto cnt = etC.numel(), bcnt = C.numel();
	ASSERT_TRUE(cnt < bcnt);
	for (numel_cnt_t i = 0; i < cnt; ++i) {
		ASSERT_DOUBLE_EQ(ptrC[i], ptrEt[i]) << "offset "<<i;
	}
	for (numel_cnt_t i = cnt; i < bcnt; ++i) {
		ASSERT_DOUBLE_EQ(ptrC[i], real_t(1.0)) << "offset " << i;
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename iMath>
void test_evMul_ip(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("********* testing evMul_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	EXPECT_TRUE(steady_clock::is_steady);
	double tstNaive, tmtNaive, tBest; //tmtVect, tstVect,
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	realmtx_t m, etM(rowsCnt, colsCnt), etDest(rowsCnt, colsCnt), etB(rowsCnt, colsCnt), B;
	ASSERT_EQ(dataSize, etM.numel());

	//filling etalon
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(etM, 5);
	rg.gen_matrix(etB, 5);
	ASSERT_TRUE(etB.cloneTo(B));
	auto ptrEtM = etM.data(), ptrDest = etDest.data(), ptretB=etB.data();
	for (unsigned i = 0; i < dataSize; ++i) ptrDest[i] = ptrEtM[i]* ptretB[i];

	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	//////////////////////////////////////////////////////////////////////////
	//single threaded naive
	ASSERT_TRUE(etM.cloneTo(m));
	iM.evMul_ip_st_naive(m, B);
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		ASSERT_TRUE(etM.cloneTo(m));
		bt = steady_clock::now();
		iM.evMul_ip_st_naive(m, B);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etDest) << "evMul_ip_st_naive";
	ASSERT_EQ(B, etB) << "evMul_ip_st_naive";
	STDCOUTL("st_naive:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded naive
	ASSERT_TRUE(etM.cloneTo(m));
	iM.evMul_ip_mt_naive(m, B);
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		ASSERT_TRUE(etM.cloneTo(m));
		bt = steady_clock::now();
		iM.evMul_ip_mt_naive(m, B);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etDest) << "evMul_ip_mt_naive";
	ASSERT_EQ(B, etB) << "evMul_ip_mt_naive";
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	//////////////////////////////////////////////////////////////////////////
	//single threaded vectorized
	/*ASSERT_TRUE(etM.cloneTo(m));
	iM.evMul_ip_st_vec(m, B);
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		ASSERT_TRUE(etM.cloneTo(m));
		bt = steady_clock::now();
		iM.evMul_ip_st_vec(m, B);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etDest) << "evMul_ip_st_vec";
	ASSERT_EQ(B, etB) << "evMul_ip_st_vec";
	STDCOUTL("st_vec:\t\t" << utils::duration_readable(diff, maxReps, &tstVect));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded vectorized
	ASSERT_TRUE(etM.cloneTo(m));
	iM.evMul_ip_mt_vec(m, B);
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		ASSERT_TRUE(etM.cloneTo(m));
		bt = steady_clock::now();
		iM.evMul_ip_mt_vec(m, B);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etDest) << "evMul_ip_mt_vec";
	ASSERT_EQ(B, etB) << "evMul_ip_mt_vec";
	STDCOUTL("mt_vec:\t\t" << utils::duration_readable(diff, maxReps, &tmtVect));
*/

	//////////////////////////////////////////////////////////////////////////
	//best guess
	ASSERT_TRUE(etM.cloneTo(m));
	iM.evMul_ip(m, B);
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		ASSERT_TRUE(etM.cloneTo(m));
		bt = steady_clock::now();
		iM.evMul_ip(m, B);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etDest) << "evMul_ip";
	ASSERT_EQ(B, etB) << "evMul_ip";
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, evMulIp) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evMul_ip, 100) test_evMul_ip(iM, i, 100);
}

//////////////////////////////////////////////////////////////////////////
template<typename iMath>
void test_evMulC_ip(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("********* testing evMulC_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	EXPECT_TRUE(steady_clock::is_steady);
	double tmtNaive, tstNaive, tBest; //tstVect, tmtVect
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	const real_t mulC = real_t(0.01);
	realmtx_t m, etM(rowsCnt, colsCnt), etDest(rowsCnt, colsCnt);
	ASSERT_EQ(dataSize, etM.numel());

	//filling etalon
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(etM, 5);
	auto ptrEtM = etM.data(), ptrDest=etDest.data();
	for (unsigned i = 0; i < dataSize; ++i) ptrDest[i] = mulC*ptrEtM[i];
	
	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	//////////////////////////////////////////////////////////////////////////
	//single threaded naive
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		ASSERT_TRUE(etM.cloneTo(m));
		bt = steady_clock::now();
		iM.evMulC_ip_st_naive(m,mulC);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etDest);
	STDCOUTL("st_naive:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded naive
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		ASSERT_TRUE(etM.cloneTo(m));
		bt = steady_clock::now();
		iM.evMulC_ip_mt_naive(m, mulC);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etDest);
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	/*//////////////////////////////////////////////////////////////////////////
	//single threaded vectorized
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		ASSERT_TRUE(etM.cloneTo(m));
		bt = steady_clock::now();
		iM.evMulC_ip_st_vec(m, mulC);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etDest);
	STDCOUTL("st_vec:\t\t" << utils::duration_readable(diff, maxReps, &tstVect));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded vectorized
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		ASSERT_TRUE(etM.cloneTo(m));
		bt = steady_clock::now();
		iM.evMulC_ip_mt_vec(m, mulC);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etDest);
	STDCOUTL("mt_vec:\t\t" << utils::duration_readable(diff, maxReps, &tmtVect));*/

	//////////////////////////////////////////////////////////////////////////
	//best guess
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		ASSERT_TRUE(etM.cloneTo(m));
		bt = steady_clock::now();
		iM.evMulC_ip(m, mulC);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etDest);
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, evMulC_ip) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evMulC_ip, 100) test_evMulC_ip(iM, i, 100);
}


//////////////////////////////////////////////////////////////////////////
template<typename base_t> struct sigm_EPS {};
template<> struct sigm_EPS<double> { static constexpr double eps = 1e-12; };
template<> struct sigm_EPS<float> { static constexpr float eps = 1e-6f; };

template<typename iMath>
void test_sigm(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	typedef typename iMath::ithreads_t threads_t;
	typedef math::smatrix_deform<real_t> realmtxdef_t;

	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("********* testing sigm() over ~" << dataSize << " elements) **************");

	EXPECT_TRUE(steady_clock::is_steady);
	double tmtNaive, tstNaive, tBest; //tstVect, tmtVect
	steady_clock::time_point bt;
	nanoseconds diff;
	const unsigned maxReps = ceil(((real_t)TEST_PERF_REPEATS_COUNT)/25.0);


	const auto threadsCount = iM.ithreads().workers_count();
	ASSERT_TRUE(threadsCount > 0);
	const auto biggestDataSize = static_cast<realmtx_t::vec_len_t>(dataSize + threadsCount);

	realmtxdef_t m, etDest(biggestDataSize, 1);
	realmtx_t etM(biggestDataSize, 1);
	ASSERT_TRUE(biggestDataSize == etM.numel());

	iM.preinit(biggestDataSize);
	ASSERT_TRUE(iM.init());

	//filling etalon
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(etM, 2);
	auto ptrEtM = etM.data(), ptrDest = etDest.data();
	for (unsigned i = 0; i < biggestDataSize; ++i) {
		ptrDest[i] = real_t(1.0) / (real_t(1.0) + std::exp(-ptrEtM[i]));
	}
	ASSERT_TRUE(m.cloneFrom(etM));
	ASSERT_TRUE(etM == m);

	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());
	
	//////////////////////////////////////////////////////////////////////////
	//single threaded naive
	diff = nanoseconds(0);
	for (threads_t::thread_id_t t = 0; t < threadsCount; ++t) {
		const unsigned iMax = static_cast<realmtx_t::vec_len_t>(dataSize + t);
		for (unsigned r = 0; r < maxReps; ++r) {
			m.deform_rows(biggestDataSize);
			ASSERT_TRUE(m.cloneFrom(etM));
			m.deform_rows(iMax);
			bt = steady_clock::now();
			iM.sigm_st(m);
			diff += steady_clock::now() - bt;
		}
		/*const auto ptr = m.data();
		if (std::is_same<real_t, float>::value) {
			for (numel_cnt_t i = 0; i < iMax; ++i) ASSERT_FLOAT_EQ(ptrDest[i], ptr[i]);
		} else {
			for (numel_cnt_t i = 0; i < iMax; ++i) ASSERT_DOUBLE_EQ(ptrDest[i], ptr[i]);
		}*/
		etDest.deform_rows(iMax);
		ASSERT_REALMTX_NEAR(etDest, m, "st_naive failed", sigm_EPS<real_t>::eps);
		etDest.deform_rows(biggestDataSize);
	}
	STDCOUTL("st_naive:\t" << utils::duration_readable(diff, maxReps*threadsCount, &tstNaive));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded naive
	diff = nanoseconds(0);
	for (threads_t::thread_id_t t = 0; t < threadsCount; ++t) {
		const unsigned iMax = static_cast<realmtx_t::vec_len_t>(dataSize + t);
		for (unsigned r = 0; r < maxReps; ++r) {
			m.deform_rows(biggestDataSize);
			ASSERT_TRUE(m.cloneFrom(etM));
			m.deform_rows(iMax);
			bt = steady_clock::now();
			iM.sigm_mt(m);
			diff += steady_clock::now() - bt;
		}
		const auto ptr = m.data();
		if (std::is_same<real_t, float>::value) {
			for (numel_cnt_t i = 0; i < iMax; ++i) ASSERT_FLOAT_EQ(ptrDest[i], ptr[i]);
		} else {
			for (numel_cnt_t i = 0; i < iMax; ++i) ASSERT_DOUBLE_EQ(ptrDest[i], ptr[i]);
		}
	}
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps*threadsCount, &tmtNaive));
/*

	//////////////////////////////////////////////////////////////////////////
	//single threaded vectorized
	diff = nanoseconds(0);
	for (threads_t::thread_id_t t = 0; t < threadsCount; ++t) {
		const unsigned iMax = static_cast<realmtx_t::vec_len_t>(dataSize + t);
		for (unsigned r = 0; r < maxReps; ++r) {
			m.deform_rows(biggestDataSize);
			ASSERT_TRUE(m.cloneFrom(etM));
			m.deform_rows(iMax);
			bt = steady_clock::now();
			iM.sigm_st_vec(m);
			diff += steady_clock::now() - bt;
		}
		const auto ptr = m.data();
		for (numel_cnt_t i = 0; i < iMax; ++i) ASSERT_DOUBLE_EQ(ptrDest[i], ptr[i]);
	}
	STDCOUTL("st_vec:\t\t" << utils::duration_readable(diff, maxReps*threadsCount, &tstVect));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded vectorized
	diff = nanoseconds(0);
	for (threads_t::thread_id_t t = 0; t < threadsCount; ++t) {
		const unsigned iMax = static_cast<realmtx_t::vec_len_t>(dataSize + t);
		for (unsigned r = 0; r < maxReps; ++r) {
			m.deform_rows(biggestDataSize);
			ASSERT_TRUE(m.cloneFrom(etM));
			m.deform_rows(iMax);
			bt = steady_clock::now();
			iM.sigm_mt_vec(m);
			diff += steady_clock::now() - bt;
		}
		const auto ptr = m.data();
		for (numel_cnt_t i = 0; i < iMax; ++i) ASSERT_DOUBLE_EQ(ptrDest[i], ptr[i]);
	}
	STDCOUTL("mt_vec:\t\t" << utils::duration_readable(diff, maxReps*threadsCount, &tmtVect));
*/

	//////////////////////////////////////////////////////////////////////////
	//best
	diff = nanoseconds(0);
	for (threads_t::thread_id_t t = 0; t < threadsCount; ++t) {
		const unsigned iMax = static_cast<realmtx_t::vec_len_t>(dataSize + t);
		for (unsigned r = 0; r < maxReps; ++r) {
			m.deform_rows(biggestDataSize);
			ASSERT_TRUE(m.cloneFrom(etM));
			m.deform_rows(iMax);
			bt = steady_clock::now();
			iM.sigm(m);
			diff += steady_clock::now() - bt;
		}
		const auto ptr = m.data();
		if (std::is_same<real_t, float>::value) {
			for (numel_cnt_t i = 0; i < iMax; ++i) ASSERT_FLOAT_EQ(ptrDest[i], ptr[i]);
		} else {
			for (numel_cnt_t i = 0; i < iMax; ++i) ASSERT_DOUBLE_EQ(ptrDest[i], ptr[i]);
		}
	}
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps*threadsCount, &tBest));
}

TEST(TestMathN, Sigm) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::sigm, 10) test_sigm(iM, i, 10);
}

//////////////////////////////////////////////////////////////////////////
template<typename iMath>
void test_dsigm(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("********* testing dsigm() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	EXPECT_TRUE(steady_clock::is_steady);
	double tmtNaive, tstNaive, tBest; //tstVect, tmtVect
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	realmtx_t m, etM(rowsCnt, colsCnt), etDest(rowsCnt, colsCnt), dest(rowsCnt, colsCnt);
	ASSERT_EQ(dataSize, etM.numel());

	//filling etalon
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix_norm(etM);
	auto ptrEtM = etM.data(), ptrDest = etDest.data();
	for (unsigned i = 0; i < dataSize; ++i) ptrDest[i] = ptrEtM[i] * (1 - ptrEtM[i]);
	ASSERT_TRUE(etM.cloneTo(m));

	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	//////////////////////////////////////////////////////////////////////////
	//single threaded naive
	dest.zeros();
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		bt = steady_clock::now();
		iM.dsigm_st(m, dest);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etM);
	ASSERT_EQ(dest, etDest);
	STDCOUTL("st_naive:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded naive
	dest.zeros();
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		bt = steady_clock::now();
		iM.dsigm_mt(m, dest);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etM);
	ASSERT_EQ(dest, etDest);
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));
/*

	//////////////////////////////////////////////////////////////////////////
	//single threaded vectorized
	dest.zeros();
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		bt = steady_clock::now();
		iM.dsigm_st_vec(m, dest);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etM);
	ASSERT_EQ(dest, etDest);
	STDCOUTL("st_vec:\t\t" << utils::duration_readable(diff, maxReps, &tstVect));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded vectorized
	dest.zeros();
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		bt = steady_clock::now();
		iM.dsigm_mt_vec(m, dest);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etM);
	ASSERT_EQ(dest, etDest);
	STDCOUTL("mt_vec:\t\t" << utils::duration_readable(diff, maxReps, &tmtVect));
*/

	//////////////////////////////////////////////////////////////////////////
	//best guess
	dest.zeros();
	diff = nanoseconds(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		bt = steady_clock::now();
		iM.dsigm(m, dest);
		diff += steady_clock::now() - bt;
	}
	ASSERT_EQ(m, etM);
	ASSERT_EQ(dest, etDest);
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, dsigm) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::dsigm, 10) test_dsigm(iM, i, 10);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void test_relu_corr(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "test_relu_corr");
	constexpr unsigned testCorrRepCnt = 10;
	realmtx_t src(rowsCnt, colsCnt, true), F(rowsCnt, colsCnt,true), F_ET(rowsCnt, colsCnt,true);
	ASSERT_TRUE(!src.isAllocationFailed() && !F.isAllocationFailed() && !F_ET.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_no_bias(src, 2);
		ASSERT_TRUE(src.test_biases_ok());
		src.cloneTo(F_ET);

		relu_ET(F_ET);
		ASSERT_TRUE(F_ET.test_biases_ok());

		src.cloneTo(F);
		iM.relu_st(F);
		ASSERT_MTX_EQ(F, F_ET, "_st() failed");

		src.cloneTo(F);
		iM.relu_mt(F);
		ASSERT_MTX_EQ(F, F_ET, "_mt() failed");

		src.cloneTo(F);
		iM.relu(F);
		ASSERT_MTX_EQ(F, F_ET, "() failed");
	}
}
TEST(TestMathN, Relu) {
	for (vec_len_t r = 1; r < g_MinDataSizeDelta; ++r) {
		for (vec_len_t c = 1; c < g_MinDataSizeDelta; ++c) {
			test_relu_corr(r, c);
		}
	}
}
void test_drelu_corr(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "test_drelu_corr");
	constexpr unsigned testCorrRepCnt = 10;
	realmtx_t df_ET(rowsCnt, colsCnt, false), F(rowsCnt, colsCnt, true), dfM(rowsCnt, colsCnt, false);
	ASSERT_TRUE(!df_ET.isAllocationFailed() && !F.isAllocationFailed() && !dfM.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_no_bias(F, 2);
		ASSERT_TRUE(F.test_biases_ok());

		drelu_ET(F,df_ET);
		ASSERT_TRUE(F.test_biases_ok());

		iM.drelu_st(F,dfM);
		ASSERT_MTX_EQ(df_ET, dfM, "_st() failed");

		iM.drelu_mt(F, dfM);
		ASSERT_MTX_EQ(df_ET, dfM, "_mt() failed");

		iM.drelu(F, dfM);
		ASSERT_MTX_EQ(df_ET, dfM, "() failed");
	}
}
TEST(TestMathN, DRelu) {
	for (vec_len_t r = 1; r < g_MinDataSizeDelta; ++r) {
		for (vec_len_t c = 1; c < g_MinDataSizeDelta; ++c) {
			test_drelu_corr(r, c);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void test_leakyrelu_corr(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "test_leakyrelu_corr");
	constexpr unsigned testCorrRepCnt = 10;
	realmtx_t src(rowsCnt, colsCnt, true), F(rowsCnt, colsCnt, true), F_ET(rowsCnt, colsCnt, true);
	ASSERT_TRUE(!src.isAllocationFailed() && !F.isAllocationFailed() && !F_ET.isAllocationFailed());
	const real_t leak = real_t(.001);
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_no_bias(src, 2);
		ASSERT_TRUE(src.test_biases_ok());
		src.cloneTo(F_ET);

		leakyrelu_ET(F_ET, leak);
		ASSERT_TRUE(F_ET.test_biases_ok());

		src.cloneTo(F);
		iM.leakyrelu_st(F, leak);
		ASSERT_MTX_EQ(F, F_ET, "_st() failed");

		src.cloneTo(F);
		iM.leakyrelu_mt(F, leak);
		ASSERT_MTX_EQ(F, F_ET, "_mt() failed");

		src.cloneTo(F);
		iM.leakyrelu(F, leak);
		ASSERT_MTX_EQ(F, F_ET, "() failed");
	}
}
TEST(TestMathN, LeakyRelu) {
	for (vec_len_t r = 1; r < g_MinDataSizeDelta; ++r) {
		for (vec_len_t c = 1; c < g_MinDataSizeDelta; ++c) {
			test_leakyrelu_corr(r, c);
		}
	}
}
void test_dleakyrelu_corr(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "test_dleakyrelu_corr");
	constexpr unsigned testCorrRepCnt = 10;
	realmtx_t df_ET(rowsCnt, colsCnt, false), F(rowsCnt, colsCnt, true), dfM(rowsCnt, colsCnt, false);
	ASSERT_TRUE(!df_ET.isAllocationFailed() && !F.isAllocationFailed() && !dfM.isAllocationFailed());
	const real_t leak = real_t(.001);
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_no_bias(F, 2);
		ASSERT_TRUE(F.test_biases_ok());

		dleakyrelu_ET(F, df_ET, leak);
		ASSERT_TRUE(F.test_biases_ok());

		iM.dleakyrelu_st(F, dfM, leak);
		ASSERT_MTX_EQ(df_ET, dfM, "_st() failed");

		iM.dleakyrelu_mt(F, dfM, leak);
		ASSERT_MTX_EQ(df_ET, dfM, "_mt() failed");

		iM.dleakyrelu(F, dfM, leak);
		ASSERT_MTX_EQ(df_ET, dfM, "() failed");
	}
}
TEST(TestMathN, DLeakyRelu) {
	for (vec_len_t r = 1; r < g_MinDataSizeDelta; ++r) {
		for (vec_len_t c = 1; c < g_MinDataSizeDelta; ++c) {
			test_dleakyrelu_corr(r, c);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void test_elu_corr(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "test_elu_corr");
	constexpr unsigned testCorrRepCnt = 10;
	realmtx_t src(rowsCnt, colsCnt, true), F(rowsCnt, colsCnt, true), F_ET(rowsCnt, colsCnt, true), FU_ET(rowsCnt, colsCnt, true);
	ASSERT_TRUE(!src.isAllocationFailed() && !F.isAllocationFailed() && !F_ET.isAllocationFailed() && !FU_ET.isAllocationFailed());

	const real_t alpha = real_t(2.5);

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_no_bias(src, 2);
		ASSERT_TRUE(src.test_biases_ok());

		src.cloneTo(F_ET);
		elu_ET(F_ET, alpha);
		ASSERT_TRUE(F_ET.test_biases_ok());
		src.cloneTo(FU_ET);
		elu_unitalpha_ET(FU_ET);
		ASSERT_TRUE(FU_ET.test_biases_ok());
		//ASSERT_TRUE(FU_ET != F_ET);

		src.cloneTo(F);
		iM.elu_st(F, alpha);
		ASSERT_MTX_EQ(F, F_ET, "elu_st() failed");
		src.cloneTo(F);
		iM.elu_unitalpha_st(F);
		ASSERT_MTX_EQ(F, FU_ET, "elu_unitalpha_st() failed");

		src.cloneTo(F);
		iM.elu_mt(F, alpha);
		ASSERT_MTX_EQ(F, F_ET, "elu_mt() failed");
		src.cloneTo(F);
		iM.elu_unitalpha_mt(F);
		ASSERT_MTX_EQ(F, FU_ET, "elu_unitalpha_mt() failed");

		src.cloneTo(F);
		iM.elu(F, alpha);
		ASSERT_MTX_EQ(F, F_ET, "elu() failed");
		src.cloneTo(F);
		iM.elu_unitalpha(F);
		ASSERT_MTX_EQ(F, FU_ET, "elu_unitalpha() failed");
	}
}
TEST(TestMathN, ELU) {
	for (vec_len_t r = 1; r < g_MinDataSizeDelta; ++r) {
		for (vec_len_t c = 1; c < g_MinDataSizeDelta; ++c) {
			test_elu_corr(r, c);
		}
	}
}
void test_delu_corr(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "test_delu_corr");
	constexpr unsigned testCorrRepCnt = 10;
	realmtx_t df_ET(rowsCnt, colsCnt, false), F(rowsCnt, colsCnt, true), dfM(rowsCnt, colsCnt, false), dfU_ET(rowsCnt, colsCnt, false);
	ASSERT_TRUE(!df_ET.isAllocationFailed() && !F.isAllocationFailed() && !dfM.isAllocationFailed() && !dfU_ET.isAllocationFailed());
	
	const real_t alpha = real_t(2.5);

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_no_bias(F, 2);
		ASSERT_TRUE(F.test_biases_ok());

		delu_ET(F, df_ET, alpha);
		ASSERT_TRUE(F.test_biases_ok());
		delu_unitalpha_ET(F, dfU_ET);
		ASSERT_TRUE(F.test_biases_ok());
		//ASSERT_TRUE(df_ET != dfU_ET);
		
		iM.delu_st(F, dfM, alpha);
		ASSERT_MTX_EQ(df_ET, dfM, "delu_st() failed");
		iM.delu_unitalpha_st(F, dfM);
		ASSERT_MTX_EQ(dfU_ET, dfM, "delu_unitalpha_st() failed");

		iM.delu_mt(F, dfM, alpha);
		ASSERT_MTX_EQ(df_ET, dfM, "delu_mt() failed");
		iM.delu_unitalpha_mt(F, dfM);
		ASSERT_MTX_EQ(dfU_ET, dfM, "delu_unitalpha_mt() failed");

		iM.delu(F, dfM, alpha);
		ASSERT_MTX_EQ(df_ET, dfM, "delu() failed");
		iM.delu_unitalpha(F, dfM);
		ASSERT_MTX_EQ(dfU_ET, dfM, "delu_unitalpha() failed");
	}
}
TEST(TestMathN, DELU) {
	for (vec_len_t r = 1; r < g_MinDataSizeDelta; ++r) {
		for (vec_len_t c = 1; c < g_MinDataSizeDelta; ++c) {
			test_delu_corr(r, c);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
template<typename base_t> struct elogu_EPS {};
template<> struct elogu_EPS <double> { static constexpr double eps = 1e-12; };
template<> struct elogu_EPS <float> { static constexpr float eps = 1e-6f; };
void test_elogu_corr(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "test_elogu_corr");
	constexpr unsigned testCorrRepCnt = 10;
	realmtx_t X(rowsCnt, colsCnt, true), F(rowsCnt, colsCnt, true)
		, F_ET(rowsCnt, colsCnt, true)
		, FUA_ET(rowsCnt, colsCnt, true)
		, FNB_ET(rowsCnt, colsCnt, true)
		, FUANB_ET(rowsCnt, colsCnt, true);

	ASSERT_TRUE(!X.isAllocationFailed() && !F.isAllocationFailed() && !F_ET.isAllocationFailed() 
		&& !FUA_ET.isAllocationFailed() && !FNB_ET.isAllocationFailed() && !FUANB_ET.isAllocationFailed());

	constexpr real_t alpha = real_t(2.5), b=real_t(2.);

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_no_bias(X, 5);
		ASSERT_TRUE(X.test_biases_ok());

		elogu_ET(X, F_ET, alpha, b);
		ASSERT_TRUE(F_ET.test_biases_ok());
		elogu_ua_ET(X, FUA_ET, b);
		ASSERT_TRUE(FUA_ET.test_biases_ok());
		elogu_nb_ET(X, FNB_ET, alpha);
		ASSERT_TRUE(FNB_ET.test_biases_ok());
		elogu_ua_nb_ET(X, FUANB_ET);
		ASSERT_TRUE(FUANB_ET.test_biases_ok());


		X.cloneTo(F);
		iM.elogu_st(F, alpha, b);
		ASSERT_REALMTX_NEAR(F, F_ET, "elogu_st() failed", elogu_EPS<real_t>::eps);
		X.cloneTo(F);
		iM.elogu_ua_st(F, b);
		ASSERT_REALMTX_NEAR(F, FUA_ET, "elogu_ua_st() failed", elogu_EPS<real_t>::eps);
		X.cloneTo(F);
		iM.elogu_nb_st(F, alpha);
		ASSERT_REALMTX_NEAR(F, FNB_ET, "elogu_nb_st() failed", elogu_EPS<real_t>::eps);
		X.cloneTo(F);
		iM.elogu_ua_nb_st(F);
		ASSERT_REALMTX_NEAR(F, FUANB_ET, "elogu_ua_nb_st() failed", elogu_EPS<real_t>::eps);


		X.cloneTo(F);
		iM.elogu_mt(F, alpha, b);
		ASSERT_REALMTX_NEAR(F, F_ET, "elogu_mt() failed", elogu_EPS<real_t>::eps);
		X.cloneTo(F);
		iM.elogu_ua_mt(F, b);
		ASSERT_REALMTX_NEAR(F, FUA_ET, "elogu_ua_mt() failed", elogu_EPS<real_t>::eps);
		X.cloneTo(F);
		iM.elogu_nb_mt(F, alpha);
		ASSERT_REALMTX_NEAR(F, FNB_ET, "elogu_nb_mt() failed", elogu_EPS<real_t>::eps);
		X.cloneTo(F);
		iM.elogu_ua_nb_mt(F);
		ASSERT_REALMTX_NEAR(F, FUANB_ET, "elogu_ua_nb_mt() failed", elogu_EPS<real_t>::eps);


		X.cloneTo(F);
		iM.elogu(F, alpha, b);
		ASSERT_REALMTX_NEAR(F, F_ET, "elogu() failed", elogu_EPS<real_t>::eps);
		X.cloneTo(F);
		iM.elogu_ua(F, b);
		ASSERT_REALMTX_NEAR(F, FUA_ET, "elogu_ua() failed", elogu_EPS<real_t>::eps);
		X.cloneTo(F);
		iM.elogu_nb(F, alpha);
		ASSERT_REALMTX_NEAR(F, FNB_ET, "elogu_nb() failed", elogu_EPS<real_t>::eps);
		X.cloneTo(F);
		iM.elogu_ua_nb(F);
		ASSERT_REALMTX_NEAR(F, FUANB_ET, "elogu_ua_nb() failed", elogu_EPS<real_t>::eps);
	}
}
TEST(TestMathN, ELogU) {
	for (vec_len_t r = 1; r < g_MinDataSizeDelta; ++r) {
		for (vec_len_t c = 1; c < g_MinDataSizeDelta; ++c) {
			test_elogu_corr(r, c);
		}
	}
}
template<typename base_t> struct delogu_EPS {};
template<> struct delogu_EPS <double> { static constexpr double eps = 1e-12; };
template<> struct delogu_EPS <float> { static constexpr float eps = 1e-6f; };
void test_delogu_corr(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	MTXSIZE_SCOPED_TRACE(rowsCnt, colsCnt, "test_delogu_corr");
	constexpr unsigned testCorrRepCnt = 10;
	realmtx_t X(rowsCnt, colsCnt, true), F(rowsCnt, colsCnt, true), DF(rowsCnt, colsCnt, false)
		, df_ET(rowsCnt, colsCnt, false), dfUA_ET(rowsCnt, colsCnt, false)
		, dfNB_ET(rowsCnt, colsCnt, false), dfUANB_ET(rowsCnt, colsCnt, false);
	ASSERT_TRUE(!X.isAllocationFailed() && !F.isAllocationFailed() && !DF.isAllocationFailed() 
		&& !df_ET.isAllocationFailed() && !dfUA_ET.isAllocationFailed()
		&& !dfNB_ET.isAllocationFailed() && !dfUANB_ET.isAllocationFailed());

	constexpr real_t alpha = real_t(2.5), b = real_t(2.);

	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	for (unsigned r = 0; r < testCorrRepCnt; ++r) {
		rg.gen_matrix_no_bias(X, 5);
		ASSERT_TRUE(X.test_biases_ok());

		delogu_ET(X, df_ET, alpha, b);
		delogu_ua_ET(X, dfUA_ET, b);
		delogu_nb_ET(X, dfNB_ET, alpha);
		delogu_ua_nb_ET(X, dfUANB_ET);

		elogu_ET(X, F, alpha, b);
		ASSERT_TRUE(F.test_biases_ok());
		DF.zeros();
		iM.delogu_st(F, DF, alpha, b);
		ASSERT_REALMTX_NEAR(df_ET, DF, "delogu_st() failed", delogu_EPS<real_t>::eps);
		DF.zeros();
		iM.delogu_mt(F, DF, alpha, b);
		ASSERT_REALMTX_NEAR(df_ET, DF, "delogu_mt() failed", delogu_EPS<real_t>::eps);
		DF.zeros();
		iM.delogu(F, DF, alpha, b);
		ASSERT_REALMTX_NEAR(df_ET, DF, "delogu() failed", delogu_EPS<real_t>::eps);

		elogu_ua_ET(X, F, b);
		ASSERT_TRUE(F.test_biases_ok());
		DF.zeros();
		iM.delogu_ua_st(F, DF, b);
		ASSERT_REALMTX_NEAR(dfUA_ET, DF, "delogu_ua_st() failed", delogu_EPS<real_t>::eps);
		DF.zeros();
		iM.delogu_ua_mt(F, DF, b);
		ASSERT_REALMTX_NEAR(dfUA_ET, DF, "delogu_ua_mt() failed", delogu_EPS<real_t>::eps);
		DF.zeros();
		iM.delogu_ua(F, DF, b);
		ASSERT_REALMTX_NEAR(dfUA_ET, DF, "delogu_ua() failed", delogu_EPS<real_t>::eps);

		elogu_nb_ET(X, F, alpha);
		ASSERT_TRUE(F.test_biases_ok());
		DF.zeros();
		iM.delogu_nb_st(F, DF, alpha);
		ASSERT_REALMTX_NEAR(dfNB_ET, DF, "delogu_nb_st() failed", delogu_EPS<real_t>::eps);
		DF.zeros();
		iM.delogu_nb_mt(F, DF, alpha);
		ASSERT_REALMTX_NEAR(dfNB_ET, DF, "delogu_nb_mt() failed", delogu_EPS<real_t>::eps);
		DF.zeros();
		iM.delogu_nb(F, DF, alpha);
		ASSERT_REALMTX_NEAR(dfNB_ET, DF, "delogu_nb() failed", delogu_EPS<real_t>::eps);

		elogu_ua_nb_ET(X, F);
		ASSERT_TRUE(F.test_biases_ok());
		DF.zeros();
		iM.delogu_ua_nb_st(F, DF);
		ASSERT_REALMTX_NEAR(dfUANB_ET, DF, "delogu_ua_nb_st() failed", delogu_EPS<real_t>::eps);
		DF.zeros();
		iM.delogu_ua_nb_mt(F, DF);
		ASSERT_REALMTX_NEAR(dfUANB_ET, DF, "delogu_ua_nb_mt() failed", delogu_EPS<real_t>::eps);
		DF.zeros();
		iM.delogu_ua_nb(F, DF);
		ASSERT_REALMTX_NEAR(dfUANB_ET, DF, "delogu_ua_nb() failed", delogu_EPS<real_t>::eps);
	}
}
TEST(TestMathN, DELogU) {
	for (vec_len_t r = 1; r < g_MinDataSizeDelta; ++r) {
		for (vec_len_t c = 1; c < g_MinDataSizeDelta; ++c) {
			test_delogu_corr(r, c);
		}
	}
}


////////////////////////////////////////////////////////////////////////// 
//////////////////////////////////////////////////////////////////////////
template<typename base_t> struct loss_quadratic_EPS {};
template<> struct loss_quadratic_EPS<double> { static constexpr double eps = 1e-10; };
template<> struct loss_quadratic_EPS<float> { static constexpr float eps = 2e-2f; };
template<typename iMath>
void test_loss_quadratic(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt=10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("********* testing loss_quadratic() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	iM.preinit(dataSize);
	ASSERT_TRUE(iM.init());

	EXPECT_TRUE(steady_clock::is_steady);
	double tmtNaive, tstNaive, tBest;//tstVect, tmtVect
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	realmtx_t A, etA(rowsCnt, colsCnt), etY(rowsCnt, colsCnt), Y;
	real_t etQuadLoss = 0, quadLoss = 0;
	ASSERT_EQ(dataSize, etA.numel());

	//filling etalon
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(etA, 5);
	rg.gen_matrix(etY, 5);
	ASSERT_TRUE(etA.cloneTo(A));
	ASSERT_TRUE(etY.cloneTo(Y));
	ASSERT_TRUE(etA == A && etY==Y);
	auto ptrEtA = etA.data(), ptrEtY = etY.data();

	for (unsigned i = 0; i < dataSize; ++i) {
		const real_t v = ptrEtA[i]- ptrEtY[i];
		etQuadLoss += v*v;
	}
	etQuadLoss = etQuadLoss / (2*etA.rows());

	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	//////////////////////////////////////////////////////////////////////////
	//single threaded naive
	diff = nanoseconds(0);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) quadLoss = iM.loss_quadratic_st_naive(A,Y);
	diff += steady_clock::now() - bt;
	ASSERT_EQ(A, etA);
	ASSERT_EQ(Y, etY);
	ASSERT_NEAR(etQuadLoss, quadLoss, loss_quadratic_EPS<real_t>::eps);
	STDCOUTL("st_naive:\t" << utils::duration_readable(diff, maxReps, &tstNaive));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded naive
	diff = nanoseconds(0);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) quadLoss = iM.loss_quadratic_mt_naive(A,Y);
	diff += steady_clock::now() - bt;
	ASSERT_EQ(A, etA);
	ASSERT_EQ(Y, etY);
	ASSERT_NEAR(etQuadLoss, quadLoss, loss_quadratic_EPS<real_t>::eps);
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

	/*//////////////////////////////////////////////////////////////////////////
	//single threaded vectorized
	diff = nanoseconds(0);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) quadLoss = iM.loss_quadratic_st_vec(A,Y);
	diff += steady_clock::now() - bt;
	ASSERT_EQ(A, etA);
	ASSERT_EQ(Y, etY);
	ASSERT_NEAR(etQuadLoss, quadLoss, loss_quadratic_EPS<real_t>::eps);
	STDCOUTL("st_vec:\t\t" << utils::duration_readable(diff, maxReps, &tstVect));
	
	//////////////////////////////////////////////////////////////////////////
	//multi threaded vectorized
	diff = nanoseconds(0);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) quadLoss = iM.loss_quadratic_mt_vec(A,Y);
	diff += steady_clock::now() - bt;
	ASSERT_EQ(A, etA);
	ASSERT_EQ(Y, etY);
	ASSERT_NEAR(etQuadLoss, quadLoss, loss_quadratic_EPS<real_t>::eps);
	STDCOUTL("mt_vec:\t\t" << utils::duration_readable(diff, maxReps, &tmtVect));*/

	//////////////////////////////////////////////////////////////////////////
	//best guess
	diff = nanoseconds(0);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) quadLoss = iM.loss_quadratic(A,Y);
	diff += steady_clock::now() - bt;
	ASSERT_EQ(A, etA);
	ASSERT_EQ(Y, etY);
	ASSERT_NEAR(etQuadLoss, quadLoss, loss_quadratic_EPS<real_t>::eps);
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, LossQuadratic) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::loss_quadratic, 100) test_loss_quadratic(iM, i, 100);
}

//////////////////////////////////////////////////////////////////////////

template<typename iMath>
void test_dSigmQuadLoss_dZ(iMath& iM, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("********* testing dSigmQuadLoss_dZ() over " << rowsCnt << "x" << colsCnt << " matrix ("<< dataSize <<" elements) **************");

	EXPECT_TRUE(steady_clock::is_steady);
	double tmtNaive, tstNaive, tBest; //tstVect, tmtVect
	steady_clock::time_point bt;
	nanoseconds diff;
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	iM.preinit(dataSize);
	ASSERT_TRUE(iM.init());
	realmtx_t etA(rowsCnt, colsCnt), etY(rowsCnt, colsCnt), etdLdZ(rowsCnt, colsCnt);
	realmtx_t A, Y, dLdZ;
	
	//filling etalons
	d_interfaces::iRng_t rg;
	rg.set_ithreads(iM.ithreads());
	rg.gen_matrix(etA, 5);
	rg.gen_matrix(etY, 5);
	ASSERT_TRUE(etA.cloneTo(A) && etY.cloneTo(Y) && dLdZ.resize(etdLdZ));
	ASSERT_TRUE(etY == Y && etA == A);
	const auto ptretA = etA.data(), ptretY = etY.data(), ptretdLdZ = etdLdZ.data();
	for (numel_cnt_t i = 0; i < dataSize; ++i) {
		const auto a = ptretA[i];
		ptretdLdZ[i] = (a - ptretY[i])*a*(real_t(1.0) - a);
	}

	//testing performance
	utils::prioritize_workers<utils::PriorityClass::PerfTesting, iMath::ithreads_t> pw(iM.ithreads());

	//////////////////////////////////////////////////////////////////////////
	//single threaded naive
	dLdZ.zeros();
	diff = nanoseconds(0);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.dSigmQuadLoss_dZ_st_naive(A,Y,dLdZ);
	diff += steady_clock::now() - bt;
	ASSERT_EQ(A, etA);
	ASSERT_EQ(Y, etY);
	ASSERT_EQ(dLdZ, etdLdZ);
	STDCOUTL("st_naive:\t" << utils::duration_readable(diff,maxReps,&tstNaive));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded naive
	dLdZ.zeros();
	diff = nanoseconds(0);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.dSigmQuadLoss_dZ_mt_naive(A, Y, dLdZ);
	diff += steady_clock::now() - bt;
	ASSERT_EQ(A, etA);
	ASSERT_EQ(Y, etY);
	ASSERT_EQ(dLdZ, etdLdZ);
	STDCOUTL("mt_naive:\t" << utils::duration_readable(diff, maxReps, &tmtNaive));

/*
	//////////////////////////////////////////////////////////////////////////
	//single threaded vectorized
	dLdZ.zeros();
	diff = nanoseconds(0);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.dSigmQuadLoss_dZ_st_vec(A,Y,dLdZ);
	diff += steady_clock::now() - bt;
	ASSERT_EQ(A, etA);
	ASSERT_EQ(Y, etY);
	ASSERT_EQ(dLdZ, etdLdZ);
	STDCOUTL("st_vec:\t\t" << utils::duration_readable(diff, maxReps, &tstVect));

	//////////////////////////////////////////////////////////////////////////
	//multi threaded vectorized
	dLdZ.zeros();
	diff = nanoseconds(0);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.dSigmQuadLoss_dZ_mt_vec(A, Y, dLdZ);
	diff += steady_clock::now() - bt;
	ASSERT_EQ(A, etA);
	ASSERT_EQ(Y, etY);
	ASSERT_EQ(dLdZ, etdLdZ);
	STDCOUTL("mt_vec:\t\t" << utils::duration_readable(diff, maxReps, &tmtVect));*/

	//////////////////////////////////////////////////////////////////////////
	//best guess
	dLdZ.zeros();
	diff = nanoseconds(0);
	bt = steady_clock::now();
	for (unsigned r = 0; r < maxReps; ++r) iM.dSigmQuadLoss_dZ(A, Y, dLdZ);
	diff += steady_clock::now() - bt;
	ASSERT_EQ(A, etA);
	ASSERT_EQ(Y, etY);
	ASSERT_EQ(dLdZ, etdLdZ);
	STDCOUTL("best:\t\t" << utils::duration_readable(diff, maxReps, &tBest));
}

TEST(TestMathN, dSigmQuadLoss_dZ) {
	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	typedef math::MathN<real_t, def_threads_t> iMB;
	iMB iM;
	NNTL_RUN_TEST2(iMB::Thresholds_t::evMulC_ip_Sub_ip, 10) test_dSigmQuadLoss_dZ(iM, i, 10);
}


