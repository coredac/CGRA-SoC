#include "VFixedBench.h"
#include "verilated.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

class Bench {
public:
  VFixedBench dut;
  uint64_t cycles = 0;

  void tick() {
    dut.clk = 0;
    dut.eval();
    dut.clk = 1;
    dut.eval();
    ++cycles;
  }

  void put(unsigned core, unsigned address, int32_t value) {
    dut.select = core;
    dut.address = address;
    dut.wdata = static_cast<uint32_t>(value);
    dut.write = 1;
    tick();
    dut.write = 0;
  }

  void drain(unsigned core) {
    unsigned waiting = 0;
    while ((dut.write_pending & (1u << core)) && waiting++ < 16)
      tick();
    if (dut.write_pending & (1u << core))
      throw std::runtime_error("Host writes did not drain");
  }

  int32_t response() {
    unsigned waiting = 0;
    while (!dut.rvalid && waiting++ < 16)
      tick();
    if (!dut.rvalid)
      throw std::runtime_error("Missing pipelined host response");
    return static_cast<int32_t>(dut.rdata);
  }

  int32_t get(unsigned core, unsigned address) {
    drain(core);
    dut.select = core;
    dut.address = address;
    dut.read = 1;
    tick();
    dut.read = 0;
    return response();
  }

  uint64_t run(unsigned core, unsigned count, unsigned first, unsigned second, unsigned destination) {
    drain(core);
    dut.count = count;
    dut.src0 = first;
    dut.src1 = second;
    dut.dst = destination;
    // A runtime fused-operation request must not change either fixed circuit.
    dut.operation = 2;
    const uint64_t begin = cycles;
    dut.start = 1u << core;
    tick();
    dut.start = 0;
    dut.src0 = 16383;
    dut.src1 = 16383;
    dut.dst = 16383;
    dut.count = 1;
    dut.operation = 3;
    while (!(dut.done & (1u << core)) && cycles - begin < 500000)
      tick();
    if (!(dut.done & (1u << core)) || (dut.busy & (1u << core)) || (dut.write_pending & (1u << core)))
      throw std::runtime_error("Fixed core completion timeout");
    return cycles - begin;
  }
};

static int32_t sample(unsigned index, unsigned input) {
  static const uint32_t boundary[] = {0, 1, 0xffffffffu, 0x7fffffffu, 0x80000000u, 0x40000000u, 0xc0000000u, 9, 0xfffffff7u};
  if (index < sizeof(boundary) / sizeof(boundary[0]))
    return static_cast<int32_t>(boundary[(index + input) % (sizeof(boundary) / sizeof(boundary[0]))]);
  return static_cast<int32_t>(0x9e3779b9u * (index + 1u) + 0x85ebca6bu * input);
}

static void check(int32_t actual, int32_t expected, const std::string &where) {
  if (actual != expected)
    throw std::runtime_error(where + " actual=" + std::to_string(actual) + " expected=" + std::to_string(expected));
}

static void chain(Bench &bench, unsigned count, unsigned input, bool inplace, bool tail = false) {
  const unsigned first = tail ? 16382 : 0;
  const unsigned second = tail ? 16381 : 4096;
  const unsigned added = tail ? 16383 : (inplace ? first : 8192);
  const unsigned relu_input = tail ? 16383 : 0;
  const unsigned output = inplace || tail ? relu_input : 8192;
  std::vector<int32_t> sums(count);
  for (unsigned index = 0; index < count; ++index) {
    const int32_t left = sample(index, input);
    const int32_t right = sample(index + 1, input + 1);
    sums[index] = static_cast<int32_t>(static_cast<uint32_t>(left) + static_cast<uint32_t>(right));
    bench.put(0, first + index, left);
    bench.put(0, second + index, right);
  }
  bench.put(1, relu_input, -1234567);
  const uint64_t add_cycles = bench.run(0, count, first, second, added);
  check(bench.get(1, relu_input), -1234567, "Independent ReLU memory");
  const uint64_t copy_begin = bench.cycles;
  for (unsigned index = 0; index < count; ++index) {
    const int32_t value = bench.get(0, added + index);
    check(value, sums[index], "Add word " + std::to_string(index));
    bench.put(1, relu_input + index, value);
  }
  bench.drain(1);
  const uint64_t copy_cycles = bench.cycles - copy_begin;
  const uint64_t relu_cycles = bench.run(1, count, relu_input, 0, output);
  for (unsigned index = 0; index < count; ++index)
    check(bench.get(1, output + index), sums[index] < 0 ? 0 : sums[index], "ReLU word " + std::to_string(index));
  std::cout << "{\"count\":" << count << ",\"input\":" << input << ",\"inplace\":" << (inplace ? "true" : "false") << ",\"last_word\":" << (tail ? "true" : "false") << ",\"add_cycles\":" << add_cycles
            << ",\"copy_cycles\":" << copy_cycles << ",\"relu_cycles\":" << relu_cycles << ",\"copy_read_bytes\":" << count * 4 << ",\"copy_write_bytes\":" << count * 4 << ",\"checked\":" << count * 2
            << ",\"passed\":true}\n";
}

int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);
  Bench bench;
  bench.dut.reset = 1;
  bench.tick();
  bench.dut.reset = 0;
  bench.tick();
  try {
    bench.put(0, 16383, 123);
    bench.drain(0);
    bench.dut.read = 1;
    bench.dut.write = 1;
    bench.dut.wdata = 456;
    bench.tick();
    bench.dut.read = 0;
    bench.dut.write = 0;
    check(bench.response(), 123, "Host memory collision is read-first");
    check(bench.get(0, 16383), 456, "Host memory collision write");
    for (unsigned count : {1u, 3u, 7u, 8u, 31u, 128u, 1024u})
      for (unsigned input = 0; input < 2; ++input)
        for (bool inplace : {false, true})
          chain(bench, count, input, inplace);
    chain(bench, 1, 0, true, true);
    chain(bench, 1, 1, true, true);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
