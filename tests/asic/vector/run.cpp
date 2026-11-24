#include "VBench.h"
#include "verilated.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

struct Stats {
  uint64_t cycles = 0, launches = 0, input = 0, output = 0, preload = 0;
  uint64_t internal = 0, link = 0, link_stalls = 0;
  void add(const Stats &other) {
    cycles += other.cycles;
    launches += other.launches;
    input += other.input;
    output += other.output;
    preload += other.preload;
    internal += other.internal;
    link += other.link;
    link_stalls += other.link_stalls;
  }
};

class Test {
public:
  VBench dut;
  uint64_t cycles = 0;
  bool failed = false;

  void tick() {
    dut.clk = 0;
    dut.eval();
    dut.clk = 1;
    dut.eval();
    ++cycles;
    dut.clk = 0;
    dut.eval();
  }

  void reset() {
    dut.reset = 1;
    tick();
    tick();
    dut.reset = 0;
    tick();
  }

  void load(unsigned bank, const std::vector<int32_t> &values, Stats &stats) {
    dut.g_bank = bank;
    dut.g_load = 1;
    for (size_t index = 0; index < values.size(); ++index) {
      dut.g_address = index;
      dut.g_data = values[index];
      tick();
      stats.preload += 4;
    }
    dut.g_load = 0;
    dut.eval();
  }

  Stats vector(unsigned op, const std::vector<int32_t> &left, const std::vector<int32_t> &right, std::vector<int32_t> &output, bool stalls) {
    Stats stats;
    output.clear();
    dut.v_operation = op;
    dut.v_length = left.size();
    dut.v_start = 1;
    dut.v_valid = 0;
    dut.v_out_ready = 0;
    const auto begin = cycles;
    tick();
    ++stats.launches;
    ++stats.internal;
    dut.v_start = 0;
    size_t index = 0;
    bool held = false;
    uint32_t previous = 0;
    for (unsigned step = 0; step < 100000; ++step) {
      dut.v_valid = index < left.size();
      dut.v_left = index < left.size() ? left[index] : 0;
      dut.v_right = index < right.size() ? right[index] : 0;
      dut.v_out_ready = !stalls || step % 7 != 3;
      dut.eval();
      if (held && (!dut.v_out_valid || previous != dut.v_output)) {
        std::cerr << "Vector output changed under backpressure\n";
        failed = true;
        break;
      }
      held = dut.v_out_valid && !dut.v_out_ready;
      previous = dut.v_output;
      if (dut.v_valid && dut.v_ready) {
        ++index;
        stats.input += op == 0 ? 4 : 8;
      }
      if (dut.v_out_valid && dut.v_out_ready) {
        output.push_back(static_cast<int32_t>(dut.v_output));
        stats.output += 4;
      }
      tick();
      if (dut.v_done)
        break;
    }
    if (!dut.v_done || index != left.size() || output.size() != left.size()) {
      std::cerr << "Vector completion/count mismatch\n";
      failed = true;
    }
    dut.v_valid = 0;
    stats.cycles = cycles - begin;
    return stats;
  }

  Stats gcn(unsigned nodes, bool fused, std::vector<int32_t> &output, bool stalls) {
    Stats stats;
    output.clear();
    dut.g_nodes = nodes;
    dut.g_fused = fused;
    dut.g_ready = 0;
    dut.g_start = 1;
    const auto begin = cycles;
    tick();
    ++stats.launches;
    ++stats.internal;
    dut.g_start = 0;
    bool held = false;
    uint32_t previous = 0;
    for (unsigned step = 0; step < 1000000; ++step) {
      dut.g_ready = !stalls || step % 7 != 3;
      dut.eval();
      if (held && (!dut.g_valid || previous != dut.g_output)) {
        std::cerr << "GCN output changed under backpressure\n";
        failed = true;
        break;
      }
      held = dut.g_valid && !dut.g_ready;
      previous = dut.g_output;
      if (dut.g_valid && dut.g_ready) {
        output.push_back(static_cast<int32_t>(dut.g_output));
        stats.output += 4;
      }
      tick();
      if (dut.g_done)
        break;
    }
    if (!dut.g_done || output.size() != nodes * 128) {
      std::cerr << "GCN completion/count mismatch\n";
      failed = true;
    }
    stats.cycles = cycles - begin;
    return stats;
  }

  void chain_load(unsigned bank, const std::vector<int32_t> &values, Stats &stats) {
    dut.c_bank = bank;
    dut.c_load = 1;
    for (size_t index = 0; index < values.size(); ++index) {
      dut.c_address = index;
      dut.c_data = values[index];
      tick();
      stats.preload += 4;
    }
    dut.c_load = 0;
    dut.eval();
  }

  Stats chain(unsigned nodes, std::vector<int32_t> &output, bool stalls) {
    Stats stats;
    output.clear();
    dut.c_nodes = nodes;
    dut.c_ready = 0;
    dut.c_start = 1;
    dut.eval();
    stats.launches += dut.c_accepted;
    stats.internal += dut.c_internal_starts;
    const auto begin = cycles;
    tick();
    dut.c_start = 0;
    bool held = false;
    uint32_t previous = 0;
    unsigned completed = 0;
    for (unsigned step = 0; step < 1000000; ++step) {
      dut.c_ready = !stalls || step % 97 >= 20;
      dut.eval();
      if (held && (!dut.c_valid || previous != dut.c_output)) {
        std::cerr << "Chained output changed under backpressure\n";
        failed = true;
        break;
      }
      held = dut.c_valid && !dut.c_ready;
      previous = dut.c_output;
      if (dut.c_link_valid && dut.c_link_ready)
        stats.link += 4;
      if (dut.c_link_valid && !dut.c_link_ready)
        ++stats.link_stalls;
      if (dut.c_valid && dut.c_ready) {
        output.push_back(static_cast<int32_t>(dut.c_output));
        stats.output += 4;
      }
      tick();
      completed += dut.c_aggregate_done;
      if (dut.c_done)
        break;
    }
    if (!dut.c_done || output.size() != nodes * 128 || completed != 1 || stats.launches != 1 || stats.internal != 2 || stats.link != nodes * 128 * 4 || (stalls && stats.link_stalls == 0)) {
      std::cerr << "Chained completion/count mismatch\n";
      failed = true;
    }
    stats.cycles = cycles - begin;
    return stats;
  }

  void compare(const std::vector<int32_t> &output, const std::vector<int32_t> &expected) {
    if (output == expected)
      return;
    failed = true;
    for (size_t index = 0; index < output.size() && index < expected.size(); ++index) {
      if (output[index] == expected[index])
        continue;
      std::cerr << "Mismatch " << index << ": " << output[index] << " != " << expected[index] << '\n';
      break;
    }
  }
};

static std::vector<int32_t> read(std::ifstream &stream) {
  size_t count = 0;
  stream >> count;
  std::vector<int32_t> values(count);
  for (auto &value : values)
    stream >> value;
  return values;
}

static void report(const char *workload, const char *kernel, unsigned input, const char *mode, bool stalls, const Stats &stats, size_t outputs, uint64_t intermediate) {
  std::cout << "{\"workload\":\"" << workload << "\",\"kernel\":\"" << kernel << "\",\"input\":" << input << ",\"mode\":\"" << mode << "\",\"backpressure\":" << (stalls ? "true" : "false")
            << ",\"cycles\":" << stats.cycles << ",\"launches\":" << stats.launches << ",\"stream_input_bytes\":" << stats.input << ",\"stream_output_bytes\":" << stats.output
            << ",\"preload_bytes\":" << stats.preload << ",\"materialized_intermediate_bytes\":" << intermediate << ",\"outer_commands\":" << stats.launches
            << ",\"internal_starts\":" << stats.internal << ",\"onchip_link_bytes\":" << stats.link << ",\"onchip_stall_cycles\":" << stats.link_stalls << ",\"outputs\":" << outputs << "}\n";
}

int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);
  if (argc != 2)
    return 2;
  std::ifstream stream(argv[1]);
  Test test;
  test.reset();
  for (unsigned input = 0; input < 2; ++input) {
    auto rowptr = read(stream);
    auto columns = read(stream);
    auto weights = read(stream);
    auto features = read(stream);
    auto sums = read(stream);
    auto expected = read(stream);
    auto activation_input = read(stream);
    auto activation_output = read(stream);
    auto main = read(stream);
    auto skip = read(stream);
    auto residual = read(stream);
    if (!stream) {
      std::cerr << "Incomplete fixture\n";
      return 2;
    }
    for (bool stalls : {false, true}) {
      for (bool fused : {false, true}) {
        const char *mode = fused ? "fused" : "staged";
        std::vector<int32_t> output, intermediate;
        Stats stats;
        test.load(0, rowptr, stats);
        test.load(1, columns, stats);
        test.load(2, weights, stats);
        test.load(3, features, stats);
        stats.add(test.gcn(rowptr.size() - 1, fused, output, stalls));
        test.compare(output, fused ? expected : sums);
        uint64_t materialized = 0;
        if (!fused) {
          intermediate = output;
          stats.add(test.vector(0, intermediate, {}, output, stalls));
          materialized = intermediate.size() * 8;
        }
        test.compare(output, expected);
        if (test.failed)
          return 1;
        report("gcn", "aggregate_relu", input, mode, stalls, stats, output.size(), materialized);

        stats = test.vector(0, activation_input, {}, output, stalls);
        test.compare(output, activation_output);
        if (test.failed)
          return 1;
        report("resnet", "relu", input, mode, stalls, stats, output.size(), 0);

        stats = test.vector(fused ? 2 : 1, main, skip, output, stalls);
        materialized = 0;
        if (!fused) {
          intermediate = output;
          stats.add(test.vector(0, intermediate, {}, output, stalls));
          materialized = intermediate.size() * 8;
        }
        test.compare(output, residual);
        if (test.failed)
          return 1;
        report("resnet", "add_relu", input, mode, stalls, stats, output.size(), materialized);
      }
      std::vector<int32_t> output;
      Stats stats;
      test.chain_load(0, rowptr, stats);
      test.chain_load(1, columns, stats);
      test.chain_load(2, weights, stats);
      test.chain_load(3, features, stats);
      stats.add(test.chain(rowptr.size() - 1, output, stalls));
      test.compare(output, expected);
      if (test.failed)
        return 1;
      report("gcn", "aggregate_relu", input, "streamed", stalls, stats, output.size(), 0);
    }
  }
  test.dut.final();
  return 0;
}
