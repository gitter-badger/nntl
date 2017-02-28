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

#include "_layer_base.h"

//This is a basic building block of almost any feedforward neural network - fully connected layer of neurons.

namespace nntl {

	template<typename ActivFunc, typename GradWorks, typename FinalPolymorphChild>
	class _layer_fully_connected : public _layer_base<typename GradWorks::interfaces_t, FinalPolymorphChild>
	{
	private:
		typedef _layer_base<typename GradWorks::interfaces_t, FinalPolymorphChild> _base_class;

	public:
		typedef ActivFunc activation_f_t;
		static_assert(std::is_base_of<activation::_i_activation<real_t>, activation_f_t>::value, "ActivFunc template parameter should be derived from activation::_i_function");

		typedef GradWorks grad_works_t;
		static_assert(std::is_base_of<_impl::_i_grad_works<real_t>, grad_works_t>::value, "GradWorks template parameter should be derived from _i_grad_works");

		static constexpr const char _defName[] = "fcl";

		//////////////////////////////////////////////////////////////////////////
		//members
	protected:
		// matrix of layer neurons activations: <batch_size rows> x <m_neurons_cnt+1(bias) cols> for fully connected layer
		// Class assumes, that it's content on the beginning of the bprop() is the same as it was on exit from fprop().
		// Don't change it outside of the class!
		realmtxdef_t m_activations;

		// layer weight matrix: <m_neurons_cnt rows> x <m_incoming_neurons_cnt +1(bias)>,
		// i.e. weights for individual neuron are stored row-wise (that's necessary to make fast cut-off of bias-related weights
		// during backpropagation  - and that's the reason, why is it deformable)
		realmtxdef_t m_weights;
		
		//matrix of dropped out neuron activations, used when 1>m_dropoutPercentActive>0
		realmtxdef_t m_dropoutMask;//<batch_size rows> x <m_neurons_cnt cols> (must not have a bias column)
		real_t m_dropoutPercentActive;//probability of keeping unit active

		//realmtx_t m_dAdZ_dLdZ;//doesn't guarantee to retain it's value between usage in different code flows; may share memory with some other data structure

		realmtxdef_t m_dLdW;//doesn't guarantee to retain it's value between usage in different code flows;
		// may share memory with some other data structure. Must be deformable for grad_works_t

	public:
		grad_works_t m_gradientWorks;

	protected:
		//this flag controls the weights matrix initialization and prevents reinitialization on next nnet.train() calls
		bool m_bWeightsInitialized;

		//////////////////////////////////////////////////////////////////////////
		//Serialization support
	private:
		friend class boost::serialization::access;
		template<class Archive>
		void serialize(Archive & ar, const unsigned int version) {
			//NB: DONT touch ANY of .useExternalStorage() matrices here, because it's absolutely temporary meaningless data
			// and moreover, underlying storage may have already been freed.

			if (utils::binary_option<true>(ar, serialization::serialize_training_parameters)) {
				ar & NNTL_SERIALIZATION_NVP(m_dropoutPercentActive);
				//ar & NNTL_SERIALIZATION_NVP(m_max_fprop_batch_size);
				//ar & NNTL_SERIALIZATION_NVP(m_training_batch_size);
			}
			
			if (utils::binary_option<true>(ar, serialization::serialize_activations)) ar & NNTL_SERIALIZATION_NVP(m_activations);
			
			if (utils::binary_option<true>(ar, serialization::serialize_weights)) ar & NNTL_SERIALIZATION_NVP(m_weights);

			if (utils::binary_option<true>(ar, serialization::serialize_grad_works)) ar & m_gradientWorks;//dont use nvp or struct here for simplicity

			if (bDropout() && utils::binary_option<true>(ar, serialization::serialize_dropout_mask)) ar & NNTL_SERIALIZATION_NVP(m_dropoutMask);
		}


		//////////////////////////////////////////////////////////////////////////
		// functions
	public:
		~_layer_fully_connected() noexcept {};
		_layer_fully_connected(const neurons_count_t _neurons_cnt
			, const real_t learningRate = real_t(.01)
			, const real_t dpa = real_t(1.0) //"dropout_percent_alive"
			, const char* pCustomName=nullptr
		)noexcept
			: _base_class(_neurons_cnt, pCustomName), m_activations(), m_weights(), m_bWeightsInitialized(false)
				, m_gradientWorks(learningRate)
				, m_dropoutMask(), m_dropoutPercentActive(dpa)
		{
			if (m_dropoutPercentActive <= real_t(+0.) || m_dropoutPercentActive > real_t(1.)) m_dropoutPercentActive = real_t(1.);
			m_activations.will_emulate_biases();
		};
		
		// Class assumes, that the content of the m_activations matrix on the beginning of the bprop() is the same as it was on exit from fprop().
		// Don't change it outside of the class!
		const realmtxdef_t& get_activations()const noexcept {
			NNTL_ASSERT(m_bActivationsValid);
			return m_activations;
		}
		const mtx_size_t get_activations_size()const noexcept { return m_activations.size(); }

		const bool is_activations_shared()const noexcept {
			const auto r = _base_class::is_activations_shared();
			NNTL_ASSERT(!r || m_activations.bDontManageStorage());//shared activations can't manage their own storage
			return r;
		}

		//#TODO: move all generic fullyconnected stuff into a special base class!

		const realmtx_t& get_weights()const noexcept {
			NNTL_ASSERT(m_bWeightsInitialized);
			return m_weights;
		}
		bool set_weights(realmtx_t&& W)noexcept {
			if (W.empty() || W.emulatesBiases()
				|| (W.cols() != get_self().get_incoming_neurons_cnt() + 1)
				|| W.rows() != get_self().get_neurons_cnt())
			{
				NNTL_ASSERT(!"Wrong weight matrix passed!");
				return false;
			}

			//m_weights = std::move(W);
			m_weights = std::forward<realmtx_t>(W);
			m_bWeightsInitialized = true;
			return true;
		}

		ErrorCode init(_layer_init_data_t& lid, real_t* pNewActivationStorage = nullptr)noexcept {
			auto ec = _base_class::init(lid, pNewActivationStorage);
			if (ErrorCode::Success != ec) return ec;

			bool bSuccessfullyInitialized = false;
			utils::scope_exit onExit([&bSuccessfullyInitialized, this]() {
				if (!bSuccessfullyInitialized) get_self().deinit();
			});
			
			NNTL_ASSERT(!m_weights.emulatesBiases());
			if (m_bWeightsInitialized) {
				//just double check everything is fine
				NNTL_ASSERT(get_self().get_neurons_cnt() == m_weights.rows());
				NNTL_ASSERT(get_self().get_incoming_neurons_cnt() + 1 == m_weights.cols());
				NNTL_ASSERT(!m_weights.empty());
			} else {
				//TODO: initialize weights from storage for nn eval only

				// initializing
				if (!m_weights.resize(get_self().get_neurons_cnt(), get_incoming_neurons_cnt() + 1)) return ErrorCode::CantAllocateMemoryForWeights;

				if (!activation_f_t::weights_scheme::init(m_weights, get_self().get_iRng(), get_self().get_iMath()))return ErrorCode::CantInitializeWeights;

				m_bWeightsInitialized = true;
			}

			lid.nParamsToLearn = m_weights.numel();

			const auto biggestBatchSize = get_self().get_biggest_batch_size();

			NNTL_ASSERT(m_activations.emulatesBiases());
			if (pNewActivationStorage) {
				m_activations.useExternalStorage(pNewActivationStorage, biggestBatchSize, get_self().get_neurons_cnt() + 1, true);
			} else {
				if (!m_activations.resize(biggestBatchSize, get_self().get_neurons_cnt()))
					return ErrorCode::CantAllocateMemoryForActivations;
			}

			//Math interface may have to operate on the following matrices:
			// m_weights, m_dLdW - (m_neurons_cnt, get_incoming_neurons_cnt() + 1)
			// m_activations - (biggestBatchSize, m_neurons_cnt+1) and unbiased matrices derived from m_activations - such as m_dAdZ
			// prevActivations - size (m_training_batch_size, get_incoming_neurons_cnt() + 1)
			get_self().get_iMath().preinit(std::max({
				m_weights.numel()
				,activation_f_t::needTempMem(m_activations,get_self().get_iMath())
				,realmtx_t::sNumel(get_self().get_training_batch_size(), get_incoming_neurons_cnt() + 1)
			}));

			if (get_self().get_training_batch_size() > 0) {
				//it'll be training session, therefore must allocate necessary supplementary matrices and form temporary memory reqs.
 				if(!_check_init_dropout()) return ErrorCode::CantAllocateMemoryForDropoutMask;

				lid.max_dLdA_numel = realmtx_t::sNumel(get_self().get_training_batch_size(), get_self().get_neurons_cnt());
				// we'll need 1 temporarily matrix for bprop(): it is a dL/dW [m_neurons_cnt x get_incoming_neurons_cnt()+1]
				lid.maxMemTrainingRequire = m_weights.numel();
			}

			if (!m_gradientWorks.init(get_self().get_common_data(), m_weights.size()))return ErrorCode::CantInitializeGradWorks;

			lid.bHasLossAddendum = hasLossAddendum();
			lid.bOutputDifferentDuringTraining = bDropout();

			bSuccessfullyInitialized = true;
			return ec;
		}

		void deinit() noexcept {
			m_gradientWorks.deinit();
			m_activations.clear();
			m_dropoutMask.clear();
			m_dLdW.clear();
			_base_class::deinit();
		}

		void initMem(real_t* ptr, numel_cnt_t cnt)noexcept {
			if (get_self().get_training_batch_size() > 0) {
				NNTL_ASSERT(ptr && cnt >= m_weights.numel());
				m_dLdW.useExternalStorage(ptr, m_weights);
				NNTL_ASSERT(!m_dLdW.emulatesBiases());
			}
		}
		
		void set_batch_size(const vec_len_t batchSize, real_t*const pNewActivationStorage = nullptr)noexcept {
			NNTL_ASSERT(batchSize > 0);
			NNTL_ASSERT(m_activations.emulatesBiases());
			m_bActivationsValid = false;

			const auto _biggest_batch_size = get_self().get_biggest_batch_size();
			NNTL_ASSERT(batchSize <= _biggest_batch_size);

			if (pNewActivationStorage) {
				NNTL_ASSERT(m_activations.bDontManageStorage());
				//m_neurons_cnt + 1 for biases
				m_activations.useExternalStorage(pNewActivationStorage, batchSize, get_self().get_neurons_cnt() + 1, true);
				//should not restore biases here, because for compound layers its a job for their fprop() implementation
			} else {
				NNTL_ASSERT(!m_activations.bDontManageStorage());
				//we don't need to restore biases in one case - if new row count equals to maximum (_biggest_batch_size). Then the original
				//(filled during resize()) bias column has been untouched
				m_activations.deform_rows(batchSize);
				if (batchSize != _biggest_batch_size) m_activations.set_biases();
				NNTL_ASSERT(m_activations.test_biases_ok());
			}

			if (get_self().isTrainingMode() && get_self().bDropout()) {
				NNTL_ASSERT(!m_dropoutMask.empty());
				m_dropoutMask.deform_rows(batchSize);
				NNTL_ASSERT(m_dropoutMask.size() == m_activations.size_no_bias());
			}
		}

	protected:
		//help compiler to isolate fprop functionality from the specific of previous layer
		void _fprop(const realmtx_t& prevActivations)noexcept {
			const bool bTrainingMode = get_self().isTrainingMode();
			auto& iI = get_self().get_iInspect();
			iI.fprop_begin(get_self().get_layer_idx(), prevActivations, bTrainingMode);

			//restoring biases, should they were altered in drop_samples()
			if (m_activations.isHoleyBiases() && !get_self().is_activations_shared()) {
				m_activations.set_biases();
			}

			NNTL_ASSERT(prevActivations.test_biases_ok());
			NNTL_ASSERT(m_activations.rows() == prevActivations.rows());
			NNTL_ASSERT(prevActivations.cols() == m_weights.cols());

			//might be necessary for Nesterov momentum application
			if (bTrainingMode) m_gradientWorks.pre_training_fprop(m_weights);

			auto& _Math = get_self().get_iMath();

			iI.fprop_makePreActivations(m_weights, prevActivations);
			_Math.mMulABt_Cnb(prevActivations, m_weights, m_activations);
			iI.fprop_preactivations(m_activations);

			NNTL_ASSERT(get_self().is_activations_shared() || m_activations.test_biases_ok());
			activation_f_t::f(m_activations, _Math);
			iI.fprop_activations(m_activations);

			NNTL_ASSERT(get_self().is_activations_shared() || m_activations.test_biases_ok());

			if (bDropout()) {
				NNTL_ASSERT(real_t(0.) < m_dropoutPercentActive && m_dropoutPercentActive < real_t(1.));
				if (bTrainingMode) {
					//must make dropoutMask and apply it
					NNTL_ASSERT(m_dropoutMask.size() == m_activations.size_no_bias());
					get_self().get_iRng().gen_matrix_norm(m_dropoutMask);
					iI.fprop_preDropout(m_activations, m_dropoutPercentActive, m_dropoutMask);
					_Math.make_dropout(m_activations, m_dropoutPercentActive, m_dropoutMask);
					iI.fprop_postDropout(m_activations, m_dropoutMask);
				} else {
					//only applying dropoutPercentActive -- we don't need to do this since make_dropout() implements so called 
					// "inverse dropout" that doesn't require this step
					//_Math.evMulC_ip_Anb(m_activations, real_t(1.0) - m_dropoutPercentActive);
				}
				NNTL_ASSERT(get_self().is_activations_shared() || m_activations.test_biases_ok());
			}

			//TODO?: sparsity penalty here

			NNTL_ASSERT(prevActivations.test_biases_ok());
			iI.fprop_end(m_activations);
			m_bActivationsValid = true;
		}

		void _cust_inspect(const realmtx_t& M)const noexcept{}

		void _bprop(realmtx_t& dLdA, const realmtx_t& prevActivations, const bool bPrevLayerIsInput, realmtx_t& dLdAPrev)noexcept {
			NNTL_ASSERT(m_bActivationsValid);
			m_bActivationsValid = false;

			auto& iI = get_self().get_iInspect();
			iI.bprop_begin(get_self().get_layer_idx(), dLdA);

			dLdA.assert_storage_does_not_intersect(dLdAPrev);
			dLdA.assert_storage_does_not_intersect(m_dLdW);
			dLdAPrev.assert_storage_does_not_intersect(m_dLdW);
			NNTL_ASSERT(get_self().isTrainingMode());
			NNTL_ASSERT(prevActivations.test_biases_ok());

			NNTL_ASSERT(m_activations.emulatesBiases() && !m_dLdW.emulatesBiases());

			NNTL_ASSERT(m_activations.size_no_bias() == dLdA.size());
			//NNTL_ASSERT(m_dAdZ_dLdZ.size() == m_activations.size_no_bias());
			NNTL_ASSERT(m_dLdW.size() == m_weights.size());

			NNTL_ASSERT(bPrevLayerIsInput || prevActivations.emulatesBiases());//input layer in batch mode may have biases included, but no emulatesBiases() set
			NNTL_ASSERT(mtx_size_t(get_self().get_training_batch_size(), get_incoming_neurons_cnt() + 1) == prevActivations.size());
			NNTL_ASSERT(bPrevLayerIsInput || dLdAPrev.size() == prevActivations.size_no_bias());//in vanilla simple BP we shouldn't calculate dLdAPrev for the first layer			

			auto& _Math = get_self().get_iMath();
			const bool bUseDropout = bDropout();

			if (bUseDropout) {
				//we must undo the scaling step from inverted dropout in order to obtain correct activation values
				//That is must be done as a basis to obtain correct dL/dA
				iI.bprop_preCancelDropout(m_activations, m_dropoutPercentActive);
				_Math.evMulC_ip_Anb(m_activations, m_dropoutPercentActive);
				iI.bprop_postCancelDropout(m_activations);
			}
			
			iI.bprop_predAdZ(m_activations);
			
			realmtx_t dLdZ;
			dLdZ.useExternalStorage_no_bias(m_activations);

			//computing dA/dZ using m_activations (aliased to dLdZ variable, which eventually will be a dL/dZ
			activation_f_t::df(dLdZ, _Math);//it's a dA/dZ actually
			iI.bprop_dAdZ(dLdZ);
			//compute dL/dZ=dL/dA.*dA/dZ into dA/dZ
			_Math.evMul_ip(dLdZ, dLdA);
			iI.bprop_dLdZ(dLdZ);

			//NB: if we're going to use some kind of regularization of the activation values, we should make sure, that excluded
			//activations (for example, by a dropout or by a gating layer) aren't influence the regularizer. For the dropout
			// there's a m_dropoutMask available (it has zeros for excluded and 1/m_dropoutPercentActive for included activations).
			// By convention, gating layer and other external 'things' would pass dLdA with zeroed elements, that corresponds to
			// excluded activations, because by the definition dL/dA_i == 0 means that i-th activation value should be left intact.

			if (bUseDropout) {
				//we must cancel activations that was dropped out by the mask (should they've been restored by activation_f_t::df())
				//and restore the scale of dL/dZ according to 1/p
				//because the true scaled_dL/dZ = 1/p * computed_dL/dZ
				NNTL_ASSERT(m_dropoutMask.size() == dLdA.size());
				_Math.evMul_ip(dLdZ, m_dropoutMask);
			}

			get_self()._cust_inspect(dLdZ);

			//compute dL/dW = 1/batchsize * (dL/dZ)` * Aprev
			// BTW: even if some of neurons of this layer could have been "disabled" by a dropout (therefore their
			// corresponding dL/dZ element is set to zero), because we're working with batches, but not a single samples,
			// due to averaging the dL/dW over the whole batch 
			// (dLdW(i's neuron,j's lower layer neuron) = Sum_over_batch( dLdZ(i)*Aprev(j) ) ), it's almost impossible
			// to get some element of dLdW equals to zero, because it'll require that dLdZ entries for some neuron over the
			// whole batch were set to zero.
			_Math.mScaledMulAtB_C(real_t(1.0) / real_t(dLdZ.rows()), dLdZ, prevActivations, m_dLdW);

			if (!bPrevLayerIsInput) {
				NNTL_ASSERT(!m_weights.emulatesBiases());
				//finally compute dL/dAprev to use in lower layer. Before that make m_weights looks like there is no bias weights
				m_weights.hide_last_col();
				_Math.mMulAB_C(dLdZ, m_weights, dLdAPrev);
				m_weights.restore_last_col();//restore weights back
			}

			//now we can apply gradient to the weights
			m_gradientWorks.apply_grad(m_weights, m_dLdW);

			NNTL_ASSERT(prevActivations.test_biases_ok());

			iI.bprop_end(dLdAPrev);
		}

	public:
		template <typename LowerLayer>
		void fprop(const LowerLayer& lowerLayer)noexcept {
			static_assert(std::is_base_of<_i_layer_fprop, LowerLayer>::value, "Template parameter LowerLayer must implement _i_layer_fprop");
			NNTL_ASSERT(lowerLayer.get_activations().test_biases_ok());
			get_self()._fprop(lowerLayer.get_activations());
			NNTL_ASSERT(lowerLayer.get_activations().test_biases_ok());
		}

		template <typename LowerLayer>
		const unsigned bprop(realmtx_t& dLdA, const LowerLayer& lowerLayer, realmtx_t& dLdAPrev)noexcept {
			static_assert(std::is_base_of<_i_layer_trainable, LowerLayer>::value, "Template parameter LowerLayer must implement _i_layer_trainable");
			NNTL_ASSERT(lowerLayer.get_activations().test_biases_ok());
			get_self()._bprop(dLdA, lowerLayer.get_activations(), std::is_base_of<m_layer_input, LowerLayer>::value, dLdAPrev);
			NNTL_ASSERT(lowerLayer.get_activations().test_biases_ok());
			return 1;
		}

		static constexpr bool is_trivial_drop_samples()noexcept { return true; }

		void drop_samples(const realmtx_t& mask, const bool bBiasesToo)noexcept {
			NNTL_ASSERT(m_bActivationsValid);
			NNTL_ASSERT(get_self().is_drop_samples_mbc());
			NNTL_ASSERT(!get_self().is_activations_shared() || !bBiasesToo);
			NNTL_ASSERT(!mask.emulatesBiases() && 1 == mask.cols() && m_activations.rows() == mask.rows() && mask.isBinary());
			NNTL_ASSERT(m_activations.emulatesBiases());

			m_activations.hide_last_col();
			get_self().get_iMath().mrwMulByVec(m_activations, mask.data());
			m_activations.restore_last_col();

			if (bBiasesToo) {
				m_activations.copy_biases_from(mask.data());
			}
		}

		//////////////////////////////////////////////////////////////////////////

		//returns a loss function summand, that's caused by this layer (for example, L2 regularizer adds term
		// l2Coefficient*Sum(weights.^2) )
		real_t lossAddendum()const noexcept { return m_gradientWorks.lossAddendum(m_weights); }
		//should return true, if the layer has a value to add to Loss function value (there's some regularizer attached)
		bool hasLossAddendum()const noexcept { return m_gradientWorks.hasLossAddendum(); }

		//////////////////////////////////////////////////////////////////////////

		const real_t dropoutPercentActive()const noexcept { return m_dropoutPercentActive; }
		self_ref_t dropoutPercentActive(real_t dpa)noexcept {
			NNTL_ASSERT(real_t(0.) < dpa && dpa <= real_t(1.));
			if (dpa <= real_t(+0.) || dpa > real_t(1.)) dpa = real_t(1.);
			m_dropoutPercentActive = dpa;
			if (!get_self()._check_init_dropout()) {
				NNTL_ASSERT(!"Failed to init dropout, probably no memory");
				abort();
			}
			return get_self();
		}
		const bool bDropout()const noexcept { return m_dropoutPercentActive < real_t(1.); }

	protected:
		const bool _check_init_dropout()noexcept {
			NNTL_ASSERT(get_self().has_common_data());
			if (get_self().get_training_batch_size() > 0 && get_self().bDropout()) {
				//condition means if (there'll be a training session) and (we're going to use dropout)
				NNTL_ASSERT(!m_dropoutMask.emulatesBiases());
				if (m_dropoutMask.empty()) {
					//resize to the biggest possible size during training
					if (!m_dropoutMask.resize(get_self().get_training_batch_size(), get_self().get_neurons_cnt())) return false;
				}//else expecting it to have correct maximum size

				if (get_self().isTrainingMode()) {
					//change size to fit m_activations
					m_dropoutMask.deform_rows(m_activations.rows());
				}//else will be changed during set_batch_size()
			}
			return true;
		}

		friend class _impl::_preinit_layers;
		void _preinit_layer(_impl::init_layer_index& ili, const neurons_count_t inc_neurons_cnt)noexcept {
			NNTL_ASSERT(0 < inc_neurons_cnt);
			_base_class::_preinit_layer(ili, inc_neurons_cnt);
			NNTL_ASSERT(get_self().get_layer_idx() > 0);
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// final implementation of layer with all functionality of _layer_fully_connected
	// If you need to derive a new class, derive it from _layer_fully_connected (to make static polymorphism work)
	template <typename ActivFunc = activation::sigm<d_interfaces::real_t>,
		typename GradWorks = grad_works<d_interfaces>
	> class LFC final 
		: public _layer_fully_connected<ActivFunc, GradWorks, LFC<ActivFunc, GradWorks>>
	{
	public:
		~LFC() noexcept {};
		LFC(const neurons_count_t _neurons_cnt, const real_t learningRate = real_t(.01)
			, const real_t dropoutFrac = real_t(0.0), const char* pCustomName = nullptr
		)noexcept
			: _layer_fully_connected<ActivFunc, GradWorks, LFC<ActivFunc, GradWorks>>
			(_neurons_cnt, learningRate, dropoutFrac, pCustomName) {};
	};

	template <typename ActivFunc = activation::sigm<d_interfaces::real_t>,
		typename GradWorks = grad_works<d_interfaces>
	> using layer_fully_connected = typename LFC<ActivFunc, GradWorks>;
}

