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

#include "../nntl/math.h"
#include "../nntl/common.h"

#include "../nntl/interface/math/mathn.h"
#include "../nntl/interfaces.h"

#include "../nntl/utils/tictoc.h"

#include "../nntl/_SNN_common.h"

#include "imath_etalons.h"

#include "../nntl/_test/functions.h"

using namespace nntl;
using namespace nntl::utils;

typedef d_interfaces::iThreads_t iThreads_t;
typedef math::MathN<real_t, iThreads_t> imath_basic_t;

static imath_basic_t iM;

#ifdef TESTS_SKIP_LONGRUNNING
constexpr unsigned TEST_PERF_REPEATS_COUNT = 10;
#else
constexpr unsigned TEST_PERF_REPEATS_COUNT = 1000;
#endif // NNTL_DEBUG


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TEST(TestMathNThr, dLoss_dZ) {
	typedef activation::Linear_Loss_quadWeighted_FP<real_t> WL_FP;

	const auto fst = [](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dLoss_dZ_st<WL_FP>(data_y, act_dLdZ); };
	const auto fmt = [](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dLoss_dZ_mt<WL_FP>(data_y, act_dLdZ); };
	const auto fb = [](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dLoss_dZ<WL_FP>(data_y, act_dLdZ); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dLoss_dZ<typename WL_FP::tag_dLdZ>::thr, 1) {
		test_dLdZ_perf<true>(fst, fmt, fb, "dLoss_dZ<WeightedLoss_FP>", i, 1);
	}
}

TEST(TestMathNThr, dSigmQuadLoss_dZ) {
	const auto fst = [](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dSigmQuadLoss_dZ_st(data_y, act_dLdZ); };
	const auto fmt = [](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dSigmQuadLoss_dZ_mt(data_y, act_dLdZ); };
	const auto fb = [](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dSigmQuadLoss_dZ(data_y, act_dLdZ); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dSigmQuadLoss_dZ, 1) {
		test_dLdZ_perf<true>(fst, fmt, fb, "dSigmQuadLoss_dZ", i, 1);
	}
}

TEST(TestMathNThr, Sigm) {
	const auto fst = [](realmtx_t& X) { iM.sigm_st(X); };
	const auto fmt = [](realmtx_t& X) {iM.sigm_mt(X); };
	const auto fb = [](realmtx_t& X) {iM.sigm(X); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::sigm, 100) {
		test_f_x_perf(fst, fmt, fb, "sigm", 100, i);
	}
}

TEST(TestMathNThr, DSigm) {
	const auto fst = [](realmtx_t& F_DF) { iM.dsigm_st(F_DF); };
	const auto fmt = [](realmtx_t& F_DF) {iM.dsigm_mt(F_DF); };
	const auto fb = [](realmtx_t& F_DF) {iM.dsigm(F_DF); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dsigm, 100) {
		test_f_x_perf<0>(fst, fmt, fb, "dsigm", 100, i);
	}
}

TEST(TestMathNThr, Relu) {
	const auto fst = [](realmtx_t& X) { iM.relu_st(X); };
	const auto fmt = [](realmtx_t& X) {iM.relu_mt(X); };
	const auto fb = [](realmtx_t& X) {iM.relu(X); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::relu, 100) {
		test_f_x_perf(fst, fmt, fb, "relu", 100, i);
	}
}

TEST(TestMathNThr, DRelu) {
	const auto fst = [](realmtx_t& F_DF) { iM.drelu_st(F_DF); };
	const auto fmt = [](realmtx_t& F_DF) {iM.drelu_mt(F_DF); };
	const auto fb = [](realmtx_t& F_DF) {iM.drelu(F_DF); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::drelu, 100) {
		test_f_x_perf(fst, fmt, fb, "drelu", 100, i);
	}
}

TEST(TestMathNThr, LeakyRelu) {
	constexpr real_t leak = real_t(.01);
	const auto fst = [leak](realmtx_t& X) { iM.leakyrelu_st(X, leak); };
	const auto fmt = [leak](realmtx_t& X) {iM.leakyrelu_mt(X, leak); };
	const auto fb = [leak](realmtx_t& X) {iM.leakyrelu(X, leak); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::leakyrelu, 100) {
		test_f_x_perf(fst, fmt, fb, "leakyrelu", 100, i);
	}
}

TEST(TestMathNThr, DLeakyRelu) {
	constexpr real_t leak = real_t(.01);
	const auto fst = [leak](realmtx_t& F_DF) { iM.dleakyrelu_st(F_DF, leak); };
	const auto fmt = [leak](realmtx_t& F_DF) {iM.dleakyrelu_mt(F_DF, leak); };
	const auto fb = [leak](realmtx_t& F_DF) {iM.dleakyrelu(F_DF, leak); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dleakyrelu, 100) {
		test_f_x_perf(fst, fmt, fb, "dleakyrelu", 100, i);
	}
}

TEST(TestMathNThr, ELU) {
	constexpr real_t alpha = real_t(2.);
	const auto fst = [alpha](realmtx_t& X) { iM.elu_st(X, alpha); };
	const auto fmt = [alpha](realmtx_t& X) {iM.elu_mt(X, alpha); };
	const auto fb = [alpha](realmtx_t& X) {iM.elu(X, alpha); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::elu, 100) {
		test_f_x_perf(fst, fmt, fb, "elu", 100, i);
	}
}

TEST(TestMathNThr, DELU) {
	constexpr real_t alpha = real_t(2.);
	const auto fst = [alpha](realmtx_t& F_DF) { iM.delu_st(F_DF, alpha); };
	const auto fmt = [alpha](realmtx_t& F_DF) {iM.delu_mt(F_DF, alpha); };
	const auto fb = [alpha](realmtx_t& F_DF) {iM.delu(F_DF, alpha); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::delu, 100) {
		test_f_x_perf(fst, fmt, fb, "delu", 100, i);
	}
}

TEST(TestMathNThr, ELU_unitalpha) {
	const auto fst = [](realmtx_t& X) { iM.elu_unitalpha_st(X); };
	const auto fmt = [](realmtx_t& X) {iM.elu_unitalpha_mt(X); };
	const auto fb = [](realmtx_t& X) {iM.elu_unitalpha(X); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::elu_unitalpha, 100) {
		test_f_x_perf(fst, fmt, fb, "elu_unitalpha", 100, i);
	}
}

TEST(TestMathNThr, DELU_unitalpha) {
	const auto fst = [](realmtx_t& F_DF) { iM.delu_unitalpha_st(F_DF); };
	const auto fmt = [](realmtx_t& F_DF) {iM.delu_unitalpha_mt(F_DF); };
	const auto fb = [](realmtx_t& F_DF) {iM.delu_unitalpha(F_DF); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::delu_unitalpha, 100) {
		test_f_x_perf(fst, fmt, fb, "delu_unitalpha", 100, i);
	}
}



TEST(TestMathNThr, SELU) {
	constexpr real_t alpha = real_t(1.673), lambda= real_t(1.051), a_t_l=alpha*lambda;
	const auto fst = [a_t_l, lambda](realmtx_t& X) { iM.selu_st(X, a_t_l, lambda); };
	const auto fmt = [a_t_l, lambda](realmtx_t& X) {iM.selu_mt(X, a_t_l, lambda); };
	const auto fb = [a_t_l, lambda](realmtx_t& X) {iM.selu(X, a_t_l, lambda); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::selu, 100) {
		test_f_x_perf(fst, fmt, fb, "selu", 100, i);
	}
}

TEST(TestMathNThr, DSELU) {
	constexpr real_t alpha = real_t(1.673), lambda = real_t(1.051), a_t_l = alpha*lambda;
	const auto fst = [a_t_l, lambda](realmtx_t& F_DF) { iM.dselu_st(F_DF, a_t_l, lambda); };
	const auto fmt = [a_t_l, lambda](realmtx_t& F_DF) {iM.dselu_mt(F_DF, a_t_l, lambda); };
	const auto fb = [a_t_l, lambda](realmtx_t& F_DF) {iM.dselu(F_DF, a_t_l, lambda); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dselu, 100) {
		test_f_x_perf(fst, fmt, fb, "dselu", 100, i);
	}
}



TEST(TestMathNThr, ELogU) {
	constexpr real_t alpha = real_t(2.), b=real_t(2.);
	const auto fst = [alpha, b](realmtx_t& X) { iM.elogu_st(X, alpha, b); };
	const auto fmt = [alpha, b](realmtx_t& X) {iM.elogu_mt(X, alpha, b); };
	const auto fb = [alpha, b](realmtx_t& X) {iM.elogu(X, alpha, b); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::elogu, 100) {
		test_f_x_perf(fst, fmt, fb, "elogu", 100, i);
	}
}

TEST(TestMathNThr, DELogU) {
	constexpr real_t alpha = real_t(2.), b = real_t(2.);
	const auto fst = [alpha, b](realmtx_t& F_DF) { iM.delogu_st(F_DF, alpha, b); };
	const auto fmt = [alpha, b](realmtx_t& F_DF) {iM.delogu_mt(F_DF, alpha, b); };
	const auto fb = [alpha, b](realmtx_t& F_DF) {iM.delogu(F_DF, alpha, b); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::delogu, 100) {
		test_f_x_perf(fst, fmt, fb, "delogu", 100, i);
	}
}

TEST(TestMathNThr, ELogU_ua) {
	constexpr real_t b = real_t(2.);
	const auto fst = [b](realmtx_t& X) { iM.elogu_ua_st(X, b); };
	const auto fmt = [b](realmtx_t& X) {iM.elogu_ua_mt(X, b); };
	const auto fb = [b](realmtx_t& X) {iM.elogu_ua(X, b); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::elogu_ua, 100) {
		test_f_x_perf(fst, fmt, fb, "elogu_ua", 100, i);
	}
}

TEST(TestMathNThr, DELogU_ua) {
	constexpr real_t b = real_t(2.);
	const auto fst = [b](realmtx_t& F_DF) { iM.delogu_ua_st(F_DF, b); };
	const auto fmt = [b](realmtx_t& F_DF) {iM.delogu_ua_mt(F_DF, b); };
	const auto fb = [b](realmtx_t& F_DF) {iM.delogu_ua(F_DF, b); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::delogu_ua, 100) {
		test_f_x_perf(fst, fmt, fb, "delogu_ua", 100, i);
	}
}

TEST(TestMathNThr, ELogU_nb) {
	constexpr real_t alpha = real_t(2.);
	const auto fst = [alpha](realmtx_t& X) { iM.elogu_nb_st(X, alpha); };
	const auto fmt = [alpha](realmtx_t& X) {iM.elogu_nb_mt(X, alpha); };
	const auto fb = [alpha](realmtx_t& X) {iM.elogu_nb(X, alpha); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::elogu_nb, 100) {
		test_f_x_perf(fst, fmt, fb, "elogu_nb", 100, i);
	}
}

TEST(TestMathNThr, DELogU_nb) {
	constexpr real_t alpha = real_t(2.);
	const auto fst = [alpha](realmtx_t& F_DF) { iM.delogu_nb_st(F_DF, alpha); };
	const auto fmt = [alpha](realmtx_t& F_DF) {iM.delogu_nb_mt(F_DF, alpha); };
	const auto fb = [alpha](realmtx_t& F_DF) {iM.delogu_nb(F_DF, alpha); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::delogu_nb, 100) {
		test_f_x_perf(fst, fmt, fb, "delogu_nb", 100, i);
	}
}

TEST(TestMathNThr, ELogU_ua_nb) {
	const auto fst = [](realmtx_t& X) { iM.elogu_ua_nb_st(X); };
	const auto fmt = [](realmtx_t& X) {iM.elogu_ua_nb_mt(X); };
	const auto fb = [](realmtx_t& X) {iM.elogu_ua_nb(X); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::elogu_ua_nb, 100) {
		test_f_x_perf(fst, fmt, fb, "elogu_ua_nb", 100, i);
	}
}

TEST(TestMathNThr, DELogU_ua_nb) {
	const auto fst = [](realmtx_t& F_DF) { iM.delogu_ua_nb_st(F_DF); };
	const auto fmt = [](realmtx_t& F_DF) {iM.delogu_ua_nb_mt(F_DF); };
	const auto fb = [](realmtx_t& F_DF) {iM.delogu_ua_nb(F_DF); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::delogu_ua_nb, 100) {
		test_f_x_perf(fst, fmt, fb, "delogu_ua_nb", 100, i);
	}
}

//////////////////////////////////////////////////////////////////////////

TEST(TestMathNThr, LogLogU) {
	constexpr real_t b_neg = real_t(3.), b_pos = real_t(2.);
	const auto fst = [b_neg, b_pos](realmtx_t& X) { iM.loglogu_st(X, b_neg, b_pos); };
	const auto fmt = [b_neg, b_pos](realmtx_t& X) {iM.loglogu_mt(X, b_neg, b_pos); };
	const auto fb = [b_neg, b_pos](realmtx_t& X) {iM.loglogu(X, b_neg, b_pos); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::loglogu, 100) {
		test_f_x_perf(fst, fmt, fb, "loglogu", 100, i);
	}
}

TEST(TestMathNThr, DLogLogU) {
	constexpr real_t b_neg = real_t(3.), b_pos = real_t(2.);
	const auto fst = [b_neg, b_pos](realmtx_t& F_DF) { iM.dloglogu_st(F_DF, b_neg, b_pos); };
	const auto fmt = [b_neg, b_pos](realmtx_t& F_DF) {iM.dloglogu_mt(F_DF, b_neg, b_pos); };
	const auto fb = [b_neg, b_pos](realmtx_t& F_DF) {iM.dloglogu(F_DF, b_neg, b_pos); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dloglogu, 100) {
		test_f_x_perf(fst, fmt, fb, "dloglogu", 100, i);
	}
}

TEST(TestMathNThr, LogLogU_nbn) {
	constexpr real_t b_pos = real_t(2.);
	const auto fst = [b_pos](realmtx_t& X) { iM.loglogu_nbn_st(X, b_pos); };
	const auto fmt = [b_pos](realmtx_t& X) {iM.loglogu_nbn_mt(X, b_pos); };
	const auto fb = [b_pos](realmtx_t& X) {iM.loglogu_nbn(X, b_pos); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::loglogu_nbn, 100) {
		test_f_x_perf(fst, fmt, fb, "loglogu_nbn", 100, i);
	}
}

TEST(TestMathNThr, DLogLogU_nbn) {
	constexpr real_t b_neg = real_t(3.), b_pos = real_t(2.);
	const auto fst = [ b_pos](realmtx_t& F_DF) { iM.dloglogu_nbn_st(F_DF, b_pos); };
	const auto fmt = [b_pos](realmtx_t& F_DF) {iM.dloglogu_nbn_mt(F_DF, b_pos); };
	const auto fb = [b_pos](realmtx_t& F_DF) {iM.dloglogu_nbn(F_DF, b_pos); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dloglogu_nbn, 100) {
		test_f_x_perf(fst, fmt, fb, "dloglogu_nbn", 100, i);
	}
}

TEST(TestMathNThr, LogLogU_nbp) {
	constexpr real_t b_neg = real_t(3.);
	const auto fst = [b_neg](realmtx_t& X) { iM.loglogu_nbp_st(X, b_neg); };
	const auto fmt = [b_neg](realmtx_t& X) {iM.loglogu_nbp_mt(X, b_neg); };
	const auto fb = [b_neg](realmtx_t& X) {iM.loglogu_nbp(X, b_neg); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::loglogu_nbp, 100) {
		test_f_x_perf(fst, fmt, fb, "loglogu_nbp", 100, i);
	}
}

TEST(TestMathNThr, DLogLogU_nbp) {
	constexpr real_t b_neg = real_t(3.);
	const auto fst = [b_neg](realmtx_t& F_DF) { iM.dloglogu_nbp_st(F_DF, b_neg); };
	const auto fmt = [b_neg](realmtx_t& F_DF) {iM.dloglogu_nbp_mt(F_DF, b_neg); };
	const auto fb = [b_neg](realmtx_t& F_DF) {iM.dloglogu_nbp(F_DF, b_neg); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dloglogu_nbp, 100) {
		test_f_x_perf(fst, fmt, fb, "dloglogu_nbp", 100, i);
	}
}

TEST(TestMathNThr, LogLogU_nbn_nbp) {
	const auto fst = [](realmtx_t& X) { iM.loglogu_nbn_nbp_st(X); };
	const auto fmt = [](realmtx_t& X) {iM.loglogu_nbn_nbp_mt(X); };
	const auto fb = [](realmtx_t& X) {iM.loglogu_nbn_nbp(X); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::loglogu_nbn_nbp, 100) {
		test_f_x_perf(fst, fmt, fb, "loglogu_nbn_nbp", 100, i);
	}
}

TEST(TestMathNThr, DLogLogU_nbn_nbp) {
	const auto fst = [](realmtx_t& F_DF) { iM.dloglogu_nbn_nbp_st(F_DF); };
	const auto fmt = [](realmtx_t& F_DF) {iM.dloglogu_nbn_nbp_mt(F_DF); };
	const auto fb = [](realmtx_t& F_DF) {iM.dloglogu_nbn_nbp(F_DF); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dloglogu_nbn_nbp, 100) {
		test_f_x_perf(fst, fmt, fb, "dloglogu_nbn_nbp", 100, i);
	}
}
//////////////////////////////////////////////////////////////////////////
TEST(TestMathNThr, SoftSign) {
	constexpr real_t alpha = real_t(2.), c = real_t(1.6);
	const auto fst = [alpha,c](realmtx_t& X) { iM.softsign_st(X, alpha,c); };
	const auto fmt = [alpha, c](realmtx_t& X) {iM.softsign_mt(X, alpha,c); };
	const auto fb = [alpha, c](realmtx_t& X) {iM.softsign(X, alpha,c); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::softsign, 100) {
		test_f_x_perf(fst, fmt, fb, "softsign", 100, i);
	}
}
TEST(TestMathNThr, SoftSign_uc) {
	constexpr real_t alpha = real_t(2.);
	const auto fst = [alpha](realmtx_t& X) { iM.softsign_uc_st(X, alpha); };
	const auto fmt = [alpha](realmtx_t& X) {iM.softsign_uc_mt(X, alpha); };
	const auto fb = [alpha](realmtx_t& X) {iM.softsign_uc(X, alpha); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::softsign_uc, 100) {
		test_f_x_perf(fst, fmt, fb, "softsign_uc", 100, i);
	}
}
TEST(TestMathNThr, DSoftSign) {
	constexpr real_t alpha = real_t(2.), c = real_t(1.6);
	const auto fst = [alpha,c](realmtx_t& F_DF) { iM.dsoftsign_st(F_DF, alpha, c); };
	const auto fmt = [alpha, c](realmtx_t& F_DF) {iM.dsoftsign_mt(F_DF, alpha, c); };
	const auto fb = [alpha, c](realmtx_t& F_DF) {iM.dsoftsign(F_DF, alpha, c); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dsoftsign, 100) {
		test_f_x_perf<1000>(fst, fmt, fb, "dsoftsign", 100, i);
	}
}
TEST(TestMathNThr, DSoftSign_ua_uc) {
	const auto fst = [](realmtx_t& F_DF) { iM.dsoftsign_ua_uc_st(F_DF); };
	const auto fmt = [](realmtx_t& F_DF) {iM.dsoftsign_ua_uc_mt(F_DF); };
	const auto fb = [](realmtx_t& F_DF) {iM.dsoftsign_ua_uc(F_DF); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dsoftsign_ua_uc, 100) {
		test_f_x_perf<1000>(fst, fmt, fb, "dsoftsign_ua_uc", 100, i);
	}
}
TEST(TestMathNThr, SoftSigm) {
	constexpr real_t alpha = real_t(2.);
	const auto fst = [alpha](realmtx_t& X) { iM.softsigm_st(X, alpha); };
	const auto fmt = [alpha](realmtx_t& X) {iM.softsigm_mt(X, alpha); };
	const auto fb = [alpha](realmtx_t& X) {iM.softsigm(X, alpha); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::softsigm, 100) {
		test_f_x_perf(fst, fmt, fb, "softsigm", 100, i);
	}
}
TEST(TestMathNThr, DSoftSigm) {
	constexpr real_t alpha = real_t(2.);
	const auto fst = [alpha](realmtx_t& F_DF) { iM.dsoftsigm_st(F_DF, alpha); };
	const auto fmt = [alpha](realmtx_t& F_DF) {iM.dsoftsigm_mt(F_DF, alpha); };
	const auto fb = [alpha](realmtx_t& F_DF) {iM.dsoftsigm(F_DF, alpha); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dsoftsigm, 100) {
		test_f_x_perf<0>(fst, fmt, fb, "dsoftsigm", 100, i);
	}
}

TEST(TestMathNThr, dSoftSigmQuadLoss_dZ) {
	constexpr real_t alpha = real_t(2.);
	const auto fst = [alpha](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dSoftSigmQuadLoss_dZ_st(data_y, act_dLdZ, alpha); };
	const auto fmt = [alpha](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dSoftSigmQuadLoss_dZ_mt(data_y, act_dLdZ, alpha); };
	const auto fb = [alpha](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dSoftSigmQuadLoss_dZ(data_y, act_dLdZ, alpha); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dSoftSigmQuadLoss_dZ, 1) {
		test_dLdZ_perf<true>(fst, fmt, fb, "dSoftSigmQuadLoss_dZ", i, 1);
	}
}
TEST(TestMathNThr, dSoftSigmXEntropyLoss_dZ) {
	constexpr real_t alpha = real_t(2.);
	const auto fst = [alpha](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dSoftSigmXEntropyLoss_dZ_st(data_y, act_dLdZ, alpha); };
	const auto fmt = [alpha](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dSoftSigmXEntropyLoss_dZ_mt(data_y, act_dLdZ, alpha); };
	const auto fb = [alpha](const realmtx_t& data_y, realmtx_t& act_dLdZ) { iM.dSoftSigmXEntropyLoss_dZ(data_y, act_dLdZ, alpha); };

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::dSoftSigmXEntropyLoss_dZ, 1) {
		test_dLdZ_perf<true>(fst, fmt, fb, "dSoftSigmXEntropyLoss_dZ", i, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void test_adam_perf(const size_t epochs, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing Adam() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());
	
	realmtx_t dW_st(rowsCnt, colsCnt), Mt_st(rowsCnt, colsCnt), Vt_st(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_st.isAllocationFailed() && !Mt_st.isAllocationFailed() && !Vt_st.isAllocationFailed());
	realmtx_t dW_mt(rowsCnt, colsCnt), Mt_mt(rowsCnt, colsCnt), Vt_mt(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_mt.isAllocationFailed() && !Mt_mt.isAllocationFailed() && !Vt_mt.isAllocationFailed());
	realmtx_t dW_(rowsCnt, colsCnt), Mt_(rowsCnt, colsCnt), Vt_(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_.isAllocationFailed() && !Mt_.isAllocationFailed() && !Vt_.isAllocationFailed());

	const real_t beta1 = real_t(.9), beta2 = real_t(.999), learningRate = real_t(.001), numStab = real_t(1e-8);

	tictoc tSt, tMt, tB, tSt2, tMt2;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {
		Mt_st.zeros(); Mt_mt.zeros(); Mt_.zeros();
		Vt_st.zeros(); Vt_mt.zeros(); Vt_.zeros();

		real_t beta1t_st = real_t(1.), beta2t_st = real_t(1.);
		real_t beta1t_mt = real_t(1.), beta2t_mt = real_t(1.);
		real_t beta1t_ = real_t(1.), beta2t_ = real_t(1.);

		for (size_t e = 0; e < epochs; ++e) {
			rg.gen_matrix(dW_st, real_t(3.0));
			ASSERT_TRUE(dW_st.clone_to(dW_mt)); ASSERT_TRUE(dW_st.clone_to(dW_));

			tSt.tic();
			iM.Adam_st(dW_st, Mt_st, Vt_st, beta1t_st, beta2t_st, learningRate, beta1, beta2, numStab);
			tSt.toc();

			tMt.tic();
			iM.Adam_mt(dW_mt, Mt_mt, Vt_mt, beta1t_mt, beta2t_mt, learningRate, beta1, beta2, numStab);
			tMt.toc();

			tSt2.tic();
			iM.Adam_st(dW_st, Mt_st, Vt_st, beta1t_st, beta2t_st, learningRate, beta1, beta2, numStab);
			tSt2.toc();

			tMt2.tic();
			iM.Adam_mt(dW_mt, Mt_mt, Vt_mt, beta1t_mt, beta2t_mt, learningRate, beta1, beta2, numStab);
			tMt2.toc();

			tB.tic();
			iM.Adam(dW_, Mt_, Vt_, beta1t_, beta2t_, learningRate, beta1, beta2, numStab);
			tB.toc();
		}
	}

	tSt.say("st");
	tSt2.say("st2");
	tMt.say("mt");
	tMt2.say("mt2");

	tB.say("best");
}

TEST(TestMathNThr, Adam) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::Adam, 100) {
		test_adam_perf(10, i, 100);
	}

#ifndef TESTS_SKIP_LONGRUNNING
	//test_adam_perf(100000, 10);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void test_adamax_perf(const size_t epochs, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing AdaMax() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	realmtx_t dW_st(rowsCnt, colsCnt), Mt_st(rowsCnt, colsCnt), Vt_st(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_st.isAllocationFailed() && !Mt_st.isAllocationFailed() && !Vt_st.isAllocationFailed());
	realmtx_t dW_mt(rowsCnt, colsCnt), Mt_mt(rowsCnt, colsCnt), Vt_mt(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_mt.isAllocationFailed() && !Mt_mt.isAllocationFailed() && !Vt_mt.isAllocationFailed());
	realmtx_t dW_(rowsCnt, colsCnt), Mt_(rowsCnt, colsCnt), Vt_(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_.isAllocationFailed() && !Mt_.isAllocationFailed() && !Vt_.isAllocationFailed());

	const real_t beta1 = real_t(.9), beta2 = real_t(.999), learningRate = real_t(.001), numStab = real_t(1e-8);

	tictoc tSt, tMt, tB, tSt2, tMt2;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {
		Mt_st.zeros(); Mt_mt.zeros(); Mt_.zeros();
		Vt_st.zeros(); Vt_mt.zeros(); Vt_.zeros();

		real_t beta1t_st = real_t(1.), beta1t_mt = real_t(1.), beta1t_ = real_t(1.);

		for (size_t e = 0; e < epochs; ++e) {
			rg.gen_matrix(dW_st, real_t(3.0));
			ASSERT_TRUE(dW_st.clone_to(dW_mt)); ASSERT_TRUE(dW_st.clone_to(dW_));

			tSt.tic();
			iM.AdaMax_st(dW_st, Mt_st, Vt_st, beta1t_st, learningRate, beta1, beta2, numStab);
			tSt.toc();

			tMt.tic();
			iM.AdaMax_mt(dW_mt, Mt_mt, Vt_mt, beta1t_mt, learningRate, beta1, beta2, numStab);
			tMt.toc();

			tSt2.tic();
			iM.AdaMax_st(dW_st, Mt_st, Vt_st, beta1t_st, learningRate, beta1, beta2, numStab);
			tSt2.toc();

			tMt2.tic();
			iM.AdaMax_mt(dW_mt, Mt_mt, Vt_mt, beta1t_mt, learningRate, beta1, beta2, numStab);
			tMt2.toc();

			tB.tic();
			iM.AdaMax(dW_, Mt_, Vt_, beta1t_, learningRate, beta1, beta2, numStab);
			tB.toc();
		}
	}

	tSt.say("st");
	tSt2.say("st2");
	tMt.say("mt");
	tMt2.say("mt2");

	tB.say("best");
}

TEST(TestMathNThr, AdaMax) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::AdaMax, 100) {
		test_adamax_perf(10, i, 100);
	}

#ifndef TESTS_SKIP_LONGRUNNING
	//test_adamax_perf(100000, 10);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void test_Nadam_perf(const size_t epochs, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing Nadam() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	realmtx_t dW_st(rowsCnt, colsCnt), Mt_st(rowsCnt, colsCnt), Vt_st(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_st.isAllocationFailed() && !Mt_st.isAllocationFailed() && !Vt_st.isAllocationFailed());
	realmtx_t dW_mt(rowsCnt, colsCnt), Mt_mt(rowsCnt, colsCnt), Vt_mt(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_mt.isAllocationFailed() && !Mt_mt.isAllocationFailed() && !Vt_mt.isAllocationFailed());
	realmtx_t dW_(rowsCnt, colsCnt), Mt_(rowsCnt, colsCnt), Vt_(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_.isAllocationFailed() && !Mt_.isAllocationFailed() && !Vt_.isAllocationFailed());

	const real_t mu = real_t(.9), eta = real_t(.999), learningRate = real_t(.001), numStab = real_t(1e-8);
	const real_t _g = real_t(0.);

	tictoc tSt, tMt, tB, tSt2, tMt2;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {
		Mt_st.zeros(); Mt_mt.zeros(); Mt_.zeros();
		Vt_st.zeros(); Vt_mt.zeros(); Vt_.zeros();

		real_t beta1t_st = real_t(1.), beta2t_st = real_t(1.);
		real_t beta1t_mt = real_t(1.), beta2t_mt = real_t(1.);
		real_t beta1t_ = real_t(1.), beta2t_ = real_t(1.);

		for (size_t e = 0; e < epochs; ++e) {
			rg.gen_matrix(dW_st, real_t(3.0));
			ASSERT_TRUE(dW_st.clone_to(dW_mt)); ASSERT_TRUE(dW_st.clone_to(dW_));

			tSt.tic();
			iM.RNadam_st(dW_st, Mt_st, Vt_st, beta1t_st, beta2t_st, learningRate, mu, eta, _g, numStab);
			tSt.toc();

			tMt.tic();
			iM.RNadam_mt(dW_mt, Mt_mt, Vt_mt, beta1t_mt, beta2t_mt, learningRate, mu, eta, _g, numStab);
			tMt.toc();

			tSt2.tic();
			iM.RNadam_st(dW_st, Mt_st, Vt_st, beta1t_st, beta2t_st, learningRate, mu, eta, _g, numStab);
			tSt2.toc();

			tMt2.tic();
			iM.RNadam_mt(dW_mt, Mt_mt, Vt_mt, beta1t_mt, beta2t_mt, learningRate, mu, eta, _g, numStab);
			tMt2.toc();

			tB.tic();
			iM.RNadam(dW_, Mt_, Vt_, beta1t_, beta2t_, learningRate, mu, eta, _g, numStab);
			tB.toc();
		}
	}

	tSt.say("st");
	tSt2.say("st2");
	tMt.say("mt");
	tMt2.say("mt2");

	tB.say("best");
}

TEST(TestMathNThr, Nadam) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::RNadam, 100) {
		test_Nadam_perf(10, i, 100);
	}

#ifndef TESTS_SKIP_LONGRUNNING
	//test_Nadam_perf(100000, 10);
#endif
}

void test_Radam_perf(const size_t epochs, vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing Radam() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	realmtx_t dW_st(rowsCnt, colsCnt), Mt_st(rowsCnt, colsCnt), Vt_st(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_st.isAllocationFailed() && !Mt_st.isAllocationFailed() && !Vt_st.isAllocationFailed());
	realmtx_t dW_mt(rowsCnt, colsCnt), Mt_mt(rowsCnt, colsCnt), Vt_mt(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_mt.isAllocationFailed() && !Mt_mt.isAllocationFailed() && !Vt_mt.isAllocationFailed());
	realmtx_t dW_(rowsCnt, colsCnt), Mt_(rowsCnt, colsCnt), Vt_(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW_.isAllocationFailed() && !Mt_.isAllocationFailed() && !Vt_.isAllocationFailed());

	const real_t mu = real_t(.9), eta = real_t(.999), learningRate = real_t(.001), numStab = real_t(1e-8);
	const real_t gamma = real_t(0.1);

	tictoc tSt, tMt, tB, tSt2, tMt2;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {
		Mt_st.zeros(); Mt_mt.zeros(); Mt_.zeros();
		Vt_st.zeros(); Vt_mt.zeros(); Vt_.zeros();

		real_t beta1t_st = real_t(1.), beta2t_st = real_t(1.);
		real_t beta1t_mt = real_t(1.), beta2t_mt = real_t(1.);
		real_t beta1t_ = real_t(1.), beta2t_ = real_t(1.);

		for (size_t e = 0; e < epochs; ++e) {
			rg.gen_matrix(dW_st, real_t(3.0));
			ASSERT_TRUE(dW_st.clone_to(dW_mt)); ASSERT_TRUE(dW_st.clone_to(dW_));

			tSt.tic();
			iM.RNadam_st(dW_st, Mt_st, Vt_st, beta1t_st, beta2t_st, learningRate, mu, eta, gamma, numStab);
			tSt.toc();

			tMt.tic();
			iM.RNadam_mt(dW_mt, Mt_mt, Vt_mt, beta1t_mt, beta2t_mt, learningRate, mu, eta, gamma, numStab);
			tMt.toc();

			tSt2.tic();
			iM.RNadam_st(dW_st, Mt_st, Vt_st, beta1t_st, beta2t_st, learningRate, mu, eta, gamma, numStab);
			tSt2.toc();

			tMt2.tic();
			iM.RNadam_mt(dW_mt, Mt_mt, Vt_mt, beta1t_mt, beta2t_mt, learningRate, mu, eta, gamma, numStab);
			tMt2.toc();

			tB.tic();
			iM.RNadam(dW_, Mt_, Vt_, beta1t_, beta2t_, learningRate, mu, eta, gamma, numStab);
			tB.toc();
		}
	}

	tSt.say("st");
	tSt2.say("st2");
	tMt.say("mt");
	tMt2.say("mt2");

	tB.say("best");
}

TEST(TestMathNThr, Radam) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::RNadam, 100) {
		test_Radam_perf(10, i, 100);
	}

#ifndef TESTS_SKIP_LONGRUNNING
	//test_Radam_perf(100000, 10);
#endif
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void test_ewBinarize_ip_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10, const real_t frac = .5) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing ewBinarize_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) with frac=" << frac << " ****");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	realmtx_t A(rowsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());
	tictoc tSt, tMt, tB/*, dt, t1, t2, dt1, dt2*/;
	real_t vv = real_t(0);
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix_norm(A);
		tSt.tic();
		iM.ewBinarize_ip_st(A, frac);
		tSt.toc();
		for (const auto& e : A) vv += e;

		/*rg.gen_matrix_norm(A);
		t1.tic();
		iM.ex_ewBinarize_ip_st(A, frac);
		t1.toc();

		rg.gen_matrix_norm(A);
		t2.tic();
		iM.ex2_ewBinarize_ip_st(A, frac);
		t2.toc();*/

		/*rg.gen_matrix_norm(A);
		dt.tic();
		iM.ewBinarize_ip_st(A, frac);
		dt.toc();*/

		/*rg.gen_matrix_norm(A);
		dt1.tic();
		iM.ex_ewBinarize_ip_st(A, frac);
		dt1.toc();

		rg.gen_matrix_norm(A);
		dt2.tic();
		iM.ex2_ewBinarize_ip_st(A, frac);
		dt2.toc();*/

		rg.gen_matrix_norm(A);
		tMt.tic();
		iM.ewBinarize_ip_mt(A, frac);
		tMt.toc();
		for (const auto& e : A) vv += e;

		rg.gen_matrix_norm(A);
		tB.tic();
		iM.ewBinarize_ip(A, frac);
		tB.toc();
		for (const auto& e : A) vv += e;
	}
	tSt.say("st");
	//dt.say("st");
	//t1.say("ex");
	//dt1.say("ex");
	//t2.say("ex2");
	//dt2.say("ex2");

	tMt.say("mt");
	tB.say("best");
	STDCOUTL(vv);
}

TEST(TestMathNThr, ewBinarizeIp) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::ewBinarize_ip, 100) {
		test_ewBinarize_ip_perf(i, 100, .5);
		//test_ewBinarize_ip_perf(i, 100, .1);
		//test_ewBinarize_ip_perf(i, 100, .9);
	}

// #ifndef TESTS_SKIP_LONGRUNNING
// 	test_ewBinarize_ip_perf(100000, 10, .5);
// #endif
}


void test_ewBinarize_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10, const real_t frac = .5) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing ewBinarize() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) with frac=" << frac << " ****");

	typedef math::smatrix<char> binmtx_t;

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	realmtx_t A(rowsCnt, colsCnt);
	binmtx_t Dest(rowsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed() && !Dest.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());
	tictoc tSt, tMt, tB;
	size_t vv = 0;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix_norm(A);
		tSt.tic();
		iM.ewBinarize_st(Dest, A, frac);
		tSt.toc();
		for (const auto& e : Dest) vv += e;
		
		rg.gen_matrix_norm(A);
		tMt.tic();
		iM.ewBinarize_mt(Dest, A, frac);
		tMt.toc();
		for (const auto& e : Dest) vv += e;

		rg.gen_matrix_norm(A);
		tB.tic();
		iM.ewBinarize(Dest, A, frac);
		tB.toc();
		for (const auto& e : Dest) vv += e;
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");
	STDCOUTL(vv);
}

TEST(TestMathNThr, ewBinarize) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::ewBinarize, 100) {
		test_ewBinarize_perf(i, 100, .5);
		//test_ewBinarize_perf(i, 100, .1);
		//test_ewBinarize_perf(i, 100, .9);
	}
// 
// #ifndef TESTS_SKIP_LONGRUNNING
// 	test_ewBinarize_perf(100000, 10, .5);
// #endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void test_softmax_parts_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing softmax_parts() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	constexpr numel_cnt_t maxDataSizeForSt = 50000;

	realmtx_t A(rowsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed());
	const auto denominatorElmsMax = realmtx_t::sNumel(rowsCnt, iM.ithreads().workers_count());
	::std::vector<real_t> vec_max(rowsCnt), vec_den(denominatorElmsMax), vec_num(dataSize);

	iM.preinit(dataSize);
	ASSERT_TRUE(iM.init());
	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	tictoc tStRw, tStCw, tSt, tMtCw, tMtRw, tMt, tB;
	//////////////////////////////////////////////////////////////////////////
	//testing performance
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, iThreads_t> pw(iM.ithreads());

	//FFFFfffffffff... don't ever think about removing rg. calls that randomizes data...
	for (unsigned r = 0; r < maxReps; ++r) {
		if (dataSize < maxDataSizeForSt) {
			rg.gen_matrix(A, 2);
			mrwMax_ET(A, &vec_max[0]);
			tStRw.tic();
			iM.softmax_parts_st_rw(A, &vec_max[0], &vec_den[0], &vec_num[0]);
			tStRw.toc();

			rg.gen_matrix(A, 2);
			mrwMax_ET(A, &vec_max[0]);
			tStCw.tic();
			iM.softmax_parts_st_cw(A, &vec_max[0], &vec_den[0], &vec_num[0]);
			tStCw.toc();
			
			rg.gen_matrix(A, 2);
			mrwMax_ET(A, &vec_max[0]);
			tSt.tic();
			iM.softmax_parts_st(A, &vec_max[0], &vec_den[0], &vec_num[0]);
			tSt.toc();
		}

		if (colsCnt > imath_basic_t::Thresholds_t::softmax_parts_mt_cw_ColsPerThread) {
			rg.gen_matrix(A, 2);
			mrwMax_ET(A, &vec_max[0]);
			tMtCw.tic();
			iM.softmax_parts_mt_cw(A, &vec_max[0], &vec_den[0], &vec_num[0]);
			tMtCw.toc();
		}
		
		rg.gen_matrix(A, 2);
		mrwMax_ET(A, &vec_max[0]);
		tMtRw.tic();
		iM.softmax_parts_mt_rw(A, &vec_max[0], &vec_den[0], &vec_num[0]);
		tMtRw.toc();
		
		rg.gen_matrix(A, 2);
		mrwMax_ET(A, &vec_max[0]);
		tMt.tic();
		iM.softmax_parts_mt(A, &vec_max[0], &vec_den[0], &vec_num[0]);
		tMt.toc();

		rg.gen_matrix(A, 2);
		mrwMax_ET(A, &vec_max[0]);
		tB.tic();
		iM.softmax_parts(A, &vec_max[0], &vec_den[0], &vec_num[0]);
		tB.toc();
	}
	tStCw.say("st_cw");
	tStRw.say("st_rw");
	tSt.say("st");
	tMtCw.say("mt_cw");
	tMtRw.say("mt_rw");
	tMt.say("mt");
	tB.say("best");
}
TEST(TestMathNThr, SoftmaxParts) {
	test_softmax_parts_perf(100, 50);
	test_softmax_parts_perf(1000, 50);

#ifndef TESTS_SKIP_LONGRUNNING
	constexpr vec_len_t maxCol = 10;
	//for (unsigned c = 2; c <= maxCol; ++c)test_softmax_parts_perf(100, c);
	for (unsigned c = 2; c <= maxCol; ++c)test_softmax_parts_perf(200, c);
	test_softmax_parts_perf(200, 100);

// 	test_softmax_parts_perf(10000, 2);
// 	test_softmax_parts_perf(10000, imath_basic_t::Thresholds_t::softmax_parts_mt_cw_ColsPerThread);
// 	test_softmax_parts_perf(10000, imath_basic_t::Thresholds_t::softmax_parts_mt_cw_ColsPerThread + 1);
// 	test_softmax_parts_perf(100000, 10);

	test_softmax_parts_perf(imath_basic_t::Thresholds_t::softmax_parts_mt_rows, 2);
	test_softmax_parts_perf(imath_basic_t::Thresholds_t::softmax_parts_mt_rows, imath_basic_t::Thresholds_t::softmax_parts_mt_cw_ColsPerThread);
	test_softmax_parts_perf(imath_basic_t::Thresholds_t::softmax_parts_mt_rows, imath_basic_t::Thresholds_t::softmax_parts_mt_cw_ColsPerThread + 1);
	test_softmax_parts_perf(imath_basic_t::Thresholds_t::softmax_parts_mt_rows, 30);

	test_softmax_parts_perf(imath_basic_t::Thresholds_t::softmax_parts_mt_rows + 10, 2);
	test_softmax_parts_perf(imath_basic_t::Thresholds_t::softmax_parts_mt_rows + 10, imath_basic_t::Thresholds_t::softmax_parts_mt_cw_ColsPerThread);
	test_softmax_parts_perf(imath_basic_t::Thresholds_t::softmax_parts_mt_rows + 10, imath_basic_t::Thresholds_t::softmax_parts_mt_cw_ColsPerThread + 1);
	test_softmax_parts_perf(imath_basic_t::Thresholds_t::softmax_parts_mt_rows + 10, 30);
	
	//test_softmax_parts_perf(100000, 10);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void test_softmax_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing softmax() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	constexpr numel_cnt_t maxDataSizeForSt = 50000;

	realmtxdef_t A(rowsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed());
	
	iM.preinit(iM.softmax_needTempMem(A));
	ASSERT_TRUE(iM.init());
	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	tictoc tSt, tMt, tB;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, iThreads_t> pw(iM.ithreads());
	//FFFFfffffffff... don't ever think about removing rg. calls that randomizes data...
	for (unsigned r = 0; r < maxReps; ++r) {
		if (dataSize < maxDataSizeForSt) {
			rg.gen_matrix(A, 10);
			tSt.tic();
			iM.softmax_st(A);
			tSt.toc();
		}

		rg.gen_matrix(A, 10);
		tMt.tic();
		iM.softmax_mt(A);
		tMt.toc();

		rg.gen_matrix(A, 10);
		tB.tic();
		iM.softmax(A);
		tB.toc();
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");
}
TEST(TestMathNThr, Softmax) {
	test_softmax_perf(100, 10);
	test_softmax_perf(100, 30);
	test_softmax_perf(200, 10);
	test_softmax_perf(200, 30);

#ifndef TESTS_SKIP_LONGRUNNING
	test_softmax_perf(60000, 10);
	test_softmax_perf(50000, 50);

	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::softmax, 10) test_softmax_perf(i, 10);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void test_loss_softmax_xentropy_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing loss_softmax_xentropy() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	constexpr numel_cnt_t maxDataSizeForSt = 50000;
	realmtx_t A(rowsCnt, colsCnt), Y(rowsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed() && !Y.isAllocationFailed());
	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	real_t lst(0), lmt, lb;
	tictoc tSt, tMt, tB;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, iThreads_t> pw(iM.ithreads());
	//FFFFfffffffff... don't ever think about removing rg. calls that randomizes data...
	for (unsigned r = 0; r < maxReps; ++r) {
		if (dataSize < maxDataSizeForSt) {
			rg.gen_matrix_norm(A);			rg.gen_matrix_norm(Y);
			tSt.tic();
			lst=iM.loss_softmax_xentropy_st(A, Y);
			tSt.toc();
		}

		rg.gen_matrix_norm(A);			rg.gen_matrix_norm(Y);
		tMt.tic();
		lmt=iM.loss_softmax_xentropy_mt(A, Y);
		tMt.toc();

		rg.gen_matrix_norm(A);			rg.gen_matrix_norm(Y);
		tB.tic();
		lb=iM.loss_softmax_xentropy(A, Y);
		tB.toc();
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");
	STDCOUTL("st=" << lst << " lmt=" << lmt << " lb=" << lb);
}
TEST(TestMathNThr, LossSoftmaxXentropy) {
	test_loss_softmax_xentropy_perf(100, 10);

#ifndef TESTS_SKIP_LONGRUNNING
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::loss_softmax_xentropy, 10) test_loss_softmax_xentropy_perf(i, 10);

// 	test_loss_softmax_xentropy_perf(60000, 10);
// 	test_loss_softmax_xentropy_perf(50000, 50);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void test_loss_xentropy_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing loss_xentropy() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	const real_t frac = .5;
	realmtx_t A(rowsCnt, colsCnt), Y(rowsCnt, colsCnt);
	real_t loss = 0;
	ASSERT_TRUE(!A.isAllocationFailed() && !Y.isAllocationFailed());

	iM.preinit(dataSize);
	ASSERT_TRUE(iM.init());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	tictoc tSt, tMt, tB;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix_norm(A);		rg.gen_matrix_norm(Y); iM.ewBinarize_ip(Y, frac);
		tSt.tic();
		loss += iM.loss_xentropy_st(A, Y);
		tSt.toc();

		rg.gen_matrix_norm(A);		rg.gen_matrix_norm(Y); iM.ewBinarize_ip(Y, frac);
		tMt.tic();
		loss += iM.loss_xentropy_mt(A, Y);
		tMt.toc();

		rg.gen_matrix_norm(A);		rg.gen_matrix_norm(Y); iM.ewBinarize_ip(Y, frac);
		tB.tic();
		loss += iM.loss_xentropy(A, Y);
		tB.toc();
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");
	STDCOUTL("l=" << loss);
}
/*
void test_loss_xentropy_perf_ns(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing loss_xentropy_ns() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	const real_t frac = .5;
	realmtx_t A(rowsCnt, colsCnt), Y(rowsCnt, colsCnt);
	real_t loss = 0;
	ASSERT_TRUE(!A.isAllocationFailed() && !Y.isAllocationFailed());

	iM.preinit(dataSize);
	ASSERT_TRUE(iM.init());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	tictoc tSt, tMt, tB;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix_norm(A);		rg.gen_matrix_norm(Y); iM.ewBinarize_ip(Y, frac);
		tSt.tic();
		loss += iM.loss_xentropy_ns_st(A, Y);
		tSt.toc();

		rg.gen_matrix_norm(A);		rg.gen_matrix_norm(Y); iM.ewBinarize_ip(Y, frac);
		tMt.tic();
		loss += iM.loss_xentropy_ns_mt(A, Y);
		tMt.toc();

		rg.gen_matrix_norm(A);		rg.gen_matrix_norm(Y); iM.ewBinarize_ip(Y, frac);
		tB.tic();
		loss += iM.loss_xentropy_ns(A, Y);
		tB.toc();
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");
	STDCOUTL("l=" << loss);
}*/
TEST(TestMathNThr, lossXentropy) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::loss_xentropy, 1) test_loss_xentropy_perf(i, 1);
	//NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::loss_xentropy_ns, 1) test_loss_xentropy_perf_ns(i, 1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void test_ApplyILR_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing apply_ILR() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) **************");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	constexpr numel_cnt_t maxDataSizeForSt = 10000;

	real_t decr = real_t(.8), incr = real_t(1.3), capH = real_t(9.9), capL = real_t(0.1);

	realmtx_t dW(rowsCnt, colsCnt), prevdW(rowsCnt, colsCnt), gain(rowsCnt, colsCnt);
	ASSERT_TRUE(!dW.isAllocationFailed() && !prevdW.isAllocationFailed() && !gain.isAllocationFailed());

	iM.preinit(dataSize);
	ASSERT_TRUE(iM.init());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());
	rg.gen_matrix(prevdW, 10);

	
	tictoc tStN, tStV, tMtN, tMtV, tB;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, iThreads_t> pw(iM.ithreads());
	
	for (unsigned r = 0; r < maxReps; ++r) {
		if (dataSize < maxDataSizeForSt) {
			rg.gen_matrix(dW, 10);
			rg.gen_matrix_gtz(gain, 10);
			tStN.tic();
			iM.apply_ILR_st_naive(dW, prevdW, gain, decr, incr, capL, capH);
			tStN.toc();

			rg.gen_matrix(dW, 10);
			rg.gen_matrix_gtz(gain, 10);
			tStV.tic();
			iM.apply_ILR_st_vec(dW, prevdW, gain, decr, incr, capL, capH);
			tStV.toc();
		}

		rg.gen_matrix(dW, 10);
		rg.gen_matrix_gtz(gain, 10);
		tMtN.tic();
		iM.apply_ILR_mt_naive(dW, prevdW, gain, decr, incr, capL, capH);
		tMtN.toc();

		rg.gen_matrix(dW, 10);
		rg.gen_matrix_gtz(gain, 10);
		tMtV.tic();
		iM.apply_ILR_mt_vec(dW, prevdW, gain, decr, incr, capL, capH);
		tMtV.toc();

		rg.gen_matrix(dW, 10);
		rg.gen_matrix_gtz(gain, 10);
		tB.tic();		
		iM.apply_ILR(dW, prevdW, gain, decr, incr, capL, capH);
		tB.toc();
	}
	tStN.say("st_naive");
	tStV.say("st_vec");
	tMtN.say("mt_naive");
	tMtV.say("mt_vec");
	tB.say("best");
}
TEST(TestMathNThr, ApplyILR) {
	//vec is faster until about 2620, after that - naive
	//NNTL_RUN_TEST4(2620, 3, 1, 10) test_ApplyILR_perf(i, 10);

	//mt threshold is at about 9000 (on avg. By minimum thrsh it's lowered to about 8500
	/*for (vec_len_t ci = 2; ci <= 24; ci += 2) {
		NNTL_RUN_TEST4(9000, 3, 1, ci) test_ApplyILR_perf(i, ci);
	}*/
	//for mt vec is better up to approx 120000. Then naive rules up to 261000

	/*for (size_t ds = 260000; ds <= 265000; ds += 1000) {
		//for (vec_len_t ci = 2; ci <= 38; ci += 6) {
			test_ApplyILR_perf(ds/10, 10);
		//}
	}*/

//#ifndef TESTS_SKIP_LONGRUNNING
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::apply_ILR_st_vec, 10) test_ApplyILR_perf(i, 10);
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::apply_ILR_mt, 10) test_ApplyILR_perf(i, 10);
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::apply_ILR_mt_vec, 10) test_ApplyILR_perf(i, 10);
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::apply_ILR_mt_vec2, 10) test_ApplyILR_perf(i, 10);
//#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
void test_mColumnsCov_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing mColumnsCov() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	
	realmtx_t A(rowsCnt, colsCnt), C(colsCnt, colsCnt);
	ASSERT_TRUE(!A.isAllocationFailed() && !C.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	tictoc tUpr, tLwr;
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, iThreads_t> pw(iM.ithreads());
	//FFFFfffffffff... don't ever think about removing rg. calls that randomizes data...
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(A,5);
		tUpr.tic();
		iM.mColumnsCov(A, C, false);
		tUpr.toc();

		rg.gen_matrix(A, 5);
		tLwr.tic();
		iM.mColumnsCov(A, C, true);
		tLwr.toc();
	}
	tUpr.say("Upr");
	tLwr.say("Lwr");
}
TEST(TestMathNThr, mColumnsCov) {
	test_mColumnsCov_perf(100, 10);

	test_mColumnsCov_perf(1000, 100);
	test_mColumnsCov_perf(100, 1000);

	test_mColumnsCov_perf(1000, 10);
	test_mColumnsCov_perf(10, 1000);

	test_mColumnsCov_perf(10000, 10);
	test_mColumnsCov_perf(10000, 100);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void test_make_alphaDropout_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10, const real_t dpa = .5) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing make_alphaDropout() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) with dpa=" << dpa << " ****");
	ASSERT_TRUE(dpa > 0 && dpa < 1);

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	realmtx_t A(rowsCnt, colsCnt, true), DM(rowsCnt, colsCnt);
	const real_t a = real_t(2), b = real_t(-3), c = real_t(4);

	ASSERT_TRUE(!A.isAllocationFailed() && !DM.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	const auto pA = A.data(), pDM = DM.data();

	real_t t = real_t(0);
	utils::tictoc tSt, tMt, tB;

	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {

		rg.gen_matrix_no_bias(A, real_t(5));
		rg.gen_matrix_norm(DM);
		tSt.tic();
		iM.make_alphaDropout_st(A, dpa, a, b, c, DM);
		tSt.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i] + pDM[i];

		rg.gen_matrix_no_bias(A, real_t(5));
		rg.gen_matrix_norm(DM);
		tMt.tic();
		iM.make_alphaDropout_mt(A, dpa, a, b, c, DM);
		tMt.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i] + pDM[i];

		rg.gen_matrix_no_bias(A, real_t(5));
		rg.gen_matrix_norm(DM);
		tB.tic();
		iM.make_alphaDropout(A, dpa, a, b, c, DM);
		tB.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i] + pDM[i];
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");

	STDCOUTL(t);
}
TEST(TestMathNThr, make_alphaDropout) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::make_alphaDropout, 100) {
		test_make_alphaDropout_perf(100, i, real_t(.5));
	}
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::make_alphaDropout, 100) {
		test_make_alphaDropout_perf(100, i, real_t(.8));
	}
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::make_alphaDropout, 100) {
		test_make_alphaDropout_perf(100, i, real_t(.9));
	}

#ifndef TESTS_SKIP_LONGRUNNING
	test_make_alphaDropout_perf(10000, 10, real_t(.8));
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void test_evSubMtxMulC_ip_nb_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing evSubMtxMulC_ip_nb() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements) ****");
	
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	realmtx_t A(rowsCnt, colsCnt, true), mB(rowsCnt, colsCnt);
	const real_t c = real_t(2);

	ASSERT_TRUE(!A.isAllocationFailed() && !mB.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	const auto pA = A.data();

	real_t t = real_t(0);
	tictoc tSt, tMt, tB;

	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {

		rg.gen_matrix_no_bias(A, real_t(5));
		rg.gen_matrix(mB, real_t(5));
		tSt.tic();
		iM.evSubMtxMulC_ip_nb_st(A, mB, c);
		tSt.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i];

		rg.gen_matrix_no_bias(A, real_t(5));
		rg.gen_matrix(mB, real_t(5));
		tMt.tic();
		iM.evSubMtxMulC_ip_nb_mt(A, mB, c);
		tMt.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i];

		rg.gen_matrix_no_bias(A, real_t(5));
		rg.gen_matrix(mB, real_t(5));
		tB.tic();
		iM.evSubMtxMulC_ip_nb(A, mB, c);
		tB.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i];
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");

	STDCOUTL(t);
}

TEST(TestMathNThr, evSubMtxMulC_ip_nb) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::evSubMtxMulC_ip_nb, 100) {
		test_evSubMtxMulC_ip_nb_perf(i, 100);
	}

#ifndef TESTS_SKIP_LONGRUNNING
	test_evSubMtxMulC_ip_nb_perf(10000, 10);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void test_evAddScaled_ip_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing evAddScaled_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements)");
	
	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	realmtx_t A(rowsCnt, colsCnt), B(rowsCnt, colsCnt);
	const real_t c = real_t(4);

	ASSERT_TRUE(!A.isAllocationFailed() && !B.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	const auto pA = A.data();

	real_t t = real_t(0);
	tictoc tSt, tMt, tB;

	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {

		rg.gen_matrix(A, real_t(2)); rg.gen_matrix(B, real_t(3));
		tSt.tic();
		iM.evAddScaled_ip_st(A, c, B);
		tSt.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i];

		rg.gen_matrix(A, real_t(2)); rg.gen_matrix(B, real_t(3));
		tMt.tic();
		iM.evAddScaled_ip_mt(A, c, B);
		tMt.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i];

		rg.gen_matrix(A, real_t(2)); rg.gen_matrix(B, real_t(3));
		tB.tic();
		iM.evAddScaled_ip(A, c, B);
		tB.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i];
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");

	STDCOUTL(t);
}

TEST(TestMathNThr, evAddScaled_ip) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::evAddScaled_ip, 10) {
		test_evAddScaled_ip_perf(i, 10);
	}
}

void test_evNZAddScaled_ip_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10, const real_t dpa=0.95) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing evNZAddScaled_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements)");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	realmtx_t A(rowsCnt, colsCnt), B(rowsCnt, colsCnt), M(rowsCnt, colsCnt);
	const real_t c = real_t(4);

	ASSERT_TRUE(!A.isAllocationFailed() && !B.isAllocationFailed() && !M.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	const auto pA = A.data();

	real_t t = real_t(0);
	tictoc tSt, tMt, tB;

	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {

		rg.gen_matrix(A, real_t(2)); rg.gen_matrix(B, real_t(3));
		rg.gen_matrix_norm(M);
		iM.ewBinarize_ip(M, dpa);
		iM.evMul_ip(A, M);		
		tSt.tic();
		iM.evNZAddScaled_ip_st(A, c, B);
		tSt.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i];

		rg.gen_matrix(A, real_t(2)); rg.gen_matrix(B, real_t(3));
		rg.gen_matrix_norm(M);
		iM.ewBinarize_ip(M, dpa);
		iM.evMul_ip(A, M);
		tMt.tic();
		iM.evNZAddScaled_ip_mt(A, c, B);
		tMt.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i];

		rg.gen_matrix(A, real_t(2)); rg.gen_matrix(B, real_t(3));
		rg.gen_matrix_norm(M);
		iM.ewBinarize_ip(M, dpa);
		iM.evMul_ip(A, M);
		tB.tic();
		iM.evNZAddScaled_ip(A, c, B);
		tB.toc();
		for (numel_cnt_t i = 0; i < dataSize; ++i) t += pA[i];
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");

	STDCOUTL(t);
}

TEST(TestMathNThr, evNZAddScaled_ip) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::evNZAddScaled_ip, 10) {
		test_evNZAddScaled_ip_perf(i, 10);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void test_evMul_ip_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("**** testing evMul_ip() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements)");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;
	realmtx_t A(rowsCnt, colsCnt), B(rowsCnt, colsCnt);
	const real_t c = real_t(4);

	ASSERT_TRUE(!A.isAllocationFailed() && !B.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	real_t t = real_t(0);
	tictoc tSt, tMt, tB;

	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());
	for (unsigned r = 0; r < maxReps; ++r) {

		rg.gen_matrix(A, real_t(2)); rg.gen_matrix(B, real_t(3));
		tSt.tic();
		iM.evMul_ip_st(A, B);
		tSt.toc();
		for (const auto& e : A) t += e;

		rg.gen_matrix(A, real_t(2)); rg.gen_matrix(B, real_t(3));
		tMt.tic();
		iM.evMul_ip_mt(A, B);
		tMt.toc();
		for (const auto& e : A) t += e;

		rg.gen_matrix(A, real_t(2)); rg.gen_matrix(B, real_t(3));
		tB.tic();
		iM.evMul_ip(A, B);
		tB.toc();
		for (const auto& e : A) t += e;
	}
	tSt.say("st");
	tMt.say("mt");
	tB.say("best");

	STDCOUTL(t);
}

TEST(TestMathNThr, evMul_ip) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::evMul_ip, 10) {
		test_evMul_ip_perf(i, 10);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<bool bWsort>
void test_mExtractRows_perf(vec_len_t rowsCnt, vec_len_t colsCnt, vec_len_t extrCnt) {
	typedef ::std::vector<realmtx_t::vec_len_t> vec_t;

	if (rowsCnt < extrCnt) extrCnt = rowsCnt;

	realmtx_t src(rowsCnt, colsCnt), dest(extrCnt, colsCnt);
	ASSERT_TRUE(!src.isAllocationFailed() && !dest.isAllocationFailed());
	vec_t vec(extrCnt);

	STDCOUTL("******* testing mExtractRows() over " << rowsCnt << "x" << colsCnt << " matrix (" << src.numel()
		<< " elems) ExtractRows=" << extrCnt << " -> " << dest.numel() << " elems *********");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	tictoc tS, tM, tB;
	//testing performance
	threads::prioritize_workers<threads::PriorityClass::PerfTesting, typename imath_basic_t::iThreads_t> pw(iM.ithreads());
	real_t v = real_t(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix(src, real_t(100));
		rg.gen_vector_gtz(&vec[0], vec.size(), rowsCnt - 1);
		tS.tic();
		if (bWsort) {
			::std::sort(vec.begin(), vec.end());
		}
		iM.mExtractRows_seqWrite_st(src, vec.begin(), dest);
		tS.toc();
		for (const auto& e : dest) v += e;
		v = ::std::log(::std::abs(v));

		rg.gen_matrix(src, real_t(100));
		rg.gen_vector_gtz(&vec[0], vec.size(), rowsCnt - 1);
		tM.tic();
		if (bWsort) {
			::std::sort(vec.begin(), vec.end());
		}
		iM.mExtractRows_seqWrite_mt(src, vec.begin(), dest);
		tM.toc();
		for (const auto& e : dest) v += e;
		v = ::std::log(::std::abs(v));

		rg.gen_matrix(src, real_t(100));
		rg.gen_vector_gtz(&vec[0], vec.size(), rowsCnt - 1);
		tB.tic();
		if (bWsort) {
			::std::sort(vec.begin(), vec.end());
		}
		iM.mExtractRows(src, vec.begin(), dest);
		tB.toc();
		for (const auto& e : dest) v += e;
		v = ::std::log(::std::abs(v));
	}
	tS.say("st");
	tM.say("mt");
	tB.say("()");
	STDCOUTL(v);
}

TEST(TestMathNThr, mExtractRowsPerf) {
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::mExtractRows, 100) test_mExtractRows_perf<false>(i, 100, 100);
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::mExtractRows, 20) test_mExtractRows_perf<false>(i, 20, 100);
}

//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
void test_make_dropout_perf(vec_len_t rowsCnt, vec_len_t colsCnt = 10, const real_t dpa = real_t(.5)) {
	const auto dataSize = realmtx_t::sNumel(rowsCnt, colsCnt);
	STDCOUTL("******* testing make_dropout() over " << rowsCnt << "x" << colsCnt << " matrix (" << dataSize << " elements), dpa = "
		<< dpa << " **************");

	constexpr unsigned maxReps = TEST_PERF_REPEATS_COUNT;

	realmtx_t act(rowsCnt, colsCnt, true), dm(rowsCnt, colsCnt);
	ASSERT_TRUE(!act.isAllocationFailed() && !dm.isAllocationFailed());

	d_interfaces::iRng_t rg;
	rg.init_ithreads(iM.ithreads());

	threads::prioritize_workers<threads::PriorityClass::PerfTesting, imath_basic_t::iThreads_t> pw(iM.ithreads());

	utils::tictoc tS, tM, tB;

	real_t v = real_t(0);
	for (unsigned r = 0; r < maxReps; ++r) {
		rg.gen_matrix_no_bias(act, 5);		rg.gen_matrix_norm(dm);
		tS.tic();
		iM.make_dropout_st(act, dpa, dm);
		tS.toc();
		for (const auto e : act) v += e;		for (const auto e : dm) v += e;
		v = ::std::log10(::std::abs(v));

		rg.gen_matrix_no_bias(act, 5);		rg.gen_matrix_norm(dm);
		tM.tic();
		iM.make_dropout_mt(act, dpa, dm);
		tM.toc();
		for (const auto e : act) v += e;		for (const auto e : dm) v += e;
		v = ::std::log10(::std::abs(v));

		rg.gen_matrix_no_bias(act, 5);		rg.gen_matrix_norm(dm);
		tB.tic();
		iM.make_dropout(act, dpa, dm);
		tB.toc();
		for (const auto e : act) v += e;		for (const auto e : dm) v += e;
		v = ::std::log10(::std::abs(v));
	}
	tS.say("_st");
	tM.say("_mt");
	tB.say("()");
	STDCOUTL(v);
}

TEST(TestMathNThr, make_dropout) {
	// 	typedef nntl::d_interfaces::iThreads_t def_threads_t;
	// 	typedef math::MathN<real_t, def_threads_t> iMB;
	// 	iMB iM;
	NNTL_RUN_TEST2(imath_basic_t::Thresholds_t::make_dropout, 10) test_make_dropout_perf(i, 10);
}
