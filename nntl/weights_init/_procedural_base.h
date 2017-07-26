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

#include "../utils/dataHolder.h"

namespace nntl {
namespace weights_init {
namespace procedural {

	namespace _impl {

		template<typename NnetT>
		struct _base : public math::smatrix_td {
		public:
			typedef NnetT nnet_t;
			typedef typename NnetT::real_t real_t;
			typedef typename NnetT::realmtx_t realmtx_t;
			typedef typename NnetT::realmtxdef_t realmtxdef_t;

		protected:
			nnet_t& m_nn;
			utils::dataHolder<real_t> m_data;

		public:
			_base(nnet_t& nn)noexcept : m_nn(nn) {}

		protected:
			void init(const vec_len_t maxBatchSize, const realmtx_t& data_x)noexcept {
				m_data.init(maxBatchSize, data_x);
			}
			void deinit()noexcept {
				m_data.deinit();
			}

		};

	}

}
}
}