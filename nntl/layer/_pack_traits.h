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

#include <map>
#include <set>

// defines some common traits and struct for layer_pack_* layers

namespace nntl {

	//////////////////////////////////////////////////////////////////////////
	// traits recognizer
	// primary template handles types that have no nested ::phl_original_t member:
	template< class, class = ::std::void_t<> >
	struct is_PHL : ::std::false_type { };
	// specialization recognizes types that do have a nested ::phl_original_t member:
	template< class T >
	struct is_PHL<T, ::std::void_t<typename T::phl_original_t>> : ::std::true_type {};

	//////////////////////////////////////////////////////////////////////////
	// Tests if a layer applies gating capabilities to its inner layers by checking the existance of ::gating_layer_t type
	template< class, class = ::std::void_t<> >
	struct is_pack_gated : ::std::false_type { };
	template< class T >
	struct is_pack_gated<T, ::std::void_t<typename T::gating_layer_t>> : ::std::true_type {};
	//Such layer must provide a function get_gating_info(_impl::GatingContext&)

	namespace _impl {

		template<typename RealT>
		struct GatingContext : public math::smatrix_td {
			typedef RealT real_t;
			typedef math::smatrix<real_t> realmtx_t;

			//it's safe to use a pointer to gating mask as the mask matrix itself doesn't move with fprop()'s/bprop()'s/etc
			const realmtx_t* pGatingMask;

			//this type makes a connection between an inner LPHG's layer and its corresponding gating mask column.
			typedef ::std::map<layer_index_t, vec_len_t> gating_mask_columns_descr_t;
			gating_mask_columns_descr_t colsDescr;

			//this type to hold layers ids of gating layers
			typedef ::std::set<layer_index_t> nongated_layers_set_t;
			nongated_layers_set_t nongatedIds;

			bool bShouldProcessLayer(const layer_index_t& lIdx) const noexcept {
				return 0 == nongatedIds.count(lIdx);
			}
		};

	}


	//////////////////////////////////////////////////////////////////////////
	// Tests if a layer applies tiling capabilities to its inner layer by checking the existance of ::tiled_layer_t type
	template< class, class = ::std::void_t<> >
	struct is_pack_tiled : ::std::false_type { };
	template< class T >
	struct is_pack_tiled<T, ::std::void_t<typename T::tiled_layer_t>> : ::std::true_type {};
}