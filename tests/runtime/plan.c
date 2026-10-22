#include "gemmini.h"
#include "gemmini_job.h"
#include "generated/runtime_plan.h"
#include "internal/driver.h"

/* Compare compiler command encodings with the native driver without issuing RoCC. */
static const soc_command_t *expected;
static soc_command_t *captured;
static unsigned count;
static unsigned cursor;
static unsigned failures;
static const uintptr_t arguments[] = {UINT64_C(0x100001000), UINT64_C(0x100002000), 0, 0, UINT64_C(0x100003000), UINT64_C(0x1ffffffc0)};

static uint64_t value(soc_operand_t operand) {
  if (operand.base == SOC_ARGUMENT) {
    return arguments[operand.index] + operand.value;
  }
  if (operand.base == SOC_SPM) {
    return soc_plans[0].devices[operand.index]->spm + operand.value;
  }
  return operand.value;
}

static void check(unsigned funct, uint64_t rs1, uint64_t rs2) {
  if (cursor >= count || expected[cursor].funct != funct || value(expected[cursor].rs1) != rs1 || value(expected[cursor].rs2) != rs2) {
    printf("Native command mismatch index=%u funct=%u rs1=%lx rs2=%lx\n", cursor, funct, (unsigned long)rs1, (unsigned long)rs2);
    ++failures;
  }
  ++cursor;
}

#undef ROCC_INSTRUCTION_RS1_RS2
#define ROCC_INSTRUCTION_RS1_RS2(opcode, rs1, rs2, funct) check(funct, (uint64_t)(rs1), (uint64_t)(rs2))

static void record(unsigned funct, uint64_t rs1, uint64_t rs2) {
  if (cursor < count) {
    captured[cursor] = (soc_command_t){.funct = funct, .rs1 = {.value = rs1, .base = SOC_LITERAL}, .rs2 = {.value = rs2, .base = SOC_LITERAL}};
  } else {
    puts("Runtime command count exceeded");
    ++failures;
  }
  ++cursor;
}

#undef ROCC_INSTRUCTION_SS
#define ROCC_INSTRUCTION_SS(opcode, rs1, rs2, funct) record(funct, (uint64_t)(rs1), (uint64_t)(rs2))
#undef accel_commands
#define accel_commands(device, ...)                                                                                                                                                                    \
  do {                                                                                                                                                                                                 \
    (void)(device);                                                                                                                                                                                    \
    __VA_ARGS__                                                                                                                                                                                        \
  } while (0)
#include "../../runtime/src/gemmini.c"

static void begin(const soc_command_t *stream, unsigned length) {
  expected = stream;
  count = length;
  cursor = 0;
}

static void end(void) {
  if (cursor != count) {
    puts("Native command count mismatch");
    ++failures;
  }
}

static void check_init(const soc_task_t *task, unsigned weight, int load_input) {
  begin(task->init, task->init_count);
  gemmini_flush(0);
  gemmini_config_ld(DIM);
  gemmini_mvin(arguments[weight], 16);
  if (load_input) {
    gemmini_config_ld(DIM);
    gemmini_extended_mvin(arguments[0], 0, DIM, SOC_PLAN_ROWS);
  }
  end();
}

static void first_commands(unsigned tile) {
  const uint32_t acc_write = UINT32_C(1) << (ADDR_LEN - 1);
  const uint32_t acc_read = acc_write | (UINT32_C(1) << (ADDR_LEN - 3));
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(DIM * sizeof(acc_t));
  gemmini_extended_preload(16, acc_write, DIM, DIM, DIM, SOC_PLAN_TILE_ROWS);
  gemmini_extended_compute_preloaded(tile * SOC_PLAN_TILE_ROWS, GARBAGE_ADDR, DIM, SOC_PLAN_TILE_ROWS, DIM, SOC_PLAN_TILE_ROWS);
  gemmini_extended_mvout_spad(130816 / DIM, 4, acc_read, DIM, SOC_PLAN_TILE_ROWS);
}

static void last_commands(unsigned tile) {
  const uint32_t acc_write = UINT32_C(1) << (ADDR_LEN - 1);
  const uint32_t acc_read = acc_write | (UINT32_C(1) << (ADDR_LEN - 3));
  gemmini_config_ld(DIM);
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(DIM * sizeof(acc_t));
  gemmini_extended_mvin(CGRA0.spm + 64 + tile * SOC_PLAN_TILE_ROWS * DIM, 0, DIM, SOC_PLAN_TILE_ROWS);
  gemmini_extended_preload(16, acc_write, DIM, DIM, DIM, SOC_PLAN_TILE_ROWS);
  gemmini_extended_compute_preloaded(0, GARBAGE_ADDR, DIM, SOC_PLAN_TILE_ROWS, DIM, SOC_PLAN_TILE_ROWS);
  gemmini_extended_mvout(arguments[5] + tile * SOC_PLAN_TILE_ROWS * DIM * sizeof(acc_t), acc_read, DIM, SOC_PLAN_TILE_ROWS);
}

static void check_execute(const soc_context_t *context, const soc_task_t *task, unsigned tile, void (*native)(unsigned)) {
  soc_command_t actual[task->command_count];
  captured = actual;
  begin(actual, task->command_count);
  soc_gemmini_execute(context, task, tile);
  end();
  if (cursor != count) {
    return;
  }
  begin(actual, task->command_count);
  native(tile);
  end();
}

int main(void) {
  const soc_plan_t *plan = &soc_plans[SOC_MANUAL];
  const soc_task_t *first = &plan->tasks[plan->steps[0].tasks[0]];
  const soc_task_t *last = &plan->tasks[plan->steps[0].tasks[2]];
  check_init(first, 1, 1);
  check_init(last, 4, 0);
  begin(first->commands, first->command_count);
  first_commands(0);
  end();
  begin(last->commands, last->command_count);
  last_commands(0);
  end();
  const soc_context_t context = {.plan = plan, .arguments = arguments};
  for (unsigned tile = 0; tile < plan->tiles; ++tile) {
    check_execute(&context, first, tile, first_commands);
    check_execute(&context, last, tile, last_commands);
  }
  if (failures) {
    return 1;
  }
  puts("Runtime native command plan: PASS");
  return 0;
}
