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

#include "../_nnet_errs.h"
#include "../interfaces.h"
#include "../serialization/serialization.h"

#include "../grad_works/grad_works.h"
#include "_init_layers.h"

namespace nntl {

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//each layer_pack_* layer is expected to have a special typedef self_t LayerPack_t
	// and it must implement for_each_layer() and for_each_packed_layer() function families
	
	//recognizer of layer_pack_* classes
	// primary template handles types that have no nested ::LayerPack_t member:
	template< class, class = ::std::void_t<> >
	struct is_layer_pack : ::std::false_type { };
	// specialization recognizes types that do have a nested ::LayerPack_t member:
	template< class T >
	struct is_layer_pack<T, ::std::void_t<typename T::LayerPack_t>> : ::std::true_type {};

	//helper function to call internal _for_each_layer(f) for layer_pack_* classes
	//it iterates through the layers from the lowmost (input) to the highmost (output).
	// layer_pack's are also passed to F!
	// Therefore the .for_each_layer() is the main mean to apply F to every layer in a network/pack
	template<typename Func, typename LayerT> inline
		::std::enable_if_t<is_layer_pack<LayerT>::value> call_F_for_each_layer(Func&& F, LayerT& l)noexcept
	{
		//l.for_each_layer(::std::forward<Func>(F));
		l.for_each_layer(F); //mustn't forward, because we'll be using F later!
		
		//must also call for the layer itself
		//F(l);//not should do ::std::forward<Func>(F)(l); here ???
		::std::forward<Func>(F)(l);//it's OK to cast to rvalue here if suitable, as we don't care what will happens with F after that.
	}
	template<typename Func, typename LayerT> inline
		::std::enable_if_t<!is_layer_pack<LayerT>::value> call_F_for_each_layer(Func&& F, LayerT& l)noexcept
	{
		::std::forward<Func>(F)(l);//OK to forward if suitable
	}

	//probably we don't need it, but let it be
	template<typename Func, typename LayerT> inline
		::std::enable_if_t<is_layer_pack<LayerT>::value> call_F_for_each_layer_down(Func&& F, LayerT& l)noexcept
	{
		//::std::forward<Func>(F)(l);
		F(l);//mustn't forward, we'll use it later
		l.for_each_layer_down(::std::forward<Func>(F));//OK, last use
	}
	template<typename Func, typename LayerT> inline
		::std::enable_if_t<!is_layer_pack<LayerT>::value> call_F_for_each_layer_down(Func&& F, LayerT& l)noexcept
	{
		::std::forward<Func>(F)(l);//OK to forward
	}

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// layer with ::grad_works_t type defined is expected to have m_gradientWorks member
	// (nonstandartized at this moment)

	template< class, class = ::std::void_t<> >
	struct layer_has_gradworks : ::std::false_type { };
	// specialization recognizes types that do have a nested ::grad_works_t member:
	template< class T >
	struct layer_has_gradworks<T, ::std::void_t<typename T::grad_works_t>> : ::std::true_type {};


	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	template <typename RealT>
	struct _i_layer_td : public math::smatrix_td {
		typedef RealT real_t;
		typedef math::smatrix<real_t> realmtx_t;
		typedef math::smatrix_deform<real_t> realmtxdef_t;
		static_assert(::std::is_base_of<realmtx_t, realmtxdef_t>::value, "smatrix_deform must be derived from smatrix!");
	};

	////////////////////////////////////////////////////////////////////////// 
	// interface that must be implemented by a layer in order to make fprop() function work
	// Layer, passed to fprop as the PrevLayer parameter must obey this interface.
	// (#TODO is it necessary? Can we just drop it?)
	template <typename RealT>
	class _i_layer_fprop : public _i_layer_td<RealT> {
	protected:
		_i_layer_fprop()noexcept {};
		~_i_layer_fprop()noexcept {};

		//!! copy constructor not needed
		_i_layer_fprop(const _i_layer_fprop& other)noexcept; // = delete; //-it should be `delete`d, but factory function won't work if it is
		//!!assignment is not needed
		_i_layer_fprop& operator=(const _i_layer_fprop& rhs) noexcept; // = delete; //-it should be `delete`d, but factory function won't work if it is

	public:
		//It is allowed to call get_activations() after fprop() and before bprop() only. bprop() invalidates activation values!
		//Furthermore, DON'T ever change the activation matrix values from the ouside of the layer!
		// Layer object expects it to be unchanged between fprop()/bprop() calls.
		nntl_interface const realmtxdef_t& get_activations()const noexcept;
		nntl_interface const mtx_size_t get_activations_size()const noexcept;
		//shared activations implies that the bias column may hold not biases, but activations of some another layer
		nntl_interface const bool is_activations_shared()const noexcept;

		//essentially the same as get_activations(), however, it is allowed to call this function anytime to obtain the pointer
		//However, if you are going to dereference the pointer, the same restrictions as for get_activations() applies.
		//NOTE: It won't trigger assert if it's dereferenced in a wrong moment, therefore you'll get invalid values,
		// so use it wisely only when you absolutely can't use the get_activations()
		nntl_interface const realmtxdef_t* get_activations_storage()const noexcept;

		//use it only if you really know what you're are doing and it won't hurt derivative calculation
		//SOME LAYERS may NOT implement this function!
		nntl_interface realmtxdef_t& _get_activations_mutable()const noexcept;

	protected:
		//see _layer_base::m_bIsDropSamplesMightBeCalled member comment
		nntl_interface const bool is_drop_samples_mbc()const noexcept;
	};

	template <typename RealT>
	class _i_layer_gate : private _i_layer_td<RealT> {
	protected:
		_i_layer_gate()noexcept {};
		~_i_layer_gate()noexcept {};

		//!! copy constructor not needed
		_i_layer_gate(const _i_layer_gate& other)noexcept; // = delete; //-it should be `delete`d, but factory function won't work if it is
																	 //!!assignment is not needed
		_i_layer_gate& operator=(const _i_layer_gate& rhs) noexcept; // = delete; //-it should be `delete`d, but factory function won't work if it is

	public:
		nntl_interface const realmtx_t& get_gate()const noexcept;
		nntl_interface const vec_len_t get_gate_width()const noexcept;
	};

	// and the same for bprop(). Derives from _i_layer_fprop because it generally need it API
	template <typename RealT>
	class _i_layer_trainable : public _i_layer_fprop<RealT>{
	protected:
		_i_layer_trainable()noexcept {};
		~_i_layer_trainable()noexcept {};

		//!! copy constructor not needed
		_i_layer_trainable(const _i_layer_trainable& other)noexcept; // = delete; //-it should be `delete`d, but factory function won't work if it is
															 //!!assignment is not needed
		_i_layer_trainable& operator=(const _i_layer_trainable& rhs) noexcept; // = delete; //-it should be `delete`d, but factory function won't work if it is
	};

	//////////////////////////////////////////////////////////////////////////
	// layer interface definition
	template<typename RealT>
	class _i_layer : public _i_layer_trainable<RealT> {
	protected:
		_i_layer()noexcept {};
		~_i_layer()noexcept {};

		//!! copy constructor not needed
		_i_layer(const _i_layer& other)noexcept; // = delete; //-it should be `delete`d, but factory function won't work if it is
		//!!assignment is not needed
		_i_layer& operator=(const _i_layer& rhs) noexcept; // = delete; //-it should be `delete`d, but factory function won't work if it is

	public:
		typedef _nnet_errs::ErrorCode ErrorCode;

		// Each layer must define an alias for type of math interface and rng interface (and they must be the same for all layers)
		//typedef .... iMath_t
		//typedef .... iRng_t

		//////////////////////////////////////////////////////////////////////////
		// base interface
		
		// class constructor MUST have const char* pCustomName as the first parameter.
		
		// each call to own functions should go through get_self() to make polymorphyc function work
		nntl_interface auto get_self() const noexcept;
		nntl_interface const layer_index_t get_layer_idx() const noexcept;
		nntl_interface const neurons_count_t get_neurons_cnt() const noexcept;

		//For internal use only. DON'T call this function unless you know very well what you're doing
		nntl_interface void _set_neurons_cnt(const neurons_count_t nc)noexcept;

		nntl_interface const neurons_count_t get_incoming_neurons_cnt()const noexcept;
		
		//must obey to matlab variables naming convention
		nntl_interface auto set_custom_name(const char* pCustName)noexcept;
		nntl_interface const char* get_custom_name()const noexcept;
		nntl_interface void get_layer_name(char* pName, const size_t cnt)const noexcept;
		nntl_interface ::std::string get_layer_name_str()const noexcept;

	private:
		//redefine in derived class in public scope. Array-style definition MUST be preserved.
		//the _defName must be unique for each final layer class and mustn't be longer than sizeof(layer_type_id_t) (it's also used as a layer typeId)
		static constexpr const char _defName[] = "_i_layer";
	public:
		//returns layer type id based on layer's _defName
		nntl_interface static constexpr layer_type_id_t get_layer_type_id()noexcept;

		// ATTN: more specific and non-templated version available for this function, see _layer_base for an example
		// On the pNewActivationStorage see comments to the on_batch_size_change(). Layers that should never be on the top of a stack of layers
		// inside of compound layers, should totally omit this parameter to break compilation.
		// For the _layer_init_data_t parameter see the _impl::_layer_init_data<>.
		template<typename _layer_init_data_t>
		nntl_interface ErrorCode init(_layer_init_data_t& lid, real_t* pNewActivationStorage = nullptr)noexcept;
		//If the layer was given a pNewActivationStorage (during the init() or on_batch_size_change()), then it MUST NOT touch a bit
		// in the bias column of the activation storage during bprop()/fprop() and everywhere else.
		// In general - if the layer allocates activation matrix by itself (when the pNewActivationStorage==nullptr),
		//					then it also allocates and sets up biases. Also, during execution of
		//					on_batch_size_change(), the layer has to restore its bias column, had the activation matrix been resized/deformed.
		//					And that are the only things the layer is allowed to do with biases!
		//			  - if the layer is given pNewActivationStorage, then it uses it supposing there's a space for a bias column,
		//					however, layer must never touch data in that column.

		// Sets a batch size of the layer. The actual mode (evaluation or training) now should be set via common_data::set_training_mode()/isTraining()
		// 
		// pNewActivationStorage is used in conjunction with compound layers, such as layer_pack_horizontal, that 
		// provide their internal activation storage for embedded layers (to reduce data copying)
		// If pNewActivationStorage is set, the layer must store its activations under this pointer
		// (by doing something like m_activations.useExternalStorage(pNewActivationStorage) ).
		// Resetting of biases is not required at this case, however.
		// Layers, that should never be a part of other compound layers, should totally omit this parameter
		// from function signature (not recommended use-case, however)
		nntl_interface void on_batch_size_change(real_t*const pNewActivationStorage = nullptr)noexcept;
		//If the layer was given a pNewActivationStorage (during the init() or on_batch_size_change()), then it MUST NOT touch a bit
		// in the bias column of the activation storage during fprop() and everywhere else.
		// For more information about how memory storage is organized, see discussion in _init_layers.h::_layer_init_data{}

		//frees any temporary resources that doesn't contain layer-specific information (i.e. layer weights shouldn't be freed).
		//In other words, this routine burns some unnecessary fat layer gained during training, but don't touch any data necessary
		// for new subsequent call to nnet.train()
		nntl_interface void deinit() noexcept;

		//provides a temporary storage for a layer. It is guaranteed, that during fprop() or bprop() the storage
		// can be modified only by the layer. However, it's not a persistent storage and layer mustn't rely on it
		// to retain it's content between calls to fprop()/bprop().
		// Compound layers (that call other's layers fprop()/bprop()) should use this storage with a great care.
		// Function is guaranteed to be called if
		// max(_layer_init_data::minMemFPropRequire,_layer_init_data::minMemBPropRequire) set to be >0 during init()
		// cnt is guaranteed to be at least as big as max(minMemFPropRequire,minMemBPropRequire)
		nntl_interface void initMem(real_t* ptr, numel_cnt_t cnt)noexcept;

		//input layer should use slightly different specialization: void fprop(const realmtx_t& data_x)noexcept
		template <typename LowerLayer>
		nntl_interface void fprop(const LowerLayer& lowerLayer)noexcept;
		// Layer MUST NOT touch a bit in the bias column of the activation storage during fprop()/bprop()
		
		// The drop_samples() function is used when someone from an outside wants to remove/set to zero some rows in
		// the layer's activation matrix (gating layers are examples of such an use-case).
		// If the drop_samples() was called, then the layer expects that the dLdA passed on subsequent bprop() call would have
		// corresponding elements zeroed by the very same mask.
		// mask is a single column array of current batch size (==m_activations.rows()).
		// bBiasesToo controls whether the pMask should be applied to the bias column too. This var also sets
		// activation matrix's m_bHoleyBiases flag to prevent false alarms of bias checking in debug builds
		// Remember to do m_activations.set_biases() during first steps of fprop() if m_activations.isHoleyBiases() set
		// nNZElems - is the number of nonzero entries in mask.
		nntl_interface void drop_samples(const realmtx_t& mask, const bool bBiasesToo, const numel_cnt_t nNZElems)noexcept;

		// If the only thing the drop_samples() do is it applies a mask to the activations, then return true from this function.
		// This would allow to optimize away a call to the drop_samples() in some situations.
		// If the drop_samples() does something more, return false. This will make drop_samples() call to always occur.
		nntl_interface bool is_trivial_drop_samples()const noexcept;

		//if drop_samples() should be called, but is_trivial_drop_samples() returns true, then this function is used to
		//notify the layer about a new samples count.
		nntl_interface void left_after_drop_samples(const numel_cnt_t nNZElems)noexcept;

		// dLdA is the derivative of the loss function wrt this layer neuron activations.
		// Size [batchSize x layer_neuron_cnt] (bias units ignored - their weights actually belongs to an upper layer
		// and therefore are updated during that layer's bprop() phase and dLdW application)
		// 
		// dLdAPrev is the derivative of the loss function wrt to a previous (lower) layer activations to be computed by the bprop().
		// Size [batchSize x prev_layer_neuron_cnt] (bias units ignored)
		// 
		// The layer must compute a dL/dW (the derivative of the loss function wrt the layer parameters (weights)) and adjust
		// its parameters accordingly after a computation of dLdAPrev during the bprop() function.
		//  
		// realmtxdef_t type is used in pack_* layers. Non-compound layers should use realmtxt_t type instead.
		// The function is allowed to use the dLdA parameter once it's not needed anymore as it wants (A resizing operation included,
		// provided that it won't resize it greater than a max size. BTW, beware! The run-time check of maximum matrix size works only
		// in DEBUG builds!). Same for the dLdAPrev, but on exit from the bprop() it must have a proper size and expected content.
		template <typename LowerLayer>
		nntl_interface unsigned bprop(realmtxdef_t& dLdA, const LowerLayer& lowerLayer, realmtxdef_t& dLdAPrev)noexcept;
		// Layer MUST NOT touch a bit in the bias column of the activation storage during fprop()/bprop()
		// 
		//Output layer must use a signature of void bprop(const realmtx_t& data_y, const LowerLayer& lowerLayer, realmtxdef_t& dLdAPrev);
		// 
		//On the function return value: in a short, simple/single layers should return 1.
		// In a long: during the init() phase, each layer returns the total size of its dLdA matrix in _layer_init_data_t::max_dLdA_numel.
		// This value, gathered from every layer in a layers stack, are gets aggregated by the max()
		// into the biggest possible dLdA size for a whole NNet.
		// Then two matrices of this (biggest) size are allocated and passed to a layers::bprop() function during backpropagation. One of these
		// matrices will be used as a dLdA and the other as a dLdAPrev during an each call to a layer::bprop().
		// What does the return value from a bprop() do is it governs whether the caller must alternate these matrices
		// on a call to a lower layer bprop() (i.e. whether a real dLdAPrev is actually stored in dLdAPrev variable (return 1) or
		// the dLdAPrev is really stored in the (appropriately resized by the layers bprop()) dLdA variable - return 0).
		// So, simple/single layers, that don't switch/reuse these matrices should return 1. However, compound layers
		// (such as the layer_pack_vertical), that consists of other layers (and must call bprop() on them), may reuse
		// dLdA and dLdAPrev variable in order to eliminate the necessity of additional temporary dLdA matrices and corresponding data coping,
		// just by switching between dLdA and dLdAPrev between calls to inner layer's bprop()s.
		// So (continuing layer_pack_vertical example) if there was
		// an even number of calls to inner layers bprop(), then the actual dLdAPrev of the whole compound
		// layer will be inside of dLdA and a caller of compound layer's bprop() should NOT switch matrices on
		// subsequent call to lower layer bprop(). Therefore, the compound layer's bprop() must return 0 in that case.
		

		
		//returns a loss function summand, that's caused by this layer (for example, L2 regularizer adds term
		// l2Coefficient*Sum(weights.^2) )
		// At this moment, the code of layers::calcLossAddendum() depends on a (possibly non-stable) fact, that a loss function
		// addendum to be calculated doesn't depend on data_x or data_y (it depends on only internal nn properties, such as weights).
		// This might not be the case in a future, - update layers::calcLossAddendum() definition then.
		nntl_interface real_t lossAddendum()const noexcept;
		//should return true, if the layer has a value to add to Loss function value (there's some regularizer attached)
		nntl_interface bool hasLossAddendum()const noexcept;

	private:
		//support for ::boost::serialization
		friend class ::boost::serialization::access;
		template<class Archive> nntl_interface void serialize(Archive & ar, const unsigned int version) {}
	};

	//////////////////////////////////////////////////////////////////////////
	// poly base class, Implements compile time polymorphism (to get rid of virtual functions)
	// and default _layer_name_ machinery
	// Every derived class MUST have typename FinalPolymorphChild as the first template parameter!
	template<typename FinalPolymorphChild, typename RealT>
	class _cpolym_layer_base : public _i_layer<RealT> {
	public:
		//////////////////////////////////////////////////////////////////////////
		//typedefs
		typedef FinalPolymorphChild self_t;
		NNTL_METHODS_SELF_CHECKED((::std::is_base_of<_cpolym_layer_base, FinalPolymorphChild>::value)
			, "FinalPolymorphChild must derive from _cpolym_layer_base<FinalPolymorphChild>");

		//can't work here
		//static constexpr bool bOutputLayer = is_layer_output<self_t>::value;

		//layer name could be used for example to name Matlab's variables,
		//so there must be some reasonable limit. Don't overcome this limit!
		static constexpr size_t layerNameMaxChars = 50;
		//limit for custom name length
		static constexpr size_t customNameMaxChars = layerNameMaxChars - 10;
	private:
		//redefine in derived class in public scope. Array-style definition MUST be preserved.
		//the _defName must be unique for each final layer class (it's also served as a layer typeId)
		static constexpr const char _defName[] = "_cpoly";

	protected:
		//just a pointer as passed, because don't want to care about memory allocation and leave a footprint as small as possible,
		//because it's just a matter of convenience.
		const char* m_customName;

	protected:
		//bool m_bTraining;
		//bool m_bActivationsValid;

	protected:
		void init()noexcept { /*m_bActivationsValid = false;*/ }
		void deinit()noexcept { /*m_bActivationsValid = false;*/ }

	public:
		//////////////////////////////////////////////////////////////////////////
		~_cpolym_layer_base()noexcept {}
		_cpolym_layer_base(const char* pCustName = nullptr)noexcept /*: m_bTraining(false),s m_bActivationsValid(false)*/ {
			set_custom_name(pCustName);
		}

		static constexpr const char* get_default_name()noexcept { return self_t::_defName; }
		self_ref_t set_custom_name(const char* pCustName)noexcept {
			NNTL_ASSERT(!pCustName || strlen(pCustName) < customNameMaxChars);
			m_customName = pCustName;
			return get_self();
		}
		const char* get_custom_name()const noexcept { return m_customName ? m_customName : get_self().get_default_name(); }

		void get_layer_name(char* pName, const size_t cnt)const noexcept {
			sprintf_s(pName, cnt, "%s_%d", get_self().get_custom_name(),static_cast<unsigned>(get_self().get_layer_idx()));
		}
		::std::string get_layer_name_str()const noexcept {
			constexpr size_t ml = layerNameMaxChars;
			char n[ml];
			get_self().get_layer_name(n, ml);
			return ::std::string(n);
		}

		//#todo: need a way to define layer type based on something more versatile than a self_t::_defName
		//probably based on https://akrzemi1.wordpress.com/2017/06/28/compile-time-string-concatenation/
		//or https://crazycpp.wordpress.com/2014/10/17/compile-time-strings-with-constexpr/
	private:
		template<unsigned LEN>
		static constexpr layer_type_id_t _get_layer_type_id(const char(&pStr)[LEN], const unsigned pos = 0)noexcept {
			return pos < LEN ? (layer_type_id_t(pStr[pos]) | (_get_layer_type_id(pStr, pos + 1) << 8)) : 0;
		}
	public:
		static constexpr layer_type_id_t get_layer_type_id()noexcept {
			static_assert(sizeof(self_t::_defName) <= sizeof(layer_type_id_t), "Too long default layer name has been used. Can't use it to derive layer_type_id");
			return _get_layer_type_id(self_t::_defName);
		}

	};
	
	//////////////////////////////////////////////////////////////////////////
	// base class for most of layers.
	// Implements compile time polymorphism (to get rid of virtual functions),
	// default _layer_name_ machinery, some default basic typedefs and basic support machinery
	// (init() function with common_data_t, layer index number, neurons count)
	template<typename FinalPolymorphChild, typename InterfacesT>
	class _layer_base 
		: public _cpolym_layer_base<FinalPolymorphChild, typename InterfacesT::iMath_t::real_t>
		, public _impl::_common_data_consumer<InterfacesT>
	{
	private:
		typedef _cpolym_layer_base<FinalPolymorphChild, typename InterfacesT::iMath_t::real_t> _base_class_t;
	public:
		//////////////////////////////////////////////////////////////////////////
		//typedefs		
		typedef _impl::_layer_init_data<common_data_t> _layer_init_data_t;

		using _base_class_t::real_t;

		//////////////////////////////////////////////////////////////////////////
		//members section (in "biggest first" order)

	private:
		neurons_count_t m_neurons_cnt, m_incoming_neurons_cnt;
		layer_index_t m_layerIdx;

	protected:
		bool m_bActivationsValid;

	private: //shouldn't be updated from derived classes. Getter methords are provided
		bool m_bIsSharedActivations;//taken from init()

		bool m_bIsDropSamplesMightBeCalled;//taken from init(). This variable is for derived classes INTERNAL use only!
		//It is NOT the same as statemed 'Are we to expect the activation matrix with removed samples (and probably biases too)?'
		//For a gating class (such as LPG or LPHG) it shows whether they could have their .drop_samples() called by an some upped
		// gating class. However, activations of these classes MAY contain zeroed samples due to their gates work.

		//#todo this flag is probably worst possible solution, however we may need some mean to switch off nonlinearity in a run-time.
		//Is there a better (non-branching when it's not necessary) solution available?
		// Might be unused in some derived class (until conditional member variables are allowed). Lives here for packing reasons.
		bool m_bLayerIsLinear;
		
	private:
		static constexpr const char _defName[] = "_base";

	protected:
		//////////////////////////////////////////////////////////////////////////
		//constructors-destructor
		~_layer_base()noexcept {};
		_layer_base(const neurons_count_t _neurons_cnt, const char* pCustomName=nullptr) noexcept 
			: _base_class_t(pCustomName)
			, m_layerIdx(0), m_neurons_cnt(_neurons_cnt), m_incoming_neurons_cnt(0), m_bActivationsValid(false)
			, m_bIsSharedActivations(false), m_bIsDropSamplesMightBeCalled(false)
			, m_bLayerIsLinear(false)
		{};
	
	public:
		//////////////////////////////////////////////////////////////////////////
		//nntl_interface overridings
		ErrorCode init(_layer_init_data_t& lid, real_t* pNewActivationStorage = nullptr)noexcept {
			NNTL_UNREF(pNewActivationStorage);
			_base_class_t::init();
			m_bActivationsValid = false;
			m_bIsSharedActivations = lid.bActivationsShareSpace;
			m_bIsDropSamplesMightBeCalled = lid.bDropSamplesMightBeCalled;
			set_common_data(lid.commonData);

			get_self().get_iInspect().init_layer(get_self().get_layer_idx(), get_self().get_layer_name_str(), get_self().get_layer_type_id());

			return ErrorCode::Success;
		}
		void deinit() noexcept { 
			m_bActivationsValid = false;
			m_bIsSharedActivations = false;
			m_bIsDropSamplesMightBeCalled = false;
			clean_common_data();
			_base_class_t::deinit();
		}

		const bool is_activations_shared()const noexcept { return m_bIsSharedActivations; }
	//protected:
		const bool is_drop_samples_mbc()const noexcept { return m_bIsDropSamplesMightBeCalled; }

	public:
		const layer_index_t& get_layer_idx() const noexcept { return m_layerIdx; }
		const neurons_count_t get_neurons_cnt() const noexcept { 
			NNTL_ASSERT(m_neurons_cnt);
			return m_neurons_cnt;
		}
		//for layers that need to calculate their neurons count in run-time (layer_pack_horizontal)
		void _set_neurons_cnt(const neurons_count_t nc)noexcept {
			NNTL_ASSERT(nc);
			//NNTL_ASSERT(!m_neurons_cnt || nc==m_neurons_cnt);//to prevent double calls
			NNTL_ASSERT(!m_neurons_cnt);//to prevent double calls
			m_neurons_cnt = nc;
		}

		const neurons_count_t get_incoming_neurons_cnt()const noexcept { 
			NNTL_ASSERT(!m_layerIdx || m_incoming_neurons_cnt);//m_incoming_neurons_cnt will be zero in input layer (it has m_layerIdx==0)
			return m_incoming_neurons_cnt;
		}		

		//returns a loss function summand, that's caused by this layer (for example, L2 regularizer adds term
		// l2Coefficient*Sum(weights.^2) )
		constexpr const real_t lossAddendum()const noexcept { return real_t(0.0); }
		
		//////////////////////////////////////////////////////////////////////////

		template<bool c = is_layer_learnable<self_t>::value >
		::std::enable_if_t<c, bool> bLayerIsLinear()const noexcept { return m_bLayerIsLinear; }

		template<bool c = is_layer_learnable<self_t>::value >
		::std::enable_if_t<c> setLayerLinear(const bool b)noexcept { m_bLayerIsLinear = b; }

		//////////////////////////////////////////////////////////////////////////
		// other funcs
	protected:
		//this is how we going to initialize layer indexes.
		friend class _impl::_preinit_layers;
		void _preinit_layer(_impl::init_layer_index& ili, const neurons_count_t inc_neurons_cnt)noexcept{
			//there should better be an exception, but we don't want exceptions at all.
			//anyway, there is nothing to help to those who'll try to abuse this API...
			NNTL_ASSERT(!m_layerIdx);
			NNTL_ASSERT(!m_incoming_neurons_cnt);

			if (m_layerIdx || m_incoming_neurons_cnt) abort();
			m_layerIdx = ili.newIndex();
			if (m_layerIdx) {//special check for the first (input) layer that doesn't have any incoming neurons
				NNTL_ASSERT(inc_neurons_cnt);
				m_incoming_neurons_cnt = inc_neurons_cnt;
			}
		}

	private:
		//////////////////////////////////////////////////////////////////////////
		//Serialization support
		friend class ::boost::serialization::access;
		//nothing to do here at this moment, also leave nntl_interface marker to prevent calls.
		//#TODO serialization function must be provided
		template<class Archive> nntl_interface void serialize(Archive & ar, const unsigned int version);
	};

	//////////////////////////////////////////////////////////////////////////
	// "light"-version of _layer_base that forwards its functions to some other layer, that is acceptable by get_self()._forwarder_layer()

	template<typename FinalPolymorphChild, typename InterfacesT>
	class _layer_base_forwarder 
		: public _cpolym_layer_base<FinalPolymorphChild, typename InterfacesT::real_t>
		, public interfaces_td<InterfacesT> 
	{
	public:
		typedef typename InterfacesT::real_t real_t;

		static constexpr bool bAllowToBlockLearning = inspector::is_gradcheck_inspector<iInspect_t>::value;

	public:
		~_layer_base_forwarder()noexcept{}
		_layer_base_forwarder(const char* pCustName = nullptr)noexcept 
			: _cpolym_layer_base<FinalPolymorphChild, typename InterfacesT::real_t>(pCustName)
		{}

		//////////////////////////////////////////////////////////////////////////
		// helpers to access common data 
		// #todo this implies, that the following functions are described in _i_layer interface. It's not the case at this moment.
		const bool has_common_data()const noexcept { return get_self()._forwarder_layer().has_common_data(); }
		const auto& get_common_data()const noexcept { return get_self()._forwarder_layer().get_common_data(); }
		iMath_t& get_iMath()const noexcept { return get_self()._forwarder_layer().get_iMath(); }
		iRng_t& get_iRng()const noexcept { return get_self()._forwarder_layer().get_iRng(); }
		iInspect_t& get_iInspect()const noexcept { return get_self()._forwarder_layer().get_iInspect(); }

		template<bool B = bAllowToBlockLearning>
		::std::enable_if_t<B, const bool> isLearningBlocked()const noexcept {
			return get_self()._forwarder_layer().isLearningBlocked();
		}
		template<bool B = bAllowToBlockLearning>
		constexpr ::std::enable_if_t<!B, bool> isLearningBlocked() const noexcept { return false; }

		/*const vec_len_t max_fprop_batch_size()const noexcept { return get_self()._forwarder_layer().max_fprop_batch_size(); }
		const vec_len_t training_batch_size()const noexcept { return get_self()._forwarder_layer().training_batch_size(); }
		const vec_len_t biggest_batch_size()const noexcept { return get_self()._forwarder_layer().biggest_batch_size(); }
		const bool is_training_mode()const noexcept { return get_self()._forwarder_layer().is_training_mode(); }
		const bool is_training_possible()const noexcept { return get_self()._forwarder_layer().is_training_possible(); }
		const vec_len_t get_cur_batch_size()const noexcept { return get_self()._forwarder_layer().get_cur_batch_size(); }*/

		const neurons_count_t get_neurons_cnt() const noexcept { return get_self()._forwarder_layer().get_neurons_cnt(); }
		const neurons_count_t get_incoming_neurons_cnt()const noexcept { return  get_self()._forwarder_layer().get_incoming_neurons_cnt(); }

		const realmtxdef_t& get_activations()const noexcept { return get_self()._forwarder_layer().get_activations(); }
		const realmtxdef_t* get_activations_storage()const noexcept { return get_self()._forwarder_layer().get_activations_storage(); }
		realmtxdef_t& _get_activations_mutable()const noexcept { return get_self()._forwarder_layer()._get_activations_mutable(); }
		const mtx_size_t get_activations_size()const noexcept { return get_self()._forwarder_layer().get_activations_size(); }
		const bool is_activations_shared()const noexcept { return get_self()._forwarder_layer().is_activations_shared(); }

		const bool is_drop_samples_mbc()const noexcept { return get_self()._forwarder_layer().is_drop_samples_mbc(); }

	};

}