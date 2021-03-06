/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* API for Brotli decompression */

#ifndef BROTLI_DEC_DECODE_H_
#define BROTLI_DEC_DECODE_H_

#include "./state.h"
#include "./streams.h"
#include "./types.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef enum {
  /* Decoding error, e.g. corrupt input or no memory */
  BROTLI_RESULT_ERROR = 0,
  /* Successfully completely done */
  BROTLI_RESULT_SUCCESS = 1,
  /* Partially done, but must be called again with more input */
  BROTLI_RESULT_NEEDS_MORE_INPUT = 2,
  /* Partially done, but must be called again with more output */
  BROTLI_RESULT_NEEDS_MORE_OUTPUT = 3
} BrotliResult;

/* BROTLI_FAILURE macro unwraps to BROTLI_RESULT_ERROR in non-debug build. */
/* In debug build it dumps file name, line and pretty function name. */
#if defined(_MSC_VER) || !defined(BROTLI_DEBUG)
#define BROTLI_FAILURE() BROTLI_RESULT_ERROR
#else
#define BROTLI_FAILURE() \
    BrotliFailure(__FILE__, __LINE__, __PRETTY_FUNCTION__)
static inline BrotliResult BrotliFailure(const char *f, int l, const char *fn) {
  fprintf(stderr, "ERROR at %s:%d (%s)\n", f, l, fn);
  fflush(stderr);
  return BROTLI_RESULT_ERROR;
}
#endif

/* Creates the instance of BrotliState and initializes it. alloc_func and
   free_func MUST be both zero or both non-zero. In the case they are both zero,
   default memory allocators are used. opaque parameter is passed to alloc_func
   and free_func when they are called. */
BrotliState* BrotliCreateState(
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);

/* Deinitializes and frees BrotliState instance. */
void BrotliDestroyState(BrotliState* state);

/* Sets *decoded_size to the decompressed size of the given encoded stream. */
/* This function only works if the encoded buffer has a single meta block, */
/* or if it has two meta-blocks, where the first is uncompressed and the */
/* second is empty. */
/* Returns 1 on success, 0 on failure. */
int BrotliDecompressedSize(size_t encoded_size,
                           const uint8_t* encoded_buffer,
                           size_t* decoded_size);

/* Decompresses the data in encoded_buffer into decoded_buffer, and sets */
/* *decoded_size to the decompressed length. */
BrotliResult BrotliDecompressBuffer(size_t encoded_size,
                                    const uint8_t* encoded_buffer,
                                    size_t* decoded_size,
                                    uint8_t* decoded_buffer);

/* Same as above, but uses the specified input and output callbacks instead */
/* of reading from and writing to pre-allocated memory buffers. */
/* DEPRECATED */
BrotliResult BrotliDecompress(BrotliInput input, BrotliOutput output);

/* Same as above, but supports the caller to call the decoder repeatedly with
   partial data to support streaming. The state must be initialized with
   BrotliStateInit and reused with every call for the same stream.
   Return values:
   0: failure.
   1: success, and done.
   2: success so far, end not reached so should call again with more input.
   The finish parameter is used as follows, for a series of calls with the
   same state:
   0: Every call except the last one must be called with finish set to 0. The
      last call may have finish set to either 0 or 1. Only if finish is 0, can
      the function return 2. It may also return 0 or 1, in that case no more
      calls (even with finish 1) may be made.
   1: Only the last call may have finish set to 1. It's ok to give empty input
      if all input was already given to previous calls. It is also ok to have
      only one single call in total, with finish 1, and with all input
      available immediately. That matches the non-streaming case. If finish is
      1, the function can only return 0 or 1, never 2. After a finish, no more
      calls may be done.
   After everything is done, the state must be cleaned with BrotliStateCleanup
   to free allocated resources.
   The given BrotliOutput must always accept all output and make enough space,
   it returning a smaller value than the amount of bytes to write always results
   in an error.
*/
/* DEPRECATED */
BrotliResult BrotliDecompressStreaming(BrotliInput input, BrotliOutput output,
                                       int finish, BrotliState* s);

/* Same as above, but with memory buffers.
   Must be called with an allocated input buffer in *next_in and an allocated
   output buffer in *next_out. The values *available_in and *available_out
   must specify the allocated size in *next_in and *next_out respectively.
   The value *total_out must be 0 initially, and will be summed with the
   amount of output bytes written after each call, so that at the end it
   gives the complete decoded size.
   After each call, *available_in will be decremented by the amount of input
   bytes consumed, and the *next_in pointer will be incremented by that amount.
   Similarly, *available_out will be decremented by the amount of output
   bytes written, and the *next_out pointer will be incremented by that
   amount.

   The input may be partial. With each next function call, *next_in and
   *available_in must be updated to point to a next part of the compressed
   input. The current implementation will always consume all input unless
   an error occurs, so normally *available_in will always be 0 after
   calling this function and the next adjacent part of input is desired.

   In the current implementation, the function requires that there is enough
   output buffer size to write all currently processed input, so
   *available_out must be large enough. Since the function updates *next_out
   each time, as long as the output buffer is large enough you can keep
   reusing this variable. It is also possible to update *next_out and
   *available_out yourself before a next call, e.g. to point to a new larger
   buffer.
*/
/* DEPRECATED */
BrotliResult BrotliDecompressBufferStreaming(size_t* available_in,
                                             const uint8_t** next_in,
                                             int finish,
                                             size_t* available_out,
                                             uint8_t** next_out,
                                             size_t* total_out,
                                             BrotliState* s);

BrotliResult BrotliDecompressStream(size_t* available_in,
                                    const uint8_t** next_in,
                                    size_t* available_out,
                                    uint8_t** next_out,
                                    size_t* total_out,
                                    BrotliState* s);

/* Fills the new state with a dictionary for LZ77, warming up the ringbuffer,
   e.g. for custom static dictionaries for data formats.
   Not to be confused with the built-in transformable dictionary of Brotli.
   The dictionary must exist in memory until decoding is done and is owned by
   the caller. To use:
   -initialize state with BrotliStateInit
   -use BrotliSetCustomDictionary
   -use BrotliDecompressBufferStreaming
   -clean up with BrotliStateCleanup
*/
void BrotliSetCustomDictionary(
    size_t size, const uint8_t* dict, BrotliState* s);


#if defined(__cplusplus) || defined(c_plusplus)
} /* extern "C" */
#endif

#endif  /* BROTLI_DEC_DECODE_H_ */
