#include "driver.h"

#include "gemmini_params.h"

_Static_assert(DIM == 16 && ADDR_LEN == 32 && sizeof(elem_t) == 1 && sizeof(acc_t) == 4, "SoC plans require the INT8/INT32 Gemmini command ABI");
_Static_assert(MVIN_SCALE_IDENTITY == 0 && ACC_SCALE_IDENTITY == 0, "SoC plans require unscaled Gemmini transfers");

/* Native funct values are shared with Gemmini's command encoding. */
static void command(unsigned funct, uint64_t rs1, uint64_t rs2) {
  switch (funct) {
  case 0: /* config */
    ROCC_INSTRUCTION_SS(3, rs1, rs2, 0);
    return;
  case 2: /* mvin */
    ROCC_INSTRUCTION_SS(3, rs1, rs2, 2);
    return;
  case 3: /* mvout */
    ROCC_INSTRUCTION_SS(3, rs1, rs2, 3);
    return;
  case 4: /* compute_preloaded */
    ROCC_INSTRUCTION_SS(3, rs1, rs2, 4);
    return;
  case 6: /* preload */
    ROCC_INSTRUCTION_SS(3, rs1, rs2, 6);
    return;
  case 7: /* flush */
    ROCC_INSTRUCTION_SS(3, rs1, rs2, 7);
    return;
  case 23: /* mvout_spad */
    ROCC_INSTRUCTION_SS(3, rs1, rs2, 23);
    return;
  }
  __builtin_unreachable();
}

static void commands(const soc_context_t *context, const soc_command_t *list, unsigned count) {
  for (unsigned index = 0; index < count; ++index) {
    command(list[index].funct, soc_operand(context, list[index].rs1), soc_operand(context, list[index].rs2));
  }
}

void soc_gemmini_init(const soc_context_t *context, const soc_task_t *task) {
  const accel_t device = *context->plan->devices[task->endpoint];
  accel_commands(device, { commands(context, task->init, task->init_count); });
}

int soc_gemmini_capture(const soc_context_t *context, const soc_task_t *task) {
  const accel_t device = *context->plan->devices[task->endpoint];
  if (gemmini_job_capture_at(device.control, task->job, task->command_count, task->patch_count) != 0) {
    return 1;
  }
  for (unsigned index = 0; index < task->patch_count; ++index) {
    gemmini_job_patch_at(device.control, &task->patches[index]);
  }
  accel_commands(device, { commands(context, task->commands, task->command_count); });
  return gemmini_job_end_at(device.control);
}

/* CPU execution uses one local slot; the compiler emits only Slot and TileId sources. */
static uint64_t source(unsigned id, unsigned tile) {
  switch (id) {
  case GEMMINI_VALUE_SLOT:
    return 0;
  case GEMMINI_VALUE_TILE_ID:
    return tile;
  }
  __builtin_unreachable();
}

static uint64_t patched(const soc_task_t *task, unsigned index, unsigned operand, uint64_t original, unsigned tile) {
  uint64_t result = original;
  for (unsigned update = 0; update < task->patch_count; ++update) {
    const gemmini_patch_t *patch = &task->patches[update];
    if (patch->command != index || patch->operand != operand) {
      continue;
    }
    const uint64_t field = UINT64_MAX >> (64 - patch->width);
    const uint64_t mask = field << patch->lsb;
    uint64_t value = source(patch->source, tile) * patch->scale + (uint64_t)(int64_t)patch->offset;
    if (patch->scale2 != 0) {
      value += source(patch->source2, tile) * patch->scale2;
    }
    if (patch->relative) {
      value += (original >> patch->lsb) & field;
    }
    result = (result & ~mask) | ((value << patch->lsb) & mask);
  }
  return result;
}

void soc_gemmini_execute(const soc_context_t *context, const soc_task_t *task, unsigned tile) {
  const accel_t device = *context->plan->devices[task->endpoint];
  accel_commands(device, {
    for (unsigned index = 0; index < task->command_count; ++index) {
      const soc_command_t *entry = &task->commands[index];
      const uint64_t rs1 = patched(task, index, 0, soc_operand(context, entry->rs1), tile);
      const uint64_t rs2 = patched(task, index, 1, soc_operand(context, entry->rs2), tile);
      command(entry->funct, rs1, rs2);
    }
  });
}
