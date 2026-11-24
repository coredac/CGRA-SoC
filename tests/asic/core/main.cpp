#include "VAsicCore.h"
#include "verilated.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

class Bench {
public:
  VAsicCore dut;
  uint64_t cycles = 0;

  void tick() {
    dut.clk = 0;
    dut.eval();
    dut.clk = 1;
    dut.eval();
    ++cycles;
  }

  void put(unsigned address, int32_t value) {
    dut.address = address;
    dut.wdata = value;
    dut.write = 1;
    tick();
    dut.write = 0;
  }

  int32_t get(unsigned address) {
    dut.address = address;
    dut.read = 1;
    tick();
    if (!dut.rvalid)
      throw std::runtime_error("Missing synchronous read response");
    int32_t result = dut.rdata;
    dut.read = 0;
    return result;
  }

  uint64_t run() {
    const auto begin = cycles;
    dut.start = 1;
    tick();
    dut.start = 0;
    // Descriptors must be captured on the accepted start, not read throughout execution.
    dut.unit = 7;
    dut.operation = 3;
    dut.src0 = 32767;
    dut.dst = 32767;
    while (!dut.done && cycles - begin < 500000)
      tick();
    if (!dut.done || dut.busy)
      throw std::runtime_error("Core completion timeout");
    return cycles - begin;
  }
};

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  Verilated::commandArgs(argc, argv);
  std::ifstream input(argv[1]);
  const unsigned last = std::stoul(argv[2]) - 1;
  Bench bench;
  bench.dut.reset = 1;
  bench.tick();
  bench.dut.reset = 0;
  bench.tick();
  try {
    bench.put(last, 123);
    bench.dut.read = 1;
    bench.dut.write = 1;
    bench.dut.wdata = 456;
    bench.tick();
    if (!bench.dut.rvalid || bench.dut.rdata != 123)
      throw std::runtime_error("Memory is not read-first");
    bench.dut.write = 0;
    bench.dut.read = 0;
    if (bench.get(last) != 456)
      throw std::runtime_error("Memory write was not retained");
    unsigned cases = 0;
    input >> cases;
    for (unsigned test = 0; test < cases; ++test) {
      std::string name;
      unsigned unit, op, fused, phase, count, rows, edges, divisor, src0, src1, src2, src3, dst, loads, checks;
      input >> name >> unit >> op >> fused >> phase >> count >> rows >> edges >> divisor >> src0 >> src1 >> src2 >> src3 >> dst >> loads >> checks;
      for (unsigned load = 0; load < loads; ++load) {
        unsigned address, size;
        input >> address >> size;
        for (unsigned i = 0; i < size; ++i) {
          int64_t value;
          input >> value;
          bench.put(address + i, static_cast<int32_t>(value));
        }
      }
      std::vector<int32_t> expected(checks);
      for (auto &value : expected)
        input >> value;
      if (!input)
        throw std::runtime_error("Malformed fixture");
      bench.dut.unit = unit;
      bench.dut.operation = op;
      bench.dut.fused = fused;
      bench.dut.phase = phase;
      bench.dut.count = count;
      bench.dut.rows = rows;
      bench.dut.edges = edges;
      bench.dut.divisor = divisor;
      bench.dut.src0 = src0;
      bench.dut.src1 = src1;
      bench.dut.src2 = src2;
      bench.dut.src3 = src3;
      bench.dut.dst = dst;
      const auto cycles = bench.run();
      unsigned mismatches = 0;
      for (unsigned i = 0; i < checks; ++i) {
        const int32_t actual = bench.get(dst + i);
        if (actual != expected[i]) {
          if (mismatches < 4)
            std::cerr << name << " index=" << i << " actual=" << actual << " expected=" << expected[i] << '\n';
          ++mismatches;
        }
      }
      std::cout << "{\"name\":\"" << name << "\",\"cycles\":" << cycles << ",\"checked\":" << checks << ",\"mismatches\":" << mismatches << ",\"passed\":" << (mismatches ? "false" : "true") << "}\n";
      if (mismatches)
        return 1;
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
