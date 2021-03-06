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

#include <type_traits>

namespace nntl {
namespace tuple_utils {

	template <typename T>
	struct is_tuple : ::std::false_type {};
	template <typename... Args>
	struct is_tuple <::std::tuple<Args...>> : ::std::true_type {};
	template <typename... Args>
	struct is_tuple <const ::std::tuple<Args...>> : ::std::true_type {};

	template <typename T>
	struct assert_tuple {
		typedef T type;
		static_assert(is_tuple<T>::value, "Type must be a std::tuple specialization!");
	};

	template <typename T>
	struct assert_tuple_or_void {
		typedef T type;
		static_assert(is_tuple<T>::value || ::std::is_same<T, void>::value
			, "Type must be a void or a std::tuple specialization!");
	};

	template <typename T>
	using is_tuple_or_void = ::std::disjunction<is_tuple<T>, ::std::is_same<T, void>>;

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//helpers to call f() for each tuple element
	// Some considerations: functor::for_each function accepts Tuple&& to make possible calls with
	// Tuple& as well as const Tuple&. It also accepts F&& to make possible calls with lvalues and rvalues
	namespace _impl {
		template<int I, class Tuple, typename F> struct _for_each_up {
			static void for_each(Tuple&& t, F&& f) noexcept {
				//_for_each_up<I - 1, Tuple, F>::for_each(t, ::std::forward<F>(f));
				_for_each_up<I - 1, ::std::add_lvalue_reference_t<Tuple>, ::std::add_lvalue_reference_t<F>>::for_each(t, f);
				::std::forward<F>(f)(::std::get<I>(::std::forward<Tuple>(t)));//OK to forward the last use
			}
		};
		template<class Tuple, typename F> struct _for_each_up<0, Tuple, F> {
			static void for_each(Tuple&& t, F&& f)noexcept {
				::std::forward<F>(f)(::std::get<0>(::std::forward<Tuple>(t)));
			}
		};
	}
	template<class Tuple, typename F>
	inline void for_each_up(Tuple&& t, F&& f)noexcept {
		_impl::_for_each_up<::std::tuple_size<::std::remove_reference_t<Tuple>>::value - 1, Tuple, F
		>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
	}
	//ignoring the last element
	template<class Tuple, typename F>
	inline void for_each_exc_last_up(Tuple&& t, F&& f)noexcept {
		static_assert(::std::tuple_size<::std::remove_reference_t<Tuple>>::value > 1, "Tuple must have at least two elements!");
		_impl::_for_each_up<::std::tuple_size<::std::remove_reference_t<Tuple>>::value - 2, Tuple, F
			>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
	}

	//////////////////////////////////////////////////////////////////////////
	//ignore first element
	namespace _impl {
		template<int I, class Tuple, typename F> struct _for_each_exc_first_up {
			static void for_each(Tuple&& t, F&& f) noexcept {
				_for_each_exc_first_up <I - 1, ::std::add_lvalue_reference_t<Tuple>, ::std::add_lvalue_reference_t<F>>::for_each(t, f);
				::std::forward<F>(f)(::std::get<I>(::std::forward<Tuple>(t)));
			}
		};
		template<class Tuple, typename F> struct _for_each_exc_first_up <1, Tuple, F> {
			static void for_each(Tuple&& t, F&& f)noexcept {
				::std::forward<F>(f)(::std::get<1>(::std::forward<Tuple>(t)));
			}
		};
	}
	
	template<class Tuple, typename F>
	inline void for_each_exc_first_up(Tuple&& t, F&& f)noexcept {
		static_assert(::std::tuple_size<::std::remove_reference_t<Tuple>>::value > 1, "Tuple must have at least two elements!");
		_impl::_for_each_exc_first_up <::std::tuple_size<::std::remove_reference_t<Tuple>>::value - 1, Tuple, F
		>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
	}
	//////////////////////////////////////////////////////////////////////////
	//downwards direction (from the tail to the head)
	namespace _impl {
		template<int I, class Tuple, typename F> struct _for_each_down {
			static void for_each(Tuple&& t, F&& f) noexcept {
				f(::std::get<I>(t));
				_for_each_down <I - 1, Tuple, F>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
			}
		};
		template<class Tuple, typename F> struct _for_each_down <0, Tuple, F> {
			static void for_each(Tuple&& t, F&& f)noexcept {
				::std::forward<F>(f)(::std::get<0>(::std::forward<Tuple>(t)));
			}
		};
	}
	template<class Tuple, typename F>
	inline void for_each_down(Tuple&& t, F&& f) noexcept {
		_impl::_for_each_down<::std::tuple_size<::std::remove_reference_t<Tuple>>::value - 1, Tuple, F
		>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
	}
	//////////////////////////////////////////////////////////////////////////
	namespace _impl {
		template<int I, class Tuple, typename F> struct _for_each_exc_first_down {
			static void for_each(Tuple&& t, F&& f) noexcept {
				f(::std::get<I>(t));
				_for_each_exc_first_down <I - 1, Tuple, F>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
			}
		};
		template<class Tuple, typename F> struct _for_each_exc_first_down <1, Tuple, F> {
			static void for_each(Tuple&& t, F&& f)noexcept {
				::std::forward<F>(f)(::std::get<1>(::std::forward<Tuple>(t)));
			}
		};
	}	
	template<class Tuple, typename F>
	inline void for_each_exc_first_down(Tuple&& t, F&& f) noexcept {
		static_assert(::std::tuple_size<::std::remove_reference_t<Tuple>>::value > 1, "Tuple must have at least two elements!");
		_impl::_for_each_exc_first_down <::std::tuple_size<::std::remove_reference_t<Tuple>>::value - 1, Tuple, F
		>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
	}

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// for each with previous element upwards
	// 
	namespace _impl {
		template<int I, class Tuple, typename F> struct _for_eachwp_up {
			static void for_each(Tuple&& t, F&& f)noexcept {
				_for_eachwp_up<I - 1, ::std::add_lvalue_reference_t<Tuple>, ::std::add_lvalue_reference_t<F>>::for_each(t, f);
				::std::forward<F>(f)(::std::get<I>(t), ::std::get<I - 1>(t), false);
			}
		};
		template<class Tuple, typename F> struct _for_eachwp_up<1, Tuple, F> {
			static void for_each(Tuple&& t, F&& f)noexcept {
				::std::forward<F>(f)(::std::get<1>(t), ::std::get<0>(t), true);
			}
		};
	}
	template<class Tuple, typename F>
	inline void for_eachwp_up(Tuple&& t, F&& f)noexcept {
		static_assert(::std::tuple_size<::std::remove_reference_t<Tuple>>::value > 1, "Tuple must have more than 1 element");
		_impl::_for_eachwp_up<::std::tuple_size<::std::remove_reference_t<Tuple>>::value - 1, Tuple, F
		>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
	}

	//////////////////////////////////////////////////////////////////////////
	//for each with previous element upwards, forwardprop version (skip last element)
	// #todo rewrite as above funcs if need this code
	/*template<int I, class Tuple, typename F> struct _for_eachwp_upfp_impl {
		static void for_each(const Tuple& t, F&& f)noexcept {
			_for_eachwp_upfp_impl<I - 1, Tuple, F>::for_each(t, ::std::forward<F>(f));
			::std::forward<F>(f)(::std::get<I>(t), ::std::get<I - 1>(t));
		}
	};
	template<class Tuple, typename F> struct _for_eachwp_upfp_impl<1, Tuple, F> {
		static void for_each(const Tuple& t, F&& f)noexcept {
			::std::forward<F>(f)(::std::get<1>(t), ::std::get<0>(t));
		}
	};
	template<class Tuple, typename F>
	inline void for_eachwp_upfp(const Tuple& t, F&& f)noexcept {
		static_assert(::std::tuple_size<Tuple>::value > 2, "Tuple must have more than 2 elements");
		_for_eachwp_upfp_impl<::std::tuple_size<Tuple>::value - 2, Tuple, F>::for_each(t, ::std::forward<F>(f));
	}*/

	//////////////////////////////////////////////////////////////////////////
	//for each with next downwards, backprop-version (don't use <LAST>)
	namespace _impl {
		template<int I, class Tuple, typename F> struct _for_eachwn_downbp {
			static void for_each(Tuple&& t, F&& f) noexcept {
				f(::std::get<I>(t), ::std::get<I - 1>(t), false);
				_for_eachwn_downbp<I - 1, Tuple, F>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
			}
		};
		template<class Tuple, typename F> struct _for_eachwn_downbp<1, Tuple, F> {
			static void for_each(Tuple&& t, F&& f)noexcept {
				::std::forward<F>(f)(::std::get<1>(t), ::std::get<0>(t), true);
			}
		};
		template<class Tuple, typename F> struct _for_eachwn_downbp<0, Tuple, F> {
			static void for_each(Tuple&& , F&& )noexcept {}
		};
	}
	template<class Tuple, typename F>
	inline void for_eachwn_downbp(Tuple&& t, F&& f)noexcept {
		constexpr auto lei = ::std::tuple_size<::std::remove_reference_t<Tuple>>::value - 1;
		static_assert(lei > 0, "Tuple must have more than 1 element");
		_impl::_for_eachwn_downbp<lei - 1, Tuple, F>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
	}

	//////////////////////////////////////////////////////////////////////////
	//for each with next downwards, full backprop-version (use <LAST>)
	namespace _impl {
		template<int I, class Tuple, typename F> struct _for_eachwn_downfullbp {
			static void for_each(Tuple&& t, F&& f) noexcept {
				f(::std::get<I>(t), ::std::get<I - 1>(t), false);
				_for_eachwn_downfullbp<I - 1, Tuple, F>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
			}
		};
		template<class Tuple, typename F> struct _for_eachwn_downfullbp<1, Tuple, F> {
			static void for_each(Tuple&& t, F&& f)noexcept {
				::std::forward<F>(f)(::std::get<1>(t), ::std::get<0>(t), true);
			}
		};
		template<class Tuple, typename F> struct _for_eachwn_downfullbp<0, Tuple, F> {
			static void for_each(Tuple&& , F&& )noexcept {}
		};
	}	
	template<class Tuple, typename F>
	inline void for_eachwn_downfullbp(Tuple&& t, F&& f)noexcept {
		constexpr auto lei = ::std::tuple_size<::std::remove_reference_t<Tuple>>::value - 1;
		static_assert(lei > 0, "Tuple must have more than 1 element");
		_impl::_for_eachwn_downfullbp<lei, Tuple, F>::for_each(::std::forward<Tuple>(t), ::std::forward<F>(f));
	}


	//////////////////////////////////////////////////////////////////////////
	// Creating a subtuple from tuple elements
	// thanks to http://stackoverflow.com/questions/17854219/creating-a-sub-tuple-starting-from-a-stdtuplesome-types
	// see also https://msdn.microsoft.com/en-us/library/mt125500.aspx
	
	template <typename... T, ::std::size_t... I>
	constexpr auto subtuple_(const ::std::tuple<T...>& t, ::std::index_sequence<I...>) {
		return ::std::make_tuple(::std::get<I>(t)...);
	}

	template <int Trim, typename... T>
	constexpr auto subtuple_trim_tail(const ::std::tuple<T...>& t) {
		return subtuple_(t, ::std::make_index_sequence<sizeof...(T)-Trim>());
	}

	//////////////////////////////////////////////////////////////////////////
	//from http://stackoverflow.com/questions/31893102/passing-stdinteger-sequence-as-template-parameter-to-a-meta-function
	template <typename T, typename U>
	struct selector;

	template <typename T, ::std::size_t... Is>
	struct selector<T, ::std::index_sequence<Is...>>
	{
		using type = ::std::tuple<typename ::std::tuple_element<Is, T>::type...>;
	};

	template <::std::size_t N, typename... Ts>
	struct remove_last_n
	{
		using Indices = ::std::make_index_sequence<sizeof...(Ts)-N>;
		using type = typename selector<::std::tuple<Ts...>, Indices>::type;
	};

// 	int main()
// 	{
// 		using X = remove_last_n<2, int, char, bool, int>::type;
// 		static_assert(::std::is_same<X, ::std::tuple<int, char>>::value, "types do not match");
// 	}

	//////////////////////////////////////////////////////////////////////////
	//idea from http://stackoverflow.com/a/16707966/1974258s
	//returns index of the first occurrence of type T in tuple<Args...>. If there's no such type in the tuple will return
	// the size of the tuple.
	// 
	template <class T, ::std::size_t N, class... Args>
	struct get_element_idx_impl { static constexpr auto value = N; };

	template <class T, ::std::size_t N, class... Args>
	struct get_element_idx_impl<T, N, T, Args...> { static constexpr auto value = N; };

	template <class T, ::std::size_t N, class U, class... Args>
	struct get_element_idx_impl<T, N, U, Args...> { static constexpr auto value = get_element_idx_impl<T, N + 1, Args...>::value; };

	template <class T, class... Args>
	constexpr size_t get_element_idx(const ::std::tuple<Args...>&) { return get_element_idx_impl<T, 0, Args...>::value; }

	template <class T, class... Args>
	constexpr size_t get_element_idx() { return get_element_idx_impl<T, 0, Args...>::value; }

	//////////////////////////////////////////////////////////////////////////
	template <class T, class Tuple>
	struct tuple_element_idx_safe {
		static_assert(is_tuple<Tuple>::value, "Not a tuple!");
		static constexpr std::size_t value = ::std::tuple_size<Tuple>::value;
	};

	template <class T, class... TupleTypes>
	struct tuple_element_idx_safe<T, ::std::tuple<T, TupleTypes...>> {
		static constexpr std::size_t value = 0;
	};

	template <class T, class U, class... TupleTypes>
	struct tuple_element_idx_safe<T, ::std::tuple<U, TupleTypes...>> {
		static constexpr std::size_t value = 1 + tuple_element_idx_safe<T, ::std::tuple<TupleTypes...>>::value;
	};
	//////////////////////////////////////////////////////////////////////////

// 	template <template <class> class F, class Tuple>
// 	struct tuple_disjunction {
// 		static_assert(is_tuple<Tuple>::value, "Not a tuple!");
// 	};
// 	template <template <class> class F, class... TupleTypes>
// 	struct tuple_disjunction<F, ::std::tuple<TupleTypes...>> : public ::std::disjunction< F<TupleTypes>... > {};
	
	template <template<class...> class A, template <class> class F, class Tuple>
	struct aggregate {
		static_assert(is_tuple<Tuple>::value, "Not a tuple!");
	};
	template <template<class...> class A, template <class> class F, class... TupleTypes>
	struct aggregate<A, F, ::std::tuple<TupleTypes...>> : public A< F<TupleTypes>... > {};


	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// helper to perform static_assert on each tuple element
	template<int I, class Tuple, template <class> class F> struct _assert_each_impl : _assert_each_impl<I - 1, Tuple, F> {
		static_assert(F<::std::tuple_element_t<I, Tuple>>::value, "Tuple property assertion failed");
	};
	template<class Tuple, template <class> class F> struct _assert_each_impl<0, Tuple, F> : ::std::true_type {
		static_assert(F<::std::tuple_element_t<0, Tuple>>::value, "Tuple property assertion failed");
	};
	template<class Tuple, template <class> class F>
	struct assert_each : _assert_each_impl<::std::tuple_size<Tuple>::value - 1, Tuple, F> {};
}
}