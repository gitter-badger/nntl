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

#include <random>

namespace nntl {
namespace rng {

	template<typename RealT>
	struct _i_rng : public math::smatrix_td {
		typedef RealT real_t;
		typedef math::smatrix<real_t> realmtx_t;

		//all typedefs below should be changed in derived classes to suit needs
		
		typedef int seed_t;
		nntl_interface void seed(seed_t s) noexcept;
		nntl_interface void seed64(uint64_t s) noexcept;

		//////////////////////////////////////////////////////////////////////////
		//Initialization functions
		// 
		//preinit_additive_*() set of functions is used to inform RNG about total expected amount of random numbers that is
		// about to be requested during one training epoch. Arguments of multiple calls are summed.
		// 
		// normal distribution
		void preinit_additive_normal_distr(const numel_cnt_t ne)noexcept { NNTL_UNREF(ne); }
		//for calls to gen_vector_norm(), gen_vector() and related
		void preinit_additive_norm(const numel_cnt_t ne)noexcept { NNTL_UNREF(ne); }

		//not using init(), there're too many different init() routines now, hard to seek them in code
		bool init_rng()noexcept { return true; }
		void deinit_rng()noexcept {}

		//////////////////////////////////////////////////////////////////////////
		// Multithreading support. iRng instance should not create own threading pool, it should be given a threads pool object
		// during initialization
		// 
		// change to appropriate value in derived class
		static constexpr bool is_multithreaded = false;

		template<typename iThreadsT>
		bool init_ithreads(iThreadsT& t, const seed_t s = static_cast<seed_t>(s64to32(::std::time(0))))noexcept { return false; }

		//////////////////////////////////////////////////////////////////////////
		// iRng object should provide the following types of random numbers:
		// - fixed point number in range [0, A], where A is given. This numbers are going to used primarily by ::std::random_shuffle()
		//		and should be implicitly convertible to ptrdiff_t.
		// - fixed point number in its full range. To be used by distributions generators such as ::std::normal_distribution.
		//		This rng must obey UniformRandomBitGenerator concept.
		// - floating point number in ranges [0,1], [0,A] and [-A,A]

		//////////////////////////////////////////////////////////////////////////
		// ::std::random_shuffle() support
		//
		// ptrdiff_t is either int on 32bits or int64 on 64bits. Type required by random_shuffle()
		typedef ptrdiff_t int_4_random_shuffle_t;
		// int_4_random_shuffle_t is either int on 32bits or int64 on 64bits
		nntl_interface int_4_random_shuffle_t gen_i(int_4_random_shuffle_t lessThan)noexcept;
		nntl_interface int_4_random_shuffle_t operator()(int_4_random_shuffle_t lessThan)noexcept;// { return gen_i(lessThan); }

		//////////////////////////////////////////////////////////////////////////
		// ::std::*_distribution<> support. This API must conform UniformRandomBitGenerator concept
		//
		typedef int int_4_distribution_t;
		nntl_interface int_4_distribution_t operator()()noexcept;//returns values that are uniformly distributed between min() and max().
		nntl_interface int_4_distribution_t min()noexcept;//returns the minimum value that is returned by the generator's operator().
		nntl_interface int_4_distribution_t max()noexcept;//returns the maximum value that is returned by the generator's operator().
		nntl_interface int_4_distribution_t gen_int()noexcept;//random full-ranged int

		//floating point generation

		//////////////////////////////////////////////////////////////////////////
		//generate FP value in range [0,1]
		nntl_interface real_t gen_f_norm()noexcept;

		//generate a vector using Bernoulli distribution with probability of success p and success value sVal.
		nntl_interface void bernoulli_vector(real_t* ptr, const size_t n, const real_t p, const real_t sVal = real_t(1.), const real_t negVal = real_t(0.))noexcept;
		nntl_interface void bernoulli_matrix(realmtx_t& A, const real_t p, const real_t sVal = real_t(1.), const real_t negVal = real_t(0.))noexcept;

		nntl_interface void normal_vector(real_t* ptr, const size_t n, const real_t m = real_t(0.), const real_t st = real_t(1.))noexcept;
		nntl_interface void normal_matrix(realmtx_t& A, const real_t m = real_t(0.), const real_t st = real_t(1.))noexcept;


		//////////////////////////////////////////////////////////////////////////
		// matrix/vector generation (sequence of numbers drawn from uniform distribution in [-a,a])
		nntl_interface void gen_vector(real_t* ptr, const size_t n, const real_t a)noexcept;
		// matrix/vector generation (sequence of numbers drawn from uniform distribution in [neg,pos])
		nntl_interface void gen_vector(real_t* ptr, const size_t n, const real_t neg, const real_t pos)noexcept;

		//////////////////////////////////////////////////////////////////////////
		//generate vector with values in range [0,1]
		nntl_interface void gen_vector_norm(real_t* ptr, const size_t n)noexcept;

		//////////////////////////////////////////////////////////////////////////
		//generate vector with values in range [0,a]
		template<typename BaseType>
		nntl_interface void gen_vector_gtz(BaseType* ptr, const size_t n, const BaseType a)noexcept;


		//generate FP value in range [0,a]
		nntl_interface real_t gen_f(const real_t a)noexcept; //{ return a*gen_f_norm(); }

		// matrix/vector generation (sequence from begin to end of numbers drawn from uniform distribution in [-a,a])
		nntl_interface void gen_matrix(realmtx_t& mtx, const real_t a)noexcept;
		nntl_interface void gen_matrix_no_bias(realmtx_t& mtx, const real_t a)noexcept;

		//generate matrix with values in range [0,1]
		nntl_interface void gen_matrix_norm(realmtx_t& mtx)noexcept;
		nntl_interface void gen_matrix_no_bias_norm(realmtx_t& mtx)noexcept;

		//generate matrix with values in range [0,a]
		nntl_interface void gen_matrix_gtz(realmtx_t& mtx, const real_t a)noexcept;
		nntl_interface void gen_matrix_no_bias_gtz(realmtx_t& mtx, const real_t a)noexcept;

		static constexpr uint32_t s64to32(const uint64_t v)noexcept {
			return static_cast<uint32_t>(v & UINT32_MAX) ^ static_cast<uint32_t>((v >> 32)&UINT32_MAX);
		}
	};

	template<typename RealT, typename int4ShuffleT, typename int4DistribsT, typename FinalPolymorphChild>
	struct rng_helper : public _i_rng<RealT> {
	protected:
		typedef FinalPolymorphChild self_t;
		NNTL_METHODS_SELF_CHECKED((::std::is_base_of<rng_helper<real_t, int4ShuffleT, int4DistribsT, FinalPolymorphChild>, FinalPolymorphChild>::value)
			, "FinalPolymorphChild must derive from _i_rng_helper<RealT,FinalPolymorphChild>");

	public:
		typedef int4ShuffleT int_4_random_shuffle_t;
		typedef int4DistribsT int_4_distribution_t;
		typedef math::s_elems_range elms_range;
		
		~rng_helper()noexcept {
			get_self().deinit_rng();
		}

		//////////////////////////////////////////////////////////////////////////
		void seed64(uint64_t s) noexcept {
			get_self().seed(static_cast<seed_t>(s64to32(s)));
		}

		//////////////////////////////////////////////////////////////////////////
		// ::std::random_shuffle() support
		int_4_random_shuffle_t operator()(int_4_random_shuffle_t lessThan)noexcept { return get_self().gen_i(lessThan); }

		//////////////////////////////////////////////////////////////////////////
		// ::std::*_distribution<> support. This API must conform UniformRandomBitGenerator concept
		int_4_distribution_t operator()()noexcept { return get_self().gen_int(); }
		//returns the minimum value that is returned by the generator's operator().
		//static constexpr int_4_distribution_t min()const noexcept { return ::std::numeric_limits<int_4_distribution_t>::min() + 1; } //+1 is essential
		static constexpr int_4_distribution_t min() noexcept { return ::std::numeric_limits<int_4_distribution_t>::min(); }
		//returns the maximum value that is returned by the generator's operator().
		static constexpr int_4_distribution_t max() noexcept { return ::std::numeric_limits<int_4_distribution_t>::max(); }

		//////////////////////////////////////////////////////////////////////////
		//generate FP value in range [0,a]
		real_t gen_f(const real_t a)noexcept { return a*get_self().gen_f_norm(); }

		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		void bernoulli_vector(real_t* ptr, const size_t n, const real_t p, const real_t posVal = real_t(1.), const real_t negVal = real_t(0.))noexcept {
			NNTL_ASSERT(ptr);
			NNTL_ASSERT(p > real_t(0) && p < real_t(1));
			const auto pE = ptr + n;
			while (ptr != pE) {
				*ptr++ = get_self().gen_f_norm() < p ? posVal : negVal;
			}
		}
		void bernoulli_matrix(realmtx_t& A, const real_t p, const real_t posVal = real_t(1.), const real_t negVal = real_t(0.))noexcept {
			get_self().bernoulli_vector(A.data(), A.numel(), p, posVal, negVal);
		}

		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		void normal_vector(real_t*const ptr, const size_t n, const real_t m = real_t(0.), const real_t st = real_t(1.))noexcept {
			::std::normal_distribution<real_t> distr(m,n);
			for (size_t i = 0; i < n; ++i) {
				ptr[i] = distr(*this);
			}
		}
		void normal_matrix(realmtx_t& A, const real_t m = real_t(0.), const real_t st = real_t(1.))noexcept {
			get_self().normal_vector(A.data(), A.numel(), m , st);
		}

		//////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////// 
		// matrix/vector generation (sequence from begin to end of numbers drawn from uniform distribution in [-a,a])
		void gen_matrix(realmtx_t& mtx, const real_t a)noexcept {
			NNTL_ASSERT(!mtx.emulatesBiases());
			get_self().gen_vector(mtx.data(), mtx.numel(), a);
		}
		void gen_matrix_no_bias(realmtx_t& mtx, const real_t a)noexcept {
			NNTL_ASSERT(mtx.emulatesBiases() && mtx.test_biases_ok());
			get_self().gen_vector(mtx.data(), mtx.numel_no_bias(), a);
			NNTL_ASSERT(mtx.test_biases_ok());
		}

		//generate matrix with values in range [0,1]
		void gen_matrix_norm(realmtx_t& mtx)noexcept {
			NNTL_ASSERT(!mtx.emulatesBiases());
			get_self().gen_vector_norm(mtx.data(), mtx.numel());
		}
		void gen_matrix_no_bias_norm(realmtx_t& mtx)noexcept {
			NNTL_ASSERT(mtx.emulatesBiases() && mtx.test_biases_ok());
			get_self().gen_vector_norm(mtx.data(), mtx.numel_no_bias());
			NNTL_ASSERT(mtx.test_biases_ok());
		}

		//////////////////////////////////////////////////////////////////////////
		//generate matrix with values in range [0,a]
		void gen_matrix_gtz(realmtx_t& mtx, const real_t a)noexcept {
			NNTL_ASSERT(!mtx.emulatesBiases());
			get_self().gen_vector_gtz(mtx.data(), mtx.numel(), a);
		}
		void gen_matrix_no_bias_gtz(realmtx_t& mtx, const real_t a)noexcept {
			NNTL_ASSERT(mtx.emulatesBiases() && mtx.test_biases_ok());
			get_self().gen_vector_gtz(mtx.data(), mtx.numel_no_bias(), a);
			NNTL_ASSERT(mtx.test_biases_ok());
		}
	};


	///////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	// helper to recognize if the Rng class provides asynchronous generation
	template< class, class = ::std::void_t<> >
	struct is_asynch : ::std::false_type { };
	// specialization recognizes types that do have a nested ::asynch_rng_t member:
	template< class RngT >
	struct is_asynch<RngT, ::std::void_t<typename RngT::asynch_rng_t>> : ::std::true_type {};

}
}