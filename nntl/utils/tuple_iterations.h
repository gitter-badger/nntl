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

#include <type_traits>

namespace nntl {
namespace utils {

	//////////////////////////////////////////////////////////////////////////
	//helpers to call f() for each tuple element
	template<int I, class Tuple, typename F> struct _for_each_up_impl {
		static void for_each(const Tuple& t, F& f) noexcept {
			_for_each_up_impl<I - 1, Tuple, F>::for_each(t, f);
			f(std::get<I>(t));
		}
	};
	template<class Tuple, typename F> struct _for_each_up_impl<0, Tuple, F> {
		static void for_each(const Tuple& t, F& f)noexcept {
			f(std::get<0>(t));
		}
	};
	template<class Tuple, typename F>
	inline void for_each_up(const Tuple& t, F& f)noexcept {
		_for_each_up_impl<std::tuple_size<Tuple>::value - 1, Tuple, F>::for_each(t, f);
	}

	//downwards direction (from the tail to the head)
	template<int I, class Tuple, typename F> struct _for_each_down_impl {
		static void for_each(const Tuple& t, F& f) noexcept {
			f(std::get<I>(t));
			_for_each_down_impl <I - 1, Tuple, F>::for_each(t, f);
		}
	};
	template<class Tuple, typename F> struct _for_each_down_impl <0, Tuple, F> {
		static void for_each(const Tuple& t, F& f)noexcept {
			f(std::get<0>(t));
		}
	};
	template<class Tuple, typename F>
	inline void for_each_down(const Tuple& t, F& f) noexcept {
		_for_each_down_impl <std::tuple_size<Tuple>::value - 1, Tuple, F>::for_each(t, f);
	}

	//////////////////////////////////////////////////////////////////////////
	// for each with previous element upwards
	// 
	template<int I, class Tuple, typename F> struct _for_eachwp_up_impl {
		static void for_each(const Tuple& t, F& f)noexcept {
			_for_eachwp_up_impl<I - 1, Tuple, F>::for_each(t, f);
			f(std::get<I>(t), std::get<I - 1>(t), false);
		}
	};
	template<class Tuple, typename F> struct _for_eachwp_up_impl<1, Tuple, F> {
		static void for_each(const Tuple& t, F& f)noexcept {
			f(std::get<1>(t), std::get<0>(t), true);
		}
	};
	template<class Tuple, typename F>
	inline void for_eachwp_up(const Tuple& t, F& f)noexcept {
		static_assert(std::tuple_size<Tuple>::value > 1, "Tuple must have more than 1 element");
		_for_eachwp_up_impl<std::tuple_size<Tuple>::value - 1, Tuple, F>::for_each(t, f);
	}

	//for each with previous element upwards, forwardprop version (skip last element)
	template<int I, class Tuple, typename F> struct _for_eachwp_upfp_impl {
		static void for_each(const Tuple& t, F& f)noexcept {
			_for_eachwp_upfp_impl<I - 1, Tuple, F>::for_each(t, f);
			f(std::get<I>(t), std::get<I - 1>(t));
		}
	};
	template<class Tuple, typename F> struct _for_eachwp_upfp_impl<1, Tuple, F> {
		static void for_each(const Tuple& t, F& f)noexcept {
			f(std::get<1>(t), std::get<0>(t));
		}
	};
	template<class Tuple, typename F>
	inline void for_eachwp_upfp(const Tuple& t, F& f)noexcept {
		static_assert(std::tuple_size<Tuple>::value > 2, "Tuple must have more than 2 elements");
		_for_eachwp_upfp_impl<std::tuple_size<Tuple>::value - 2, Tuple, F>::for_each(t, f);
	}

	//for each with next downwards, backprop-version (don't use <LAST>)
	template<int I, class Tuple, typename F> struct _for_eachwn_downbp_impl {
		static void for_each(const Tuple& t, F& f) noexcept {
			f(std::get<I>(t), std::get<I - 1>(t), false);
			_for_eachwn_downbp_impl<I - 1, Tuple, F>::for_each(t, f);
		}
	};
	template<class Tuple, typename F> struct _for_eachwn_downbp_impl<1, Tuple, F> {
		static void for_each(const Tuple& t, F& f)noexcept {
			f(std::get<1>(t), std::get<0>(t), true);
		}
	};
	template<class Tuple, typename F>
	inline void for_eachwn_downbp(const Tuple& t, F& f)noexcept {
		constexpr auto lei = std::tuple_size<Tuple>::value - 1;
		static_assert(lei > 0, "Tuple must have more than 1 element");
		//f(std::get<lei>(t), std::get<lei>(t), true);
		_for_eachwn_downbp_impl<lei - 1, Tuple, F>::for_each(t, f);
	}



	/*
	* Cant make it work
	*template<int I, class Tuple, template<int, class Tuple> typename F > struct _for_i_up_impl {
	static void for_i(const Tuple& t, F<I, Tuple>& f) {
	_for_i_up_impl<I - 1, Tuple, F>::for_i(t, f);
	f<I, Tuple>(t);
	}
	};
	template<class Tuple, template<int, class Tuple> typename F > struct _for_i_up_impl<0, Tuple, F> {
	static void for_i(const Tuple& t, F<0, Tuple>& f) {
	f<0, Tuple>(t);
	}
	};

	template<typename Tuple>
	constexpr size_t _get_size(const Tuple& t){
	return std::tuple_size<Tuple>::value;
	}

	template<class Tuple, template<int, class Tuple> typename F >
	void for_i_up(const Tuple& t, F<_get_size<Tuple>(t), Tuple> & f) {
	_for_i_up_impl<std::tuple_size<Tuple>::value - 1, Tuple, F>::for_i(t,f);
	}*/
}
}