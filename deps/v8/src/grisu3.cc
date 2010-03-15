// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "grisu3.h"

#include "cached_powers.h"
#include "diy_fp.h"
#include "double.h"

namespace v8 {
namespace internal {

template <int alpha = -60, int gamma = -32>
class Grisu3 {
 public:
  // Provides a decimal representation of v.
  // Returns true if it succeeds, otherwise the result can not be trusted.
  // There will be *length digits inside the buffer (not null-terminated).
  // If the function returns true then
  //        v == (double) (buffer * 10^decimal_exponent).
  // The digits in the buffer are the shortest representation possible: no
  // 0.099999999999 instead of 0.1.
  // The last digit will be closest to the actual v. That is, even if several
  // digits might correctly yield 'v' when read again, the closest will be
  // computed.
  static bool grisu3(double v,
                     char* buffer, int* length, int* decimal_exponent);

 private:
  // Rounds the buffer according to the rest.
  // If there is too much imprecision to round then false is returned.
  // Similarily false is returned when the buffer is not within Delta.
  static bool RoundWeed(char* buffer, int len, uint64_t wp_W, uint64_t Delta,
                        uint64_t rest, uint64_t ten_kappa, uint64_t ulp);
  // Dispatches to the a specialized digit-generation routine. The chosen
  // routine depends on w.e (which in turn depends on alpha and gamma).
  // Currently there is only one digit-generation routine, but it would be easy
  // to add others.
  static bool DigitGen(DiyFp low, DiyFp w, DiyFp high,
                       char* buffer, int* len, int* kappa);
  // Generates w's digits. The result is the shortest in the interval low-high.
  // All DiyFp are assumed to be imprecise and this function takes this
  // imprecision into account. If the function cannot compute the best
  // representation (due to the imprecision) then false is returned.
  static bool DigitGen_m60_m32(DiyFp low, DiyFp w, DiyFp high,
                               char* buffer, int* length, int* kappa);
};


template<int alpha, int gamma>
bool Grisu3<alpha, gamma>::grisu3(double v,
                                  char* buffer,
                                  int* length,
                                  int* decimal_exponent) {
  DiyFp w = Double(v).AsNormalizedDiyFp();
  // boundary_minus and boundary_plus are the boundaries between v and its
  // neighbors. Any number strictly between boundary_minus and boundary_plus
  // will round to v when read as double.
  // Grisu3 will never output representations that lie exactly on a boundary.
  DiyFp boundary_minus, boundary_plus;
  Double(v).NormalizedBoundaries(&boundary_minus, &boundary_plus);
  ASSERT(boundary_plus.e() == w.e());
  DiyFp ten_mk;  // Cached power of ten: 10^-k
  int mk;        // -k
  GetCachedPower(w.e() + DiyFp::kSignificandSize, alpha, gamma, &mk, &ten_mk);
  ASSERT(alpha <= w.e() + ten_mk.e() + DiyFp::kSignificandSize &&
         gamma >= w.e() + ten_mk.e() + DiyFp::kSignificandSize);
  // Note that ten_mk is only an approximation of 10^-k. A DiyFp only contains a
  // 64 bit significand and ten_mk is thus only precise up to 64 bits.

  // The DiyFp::Times procedure rounds its result, and ten_mk is approximated
  // too. The variable scaled_w (as well as scaled_boundary_minus/plus) are now
  // off by a small amount.
  // In fact: scaled_w - w*10^k < 1ulp (unit in the last place) of scaled_w.
  // In other words: let f = scaled_w.f() and e = scaled_w.e(), then
  //           (f-1) * 2^e < w*10^k < (f+1) * 2^e
  DiyFp scaled_w = DiyFp::Times(w, ten_mk);
  ASSERT(scaled_w.e() ==
         boundary_plus.e() + ten_mk.e() + DiyFp::kSignificandSize);
  // In theory it would be possible to avoid some recomputations by computing
  // the difference between w and boundary_minus/plus (a power of 2) and to
  // compute scaled_boundary_minus/plus by subtracting/adding from
  // scaled_w. However the code becomes much less readable and the speed
  // enhancements are not terriffic.
  DiyFp scaled_boundary_minus = DiyFp::Times(boundary_minus, ten_mk);
  DiyFp scaled_boundary_plus  = DiyFp::Times(boundary_plus,  ten_mk);

  // DigitGen will generate the digits of scaled_w. Therefore we have
  // v == (double) (scaled_w * 10^-mk).
  // Set decimal_exponent == -mk and pass it to DigitGen. If scaled_w is not an
  // integer than it will be updated. For instance if scaled_w == 1.23 then
  // the buffer will be filled with "123" und the decimal_exponent will be
  // decreased by 2.
  int kappa;
  bool result = DigitGen(scaled_boundary_minus, scaled_w, scaled_boundary_plus,
                         buffer, length, &kappa);
  *decimal_exponent = -mk + kappa;
  return result;
}

// Generates the digits of input number w.
// w is a floating-point number (DiyFp), consisting of a significand and an
// exponent. Its exponent is bounded by alpha and gamma. Typically alpha >= -63
// and gamma <= 3.
// Returns false if it fails, in which case the generated digits in the buffer
// should not be used.
// Preconditions:
//  * low, w and high are correct up to 1 ulp (unit in the last place). That
//    is, their error must be less that a unit of their last digits.
//  * low.e() == w.e() == high.e()
//  * low < w < high, and taking into account their error: low~ <= high~
//  * alpha <= w.e() <= gamma
// Postconditions: returns false if procedure fails.
//   otherwise:
//     * buffer is not null-terminated, but len contains the number of digits.
//     * buffer contains the shortest possible decimal digit-sequence
//       such that LOW < buffer * 10^kappa < HIGH, where LOW and HIGH are the
//       correct values of low and high (without their error).
//     * if more than one decimal representation gives the minimal number of
//       decimal digits then the one closest to W (where W is the correct value
//       of w) is chosen.
// Remark: this procedure takes into account the imprecision of its input
//   numbers. If the precision is not enough to guarantee all the postconditions
//   then false is returned. This usually happens rarely (~0.5%).
template<int alpha, int gamma>
bool Grisu3<alpha, gamma>::DigitGen(DiyFp low,
                                    DiyFp w,
                                    DiyFp high,
                                    char* buffer,
                                    int* len,
                                    int* kappa) {
  ASSERT(low.e() == w.e() && w.e() == high.e());
  ASSERT(low.f() + 1 <= high.f() - 1);
  ASSERT(alpha <= w.e() && w.e() <= gamma);
  // The following tests use alpha and gamma to avoid unnecessary dynamic tests.
  if ((alpha >= -60 && gamma <= -32) ||  // -60 <= w.e() <= -32
      (alpha <= -32 && gamma >= -60 &&   // Alpha/gamma overlaps -60/-32 region.
       -60 <= w.e() && w.e() <= -32)) {
    return DigitGen_m60_m32(low, w, high, buffer, len, kappa);
  } else {
    // A simple adaption of the special case -60/-32 would allow greater ranges
    // of alpha/gamma and thus reduce the number of precomputed cached powers of
    // ten.
    UNIMPLEMENTED();
    return false;
  }
}

static const uint32_t kTen4 = 10000;
static const uint32_t kTen5 = 100000;
static const uint32_t kTen6 = 1000000;
static const uint32_t kTen7 = 10000000;
static const uint32_t kTen8 = 100000000;
static const uint32_t kTen9 = 1000000000;

// Returns the biggest power of ten that is <= than the given number. We
// furthermore receive the maximum number of bits 'number' has.
// If number_bits == 0 then 0^-1 is returned
// The number of bits must be <= 32.
static void BiggestPowerTen(uint32_t number,
                            int number_bits,
                            uint32_t* power,
                            int* exponent) {
  switch (number_bits) {
    case 32:
    case 31:
    case 30:
      if (kTen9 <= number) {
        *power = kTen9;
        *exponent = 9;
        break;
      }  // else fallthrough
    case 29:
    case 28:
    case 27:
      if (kTen8 <= number) {
        *power = kTen8;
        *exponent = 8;
        break;
      }  // else fallthrough
    case 26:
    case 25:
    case 24:
      if (kTen7 <= number) {
        *power = kTen7;
        *exponent = 7;
        break;
      }  // else fallthrough
    case 23:
    case 22:
    case 21:
    case 20:
      if (kTen6 <= number) {
        *power = kTen6;
        *exponent = 6;
        break;
      }  // else fallthrough
    case 19:
    case 18:
    case 17:
      if (kTen5 <= number) {
        *power = kTen5;
        *exponent = 5;
        break;
      }  // else fallthrough
    case 16:
    case 15:
    case 14:
      if (kTen4 <= number) {
        *power = kTen4;
        *exponent = 4;
        break;
      }  // else fallthrough
    case 13:
    case 12:
    case 11:
    case 10:
      if (1000 <= number) {
        *power = 1000;
        *exponent = 3;
        break;
      }  // else fallthrough
    case 9:
    case 8:
    case 7:
      if (100 <= number) {
        *power = 100;
        *exponent = 2;
        break;
      }  // else fallthrough
    case 6:
    case 5:
    case 4:
      if (10 <= number) {
        *power = 10;
        *exponent = 1;
        break;
      }  // else fallthrough
    case 3:
    case 2:
    case 1:
      if (1 <= number) {
        *power = 1;
        *exponent = 0;
        break;
      }  // else fallthrough
    case 0:
      *power = 0;
      *exponent = -1;
      break;
    default:
      // Following assignments are here to silence compiler warnings.
      *power = 0;
      *exponent = 0;
      UNREACHABLE();
  }
}


// Same comments as for DigitGen but with additional precondition:
//    -60 <= w.e() <= -32
//
// Say, for the sake of example, that
//   w.e() == -48, and w.f() == 0x1234567890abcdef
// w's value can be computed by w.f() * 2^w.e()
// We can obtain w's integral digits by simply shifting w.f() by -w.e().
//  -> w's integral part is 0x1234
//  w's fractional part is therefore 0x567890abcdef.
// Printing w's integral part is easy (simply print 0x1234 in decimal).
// In order to print its fraction we repeatedly multiply the fraction by 10 and
// get each digit. Example the first digit after the comma would be computed by
//   (0x567890abcdef * 10) >> 48. -> 3
// The whole thing becomes slightly more complicated because we want to stop
// once we have enough digits. That is, once the digits inside the buffer
// represent 'w' we can stop. Everything inside the interval low - high
// represents w. However we have to pay attention to low, high and w's
// imprecision.
template<int alpha, int gamma>
bool Grisu3<alpha, gamma>::DigitGen_m60_m32(DiyFp low,
                                            DiyFp w,
                                            DiyFp high,
                                            char* buffer,
                                            int* length,
                                            int* kappa) {
  // low, w and high are imprecise, but by less than one ulp (unit in the last
  // place).
  // If we remove (resp. add) 1 ulp from low (resp. high) we are certain that
  // the new numbers are outside of the interval we want the final
  // representation to lie in.
  // Inversely adding (resp. removing) 1 ulp from low (resp. high) would yield
  // numbers that are certain to lie in the interval. We will use this fact
  // later on.
  // We will now start by generating the digits within the uncertain
  // interval. Later we will weed out representations that lie outside the safe
  // interval and thus _might_ lie outside the correct interval.
  uint64_t unit = 1;
  DiyFp too_low = DiyFp(low.f() - unit, low.e());
  DiyFp too_high = DiyFp(high.f() + unit, high.e());
  // too_low and too_high are guaranteed to lie outside the interval we want the
  // generated number in.
  DiyFp unsafe_interval = DiyFp::Minus(too_high, too_low);
  // We now cut the input number into two parts: the integral digits and the
  // fractionals. We will not write any decimal separator though, but adapt
  // kappa instead.
  // Reminder: we are currently computing the digits (stored inside the buffer)
  // such that:   too_low < buffer * 10^kappa < too_high
  // We use too_high for the digit_generation and stop as soon as possible.
  // If we stop early we effectively round down.
  DiyFp one = DiyFp(static_cast<uint64_t>(1) << -w.e(), w.e());
  // Division by one is a shift.
  uint32_t integrals = static_cast<uint32_t>(too_high.f() >> -one.e());
  // Modulo by one is an and.
  uint64_t fractionals = too_high.f() & (one.f() - 1);
  uint32_t divider;
  int divider_exponent;
  BiggestPowerTen(integrals, DiyFp::kSignificandSize - (-one.e()),
                  &divider, &divider_exponent);
  *kappa = divider_exponent + 1;
  *length = 0;
  // Loop invariant: buffer = too_high / 10^kappa  (integer division)
  // The invariant holds for the first iteration: kappa has been initialized
  // with the divider exponent + 1. And the divider is the biggest power of ten
  // that is smaller than integrals.
  while (*kappa > 0) {
    int digit = integrals / divider;
    buffer[*length] = '0' + digit;
    (*length)++;
    integrals %= divider;
    (*kappa)--;
    // Note that kappa now equals the exponent of the divider and that the
    // invariant thus holds again.
    uint64_t rest =
        (static_cast<uint64_t>(integrals) << -one.e()) + fractionals;
    // Invariant: too_high = buffer * 10^kappa + DiyFp(rest, one.e())
    // Reminder: unsafe_interval.e() == one.e()
    if (rest < unsafe_interval.f()) {
      // Rounding down (by not emitting the remaining digits) yields a number
      // that lies within the unsafe interval.
      return RoundWeed(buffer, *length, DiyFp::Minus(too_high, w).f(),
                       unsafe_interval.f(), rest,
                       static_cast<uint64_t>(divider) << -one.e(), unit);
    }
    divider /= 10;
  }

  // The integrals have been generated. We are at the point of the decimal
  // separator. In the following loop we simply multiply the remaining digits by
  // 10 and divide by one. We just need to pay attention to multiply associated
  // data (like the interval or 'unit'), too.
  // Instead of multiplying by 10 we multiply by 5 (cheaper operation) and
  // increase its (imaginary) exponent. At the same time we decrease the
  // divider's (one's) exponent and shift its significand.
  // Basically, if fractionals was a DiyFp (with fractionals.e == one.e):
  //      fractionals.f *= 10;
  //      fractionals.f >>= 1; fractionals.e++; // value remains unchanged.
  //      one.f >>= 1; one.e++;                 // value remains unchanged.
  //      and we have again fractionals.e == one.e which allows us to divide
  //           fractionals.f() by one.f()
  // We simply combine the *= 10 and the >>= 1.
  while (true) {
    fractionals *= 5;
    unit *= 5;
    unsafe_interval.set_f(unsafe_interval.f() * 5);
    unsafe_interval.set_e(unsafe_interval.e() + 1);  // Will be optimized out.
    one.set_f(one.f() >> 1);
    one.set_e(one.e() + 1);
    // Integer division by one.
    int digit = static_cast<int>(fractionals >> -one.e());
    buffer[*length] = '0' + digit;
    (*length)++;
    fractionals &= one.f() - 1;  // Modulo by one.
    (*kappa)--;
    if (fractionals < unsafe_interval.f()) {
      return RoundWeed(buffer, *length, DiyFp::Minus(too_high, w).f() * unit,
                       unsafe_interval.f(), fractionals, one.f(), unit);
    }
  }
}


// Rounds the given generated digits in the buffer and weeds out generated
// digits that are not in the safe interval, or where we cannot find a rounded
// representation.
// Input: * buffer containing the digits of too_high / 10^kappa
//        * the buffer's length
//        * distance_too_high_w == (too_high - w).f() * unit
//        * unsafe_interval == (too_high - too_low).f() * unit
//        * rest = (too_high - buffer * 10^kappa).f() * unit
//        * ten_kappa = 10^kappa * unit
//        * unit = the common multiplier
// Output: returns true on success.
//    Modifies the generated digits in the buffer to approach (round towards) w.
template<int alpha, int gamma>
bool Grisu3<alpha, gamma>::RoundWeed(char* buffer,
                                     int length,
                                     uint64_t distance_too_high_w,
                                     uint64_t unsafe_interval,
                                     uint64_t rest,
                                     uint64_t ten_kappa,
                                     uint64_t unit) {
  uint64_t small_distance = distance_too_high_w - unit;
  uint64_t big_distance = distance_too_high_w + unit;
  // Let w- = too_high - big_distance, and
  //     w+ = too_high - small_distance.
  // Note: w- < w < w+
  //
  // The real w (* unit) must lie somewhere inside the interval
  // ]w-; w+[ (often written as "(w-; w+)")

  // Basically the buffer currently contains a number in the unsafe interval
  // ]too_low; too_high[ with too_low < w < too_high
  //
  // By generating the digits of too_high we got the biggest last digit.
  // In the case that w+ < buffer < too_high we try to decrement the buffer.
  // This way the buffer approaches (rounds towards) w.
  // There are 3 conditions that stop the decrementation process:
  //   1) the buffer is already below w+
  //   2) decrementing the buffer would make it leave the unsafe interval
  //   3) decrementing the buffer would yield a number below w+ and farther away
  //      than the current number. In other words:
  //                       (buffer{-1} < w+) && w+ - buffer{-1} > buffer - w+
  // Instead of using the buffer directly we use its distance to too_high.
  // Conceptually rest ~= too_high - buffer
  while (rest < small_distance &&  // Negated condition 1
         unsafe_interval - rest >= ten_kappa &&  // Negated condition 2
         (rest + ten_kappa < small_distance ||  // buffer{-1} > w+
          small_distance - rest >= rest + ten_kappa - small_distance)) {
    buffer[length - 1]--;
    rest += ten_kappa;
  }

  // We have approached w+ as much as possible. We now test if approaching w-
  // would require changing the buffer. If yes, then we have two possible
  // representations close to w, but we cannot decide which one is closer.
  if (rest < big_distance &&
      unsafe_interval - rest >= ten_kappa &&
      (rest + ten_kappa < big_distance ||
       big_distance - rest > rest + ten_kappa - big_distance)) {
    return false;
  }

  // Weeding test.
  //   The safe interval is [too_low + 2 ulp; too_high - 2 ulp]
  //   Since too_low = too_high - unsafe_interval this is equivalent too
  //      [too_high - unsafe_interval + 4 ulp; too_high - 2 ulp]
  //   Conceptually we have: rest ~= too_high - buffer
  return (2 * unit <= rest) && (rest <= unsafe_interval - 4 * unit);
}


bool grisu3(double v, char* buffer, int* sign, int* length, int* point) {
  ASSERT(v != 0);
  ASSERT(!Double(v).IsSpecial());

  if (v < 0) {
    v = -v;
    *sign = 1;
  } else {
    *sign = 0;
  }
  int decimal_exponent;
  bool result = Grisu3<-60, -32>::grisu3(v, buffer, length, &decimal_exponent);
  *point = *length + decimal_exponent;
  buffer[*length] = '\0';
  return result;
}

} }  // namespace v8::internal
