/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <math.h>  /* for abs() */
#include <stdio.h>  /* for printf() */
#include <string.h> /* for memset() */
#include <stdint.h> /* for uint64 definition */
#include <stdlib.h> /* for exit() definition */
#include <time.h> /* for clock_gettime */

#include "../dsp_util.h"

// Constant for converting time to milliseconds.
#define BILLION 1000000000LL
// Number of iterations for performance testing.
#define ITERATIONS 40000

void dsp_util_deinterleave_reference(int16_t *input, float *const *output,
				     int channels, int frames)
{
	float *output_ptr[channels];
	int i, j;

	for (i = 0; i < channels; i++)
		output_ptr[i] = output[i];

	for (i = 0; i < frames; i++)
		for (j = 0; j < channels; j++)
			*(output_ptr[j]++) = *input++ / 32768.0f;
}

void dsp_util_interleave_reference(float *const *input, int16_t *output,
				   int channels, int frames)
{
	float *input_ptr[channels];
	int i, j;

	for (i = 0; i < channels; i++)
		input_ptr[i] = input[i];

	for (i = 0; i < frames; i++)
		for (j = 0; j < channels; j++) {
			int16_t i16;
			float f = *(input_ptr[j]++) * 32768.0f;
			if (f > 32767)
				i16 = 32767;
			else if (f < -32768)
				i16 = -32768;
			else
				i16 = (int16_t) (f > 0 ? f + 0.5f : f - 0.5f);
			*output++ = i16;
		}
}

// Use fixed size allocation to avoid performance fluctuation of allocation.
#define MAXSAMPLES (65536)
// PAD buffer to check for overflows.
#define PAD 4096

void TestRounding(float in, int16_t expected, int samples)
{
	int i;
	int max_diff;
	int d;

	short* in_shorts = (short*) malloc(MAXSAMPLES * 2 * 2 + PAD);
	float* out_floats_left_c = (float*) malloc(MAXSAMPLES * 4 + PAD);
	float* out_floats_right_c = (float*) malloc(MAXSAMPLES * 4 + PAD);
	float* out_floats_left_opt = (float*) malloc(MAXSAMPLES * 4 + PAD);
	float* out_floats_right_opt = (float*) malloc(MAXSAMPLES * 4 + PAD);
	short* out_shorts_c = (short*) malloc(MAXSAMPLES * 2 * 2 + PAD);
	short* out_shorts_opt = (short*) malloc(MAXSAMPLES * 2 * 2 + PAD);

	memset(in_shorts, 0xfb, MAXSAMPLES * 2 * 2 + PAD);
	memset(out_floats_left_c, 0xfb, MAXSAMPLES * 4 + PAD);
	memset(out_floats_right_c, 0xfb, MAXSAMPLES * 4 + PAD);
	memset(out_floats_left_opt, 0xfb, MAXSAMPLES * 4 + PAD);
	memset(out_floats_right_opt, 0xfb, MAXSAMPLES * 4 + PAD);
	memset(out_shorts_c, 0xfb, MAXSAMPLES * 2 * 2 + PAD);
	memset(out_shorts_opt, 0xfb, MAXSAMPLES * 2 * 2 + PAD);

	float *out_floats_ptr_c[2];
	float *out_floats_ptr_opt[2];

	out_floats_ptr_c[0] = out_floats_left_c;
	out_floats_ptr_c[1] = out_floats_right_c;
	out_floats_ptr_opt[0] = out_floats_left_opt;
	out_floats_ptr_opt[1] = out_floats_right_opt;

	for (i = 0; i < MAXSAMPLES; ++i) {
		out_floats_left_c[i] = in;
		out_floats_right_c[i] = in;
	}

	/*  referenceinal C interleave */
	dsp_util_interleave_reference(out_floats_ptr_c, out_shorts_c, 2,
				      samples);

	/* measure optimized interleave */
	for (i = 0; i < ITERATIONS; ++i) {
		dsp_util_interleave(out_floats_ptr_c, out_shorts_opt, 2,
				    samples);
	}

	max_diff = 0;
	for (i = 0; i < (MAXSAMPLES * 2 + PAD / 2); ++i) {
		d = abs(out_shorts_c[i] - out_shorts_opt[i]);
		if (d > max_diff) {
			max_diff = d;
		}
	}
	printf("test interleave compare %6d, %10f %13f %6d %6d %6d %s\n",
		max_diff, in, in * 32768.0f, out_shorts_c[0], out_shorts_opt[0],
		expected,
		max_diff == 0 ? "PASS" : (out_shorts_opt[0] == expected ?
		"EXPECTED DIFFERENCE" : "UNEXPECTED DIFFERENCE"));

	/* measure referenceinal C deinterleave */
	dsp_util_deinterleave_reference(in_shorts, out_floats_ptr_c, 2,
					samples);

	/* measure optimized deinterleave */
	dsp_util_deinterleave(in_shorts, out_floats_ptr_opt, 2, samples);

	d = memcmp(out_floats_ptr_c[0], out_floats_ptr_opt[0], samples * 4);
	if (d) printf("left compare %d, %f %f\n", d, out_floats_ptr_c[0][0],
		      out_floats_ptr_opt[0][0]);
	d = memcmp(out_floats_ptr_c[1], out_floats_ptr_opt[1], samples * 4);
	if (d) printf("right compare %d, %f %f\n", d, out_floats_ptr_c[1][0],
			out_floats_ptr_opt[1][0]);

	free(in_shorts);
	free(out_floats_left_c);
	free(out_floats_right_c);
	free(out_floats_left_opt);
	free(out_floats_right_opt);
	free(out_shorts_c);
	free(out_shorts_opt);
}

int main(int argc, char **argv)
{
	float e = 0.000000001f;
	int samples = 16;

	dsp_enable_flush_denormal_to_zero();

	// test clamping
	TestRounding(1.0f, 32767, samples);
	TestRounding(-1.0f, -32768, samples);
	TestRounding(1.1f, 32767, samples);
	TestRounding(-1.1f, -32768, samples);
	TestRounding(2000000000.f / 32768.f, 32767, samples);
	TestRounding(-2000000000.f / 32768.f, -32768, samples);
	TestRounding(5000000000.f / 32768.f, 32767, samples);
	TestRounding(-5000000000.f / 32768.f, -32768, samples);

	// casting inf float to int produces different results
#ifdef __aarch64__
	int inf_int = 0x7f7fffff;;
	float inf = *(float *)&inf_int;
	TestRounding(inf, 0, samples);
	inf_int = 0xff7fffff;;
	inf = *(float *)&inf_int;
	TestRounding(inf, 0, samples);
#else
	int inf_int = 0x7f7fffff;;
	float inf = *(float *)&inf_int;
	TestRounding(inf, -32768, samples);
	inf_int = 0xff7fffff;;
	inf = *(float *)&inf_int;
	TestRounding(inf, -32768, samples);
#endif

	// test rounding
	TestRounding(0.25f, 8192, samples);
	TestRounding(-0.25f, -8192, samples);
	TestRounding(0.50f, 16384, samples);
	TestRounding(-0.50f, -16384, samples);
	TestRounding(1.0f / 32768.0f, 1, samples);
	TestRounding(-1.0f / 32768.0f, -1, samples);
	TestRounding(1.0f / 32768.0f + e, 1, samples);
	TestRounding(-1.0f / 32768.0f - e, -1, samples);
	TestRounding(1.0f / 32768.0f - e, 1, samples);
	TestRounding(-1.0f / 32768.0f + e, -1, samples);

	// Rounding on 'tie' is not consistent at this time.
#if defined(__i386__) || defined(__x86_64__)
	TestRounding(0.5f / 32768.0f, 0, samples);  // expect round to even
	TestRounding(-0.5f / 32768.0f, 0, samples);
#else
	TestRounding(0.5f / 32768.0f, 1, samples);  // expect round away
	TestRounding(-0.5f / 32768.0f, -1, samples);
#endif

	TestRounding(0.5f / 32768.0f + e, 1, samples);
	TestRounding(-0.5f / 32768.0f - e, 1, samples);
	TestRounding(0.5f / 32768.0f - e, 0, samples);
	TestRounding(-0.5f / 32768.0f + e, 0, samples);

	TestRounding(1.5f / 32768.0f, 2, samples);  // expect fail?
	TestRounding(-1.5f / 32768.0f, -2, samples);  // expect fail?
	TestRounding(1.5f / 32768.0f + e, 2, samples);
	TestRounding(-1.5f / 32768.0f - e, -2, samples);
	TestRounding(1.5f / 32768.0f - e, 1, samples);
	TestRounding(-1.5f / 32768.0f + e, -1, samples);

	// test denormals
	int denorm_int = 1;
	float denorm = *(float *)&denorm_int;
	TestRounding(denorm, 0, samples);
	denorm_int = 0x80000001;
	denorm = *(float *)&denorm_int;
	TestRounding(denorm, 0, samples);

	// test nans. caveat Results vary by implementation.
#if defined(__i386__) || defined(__x86_64__)
#define EXPECTED_NAN_RESULT 32767
#else
#define EXPECTED_NAN_RESULT 0
#endif
	unsigned int nan_int = 0x7f800001;
	float nan = *(float *)&nan_int;
	TestRounding(nan, EXPECTED_NAN_RESULT, samples);
	nan_int = 0xff800001;
	nan = *(float *)&nan_int;
	TestRounding(nan, EXPECTED_NAN_RESULT, samples);

	/* Test Performance */
	uint64_t diff;
	struct timespec start, end;
	int i;
	int d;

	short* in_shorts = (short*) malloc(MAXSAMPLES * 2 * 2 + PAD);
	float* out_floats_left_c = (float*) malloc(MAXSAMPLES * 4 + PAD);
	float* out_floats_right_c = (float*) malloc(MAXSAMPLES * 4 + PAD);
	float* out_floats_left_opt = (float*) malloc(MAXSAMPLES * 4 + PAD);
	float* out_floats_right_opt = (float*) malloc(MAXSAMPLES * 4 + PAD);
	short* out_shorts_c = (short*) malloc(MAXSAMPLES * 2 * 2 + PAD);
	short* out_shorts_opt = (short*) malloc(MAXSAMPLES * 2 * 2 + PAD);

	memset(in_shorts, 0x11, MAXSAMPLES * 2 * 2 + PAD);
	memset(out_floats_left_c, 0x22, MAXSAMPLES * 4 + PAD);
	memset(out_floats_right_c, 0x33, MAXSAMPLES * 4 + PAD);
	memset(out_floats_left_opt, 0x44, MAXSAMPLES * 4 + PAD);
	memset(out_floats_right_opt, 0x55, MAXSAMPLES * 4 + PAD);
	memset(out_shorts_c, 0x66, MAXSAMPLES * 2 * 2 + PAD);
	memset(out_shorts_opt, 0x66, MAXSAMPLES * 2 * 2 + PAD);

	float *out_floats_ptr_c[2];
	float *out_floats_ptr_opt[2];

	out_floats_ptr_c[0] = out_floats_left_c;
	out_floats_ptr_c[1] = out_floats_right_c;
	out_floats_ptr_opt[0] = out_floats_left_opt;
	out_floats_ptr_opt[1] = out_floats_right_opt;

	/* Benchmark dsp_util_interleave */
	for (samples = MAXSAMPLES; samples >= 1024; samples /= 2) {

		/* measure original C interleave */
		clock_gettime(CLOCK_MONOTONIC, &start); /* mark start time */
		for (i = 0; i < ITERATIONS; ++i) {
			dsp_util_interleave_reference(out_floats_ptr_c,
						      out_shorts_c,
						      2, samples);
		}
		clock_gettime(CLOCK_MONOTONIC, &end); /* mark the end time */
		diff = (BILLION * (end.tv_sec - start.tv_sec) +
			end.tv_nsec - start.tv_nsec) / 1000000;
		printf("interleave   ORIG size = %6d, elapsed time = %llu ms\n",
		       samples, (long long unsigned int) diff);

		/* measure optimized interleave */
		clock_gettime(CLOCK_MONOTONIC, &start); /* mark start time */
		for (i = 0; i < ITERATIONS; ++i) {
			dsp_util_interleave(out_floats_ptr_c, out_shorts_opt, 2,
					    samples);
		}
		clock_gettime(CLOCK_MONOTONIC, &end); /* mark the end time */
		diff = (BILLION * (end.tv_sec - start.tv_sec) +
			end.tv_nsec - start.tv_nsec) / 1000000;
		printf("interleave   SIMD size = %6d, elapsed time = %llu ms\n",
		       samples, (long long unsigned int) diff);

		/* Test C and SIMD output match */
		d = memcmp(out_shorts_c, out_shorts_opt,
			   MAXSAMPLES * 2 * 2 + PAD);
		if (d) printf("interleave compare %d, %d %d, %d %d\n", d,
			      out_shorts_c[0], out_shorts_c[1],
			      out_shorts_opt[0], out_shorts_opt[1]);
	}

	/* Benchmark dsp_util_deinterleave */
	for (samples = MAXSAMPLES; samples >= 1024; samples /= 2) {

		/* Measure original C deinterleave */
		clock_gettime(CLOCK_MONOTONIC, &start); /* mark start time */
		for (i = 0; i < ITERATIONS; ++i) {
			dsp_util_deinterleave_reference(in_shorts,
							out_floats_ptr_c,
							2, samples);
		}
		clock_gettime(CLOCK_MONOTONIC, &end); /* mark the end time */
		diff = (BILLION * (end.tv_sec - start.tv_sec) +
			end.tv_nsec - start.tv_nsec) / 1000000;
			printf("deinterleave ORIG size = %6d, "
			       "elapsed time = %llu ms\n",
			       samples, (long long unsigned int) diff);

		/* Measure optimized deinterleave */
		clock_gettime(CLOCK_MONOTONIC, &start); /* mark start time */
		for (i = 0; i < ITERATIONS; ++i) {
			dsp_util_deinterleave(in_shorts, out_floats_ptr_opt, 2,
					      samples);
		}
		clock_gettime(CLOCK_MONOTONIC, &end); /* mark the end time */
		diff = (BILLION * (end.tv_sec - start.tv_sec) +
			end.tv_nsec - start.tv_nsec) / 1000000;
		printf("deinterleave SIMD size = %6d, elapsed time = %llu ms\n",
			samples, (long long unsigned int) diff);

		/* Test C and SIMD output match */
		d = memcmp(out_floats_ptr_c[0], out_floats_ptr_opt[0],
			   samples * 4);
		if (d) printf("left compare %d, %f %f\n", d,
			      out_floats_ptr_c[0][0], out_floats_ptr_opt[0][0]);
		d = memcmp(out_floats_ptr_c[1], out_floats_ptr_opt[1],
			   samples * 4);
		if (d) printf("right compare %d, %f %f\n", d,
			      out_floats_ptr_c[1][0], out_floats_ptr_opt[1][0]);
	}

	free(in_shorts);
	free(out_floats_left_c);
	free(out_floats_right_c);
	free(out_floats_left_opt);
	free(out_floats_right_opt);
	free(out_shorts_c);
	free(out_shorts_opt);
	return 0;
}