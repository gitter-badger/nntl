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

// LI defines a layer that just passes it's incoming neurons as activation neurons without any processing.
// Can be used to pass a source data unmodified to some upper feature detectors.
// 
// LIG can also serve as a gating source for the layer_pack_horizontal_gated.
// 
// Also, be aware that if the drop_samples() of LI/LIG gets called, then the mask
// passed to the drop_samples() does NOT get passed to the donoring/lower layer. This is an intentional behavior and it won't hurt
// you provided you've assembled right NN architecture.
// 
// Both classes (LI/LIG) are intended to be used within a layer_pack_horizontal (and can't/shouldn't be used elsewhere)

#include <type_traits>

#include "_layer_base.h"

namespace nntl {

	template<typename FinalPolymorphChild, typename Interfaces>
	class _LI : public _layer_base<FinalPolymorphChild, Interfaces>, public m_layer_autoneurons_cnt
	{
	private:
		typedef _layer_base<FinalPolymorphChild, Interfaces> _base_class;

		//////////////////////////////////////////////////////////////////////////
		//members section (in "biggest first" order)
	protected:
		realmtxdef_t m_activations;

	public:
		~_LI()noexcept {}
		_LI(const char* pCustomName)noexcept : _base_class(0, pCustomName) {
			m_activations.will_emulate_biases();
		}
		/*_LI(const char* pCustomName, const neurons_count_t)noexcept : _base_class(pCustomName) {
			m_gate.dont_emulate_biases();
			STDCOUTL("*** WARNING: non standard contructor of _LI<> has been used. OK for for gradcheck only");
		}*/

		static constexpr const char _defName[] = "li";

		static constexpr bool hasLossAddendum()noexcept { return false; }
		//returns a loss function summand, that's caused by this layer (for example, L2 regularizer adds term
		// l2Coefficient*Sum(weights.^2) )
		static constexpr real_t lossAddendum()noexcept { return real_t(0.); }

		//////////////////////////////////////////////////////////////////////////
		const realmtxdef_t& get_activations()const noexcept {
			NNTL_ASSERT(m_bActivationsValid);
			return m_activations;
		}
		const realmtxdef_t* get_activations_storage()const noexcept { return &m_activations; }
		const mtx_size_t get_activations_size()const noexcept { return m_activations.size(); }

		const bool is_activations_shared()const noexcept {
			const auto r = _base_class::is_activations_shared();
			NNTL_ASSERT(!r || m_activations.bDontManageStorage());//shared activations can't manage their own storage
			return r;
		}

		// pNewActivationStorage MUST be specified (we're expecting to be encapsulated into a layer_pack_horizontal)
		ErrorCode init(_layer_init_data_t& lid, real_t* pNewActivationStorage)noexcept {
			NNTL_ASSERT(pNewActivationStorage);

			auto ec = _base_class::init(lid, pNewActivationStorage);
			if (ErrorCode::Success != ec) return ec;

			NNTL_ASSERT(get_self().get_neurons_cnt());
			m_activations.useExternalStorage(pNewActivationStorage, get_self().get_common_data().biggest_batch_size(), get_self().get_neurons_cnt() + 1, true);
			return ec;
		}

		void deinit() noexcept {
			m_activations.clear();
			_base_class::deinit();
		}
		void initMem(real_t* , numel_cnt_t )noexcept {}

		// pNewActivationStorage MUST be specified (we're expecting to be encapsulated into layer_pack_horizontal)
		void on_batch_size_change(real_t*const pNewActivationStorage)noexcept {
			NNTL_ASSERT(pNewActivationStorage);
			m_bActivationsValid = false;
			const vec_len_t batchSize = get_self().get_common_data().get_cur_batch_size();
			NNTL_ASSERT(batchSize <= get_self().get_common_data().biggest_batch_size());

			NNTL_ASSERT(m_activations.emulatesBiases() && get_self().is_activations_shared() && get_self().get_neurons_cnt());
			//m_neurons_cnt + 1 for biases
			m_activations.useExternalStorage(pNewActivationStorage, batchSize, get_self().get_neurons_cnt() + 1, true);
			//should not restore biases here, because for compound layers its a job for their fprop() implementation
		}
	protected:

		void _fprop(const realmtx_t& prevActivations)noexcept {
			NNTL_ASSERT(prevActivations.size() == m_activations.size());
			NNTL_ASSERT(get_self().is_activations_shared());
			NNTL_ASSERT(prevActivations.test_biases_ok());
			NNTL_ASSERT(m_activations.rows() == get_self().get_common_data().get_cur_batch_size());

			auto& iI = get_self().get_iInspect();
			iI.fprop_begin(get_self().get_layer_idx(), prevActivations, get_self().get_common_data().is_training_mode());

			//restoring biases, should they were altered in drop_samples()
			if (m_activations.isHoleyBiases() && !get_self().is_activations_shared()) {
				m_activations.set_biases();
			}

			// just copying the data from prevActivations to m_activations
			// We must copy the data, because layer_pack_horizontal uses its own storage for activations, therefore
			// we can't just use the m_activations as an alias to prevActivations - we have to physically copy the data
			// to a new storage within layer_pack_horizontal activations
			//const bool r = prevActivations.clone_to(m_activations);
			// we mustn't touch the bias column!
			const auto r = prevActivations.copy_data_skip_bias(m_activations);
			NNTL_ASSERT(r);

			iI.fprop_activations(m_activations);
			iI.fprop_end(m_activations);
		}

	public:
		//we're restricting the use of the LI to the layer_pack_horizontal only
		template <typename LowerLayerWrapper>
		::std::enable_if_t<_impl::is_layer_wrapper<LowerLayerWrapper>::value> fprop(const LowerLayerWrapper& lowerLayer)noexcept
		{
			static_assert(::std::is_base_of<_i_layer_fprop, LowerLayerWrapper>::value, "Template parameter LowerLayerWrapper must implement _i_layer_fprop");
			NNTL_ASSERT(lowerLayer.get_activations().test_biases_ok());
			get_self()._fprop(lowerLayer.get_activations());
			NNTL_ASSERT(lowerLayer.get_activations().test_biases_ok());
			m_bActivationsValid = true;
		}

		template <typename LowerLayerWrapper>
		::std::enable_if_t<_impl::is_layer_wrapper<LowerLayerWrapper>::value, const unsigned>
			bprop(realmtx_t& dLdA, const LowerLayerWrapper& lowerLayer, realmtx_t& dLdAPrev)noexcept
		{
			NNTL_UNREF(dLdAPrev); NNTL_UNREF(lowerLayer);
			NNTL_ASSERT(m_bActivationsValid);
			NNTL_ASSERT(m_activations.rows() == get_self().get_common_data().get_cur_batch_size());
			m_bActivationsValid = false;
			auto& iI = get_self().get_iInspect();
			iI.bprop_begin(get_self().get_layer_idx(), dLdA);
			iI.bprop_finaldLdA(dLdA);

			//nothing to do here
			NNTL_ASSERT(lowerLayer.get_activations().test_biases_ok());

			iI.bprop_end(dLdA);
			return 0;//indicating that dL/dA for a previous layer is actually in the dLdA parameter (not in the dLdAPrev)
		}

		static constexpr bool is_trivial_drop_samples()noexcept { return true; }

		static constexpr void left_after_drop_samples(const numel_cnt_t nNZElems)noexcept {
			NNTL_UNREF(nNZElems);
		}

		void drop_samples(const realmtx_t& mask, const bool bBiasesToo, const numel_cnt_t nNZElems)noexcept {
			NNTL_UNREF(nNZElems);
			NNTL_ASSERT(m_bActivationsValid);
			NNTL_ASSERT(get_self().is_drop_samples_mbc());
			NNTL_ASSERT(!get_self().is_activations_shared() || !bBiasesToo);
			NNTL_ASSERT(!mask.emulatesBiases() && 1 == mask.cols() && m_activations.rows()==mask.rows() && mask.isBinary());
			NNTL_ASSERT(m_activations.emulatesBiases());

			m_activations.hide_last_col();
			get_self().get_iMath().mrwMulByVec(m_activations, mask.data());
			m_activations.restore_last_col();

			if (bBiasesToo) {
				m_activations.copy_biases_from(mask.data());
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// other funcs
	protected:
		friend class _impl::_preinit_layers;
		void _preinit_layer(_impl::init_layer_index& ili, const neurons_count_t inc_neurons_cnt)noexcept {
			NNTL_ASSERT(inc_neurons_cnt > 0);
			NNTL_ASSERT(get_self().get_neurons_cnt() == inc_neurons_cnt);

			_base_class::_preinit_layer(ili, inc_neurons_cnt);
			//get_self()._set_neurons_cnt(inc_neurons_cnt);
		}

	private:
		//////////////////////////////////////////////////////////////////////////
		//Serialization support
		friend class ::boost::serialization::access;
		template<class Archive> void serialize(Archive & ar, const unsigned int ) {
			if (utils::binary_option<true>(ar, serialization::serialize_activations)) ar & NNTL_SERIALIZATION_NVP(m_activations);
		}

	};

	//////////////////////////////////////////////////////////////////////////
	template<typename FinalPolymorphChild, typename Interfaces>
	class _LIG : public _LI<FinalPolymorphChild, Interfaces>
		, public _i_layer_gate<typename Interfaces::iMath_t::real_t>
	{
	private:
		typedef _LI<FinalPolymorphChild, Interfaces> _base_class;

	public:
		using _base_class::real_t;

		//////////////////////////////////////////////////////////////////////////
		//members section (in "biggest first" order)
	protected:
		//gate matrix doesn't have a bias column, therefore we can't directly use m_activations variable and
		// should create a separate alias to the activation storage.
		realmtxdef_t m_gate;

	public:
		~_LIG()noexcept {}
		_LIG(const char* pCustomName)noexcept : _base_class(pCustomName) {
			m_gate.dont_emulate_biases();
		}

		static constexpr const char _defName[] = "lig";

		//////////////////////////////////////////////////////////////////////////
		const realmtx_t& get_gate()const noexcept {
			NNTL_ASSERT(m_bActivationsValid);
			return m_gate;
		}
		const vec_len_t get_gate_width()const noexcept { return get_self().get_neurons_cnt(); }

		// pNewActivationStorage MUST be specified (we're expecting to be encapsulated into a layer_pack_horizontal)
		ErrorCode init(_layer_init_data_t& lid, real_t* pNewActivationStorage)noexcept {
			NNTL_ASSERT(pNewActivationStorage);

			auto ec = _base_class::init(lid, pNewActivationStorage);
			if (ErrorCode::Success != ec) return ec;

			NNTL_ASSERT(get_self().get_neurons_cnt());
			m_gate.useExternalStorage(pNewActivationStorage, get_self().get_common_data().biggest_batch_size(), get_self().get_neurons_cnt(), false);
			return ec;
		}

		void deinit() noexcept {
			m_gate.clear();
			_base_class::deinit();
		}
		// pNewActivationStorage MUST be specified (we're expecting to be encapsulated into layer_pack_horizontal)
		void on_batch_size_change(real_t*const pNewActivationStorage)noexcept {
			NNTL_ASSERT(pNewActivationStorage);
			_base_class::on_batch_size_change(pNewActivationStorage);

			const vec_len_t batchSize = get_self().get_common_data().get_cur_batch_size();
			m_gate.useExternalStorage(pNewActivationStorage, batchSize, get_self().get_neurons_cnt(), false);
		}

	private:
		//////////////////////////////////////////////////////////////////////////
		//Serialization support
		friend class ::boost::serialization::access;
		template<class Archive> void serialize(Archive & ar, const unsigned int) {
			//#todo must correctly call base class serialize()
			//NNTL_ASSERT(!"must correctly call base class serialize()");
			ar & ::boost::serialization::base_object<_base_class>(*this);
			//ar & serialization::serialize_base_class<_base_class>(*this);
		}
	};

	//////////////////////////////////////////////////////////////////////////
	//
	// 
	template <typename Interfaces = d_interfaces>
	class LI final : public _LI<LI<Interfaces>, Interfaces>
	{
	public:
		~LI() noexcept {};
		LI(const char* pCustomName = nullptr) noexcept 
			: _LI<LI<Interfaces>, Interfaces> (pCustomName) {};
	};

	template <typename Interfaces = d_interfaces>
	using layer_identity = LI<Interfaces>;

	template <typename Interfaces = d_interfaces>
	class LIG final : public _LIG<LIG<Interfaces>, Interfaces>
	{
	public:
		~LIG() noexcept {};
		LIG(const char* pCustomName = nullptr) noexcept 
			: _LIG<LIG<Interfaces>, Interfaces>(pCustomName) {};
	};

	template <typename Interfaces = d_interfaces>
	using layer_identity_gate = LIG<Interfaces>;
}
