/*
 * Copyright (c) 2020-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of
 *       conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the NVIDIA CORPORATION nor the names of its contributors may be used
 *       to endorse or promote products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TOR (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @file   common.h
 *  @author Thomas Müller and Nikolaus Binder, NVIDIA
 *  @brief  Common utilities that are needed by pretty much every component of this framework.
 */

#pragma once

#if defined(_WIN32) && !defined(NOMINMAX)
#	define NOMINMAX
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined(__CUDACC__)
#	include <cuda_fp16.h>
#endif

//////////////////////////////////////
// CUDA ERROR HANDLING (EXCEPTIONS) //
//////////////////////////////////////

#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)
#define FILE_LINE __FILE__ ":" STR(__LINE__)

#if defined(__CUDA_ARCH__)
#	define TCNN_PRAGMA_UNROLL _Pragma("unroll")
#	define TCNN_PRAGMA_NO_UNROLL _Pragma("unroll 1")
#else
#	define TCNN_PRAGMA_UNROLL
#	define TCNN_PRAGMA_NO_UNROLL
#endif

#ifdef __CUDACC__
#	ifdef __NVCC_DIAG_PRAGMA_SUPPORT__
#		pragma nv_diag_suppress = unsigned_compare_with_zero
#	else
#		pragma diag_suppress = unsigned_compare_with_zero
#	endif
#endif

#if defined(__CUDACC__) || (defined(__clang__) && defined(__CUDA__))
#	define TCNN_HOST_DEVICE __host__ __device__
#	define TCNN_DEVICE __device__
#	define TCNN_HOST __host__
#else
#	define TCNN_HOST_DEVICE
#	define TCNN_DEVICE
#	define TCNN_HOST
#endif

#include <tiny-cuda-nn/vec.h>

// #if defined(__CUDA_ARCH__)
// static_assert(__CUDA_ARCH__ >= TCNN_MIN_GPU_ARCH * 10, "MIN_GPU_ARCH=" STR(TCNN_MIN_GPU_ARCH) "0 must bound __CUDA_ARCH__="
// STR(__CUDA_ARCH__) " from below, but doesn't."); #endif

namespace tcnn {

static constexpr uint32_t MIN_GPU_ARCH = TCNN_MIN_GPU_ARCH;

// When TCNN managed its model parameters, they are always aligned,
// which yields performance benefits in practice. However, parameters
// supplied by PyTorch are not necessarily aligned. The following
// variable controls whether TCNN must deal with unaligned data.
#if defined(TCNN_PARAMS_UNALIGNED)
static constexpr bool PARAMS_ALIGNED = false;
#else
static constexpr bool PARAMS_ALIGNED = true;
#endif

#define TCNN_HALF_PRECISION (!(TCNN_MIN_GPU_ARCH == 61 || TCNN_MIN_GPU_ARCH <= 52))

// TCNN has the following behavior depending on GPU arch.
// Refer to the first row of the table at the following URL for information about
// when to pick fp16 versus fp32 precision for maximum performance.
// https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#arithmetic-instructions__throughput-native-arithmetic-instructions
//
//  GPU Arch | FullyFusedMLP supported | CUTLASS SmArch supported |                 Precision
// ----------|-------------------------|--------------------------|--------------------------
//     80-90 |                     yes |                       80 |                    __half
//        75 |                     yes |                       75 |                    __half
//        70 |                      no |                       70 |                    __half
// 53-60, 62 |                      no |                       70 |  __half (no tensor cores)
//  <=52, 61 |                      no |                       70 |   float (no tensor cores)

#if defined(__CUDACC__)
#	if TCNN_HALF_PRECISION
using network_precision_t = __half;
#	else
using network_precision_t = float;
#	endif

// Optionally: set the precision to `float` to disable tensor cores and debug potential
//             problems with mixed-precision training.
// using network_precision_t = float;
#endif

enum class Activation {
	ReLU,
	LeakyReLU,
	Exponential,
	Sine,
	Sigmoid,
	Squareplus,
	Softplus,
	Tanh,
	None,
};

enum class GridType {
	Hash,
	Dense,
	Tiled,
};

enum class HashType {
	Prime,
	CoherentPrime,
	ReversedPrime,
	Rng,
};

enum class InterpolationType {
	Nearest,
	Linear,
	Smoothstep,
};

enum class MatrixLayout {
	RowMajor = 0,
	SoA = 0, // For data matrices TCNN's convention is RowMajor == SoA (struct of arrays)
	ColumnMajor = 1,
	AoS = 1,
};

static constexpr MatrixLayout RM = MatrixLayout::RowMajor;
static constexpr MatrixLayout SoA = MatrixLayout::SoA;
static constexpr MatrixLayout CM = MatrixLayout::ColumnMajor;
static constexpr MatrixLayout AoS = MatrixLayout::AoS;

enum class ReductionType {
	Concatenation,
	Sum,
	Product,
};

//////////////////
// Misc helpers //
//////////////////

template <typename T> TCNN_HOST_DEVICE void host_device_swap(T& a, T& b) {
	T c(a);
	a = b;
	b = c;
}

template <typename T> TCNN_HOST_DEVICE T gcd(T a, T b) {
	while (a != 0) {
		b %= a;
		host_device_swap(a, b);
	}
	return b;
}

template <typename T> TCNN_HOST_DEVICE T lcm(T a, T b) {
	T tmp = gcd(a, b);
	return tmp ? (a / tmp) * b : 0;
}

template <typename T> TCNN_HOST_DEVICE T div_round_up(T val, T divisor) { return (val + divisor - 1) / divisor; }

template <typename T> TCNN_HOST_DEVICE T next_multiple(T val, T divisor) { return div_round_up(val, divisor) * divisor; }

template <typename T> TCNN_HOST_DEVICE T previous_multiple(T val, T divisor) { return (val / divisor) * divisor; }

template <typename T> constexpr TCNN_HOST_DEVICE bool is_pot(T val) { return (val & (val - 1)) == 0; }

inline constexpr TCNN_HOST_DEVICE uint32_t next_pot(uint32_t v) {
	--v;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return v + 1;
}

template <typename T> constexpr TCNN_HOST_DEVICE float default_loss_scale();
template <> constexpr TCNN_HOST_DEVICE float default_loss_scale<float>() { return 1.0f; }
#ifdef __CUDACC__
template <> constexpr TCNN_HOST_DEVICE float default_loss_scale<__half>() { return 128.0f; }
#endif

constexpr uint32_t BATCH_SIZE_GRANULARITY = 256;
constexpr uint32_t N_THREADS_LINEAR = 128;
constexpr uint32_t WARP_SIZE = 32;

// Lower-case constants kept for backward compatibility with user code.
constexpr uint32_t batch_size_granularity = BATCH_SIZE_GRANULARITY;
constexpr uint32_t n_threads_linear = N_THREADS_LINEAR;

template <typename T> constexpr TCNN_HOST_DEVICE uint32_t n_blocks_linear(T n_elements, uint32_t n_threads = N_THREADS_LINEAR) {
	return (uint32_t)div_round_up(n_elements, (T)n_threads);
}

template <typename T> struct PitchedPtr {
	TCNN_HOST_DEVICE PitchedPtr() : ptr{nullptr}, stride_in_bytes{sizeof(T)} {}
	TCNN_HOST_DEVICE PitchedPtr(T* ptr, size_t stride_in_elements, size_t offset = 0, size_t extra_stride_bytes = 0) :
		ptr{ptr + offset}, stride_in_bytes{(uint32_t)(stride_in_elements * sizeof(T) + extra_stride_bytes)} {}

	template <typename U>
	TCNN_HOST_DEVICE explicit PitchedPtr(PitchedPtr<U> other) : ptr{(T*)other.ptr}, stride_in_bytes{other.stride_in_bytes} {}

	TCNN_HOST_DEVICE T* operator()(uint32_t y) const { return (T*)((const char*)ptr + y * stride_in_bytes); }

	TCNN_HOST_DEVICE void operator+=(uint32_t y) { ptr = (T*)((const char*)ptr + y * stride_in_bytes); }

	TCNN_HOST_DEVICE void operator-=(uint32_t y) { ptr = (T*)((const char*)ptr - y * stride_in_bytes); }

	TCNN_HOST_DEVICE explicit operator bool() const { return ptr; }

	T* ptr;
	uint32_t stride_in_bytes;
};

template <typename T> struct MatrixView {
	TCNN_HOST_DEVICE MatrixView() : data{nullptr}, stride_i{0}, stride_j{0} {}
	TCNN_HOST_DEVICE MatrixView(T* data, uint32_t stride_i, uint32_t stride_j) : data{data}, stride_i{stride_i}, stride_j{stride_j} {}
	TCNN_HOST_DEVICE MatrixView(const MatrixView<std::remove_const_t<T>>& other) :
		data{other.data}, stride_i{other.stride_i}, stride_j{other.stride_j} {}

	TCNN_HOST_DEVICE T& operator()(uint32_t i, uint32_t j = 0) const { return data[i * stride_i + j * stride_j]; }

	TCNN_HOST_DEVICE void advance(uint32_t m, uint32_t n) { data = &(*this)(m, n); }

	TCNN_HOST_DEVICE void advance_rows(uint32_t m) { advance(m, 0); }

	TCNN_HOST_DEVICE void advance_cols(uint32_t n) { advance(0, n); }

	template <uint32_t N> TCNN_HOST_DEVICE tvec<std::remove_const_t<T>, N> row(uint32_t m) const {
		tvec<std::remove_const_t<T>, N> result;
		TCNN_PRAGMA_UNROLL
		for (uint32_t i = 0; i < N; ++i) {
			result[i] = (*this)(m, i);
		}
		return result;
	}

	template <uint32_t N> TCNN_HOST_DEVICE tvec<std::remove_const_t<T>, N> col(uint32_t n) const {
		tvec<std::remove_const_t<T>, N> result;
		TCNN_PRAGMA_UNROLL
		for (uint32_t i = 0; i < N; ++i) {
			result[i] = (*this)(i, n);
		}
		return result;
	}

	template <typename U, uint32_t N, size_t A> TCNN_HOST_DEVICE void set_row(uint32_t m, const tvec<U, N, A>& val) {
		TCNN_PRAGMA_UNROLL
		for (uint32_t i = 0; i < N; ++i) {
			(*this)(m, i) = val[i];
		}
	}

	template <typename U, uint32_t N, size_t A> TCNN_HOST_DEVICE void set_col(uint32_t n, const tvec<U, N, A>& val) {
		TCNN_PRAGMA_UNROLL
		for (uint32_t i = 0; i < N; ++i) {
			(*this)(i, n) = val[i];
		}
	}

	TCNN_HOST_DEVICE explicit operator bool() const { return data; }

	T* data;
	uint32_t stride_i, stride_j;
};

} // namespace tcnn
