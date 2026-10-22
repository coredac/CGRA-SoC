#ifndef SOC_DRIVER_API_H
#define SOC_DRIVER_API_H

typedef struct soc_context soc_context_t;
typedef struct soc_task soc_task_t;

/* Init and capture complete preparation; execute submits one tile. */
typedef struct {
  int (*init)(const soc_context_t *, const soc_task_t *);
  int (*capture)(const soc_context_t *, const soc_task_t *);
  int (*execute)(soc_context_t *, const soc_task_t *, unsigned tile);
  /* Optional state update after a successful automatic completion report. */
  void (*complete)(soc_context_t *, const soc_task_t *);
} soc_driver_t;

extern const soc_driver_t soc_gemmini_driver;
extern const soc_driver_t soc_cgra_driver;

#endif
