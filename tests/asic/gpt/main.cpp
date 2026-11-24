#include "VGptAsic.h"
#include "verilated.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static VGptAsic dut;
static uint64_t ticks;

static void step() {
  dut.clk = 0;
  dut.eval();
  dut.clk = 1;
  dut.eval();
  ++ticks;
}

static void load(unsigned bank, const std::vector<int32_t> &values) {
  dut.bank = bank;
  dut.write = 1;
  for (unsigned i = 0; i < values.size(); ++i) {
    dut.address = i;
    dut.wdata = values[i];
    step();
  }
  dut.write = 0;
}

static uint64_t launch(unsigned phase, bool fused) {
  const uint64_t begin = ticks;
  dut.phase = phase;
  dut.fused = fused;
  dut.start = 1;
  step();
  dut.start = 0;
  while (!dut.done && ticks - begin < 50000)
    step();
  return dut.done ? ticks - begin : 0;
}

static std::vector<int32_t> read(std::istream &input, unsigned size) {
  std::vector<int32_t> values(size);
  for (auto &value : values)
    input >> value;
  return values;
}

int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);
  if (argc != 2) {
    std::cerr << "usage: VGptAsic fixture.txt\n";
    return 2;
  }
  std::ifstream input(argv[1]);
  unsigned cases;
  input >> cases;
  if (!input)
    return 2;
  dut.reset = 1;
  step();
  dut.reset = 0;
  unsigned failures = 0;
  for (unsigned test = 0; test < cases; ++test) {
    std::string name;
    unsigned unit, size, row, operation, divisor, phases;
    input >> name >> unit >> size >> row >> operation >> divisor >> phases;
    auto source = read(input, size);
    auto extra = read(input, unit == 0 || operation == 2 ? size : 0);
    auto beta = read(input, unit == 0 ? size : 0);
    auto expected = read(input, size);
    if (!input)
      return 2;
    dut.unit = unit;
    dut.row = row;
    dut.operation = operation;
    dut.divisor = divisor;
    dut.count = size;
    load(0, source);
    load(1, extra);
    load(2, beta);
    uint64_t cycles = 0;
    bool completed = true;
    for (unsigned phase = 0; phase < phases; ++phase) {
      uint64_t duration = launch(phase, phases == 1);
      completed = completed && duration != 0;
      cycles += duration;
    }
    unsigned mismatches = 0;
    for (unsigned i = 0; i < size; ++i) {
      dut.address = i;
      dut.eval();
      int32_t actual = dut.rdata;
      if (actual != expected[i]) {
        if (mismatches < 3)
          std::cerr << name << " output " << i << " expected " << expected[i] << " actual " << actual << '\n';
        ++mismatches;
      }
      step();
    }
    bool passed = completed && mismatches == 0;
    failures += !passed;
    std::cout << "{\"name\":\"" << name << "\",\"cycles\":" << cycles << ",\"launches\":" << phases << ",\"input_port_bytes\":" << (source.size() + extra.size() + beta.size()) * 4
              << ",\"output_port_bytes\":" << size * 4 << ",\"checked\":" << size << ",\"mismatches\":" << mismatches << ",\"passed\":" << (passed ? "true" : "false") << "}\n";
  }
  dut.final();
  return failures ? 1 : 0;
}
