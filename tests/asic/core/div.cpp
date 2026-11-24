#include "VGptDivider.h"
#include "verilated.h"

#include <array>
#include <cstdint>
#include <iostream>

static VGptDivider dut;

static void tick() {
  dut.clk = 0;
  dut.eval();
  dut.clk = 1;
  dut.eval();
}

static bool check(uint32_t numerator, uint32_t denominator, bool signed_mode) {
  const uint32_t expected = signed_mode ? static_cast<uint32_t>(static_cast<int64_t>(static_cast<int32_t>(numerator)) / static_cast<int32_t>(denominator)) : numerator / denominator;
  dut.dividend = numerator;
  dut.divisor = denominator;
  dut.signed_mode = signed_mode;
  dut.start = 1;
  tick();
  dut.start = 0;
  dut.dividend = 0;
  dut.divisor = 1;
  dut.signed_mode = !signed_mode;
  for (unsigned step = 0; step < 32; ++step) {
    if (!dut.busy || dut.done)
      return false;
    tick();
  }
  if (dut.busy || !dut.done || dut.quotient != expected) {
    std::cerr << "divide " << numerator << " / " << denominator << " signed=" << signed_mode << " actual=" << dut.quotient << " expected=" << expected << '\n';
    return false;
  }
  return true;
}

int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);
  dut.reset = 1;
  tick();
  dut.reset = 0;
  unsigned checked = 0;
  const std::array<uint32_t, 7> values = {0, 1, 2, 3, 0x7fffffff, 0x80000000, 0xffffffff};
  uint32_t random = 0x159a47fe;
  for (unsigned mode = 0; mode < 2; ++mode) {
    for (uint32_t numerator : values) {
      for (uint32_t denominator : values) {
        if (!denominator)
          continue;
        if (!check(numerator, denominator, mode))
          return 1;
        ++checked;
      }
    }
    for (unsigned sample = 0; sample < 128; ++sample) {
      random ^= random << 13;
      random ^= random >> 17;
      random ^= random << 5;
      const uint32_t numerator = random;
      random = random * 1664525 + 1013904223;
      if (!check(numerator, random | 1, mode))
        return 1;
      ++checked;
    }
  }
  dut.start = 1;
  dut.dividend = 0xffffffff;
  dut.divisor = 3;
  tick();
  dut.start = 0;
  tick();
  dut.reset = 1;
  tick();
  dut.reset = 0;
  tick();
  if (dut.busy || dut.done || !check(17, 3, false))
    return 1;
  ++checked;
  std::cout << "{\"passed\":true,\"cases\":" << checked << ",\"cycles_per_division\":33}\n";
  return 0;
}
