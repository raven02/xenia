/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/backend/x64/x64_tracers.h"

#include "xenia/base/vec128.h"
#include "xenia/cpu/backend/x64/x64_emitter.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"

using namespace xe;
using namespace xe::cpu::backend::x64;
using namespace xe::cpu;

namespace xe {
namespace cpu {
namespace backend {
namespace x64 {

#define ITRACE 0
#define DTRACE 0

#define TARGET_THREAD 0

bool trace_enabled = true;

#define THREAD_MATCH \
  (!TARGET_THREAD || thread_state->thread_id() == TARGET_THREAD)
#if !DTRACE
#define IFLUSH() \
  if (trace_enabled && THREAD_MATCH) fflush(stdout)
#else
#define IFLUSH()
#endif
#define IPRINT \
  if (trace_enabled && THREAD_MATCH) printf
#define DFLUSH() \
  if (trace_enabled && THREAD_MATCH) fflush(stdout)
#define DPRINT \
  DFLUSH();    \
  if (trace_enabled && THREAD_MATCH) printf

uint32_t GetTracingMode() {
  uint32_t mode = 0;
#if ITRACE
  mode |= TRACING_INSTR;
#endif  // ITRACE
#if DTRACE
  mode |= TRACING_DATA;
#endif  // DTRACE
  return mode;
}

void TraceString(void* raw_context, const char* str) {
  auto thread_state = *((ThreadState**)raw_context);
  IPRINT("XE[t] :%d: %s\n", thread_state->thread_id(), str);
  IFLUSH();
}

void TraceContextLoadI8(void* raw_context, uint64_t offset, uint8_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%d (%X) = ctx i8 +%llu\n", (int8_t)value, value, offset);
}
void TraceContextLoadI16(void* raw_context, uint64_t offset, uint16_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%d (%X) = ctx i16 +%llu\n", (int16_t)value, value, offset);
}
void TraceContextLoadI32(void* raw_context, uint64_t offset, uint32_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%d (%X) = ctx i32 +%llu\n", (int32_t)value, value, offset);
}
void TraceContextLoadI64(void* raw_context, uint64_t offset, uint64_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%lld (%llX) = ctx i64 +%llu\n", (int64_t)value, value, offset);
}
void TraceContextLoadF32(void* raw_context, uint64_t offset, __m128 value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%e (%X) = ctx f32 +%llu\n", xe::m128_f32<0>(value),
         xe::m128_i32<0>(value), offset);
}
void TraceContextLoadF64(void* raw_context, uint64_t offset,
                         const double* value) {
  auto thread_state = *((ThreadState**)raw_context);
  auto v = _mm_loadu_pd(value);
  DPRINT("%le (%llX) = ctx f64 +%llu\n", xe::m128_f64<0>(v), xe::m128_i64<0>(v),
         offset);
}
void TraceContextLoadV128(void* raw_context, uint64_t offset, __m128 value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("[%e, %e, %e, %e] [%.8X, %.8X, %.8X, %.8X] = ctx v128 +%llu\n",
         xe::m128_f32<0>(value), xe::m128_f32<1>(value), xe::m128_f32<2>(value),
         xe::m128_f32<3>(value), xe::m128_i32<0>(value), xe::m128_i32<1>(value),
         xe::m128_i32<2>(value), xe::m128_i32<3>(value), offset);
}

void TraceContextStoreI8(void* raw_context, uint64_t offset, uint8_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("ctx i8 +%llu = %d (%X)\n", offset, (int8_t)value, value);
}
void TraceContextStoreI16(void* raw_context, uint64_t offset, uint16_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("ctx i16 +%llu = %d (%X)\n", offset, (int16_t)value, value);
}
void TraceContextStoreI32(void* raw_context, uint64_t offset, uint32_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("ctx i32 +%llu = %d (%X)\n", offset, (int32_t)value, value);
}
void TraceContextStoreI64(void* raw_context, uint64_t offset, uint64_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("ctx i64 +%llu = %lld (%llX)\n", offset, (int64_t)value, value);
}
void TraceContextStoreF32(void* raw_context, uint64_t offset, __m128 value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("ctx f32 +%llu = %e (%X)\n", offset, xe::m128_f32<0>(value),
         xe::m128_i32<0>(value));
}
void TraceContextStoreF64(void* raw_context, uint64_t offset,
                          const double* value) {
  auto thread_state = *((ThreadState**)raw_context);
  auto v = _mm_loadu_pd(value);
  DPRINT("ctx f64 +%llu = %le (%llX)\n", offset, xe::m128_f64<0>(v),
         xe::m128_i64<0>(v));
}
void TraceContextStoreV128(void* raw_context, uint64_t offset, __m128 value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("ctx v128 +%llu = [%e, %e, %e, %e] [%.8X, %.8X, %.8X, %.8X]\n", offset,
         xe::m128_f32<0>(value), xe::m128_f32<1>(value), xe::m128_f32<2>(value),
         xe::m128_f32<3>(value), xe::m128_i32<0>(value), xe::m128_i32<1>(value),
         xe::m128_i32<2>(value), xe::m128_i32<3>(value));
}

void TraceMemoryLoadI8(void* raw_context, uint32_t address, uint8_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%d (%X) = load.i8 %.8X\n", (int8_t)value, value, address);
}
void TraceMemoryLoadI16(void* raw_context, uint32_t address, uint16_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%d (%X) = load.i16 %.8X\n", (int16_t)value, value, address);
}
void TraceMemoryLoadI32(void* raw_context, uint32_t address, uint32_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%d (%X) = load.i32 %.8X\n", (int32_t)value, value, address);
}
void TraceMemoryLoadI64(void* raw_context, uint32_t address, uint64_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%lld (%llX) = load.i64 %.8X\n", (int64_t)value, value, address);
}
void TraceMemoryLoadF32(void* raw_context, uint32_t address, __m128 value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%e (%X) = load.f32 %.8X\n", xe::m128_f32<0>(value),
         xe::m128_i32<0>(value), address);
}
void TraceMemoryLoadF64(void* raw_context, uint32_t address, __m128 value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("%le (%llX) = load.f64 %.8X\n", xe::m128_f64<0>(value),
         xe::m128_i64<0>(value), address);
}
void TraceMemoryLoadV128(void* raw_context, uint32_t address, __m128 value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("[%e, %e, %e, %e] [%.8X, %.8X, %.8X, %.8X] = load.v128 %.8X\n",
         xe::m128_f32<0>(value), xe::m128_f32<1>(value), xe::m128_f32<2>(value),
         xe::m128_f32<3>(value), xe::m128_i32<0>(value), xe::m128_i32<1>(value),
         xe::m128_i32<2>(value), xe::m128_i32<3>(value), address);
}

void TraceMemoryStoreI8(void* raw_context, uint32_t address, uint8_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("store.i8 %.8X = %d (%X)\n", address, (int8_t)value, value);
}
void TraceMemoryStoreI16(void* raw_context, uint32_t address, uint16_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("store.i16 %.8X = %d (%X)\n", address, (int16_t)value, value);
}
void TraceMemoryStoreI32(void* raw_context, uint32_t address, uint32_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("store.i32 %.8X = %d (%X)\n", address, (int32_t)value, value);
}
void TraceMemoryStoreI64(void* raw_context, uint32_t address, uint64_t value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("store.i64 %.8X = %lld (%llX)\n", address, (int64_t)value, value);
}
void TraceMemoryStoreF32(void* raw_context, uint32_t address, __m128 value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("store.f32 %.8X = %e (%X)\n", address, xe::m128_f32<0>(value),
         xe::m128_i32<0>(value));
}
void TraceMemoryStoreF64(void* raw_context, uint32_t address, __m128 value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("store.f64 %.8X = %le (%llX)\n", address, xe::m128_f64<0>(value),
         xe::m128_i64<0>(value));
}
void TraceMemoryStoreV128(void* raw_context, uint32_t address, __m128 value) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("store.v128 %.8X = [%e, %e, %e, %e] [%.8X, %.8X, %.8X, %.8X]\n",
         address, xe::m128_f32<0>(value), xe::m128_f32<1>(value),
         xe::m128_f32<2>(value), xe::m128_f32<3>(value), xe::m128_i32<0>(value),
         xe::m128_i32<1>(value), xe::m128_i32<2>(value),
         xe::m128_i32<3>(value));
}

void TraceMemset(void* raw_context, uint32_t address, uint8_t value,
                 uint32_t length) {
  auto thread_state = *((ThreadState**)raw_context);
  DPRINT("memset %.8X-%.8X (%d) = %.2X", address, address + length, length,
         value);
}

}  // namespace x64
}  // namespace backend
}  // namespace cpu
}  // namespace xe
