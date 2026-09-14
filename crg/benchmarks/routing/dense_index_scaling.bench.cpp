// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — DenseIndexStore linear scan scaling
// =============================================================================
//
// What we measure
// ---------------
// DenseIndexStore<D>::Find(hash) is a linear scan: O(N_models).
// The talk implies constant-time routing. This benchmark verifies at what N
// the O(N) scan becomes measurable.
//
// Method: declare N model types, register all, then benchmark Find() for the
// last (worst-case) slot. Use template recursion to instantiate 1..1024 models.
//
// Outputs:
//   ns_per_call at N = 8, 16, 32, 64, 128, 256, 512, 1024
//
// Expected: flat up to ~64 models (fits in a cache line or two), then linear.
// =============================================================================

#include <benchmark/benchmark.h>
#include "crg/crg.hpp"
#include "crg/models/model_key.hpp"

#include <cstdint>
#include <array>
#include <utility>

// =============================================================================
// One domain per benchmark case (each needs its own DenseIndexStore state).
// We create N distinct model types via a template index tag.
// =============================================================================
namespace crg::bench::dense
{
    // Domain tags — one per benchmark scale
    struct DDomain8   {};
    struct DDomain16  {};
    struct DDomain32  {};
    struct DDomain64  {};
    struct DDomain128 {};
    struct DDomain256 {};

    // A model type parameterised by integer tag so N distinct types can be declared
    template<typename TDomain, int N>
    struct DModel {};

} // namespace crg::bench::dense

// ----- Domain declarations -----
CRG_DECLARE_DOMAIN(crg::bench::dense::DDomain8)
CRG_DECLARE_DOMAIN(crg::bench::dense::DDomain16)
CRG_DECLARE_DOMAIN(crg::bench::dense::DDomain32)
CRG_DECLARE_DOMAIN(crg::bench::dense::DDomain64)
CRG_DECLARE_DOMAIN(crg::bench::dense::DDomain128)
CRG_DECLARE_DOMAIN(crg::bench::dense::DDomain256)

// CRG_DEFINE_DOMAIN is only needed when CRG_PLUGINS_ENABLED=1 (DLL mode).
// In monolith mode (CRG_PLUGINS_ENABLED=0) it expands to nothing.
CRG_DEFINE_DOMAIN(crg::bench::dense::DDomain8)
CRG_DEFINE_DOMAIN(crg::bench::dense::DDomain16)
CRG_DEFINE_DOMAIN(crg::bench::dense::DDomain32)
CRG_DEFINE_DOMAIN(crg::bench::dense::DDomain64)
CRG_DEFINE_DOMAIN(crg::bench::dense::DDomain128)
CRG_DEFINE_DOMAIN(crg::bench::dense::DDomain256)

// ----- Model declarations using variadic macro per domain -----
//
// CRG_DECLARE_DOMAIN_MODELS does not accept template types directly.
// We declare a flat alias per (domain, index) pair and register it.
// Helper macro to reduce the repetition for up to 256 models.

#define DECL_DMODEL(Dom, N) \
    namespace crg::bench::dense { using DM_##Dom##_##N = DModel<Dom, N>; } \
    CRG_DECLARE_DOMAIN_MODELS(crg::bench::dense::Dom, crg::bench::dense::DM_##Dom##_##N)

// 8 models
DECL_DMODEL(DDomain8,   0) DECL_DMODEL(DDomain8,   1) DECL_DMODEL(DDomain8,   2) DECL_DMODEL(DDomain8,   3)
DECL_DMODEL(DDomain8,   4) DECL_DMODEL(DDomain8,   5) DECL_DMODEL(DDomain8,   6) DECL_DMODEL(DDomain8,   7)

// 16 models (inherits the 8 above for DDomain16)
DECL_DMODEL(DDomain16,  0) DECL_DMODEL(DDomain16,  1) DECL_DMODEL(DDomain16,  2) DECL_DMODEL(DDomain16,  3)
DECL_DMODEL(DDomain16,  4) DECL_DMODEL(DDomain16,  5) DECL_DMODEL(DDomain16,  6) DECL_DMODEL(DDomain16,  7)
DECL_DMODEL(DDomain16,  8) DECL_DMODEL(DDomain16,  9) DECL_DMODEL(DDomain16, 10) DECL_DMODEL(DDomain16, 11)
DECL_DMODEL(DDomain16, 12) DECL_DMODEL(DDomain16, 13) DECL_DMODEL(DDomain16, 14) DECL_DMODEL(DDomain16, 15)

// 32 models
DECL_DMODEL(DDomain32,  0) DECL_DMODEL(DDomain32,  1) DECL_DMODEL(DDomain32,  2) DECL_DMODEL(DDomain32,  3)
DECL_DMODEL(DDomain32,  4) DECL_DMODEL(DDomain32,  5) DECL_DMODEL(DDomain32,  6) DECL_DMODEL(DDomain32,  7)
DECL_DMODEL(DDomain32,  8) DECL_DMODEL(DDomain32,  9) DECL_DMODEL(DDomain32, 10) DECL_DMODEL(DDomain32, 11)
DECL_DMODEL(DDomain32, 12) DECL_DMODEL(DDomain32, 13) DECL_DMODEL(DDomain32, 14) DECL_DMODEL(DDomain32, 15)
DECL_DMODEL(DDomain32, 16) DECL_DMODEL(DDomain32, 17) DECL_DMODEL(DDomain32, 18) DECL_DMODEL(DDomain32, 19)
DECL_DMODEL(DDomain32, 20) DECL_DMODEL(DDomain32, 21) DECL_DMODEL(DDomain32, 22) DECL_DMODEL(DDomain32, 23)
DECL_DMODEL(DDomain32, 24) DECL_DMODEL(DDomain32, 25) DECL_DMODEL(DDomain32, 26) DECL_DMODEL(DDomain32, 27)
DECL_DMODEL(DDomain32, 28) DECL_DMODEL(DDomain32, 29) DECL_DMODEL(DDomain32, 30) DECL_DMODEL(DDomain32, 31)

// 64 models
DECL_DMODEL(DDomain64,  0) DECL_DMODEL(DDomain64,  1) DECL_DMODEL(DDomain64,  2) DECL_DMODEL(DDomain64,  3)
DECL_DMODEL(DDomain64,  4) DECL_DMODEL(DDomain64,  5) DECL_DMODEL(DDomain64,  6) DECL_DMODEL(DDomain64,  7)
DECL_DMODEL(DDomain64,  8) DECL_DMODEL(DDomain64,  9) DECL_DMODEL(DDomain64, 10) DECL_DMODEL(DDomain64, 11)
DECL_DMODEL(DDomain64, 12) DECL_DMODEL(DDomain64, 13) DECL_DMODEL(DDomain64, 14) DECL_DMODEL(DDomain64, 15)
DECL_DMODEL(DDomain64, 16) DECL_DMODEL(DDomain64, 17) DECL_DMODEL(DDomain64, 18) DECL_DMODEL(DDomain64, 19)
DECL_DMODEL(DDomain64, 20) DECL_DMODEL(DDomain64, 21) DECL_DMODEL(DDomain64, 22) DECL_DMODEL(DDomain64, 23)
DECL_DMODEL(DDomain64, 24) DECL_DMODEL(DDomain64, 25) DECL_DMODEL(DDomain64, 26) DECL_DMODEL(DDomain64, 27)
DECL_DMODEL(DDomain64, 28) DECL_DMODEL(DDomain64, 29) DECL_DMODEL(DDomain64, 30) DECL_DMODEL(DDomain64, 31)
DECL_DMODEL(DDomain64, 32) DECL_DMODEL(DDomain64, 33) DECL_DMODEL(DDomain64, 34) DECL_DMODEL(DDomain64, 35)
DECL_DMODEL(DDomain64, 36) DECL_DMODEL(DDomain64, 37) DECL_DMODEL(DDomain64, 38) DECL_DMODEL(DDomain64, 39)
DECL_DMODEL(DDomain64, 40) DECL_DMODEL(DDomain64, 41) DECL_DMODEL(DDomain64, 42) DECL_DMODEL(DDomain64, 43)
DECL_DMODEL(DDomain64, 44) DECL_DMODEL(DDomain64, 45) DECL_DMODEL(DDomain64, 46) DECL_DMODEL(DDomain64, 47)
DECL_DMODEL(DDomain64, 48) DECL_DMODEL(DDomain64, 49) DECL_DMODEL(DDomain64, 50) DECL_DMODEL(DDomain64, 51)
DECL_DMODEL(DDomain64, 52) DECL_DMODEL(DDomain64, 53) DECL_DMODEL(DDomain64, 54) DECL_DMODEL(DDomain64, 55)
DECL_DMODEL(DDomain64, 56) DECL_DMODEL(DDomain64, 57) DECL_DMODEL(DDomain64, 58) DECL_DMODEL(DDomain64, 59)
DECL_DMODEL(DDomain64, 60) DECL_DMODEL(DDomain64, 61) DECL_DMODEL(DDomain64, 62) DECL_DMODEL(DDomain64, 63)

// 128 models — extend DDomain128 from 64 model declarations
DECL_DMODEL(DDomain128,  0) DECL_DMODEL(DDomain128,  1) DECL_DMODEL(DDomain128,  2) DECL_DMODEL(DDomain128,  3)
DECL_DMODEL(DDomain128,  4) DECL_DMODEL(DDomain128,  5) DECL_DMODEL(DDomain128,  6) DECL_DMODEL(DDomain128,  7)
DECL_DMODEL(DDomain128,  8) DECL_DMODEL(DDomain128,  9) DECL_DMODEL(DDomain128, 10) DECL_DMODEL(DDomain128, 11)
DECL_DMODEL(DDomain128, 12) DECL_DMODEL(DDomain128, 13) DECL_DMODEL(DDomain128, 14) DECL_DMODEL(DDomain128, 15)
DECL_DMODEL(DDomain128, 16) DECL_DMODEL(DDomain128, 17) DECL_DMODEL(DDomain128, 18) DECL_DMODEL(DDomain128, 19)
DECL_DMODEL(DDomain128, 20) DECL_DMODEL(DDomain128, 21) DECL_DMODEL(DDomain128, 22) DECL_DMODEL(DDomain128, 23)
DECL_DMODEL(DDomain128, 24) DECL_DMODEL(DDomain128, 25) DECL_DMODEL(DDomain128, 26) DECL_DMODEL(DDomain128, 27)
DECL_DMODEL(DDomain128, 28) DECL_DMODEL(DDomain128, 29) DECL_DMODEL(DDomain128, 30) DECL_DMODEL(DDomain128, 31)
DECL_DMODEL(DDomain128, 32) DECL_DMODEL(DDomain128, 33) DECL_DMODEL(DDomain128, 34) DECL_DMODEL(DDomain128, 35)
DECL_DMODEL(DDomain128, 36) DECL_DMODEL(DDomain128, 37) DECL_DMODEL(DDomain128, 38) DECL_DMODEL(DDomain128, 39)
DECL_DMODEL(DDomain128, 40) DECL_DMODEL(DDomain128, 41) DECL_DMODEL(DDomain128, 42) DECL_DMODEL(DDomain128, 43)
DECL_DMODEL(DDomain128, 44) DECL_DMODEL(DDomain128, 45) DECL_DMODEL(DDomain128, 46) DECL_DMODEL(DDomain128, 47)
DECL_DMODEL(DDomain128, 48) DECL_DMODEL(DDomain128, 49) DECL_DMODEL(DDomain128, 50) DECL_DMODEL(DDomain128, 51)
DECL_DMODEL(DDomain128, 52) DECL_DMODEL(DDomain128, 53) DECL_DMODEL(DDomain128, 54) DECL_DMODEL(DDomain128, 55)
DECL_DMODEL(DDomain128, 56) DECL_DMODEL(DDomain128, 57) DECL_DMODEL(DDomain128, 58) DECL_DMODEL(DDomain128, 59)
DECL_DMODEL(DDomain128, 60) DECL_DMODEL(DDomain128, 61) DECL_DMODEL(DDomain128, 62) DECL_DMODEL(DDomain128, 63)
DECL_DMODEL(DDomain128, 64) DECL_DMODEL(DDomain128, 65) DECL_DMODEL(DDomain128, 66) DECL_DMODEL(DDomain128, 67)
DECL_DMODEL(DDomain128, 68) DECL_DMODEL(DDomain128, 69) DECL_DMODEL(DDomain128, 70) DECL_DMODEL(DDomain128, 71)
DECL_DMODEL(DDomain128, 72) DECL_DMODEL(DDomain128, 73) DECL_DMODEL(DDomain128, 74) DECL_DMODEL(DDomain128, 75)
DECL_DMODEL(DDomain128, 76) DECL_DMODEL(DDomain128, 77) DECL_DMODEL(DDomain128, 78) DECL_DMODEL(DDomain128, 79)
DECL_DMODEL(DDomain128, 80) DECL_DMODEL(DDomain128, 81) DECL_DMODEL(DDomain128, 82) DECL_DMODEL(DDomain128, 83)
DECL_DMODEL(DDomain128, 84) DECL_DMODEL(DDomain128, 85) DECL_DMODEL(DDomain128, 86) DECL_DMODEL(DDomain128, 87)
DECL_DMODEL(DDomain128, 88) DECL_DMODEL(DDomain128, 89) DECL_DMODEL(DDomain128, 90) DECL_DMODEL(DDomain128, 91)
DECL_DMODEL(DDomain128, 92) DECL_DMODEL(DDomain128, 93) DECL_DMODEL(DDomain128, 94) DECL_DMODEL(DDomain128, 95)
DECL_DMODEL(DDomain128,  96) DECL_DMODEL(DDomain128,  97) DECL_DMODEL(DDomain128,  98) DECL_DMODEL(DDomain128,  99)
DECL_DMODEL(DDomain128, 100) DECL_DMODEL(DDomain128, 101) DECL_DMODEL(DDomain128, 102) DECL_DMODEL(DDomain128, 103)
DECL_DMODEL(DDomain128, 104) DECL_DMODEL(DDomain128, 105) DECL_DMODEL(DDomain128, 106) DECL_DMODEL(DDomain128, 107)
DECL_DMODEL(DDomain128, 108) DECL_DMODEL(DDomain128, 109) DECL_DMODEL(DDomain128, 110) DECL_DMODEL(DDomain128, 111)
DECL_DMODEL(DDomain128, 112) DECL_DMODEL(DDomain128, 113) DECL_DMODEL(DDomain128, 114) DECL_DMODEL(DDomain128, 115)
DECL_DMODEL(DDomain128, 116) DECL_DMODEL(DDomain128, 117) DECL_DMODEL(DDomain128, 118) DECL_DMODEL(DDomain128, 119)
DECL_DMODEL(DDomain128, 120) DECL_DMODEL(DDomain128, 121) DECL_DMODEL(DDomain128, 122) DECL_DMODEL(DDomain128, 123)
DECL_DMODEL(DDomain128, 124) DECL_DMODEL(DDomain128, 125) DECL_DMODEL(DDomain128, 126) DECL_DMODEL(DDomain128, 127)

// 256 models
DECL_DMODEL(DDomain256,   0) DECL_DMODEL(DDomain256,   1) DECL_DMODEL(DDomain256,   2) DECL_DMODEL(DDomain256,   3)
DECL_DMODEL(DDomain256,   4) DECL_DMODEL(DDomain256,   5) DECL_DMODEL(DDomain256,   6) DECL_DMODEL(DDomain256,   7)
DECL_DMODEL(DDomain256,   8) DECL_DMODEL(DDomain256,   9) DECL_DMODEL(DDomain256,  10) DECL_DMODEL(DDomain256,  11)
DECL_DMODEL(DDomain256,  12) DECL_DMODEL(DDomain256,  13) DECL_DMODEL(DDomain256,  14) DECL_DMODEL(DDomain256,  15)
DECL_DMODEL(DDomain256,  16) DECL_DMODEL(DDomain256,  17) DECL_DMODEL(DDomain256,  18) DECL_DMODEL(DDomain256,  19)
DECL_DMODEL(DDomain256,  20) DECL_DMODEL(DDomain256,  21) DECL_DMODEL(DDomain256,  22) DECL_DMODEL(DDomain256,  23)
DECL_DMODEL(DDomain256,  24) DECL_DMODEL(DDomain256,  25) DECL_DMODEL(DDomain256,  26) DECL_DMODEL(DDomain256,  27)
DECL_DMODEL(DDomain256,  28) DECL_DMODEL(DDomain256,  29) DECL_DMODEL(DDomain256,  30) DECL_DMODEL(DDomain256,  31)
DECL_DMODEL(DDomain256,  32) DECL_DMODEL(DDomain256,  33) DECL_DMODEL(DDomain256,  34) DECL_DMODEL(DDomain256,  35)
DECL_DMODEL(DDomain256,  36) DECL_DMODEL(DDomain256,  37) DECL_DMODEL(DDomain256,  38) DECL_DMODEL(DDomain256,  39)
DECL_DMODEL(DDomain256,  40) DECL_DMODEL(DDomain256,  41) DECL_DMODEL(DDomain256,  42) DECL_DMODEL(DDomain256,  43)
DECL_DMODEL(DDomain256,  44) DECL_DMODEL(DDomain256,  45) DECL_DMODEL(DDomain256,  46) DECL_DMODEL(DDomain256,  47)
DECL_DMODEL(DDomain256,  48) DECL_DMODEL(DDomain256,  49) DECL_DMODEL(DDomain256,  50) DECL_DMODEL(DDomain256,  51)
DECL_DMODEL(DDomain256,  52) DECL_DMODEL(DDomain256,  53) DECL_DMODEL(DDomain256,  54) DECL_DMODEL(DDomain256,  55)
DECL_DMODEL(DDomain256,  56) DECL_DMODEL(DDomain256,  57) DECL_DMODEL(DDomain256,  58) DECL_DMODEL(DDomain256,  59)
DECL_DMODEL(DDomain256,  60) DECL_DMODEL(DDomain256,  61) DECL_DMODEL(DDomain256,  62) DECL_DMODEL(DDomain256,  63)
DECL_DMODEL(DDomain256,  64) DECL_DMODEL(DDomain256,  65) DECL_DMODEL(DDomain256,  66) DECL_DMODEL(DDomain256,  67)
DECL_DMODEL(DDomain256,  68) DECL_DMODEL(DDomain256,  69) DECL_DMODEL(DDomain256,  70) DECL_DMODEL(DDomain256,  71)
DECL_DMODEL(DDomain256,  72) DECL_DMODEL(DDomain256,  73) DECL_DMODEL(DDomain256,  74) DECL_DMODEL(DDomain256,  75)
DECL_DMODEL(DDomain256,  76) DECL_DMODEL(DDomain256,  77) DECL_DMODEL(DDomain256,  78) DECL_DMODEL(DDomain256,  79)
DECL_DMODEL(DDomain256,  80) DECL_DMODEL(DDomain256,  81) DECL_DMODEL(DDomain256,  82) DECL_DMODEL(DDomain256,  83)
DECL_DMODEL(DDomain256,  84) DECL_DMODEL(DDomain256,  85) DECL_DMODEL(DDomain256,  86) DECL_DMODEL(DDomain256,  87)
DECL_DMODEL(DDomain256,  88) DECL_DMODEL(DDomain256,  89) DECL_DMODEL(DDomain256,  90) DECL_DMODEL(DDomain256,  91)
DECL_DMODEL(DDomain256,  92) DECL_DMODEL(DDomain256,  93) DECL_DMODEL(DDomain256,  94) DECL_DMODEL(DDomain256,  95)
DECL_DMODEL(DDomain256,  96) DECL_DMODEL(DDomain256,  97) DECL_DMODEL(DDomain256,  98) DECL_DMODEL(DDomain256,  99)
DECL_DMODEL(DDomain256, 100) DECL_DMODEL(DDomain256, 101) DECL_DMODEL(DDomain256, 102) DECL_DMODEL(DDomain256, 103)
DECL_DMODEL(DDomain256, 104) DECL_DMODEL(DDomain256, 105) DECL_DMODEL(DDomain256, 106) DECL_DMODEL(DDomain256, 107)
DECL_DMODEL(DDomain256, 108) DECL_DMODEL(DDomain256, 109) DECL_DMODEL(DDomain256, 110) DECL_DMODEL(DDomain256, 111)
DECL_DMODEL(DDomain256, 112) DECL_DMODEL(DDomain256, 113) DECL_DMODEL(DDomain256, 114) DECL_DMODEL(DDomain256, 115)
DECL_DMODEL(DDomain256, 116) DECL_DMODEL(DDomain256, 117) DECL_DMODEL(DDomain256, 118) DECL_DMODEL(DDomain256, 119)
DECL_DMODEL(DDomain256, 120) DECL_DMODEL(DDomain256, 121) DECL_DMODEL(DDomain256, 122) DECL_DMODEL(DDomain256, 123)
DECL_DMODEL(DDomain256, 124) DECL_DMODEL(DDomain256, 125) DECL_DMODEL(DDomain256, 126) DECL_DMODEL(DDomain256, 127)
DECL_DMODEL(DDomain256, 128) DECL_DMODEL(DDomain256, 129) DECL_DMODEL(DDomain256, 130) DECL_DMODEL(DDomain256, 131)
DECL_DMODEL(DDomain256, 132) DECL_DMODEL(DDomain256, 133) DECL_DMODEL(DDomain256, 134) DECL_DMODEL(DDomain256, 135)
DECL_DMODEL(DDomain256, 136) DECL_DMODEL(DDomain256, 137) DECL_DMODEL(DDomain256, 138) DECL_DMODEL(DDomain256, 139)
DECL_DMODEL(DDomain256, 140) DECL_DMODEL(DDomain256, 141) DECL_DMODEL(DDomain256, 142) DECL_DMODEL(DDomain256, 143)
DECL_DMODEL(DDomain256, 144) DECL_DMODEL(DDomain256, 145) DECL_DMODEL(DDomain256, 146) DECL_DMODEL(DDomain256, 147)
DECL_DMODEL(DDomain256, 148) DECL_DMODEL(DDomain256, 149) DECL_DMODEL(DDomain256, 150) DECL_DMODEL(DDomain256, 151)
DECL_DMODEL(DDomain256, 152) DECL_DMODEL(DDomain256, 153) DECL_DMODEL(DDomain256, 154) DECL_DMODEL(DDomain256, 155)
DECL_DMODEL(DDomain256, 156) DECL_DMODEL(DDomain256, 157) DECL_DMODEL(DDomain256, 158) DECL_DMODEL(DDomain256, 159)
DECL_DMODEL(DDomain256, 160) DECL_DMODEL(DDomain256, 161) DECL_DMODEL(DDomain256, 162) DECL_DMODEL(DDomain256, 163)
DECL_DMODEL(DDomain256, 164) DECL_DMODEL(DDomain256, 165) DECL_DMODEL(DDomain256, 166) DECL_DMODEL(DDomain256, 167)
DECL_DMODEL(DDomain256, 168) DECL_DMODEL(DDomain256, 169) DECL_DMODEL(DDomain256, 170) DECL_DMODEL(DDomain256, 171)
DECL_DMODEL(DDomain256, 172) DECL_DMODEL(DDomain256, 173) DECL_DMODEL(DDomain256, 174) DECL_DMODEL(DDomain256, 175)
DECL_DMODEL(DDomain256, 176) DECL_DMODEL(DDomain256, 177) DECL_DMODEL(DDomain256, 178) DECL_DMODEL(DDomain256, 179)
DECL_DMODEL(DDomain256, 180) DECL_DMODEL(DDomain256, 181) DECL_DMODEL(DDomain256, 182) DECL_DMODEL(DDomain256, 183)
DECL_DMODEL(DDomain256, 184) DECL_DMODEL(DDomain256, 185) DECL_DMODEL(DDomain256, 186) DECL_DMODEL(DDomain256, 187)
DECL_DMODEL(DDomain256, 188) DECL_DMODEL(DDomain256, 189) DECL_DMODEL(DDomain256, 190) DECL_DMODEL(DDomain256, 191)
DECL_DMODEL(DDomain256, 192) DECL_DMODEL(DDomain256, 193) DECL_DMODEL(DDomain256, 194) DECL_DMODEL(DDomain256, 195)
DECL_DMODEL(DDomain256, 196) DECL_DMODEL(DDomain256, 197) DECL_DMODEL(DDomain256, 198) DECL_DMODEL(DDomain256, 199)
DECL_DMODEL(DDomain256, 200) DECL_DMODEL(DDomain256, 201) DECL_DMODEL(DDomain256, 202) DECL_DMODEL(DDomain256, 203)
DECL_DMODEL(DDomain256, 204) DECL_DMODEL(DDomain256, 205) DECL_DMODEL(DDomain256, 206) DECL_DMODEL(DDomain256, 207)
DECL_DMODEL(DDomain256, 208) DECL_DMODEL(DDomain256, 209) DECL_DMODEL(DDomain256, 210) DECL_DMODEL(DDomain256, 211)
DECL_DMODEL(DDomain256, 212) DECL_DMODEL(DDomain256, 213) DECL_DMODEL(DDomain256, 214) DECL_DMODEL(DDomain256, 215)
DECL_DMODEL(DDomain256, 216) DECL_DMODEL(DDomain256, 217) DECL_DMODEL(DDomain256, 218) DECL_DMODEL(DDomain256, 219)
DECL_DMODEL(DDomain256, 220) DECL_DMODEL(DDomain256, 221) DECL_DMODEL(DDomain256, 222) DECL_DMODEL(DDomain256, 223)
DECL_DMODEL(DDomain256, 224) DECL_DMODEL(DDomain256, 225) DECL_DMODEL(DDomain256, 226) DECL_DMODEL(DDomain256, 227)
DECL_DMODEL(DDomain256, 228) DECL_DMODEL(DDomain256, 229) DECL_DMODEL(DDomain256, 230) DECL_DMODEL(DDomain256, 231)
DECL_DMODEL(DDomain256, 232) DECL_DMODEL(DDomain256, 233) DECL_DMODEL(DDomain256, 234) DECL_DMODEL(DDomain256, 235)
DECL_DMODEL(DDomain256, 236) DECL_DMODEL(DDomain256, 237) DECL_DMODEL(DDomain256, 238) DECL_DMODEL(DDomain256, 239)
DECL_DMODEL(DDomain256, 240) DECL_DMODEL(DDomain256, 241) DECL_DMODEL(DDomain256, 242) DECL_DMODEL(DDomain256, 243)
DECL_DMODEL(DDomain256, 244) DECL_DMODEL(DDomain256, 245) DECL_DMODEL(DDomain256, 246) DECL_DMODEL(DDomain256, 247)
DECL_DMODEL(DDomain256, 248) DECL_DMODEL(DDomain256, 249) DECL_DMODEL(DDomain256, 250) DECL_DMODEL(DDomain256, 251)
DECL_DMODEL(DDomain256, 252) DECL_DMODEL(DDomain256, 253) DECL_DMODEL(DDomain256, 254) DECL_DMODEL(DDomain256, 255)

// =============================================================================
// BENCHMARK TEMPLATE
// Measure Find() for the LAST slot (worst case: scan all N entries).
// =============================================================================
template<typename TDomain, typename TLastModel>
static void RunDenseScalingBench(benchmark::State& state, int nModels) {
    // Warm up: force DenseIndexStore to build its table
    auto warmup = crg::models::ModelToken<TDomain>::template FromType<TLastModel>();
    benchmark::DoNotOptimize(warmup);

    for (auto _ : state) {
        auto h = crg::models::ModelToken<TDomain>::template FromType<TLastModel>();
        benchmark::DoNotOptimize(h);
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["n_models"]   = benchmark::Counter(static_cast<double>(nModels));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}

using namespace crg::bench::dense;
static void BM_DenseIndex_8models(benchmark::State& s)   { RunDenseScalingBench<DDomain8,   DM_DDomain8_7>(s, 8); }
static void BM_DenseIndex_16models(benchmark::State& s)  { RunDenseScalingBench<DDomain16,  DM_DDomain16_15>(s, 16); }
static void BM_DenseIndex_32models(benchmark::State& s)  { RunDenseScalingBench<DDomain32,  DM_DDomain32_31>(s, 32); }
static void BM_DenseIndex_64models(benchmark::State& s)  { RunDenseScalingBench<DDomain64,  DM_DDomain64_63>(s, 64); }
static void BM_DenseIndex_128models(benchmark::State& s) { RunDenseScalingBench<DDomain128, DM_DDomain128_127>(s, 128); }
static void BM_DenseIndex_256models(benchmark::State& s) { RunDenseScalingBench<DDomain256, DM_DDomain256_255>(s, 256); }

BENCHMARK(BM_DenseIndex_8models);
BENCHMARK(BM_DenseIndex_16models);
BENCHMARK(BM_DenseIndex_32models);
BENCHMARK(BM_DenseIndex_64models);
BENCHMARK(BM_DenseIndex_128models);
BENCHMARK(BM_DenseIndex_256models);
