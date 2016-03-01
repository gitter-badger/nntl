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

// layer_pack_horizontal provides a way to gather other layers into a parallel (horizontal) pack and feed different ranges of
// underlying activation units into different set of layers. I.e. it defines a layer (an object with _i_layer interface), that
// consists of a set of other layers. Concatenation of their activations constitute a total layer activation.
// 
//    \  |  |  |  |     |  |  |  | /
// |------layer_pack_horizontal-------|
// |  \  |  |  |  |  .  |  |  |  | /  |
// |   |--layer1--|  .  |--layerN--|  | 
// |    / | | | | |  .  | | | | | \   |
// |----------------------------------|
//      / | | | | |  .  | | | | | \
//
// layer_pack_horizontal uses neurons of the last layer some_layer_last as its activation units and
// has a number of incoming neurons equal to the number of incoming neurons of first layer some_layer_first
// 
#include "_pack_.h"
#include "../utils.h"

namespace nntl {

	

	template<typename FinalPolymorphChild, typename ...PHLsT>
	class _layer_pack_horizontal : public _i_layer<typename std::remove_reference<typename std::tuple_element<0, const std::tuple<PHLsT...>>
		::type>::type::layer_t::iMath_t::real_t>
	{
	public:
		typedef FinalPolymorphChild self_t;
		typedef FinalPolymorphChild& self_ref_t;
		typedef const FinalPolymorphChild& self_cref_t;
		typedef FinalPolymorphChild* self_ptr_t;

		//shouldn't use references here (with PHLsT...)
		typedef const std::tuple<PHLsT...> _phl_tuple;

		static constexpr size_t phl_count = sizeof...(PHLsT);
		static_assert(phl_count > 1, "For a pack with a single inner layer use that layer instead");

		static_assert(is_PHL<typename std::remove_reference<typename std::tuple_element<0, _phl_tuple>::type>::type>::value, "_layer_pack_horizontal must be assembled from PHL objects!");
		static_assert(is_PHL<typename std::remove_reference<typename std::tuple_element<phl_count - 1, _phl_tuple>::type>::type>::value, "_layer_pack_horizontal must be assembled from PHL objects!");

		typedef typename std::remove_reference<typename std::tuple_element<0, _phl_tuple>::type>::type::layer_t first_layer_t;
		typedef typename std::remove_reference<typename std::tuple_element<phl_count - 1, _phl_tuple>::type>::type::layer_t last_layer_t;
		
		typedef typename first_layer_t::iMath_t iMath_t;
		typedef typename first_layer_t::iRng_t iRng_t;
		typedef typename first_layer_t::_layer_init_data_t _layer_init_data_t;

	protected:
		//we need 2 matrices for bprop()
		//typedef std::array<realmtxdef_t*, 2> realmtxdefptr_array_t;

	protected:
		_phl_tuple m_phl_tuple;
		realmtxdef_t m_activations;//its content assembled from individual activations of inner layers in-place

		real_t* m_pTmpBiasStorage;//addresses at max m_max_fprop_batch_size elements to store a column of data of previous layer activation,
		// that will be replaced by biases during fprop phase.

		numel_cnt_t m_layers_max_dLdA_numel;//max lid.max_dLdA_numel gathered from inner layers during init() phase. This value is used 
		// to allocate dLdA matricies to pass to layers bprop()

		realmtxdef_t m_innerdLdA, m_innerdLdAPrev;

		iMath_t* m_pMath;

		vec_len_t m_max_fprop_batch_size, m_training_batch_size;

	private:
		neurons_count_t m_neurons_cnt, m_incoming_neurons_cnt;
		layer_index_t m_layerIdx;

		//////////////////////////////////////////////////////////////////////////
		//
	protected:
		//this is how we going to initialize layer indexes.
		friend class _impl::_preinit_layers;
		void _preinit_layer(layer_index_t& idx, const neurons_count_t inc_neurons_cnt)noexcept {
			//there should better be an exception, but we don't want exceptions at all.
			//anyway, there is nothing to help to those who'll try to abuse this API...
			NNTL_ASSERT(!m_layerIdx && idx > 0 && inc_neurons_cnt > 0);

			if (m_layerIdx) abort();
			m_layerIdx = idx;
			m_incoming_neurons_cnt = inc_neurons_cnt;

			_impl::_preinit_layers initializer(idx + 1, inc_neurons_cnt);
			if (initializer.preparePHLCheck()) {
				utils::for_each_up(m_phl_tuple, initializer);
				if (!initializer.PHLCheck()) {
					NNTL_ASSERT(!"All lower layer activations must be covered by a set of inner layers of _layer_pack_horizontal!");
					//#todo: probably need a better way to return error
					abort();
				}
			} else {
				NNTL_ASSERT(!"Failed to prepare for PHLCheck / No memory, probably");
				//#todo: probably need a better way to return error
				abort();
			}
			
			idx = initializer._idx;
		}

		first_layer_t& first_layer()const noexcept { return std::get<0>(m_phl_tuple).l; }
		last_layer_t& last_layer()const noexcept { return std::get<phl_count - 1>(m_phl_tuple).l; }

	public:
		~_layer_pack_horizontal()noexcept {}
		_layer_pack_horizontal(PHLsT&... phls)noexcept : m_phl_tuple(phls...), m_layerIdx(0), m_max_fprop_batch_size(0)
			, m_training_batch_size(0), m_incoming_neurons_cnt(0), m_pTmpBiasStorage(nullptr)
			, m_layers_max_dLdA_numel(0), m_pMath(nullptr)
		{
			//calculate m_neurons_cnt
			neurons_count_t nc = 0;			
			utils::for_each_up(m_phl_tuple, [&nc](auto& phl)noexcept {
				static_assert(is_PHL<std::remove_reference_t<decltype(phl)>>::value, "_layer_pack_horizontal must be assembled from PHL objects!");
				static_assert(!std::is_base_of<m_layer_input, std::remove_reference_t<decltype(phl)>::layer_t>::value
					&& !std::is_base_of<m_layer_output, std::remove_reference_t<decltype(phl)>::layer_t>::value
					, "Inner layers of _layer_pack_horizontal mustn't be input or output layers!");
				nc += phl.l.get_neurons_cnt();
			});
			m_neurons_cnt = nc;

			m_activations.will_emulate_biases();
		}

		self_ref_t get_self() noexcept {
			static_assert(std::is_base_of<_layer_pack_horizontal, FinalPolymorphChild>::value
				, "FinalPolymorphChild must derive from _layer_pack_horizontal");
			return static_cast<self_ref_t>(*this);
		}
		self_cref_t get_self() const noexcept {
			static_assert(std::is_base_of<_layer_pack_horizontal, FinalPolymorphChild>::value
				, "FinalPolymorphChild must derive from _layer_pack_horizontal");
			return static_cast<self_cref_t>(*this);
		}

		const layer_index_t get_layer_idx() const noexcept { return m_layerIdx; }
		const neurons_count_t get_neurons_cnt() const noexcept { return m_neurons_cnt; }
		const neurons_count_t get_incoming_neurons_cnt()const noexcept { return m_incoming_neurons_cnt; }

		const realmtx_t& get_activations()const noexcept { return m_activations; }

		//and apply function _Func(auto& layer) to each layer here
		template<typename _Func>
		void for_each_layer(_Func& f)const noexcept {
			utils::for_each_up(m_phl_tuple, [&f](auto& phl)noexcept {
				f(phl.l); 
			});
		}
		/*template<typename _Func>
		void for_each_phl(_Func&& f)const noexcept {
			utils::for_each_up(m_phl_tuple, std::move(f));
		}*/

		void get_layer_name(char* pName, const size_t cnt)const noexcept {
			sprintf_s(pName, cnt, "lph%d", static_cast<unsigned>(get_layer_idx()));
		}
		std::string get_layer_name_str()const noexcept {
			constexpr size_t ml = 16;
			char n[ml];
			get_self().get_layer_name(n, ml);
			return std::string(n);
		}

		//should return true, if the layer has a value to add to Loss function value (there's some regularizer attached)
		bool hasLossAddendum()const noexcept {
			bool b = false;
			for_each_layer([&b](auto& l) {				b |= l.hasLossAddendum();			});
			return b;
		}
		//returns a loss function summand, that's caused by this layer
		real_t lossAddendum()const noexcept {
			real_t la(.0);
			for_each_layer([&la](auto& l) {				la += l.lossAddendum();			});
			return la;
		}

		//////////////////////////////////////////////////////////////////////////
		// we are not expecting this layer to be used on top of pack of other layers inside a compound layer,
		// therefore directly omitting last (special) parameter for that.
		ErrorCode init(_layer_init_data_t& lid)noexcept {
			m_max_fprop_batch_size = lid.max_fprop_batch_size;
			m_training_batch_size = lid.training_batch_size;
			NNTL_ASSERT(m_max_fprop_batch_size >= m_training_batch_size);

			//allocating m_activations
			NNTL_ASSERT(m_activations.emulatesBiases());
			if (!m_activations.resize(m_max_fprop_batch_size, m_neurons_cnt)) return ErrorCode::CantAllocateMemoryForActivations;

			m_pMath = &lid.iMath;

			ErrorCode ec = ErrorCode::Success;
			layer_index_t failedLayerIdx = 0;

			NNTL_ASSERT(0 == lid.max_dLdA_numel && 0 == lid.maxMemFPropRequire && 0 == lid.maxMemBPropRequire);
			auto initD = lid.dupe();
			auto& act = m_activations;
			neurons_count_t firstNeuronOfs = 0, maxIncNeuronsCnt = 0;
			for_each_layer([&](auto& l)noexcept {
				if (ErrorCode::Success == ec) {
					initD.clean();
					maxIncNeuronsCnt = std::max(maxIncNeuronsCnt, l.get_incoming_neurons_cnt());
					ec = l.init(initD, act.colDataAsVec(firstNeuronOfs));
					if (ErrorCode::Success == ec) {
						lid.update(initD);
						firstNeuronOfs += l.get_neurons_cnt();
					} else failedLayerIdx = l.get_layer_idx();
				}
			});
			NNTL_ASSERT(firstNeuronOfs + 1 == act.cols());

			if (ErrorCode::Success == ec) {
				//appending training requirements for this (top-level) layer

				//we'll need a column-vector of length m_max_fprop_batch_size to store a data column of previous layer activation,
				// that will be substituted by biases for fprop() of one inner layer
				lid.maxMemFPropRequire += m_max_fprop_batch_size;

				if (m_training_batch_size > 0) {
					//adding backprop requirements
					// saving max_dLdA_numel, gathered from inner layers and substituting it for this layer max_dLdA_numel
					m_layers_max_dLdA_numel = std::max(lid.max_dLdA_numel, realmtx_t::sNumel(m_training_batch_size, maxIncNeuronsCnt));
					lid.max_dLdA_numel = realmtx_t::sNumel(m_training_batch_size, m_neurons_cnt);
					
					//reserving memory for two inner dLdA matrices AND a column-vector of length m_max_fprop_batch_size to store a data column of previous layer activation
					lid.maxMemBPropRequire += m_max_fprop_batch_size + 2 * m_layers_max_dLdA_numel;
				}
			}
			//#TODO need some way to return failedLayerIdx
			return ec;
		}

		void deinit() noexcept {
			for_each_layer([](auto& l) {l.deinit(); });
			m_activations.clear();
			m_pTmpBiasStorage = nullptr;
			m_layers_max_dLdA_numel = 0;
			m_innerdLdA.clear();
			m_innerdLdAPrev.clear();
			m_pMath = nullptr;
		}

		void initMem(real_t* ptr, numel_cnt_t cnt)noexcept {
			//for fprop()
			NNTL_ASSERT(ptr && cnt >= m_max_fprop_batch_size);
			m_pTmpBiasStorage = ptr;
			ptr += m_max_fprop_batch_size;
			cnt -= m_max_fprop_batch_size;

			if (m_training_batch_size > 0) {
				NNTL_ASSERT(cnt >= 2 * m_layers_max_dLdA_numel);
				m_innerdLdA.useExternalStorage(ptr, m_layers_max_dLdA_numel, false);
				ptr += m_layers_max_dLdA_numel;
				m_innerdLdAPrev.useExternalStorage(ptr, m_layers_max_dLdA_numel, false);
				ptr += m_layers_max_dLdA_numel;
				cnt -= 2 * m_layers_max_dLdA_numel;
			}

			for_each_layer([=](auto& l) {l.initMem(ptr, cnt); });
		}

		// we are not expecting this layer to be used on top of pack of other layers inside a compound layer,
		// therefore directly omitting last (special) parameter for that.
		void set_mode(vec_len_t batchSize)noexcept {
			NNTL_ASSERT(m_activations.emulatesBiases());
			// now we must resize m_activations and update activations of inner layers with set_mode variation

			bool bRestoreBiases;
			if (0 == batchSize) {//training mode, batch size is predefined
				m_activations.deform_rows(m_training_batch_size);
				bRestoreBiases = m_training_batch_size != m_max_fprop_batch_size;
			} else {//evaluation mode, just make sure the batchSize is less then or equal to m_max_fprop_batch_size
				NNTL_ASSERT(batchSize <= m_max_fprop_batch_size);
				m_activations.deform_rows(batchSize);
				bRestoreBiases = batchSize != m_max_fprop_batch_size;
			}

			auto& act = m_activations;
			neurons_count_t firstNeuronOfs = 0;
			for_each_layer([batchSize, &act, &firstNeuronOfs](auto& lyr)noexcept {
				lyr.set_mode(batchSize, act.colDataAsVec(firstNeuronOfs));
				firstNeuronOfs += lyr.get_neurons_cnt();
			});
			NNTL_ASSERT(firstNeuronOfs + 1 == act.cols());

			if (bRestoreBiases) m_activations.set_biases();
			NNTL_ASSERT(m_activations.assert_biases_ok());
		}

		template <typename LowerLayer>
		void fprop(const LowerLayer& lowerLayer)noexcept{
			static_assert(std::is_base_of<_i_layer_fprop, LowerLayer>::value, "Template parameter LowerLayer must implement _i_layer_fprop");
			NNTL_ASSERT(m_activations.assert_biases_ok());
			const auto pTmpBiasStorage = m_pTmpBiasStorage;
			utils::for_each_up(m_phl_tuple, [&lowerLayer, pTmpBiasStorage](const auto& phl) {
				phl.l.fprop(_impl::trainable_partial_layer_wrapper<LowerLayer>(lowerLayer.get_activations(), pTmpBiasStorage, phl));
			});
			NNTL_ASSERT(m_activations.assert_biases_ok());
		}

		// in order to implement backprop for inner layers, we must provide them with correct dLdA and dLdAPrev, each of which must
		// address at least _layer_init_data_t::max_dLdA_numel elements, that layers returned during init() phase.
		// Some things to consider:
		// - there might be a compound layer in m_phl_tuple (such as layer_pack_vertical). That means, that it may require a far bigger
		// max_dLdA_numel, than might be provided by slicing dLdA passed to this function as argument to corresponding parts. So we'll
		// need a way to protect out-of-slice data from being overwritten by layer.bprop() (because we allow layer.brop() to use dLdA&dLdAPrev
		// almost without restrictions)
		// - inner layers may use the same (intersecting) lowerLayer activation units (because we don't require inner layers to use different
		// lower layer activations). That means, that after we'll get their individual dLdAPrev, we must aggregate them into resulting dLdAPrev.
		// 
		// Therefore it looks much more safe to allocate and use for inner layers special dLdA and dLdAPrev matrices, that are independent
		// from dLdA&dLdAPrev, passed to this function as argument. It's possible however to reuse passed dLdA&dLdAPrev for that task, but
		// it would require significantly more complicated and error-prone code to keep all data safe
		template <typename LowerLayer>
		const unsigned bprop(realmtx_t& dLdA, const LowerLayer& lowerLayer, realmtx_t& dLdAPrev)noexcept {
			static_assert(std::is_base_of<_i_layer_trainable, LowerLayer>::value, "Template parameter LowerLayer must implement _i_layer_trainable");
			NNTL_ASSERT(m_activations.assert_biases_ok());
			NNTL_ASSERT(dLdA.size() == get_self().get_activations().size_no_bias());
			NNTL_ASSERT((std::is_base_of<m_layer_input, LowerLayer>::value) || dLdAPrev.size() == lowerLayer.get_activations().size_no_bias());
			
			// We'll copy corresponding parts of dLdA into m_innerdLdA and on inner layer.bprop() return we'll ADD corresponding dLdA to dLdAPrev passed
			if (!std::is_base_of<m_layer_input, LowerLayer>::value) dLdAPrev.zeros();
			
			neurons_count_t firstNeuronOfs = 0;
			//the order of traversing m_phl_tuple is not important, because it's kind a 'parallel' work
			utils::for_each_up(m_phl_tuple, [&firstNeuronOfs, &lowerLayer, &dLdA, &dLdAPrev, this](const auto& phl) {
				auto& lyr = phl.l;
				
				constexpr bool bLowerLayerIsInput = std::is_base_of<m_layer_input, LowerLayer>::value;

				//setting up the m_innerdLdA
				m_innerdLdA.deform_like_no_bias(lyr.get_activations());
				NNTL_ASSERT(firstNeuronOfs + m_innerdLdA.cols() <= dLdA.cols());
				NNTL_ASSERT(m_innerdLdA.rows() == dLdA.rows() && m_training_batch_size == m_innerdLdA.rows());
				memcpy(m_innerdLdA.data(), dLdA.colDataAsVec(firstNeuronOfs), m_innerdLdA.byte_size());

				//setting up the m_innerdLdAPrev
				if (bLowerLayerIsInput) {
					m_innerdLdAPrev.deform(0, 0);
				}else m_innerdLdAPrev.deform(m_training_batch_size, phl.m_count);
				NNTL_ASSERT(bLowerLayerIsInput || m_innerdLdAPrev.rows() == dLdAPrev.rows());

				const auto switchMtxs = lyr.bprop( m_innerdLdA,
					_impl::trainable_partial_layer_wrapper<LowerLayer>(lowerLayer.get_activations(), m_pTmpBiasStorage, phl), m_innerdLdAPrev);

				if (!bLowerLayerIsInput) {
					NNTL_ASSERT(switchMtxs ? m_innerdLdAPrev.size() == realmtx_t::mtx_size_t(m_training_batch_size, phl.m_count)
						: m_innerdLdA.size() == realmtx_t::mtx_size_t(m_training_batch_size, phl.m_count));
					//saving m_innerdLdAPrev to dLdAPrev
					m_pMath->vAdd_ip(dLdAPrev.colDataAsVec(phl.m_offset), switchMtxs ? m_innerdLdAPrev.data() : m_innerdLdA.data()
						, realmtx_t::sNumel(m_training_batch_size, phl.m_count));
				}

				firstNeuronOfs += lyr.get_neurons_cnt();
			});
			NNTL_ASSERT(firstNeuronOfs + 1 == m_activations.cols());

			return 1;
		}

	private:
		//support for boost::serialization
		friend class boost::serialization::access;
		template<class Archive> void serialize(Archive & ar, const unsigned int version) {
			for_each_layer([&ar](auto& l) {
				constexpr size_t maxStrlen = 16;
				char lName[maxStrlen];
				l.get_layer_name(lName, maxStrlen);
				ar & serialization::make_named_struct(lName, l);
			});
		}
	};


	//////////////////////////////////////////////////////////////////////////
	// final implementation of layer with all functionality of _layer_fully_connected
	// If you need to derive a new class, derive it from _layer_fully_connected (to make static polymorphism work)
	template <typename ...PHLsT>
	class layer_pack_horizontal final
		: public _layer_pack_horizontal<layer_pack_horizontal<PHLsT...>, PHLsT...>
	{
	public:
		~layer_pack_horizontal() noexcept {};
		layer_pack_horizontal(PHLsT&... phls) noexcept
			: _layer_pack_horizontal<layer_pack_horizontal<PHLsT...>, PHLsT...>(phls...){};
	};

	template <typename ...PHLsT> inline
		layer_pack_horizontal <PHLsT...> make_layer_pack_horizontal(PHLsT&... phls) noexcept {
		return layer_pack_horizontal<PHLsT...>(phls...);
	}
}