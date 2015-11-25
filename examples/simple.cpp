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

#include "stdafx.h"

#include "../nntl/interface/math.h"
#include "../nntl/nntl.h"
#include "../nntl/_supp/binfile.h"

using namespace nntl;
typedef nntl_supp::binfile reader_t;
using float_t_ = math_types::float_ty;

#if defined(TESTS_SKIP_NNET_LONGRUNNING)
//ALWAYS run debug build with similar relations of data sizes:
// if release will run in minibatches - make sure, there will be at least 2 minibatches)
// if release will use different data sizes for train/test - make sure, debug will also run on different datasizes
#define MNIST_FILE "../data/mnist200_100.bin"
#else
#define MNIST_FILE "../data/mnist60000.bin"
#endif // _DEBUG

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// make a simple NN without any fancy things, just simple 768->500->300->10 net without momentum and dropout
TEST(Simple, PlainFFN) {
	train_data td; //storage for MNIST train and test(validation) data
	reader_t reader; //.bin file reader object

	//1. reading training data from file into train_data td storage
	STDCOUTL("Reading datafile '" << MNIST_FILE << "'...");
	reader_t::ErrorCode rec = reader.read(NNTL_STRING(MNIST_FILE), td);
	ASSERT_EQ(reader_t::ErrorCode::Success, rec) << "Error code description: " << reader.get_last_error_str();


	//2. define NN layers and their properties
	size_t epochs = 20;
	const float_t_ learningRate = .1;

	// a. input layer (take a look at .cols_no_bias() call - it's required here instead .cols() because upon data
	// reading from file, the code appends to all _x data
	// additional last column with ones that will act as bias units during NN computation. _no_bias() version of
	// any function "hides" this bias column and acts like there's no biases. In this case it returns correct number
	// of columns in _x data)
	// Input layer typically does nothing more than offering a unified API to _x data.
	layer_input inp(td.train_x().cols_no_bias());

	//b. hidden layers - that's where the magic happening
	layer_fully_connected<activation::sigm> fcl(500, learningRate);
	layer_fully_connected<activation::sigm> fcl2(300, learningRate);

	//c. output layer - using quadratic loss function.
	layer_output<activation::sigm_quad_loss> outp(td.train_y().cols(), learningRate);

	//3. assemble layer references (!! - not layer objects, but references to them) into a single object - layer_pack. 
	auto lp = make_layers_pack(inp, fcl, fcl2, outp);

	//4. define NN training options (epochs count, conditions when to evaluate NN performance, etc)
	nnet_cond_epoch_eval cee(epochs);
	nnet_train_opts<decltype(cee)> opts(std::move(cee));

	opts.batchSize(100);

	//5. make instance of NN 
	auto nn = make_nnet(lp);

	//6. launch training on td data with opts options.
	auto ec = nn.train(td, opts);

	//7. test there were no errors
	ASSERT_EQ(decltype(nn)::ErrorCode::Success, ec) << "Error code description: " << nn.get_last_error_string();
}


// make a slightly more sophisticated 768->500->300->10 net with Nesterov Momentum, Dropout, RMSProp,
// learning rate decay and individual adaptive learning rates.
// I got 1.66% validation error rate at 30 epochs, which is pretty good result for this kind of NN according
// to http://yann.lecun.com/exdb/mnist/index.html. There were no signs of overfitting, so longer learning could
// lead to even better results.
TEST(Simple, NotSoPlainFFN) {
	train_data td;
	reader_t reader;

	//1. reading training data from file into train_data td storage
	STDCOUTL("Reading datafile '" << MNIST_FILE << "'...");
	reader_t::ErrorCode rec = reader.read(NNTL_STRING(MNIST_FILE), td);
	ASSERT_EQ(reader_t::ErrorCode::Success, rec) << "Error code description: " << reader.get_last_error_str();


	//2. define NN layers and their properties
	size_t epochs = 30;
	const float_t_ learningRate = .001, dropoutRate=.5, momentum=.93, learningRateDecayCoeff=.95;
	const ILR ilr(.94, 1.05, .001, 10);

	// a. input layer
	layer_input inp(td.train_x().cols_no_bias());

	//b. hidden layers
	layer_fully_connected<activation::sigm> fcl(500, learningRate, dropoutRate);
	layer_fully_connected<activation::sigm> fcl2(300, learningRate, dropoutRate);

	//c. output layer
	layer_output<activation::sigm_quad_loss> outp(td.train_y().cols(), learningRate);

	//d. setting layers properties
	auto optimizerType = decltype(fcl)::grad_works_t::RMSProp_Hinton;
	fcl.m_gradientWorks.set_type(optimizerType).set_nesterov_momentum(momentum,false).set_ILR(ilr);
	fcl2.m_gradientWorks.set_type(optimizerType).set_nesterov_momentum(momentum, false).set_ILR(ilr);
	outp.m_gradientWorks.set_type(optimizerType).set_nesterov_momentum(momentum, false).set_ILR(ilr);

	//3. assemble layer references (!! - not layer objects, but references to them) into a single object - layer_pack. 
	auto lp = make_layers_pack(inp, fcl, fcl2, outp);

	//4. define NN training options (epochs count, conditions when to evaluate NN performance, etc)
	nnet_cond_epoch_eval cee(epochs);
	nnet_train_opts<decltype(cee)> opts(std::move(cee));

	opts.batchSize(100);

	//5. make instance of NN 
	auto nn = make_nnet(lp);

	//5.5 define callback
	auto onEpochEndCB = [learningRateDecayCoeff](auto& nn, auto& opts, const size_t epochIdx)->bool {
		// well, we can capture references to layer objects in lambda capture clause and use them directly here,
		// but lets get an access to them through nn object, passed as function parameter.
		nn.get_layer_pack().for_each_layer([learningRateDecayCoeff](auto& l) {
			if (! l.is_input_layer()) {
				l.learning_rate(l.learning_rate()*learningRateDecayCoeff);
			}
		});
		//return false to stop learning
		return true;
	};

	//6. launch training on td data with opts options.
	auto ec = nn.train(td, opts, onEpochEndCB);

	//7. test there were no errors
	ASSERT_EQ(decltype(nn)::ErrorCode::Success, ec) << "Error code description: " << nn.get_last_error_string();
}