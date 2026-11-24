# Split ASIC and CGRA with on-chip ResNet intermediates

This experiment preserves the earlier independent Add/ReLU ASIC hardware and on-chip ResNet data path, then uses regional Hybrid control for both backends. It is separate from the general five-unit fused ASIC comparison in `../current/`.

The unchanged split simulator has SHA256 `d2b5f2169deea6433be8eaa8707aa0008feff30377aa309cfd174bf962bacb47`. `abi.json` compares the original split runtime-header hashes against those used here. No RTL, accelerator configuration, frozen packet header or shared build output is changed.

| Property | CGRA | Split ASIC |
| --- | --- | --- |
| Compute hardware | One 4×4 CGRA | Independent fixed Add and ReLU endpoints |
| Data SPM | 128 KiB | 64 KiB per endpoint, 128 KiB total |
| First region | ReLU | ReLU |
| Second region | Fused AddReLU | Add, automatic peer SPM copy, ReLU |
| CPU region submissions per inference | 2 | 2 |
| Native compute executions per inference | 2 | 3 |
| Intermediate representation conversion | Gemmini INT8/INT32 conversion | Same |
| Intermediate payload placement | On-chip SPM | On-chip SPM |

The CPU sequences the three Gemmini convolutions, conversion and boundary copies. Cached AutoLink profiles execute each nonmatrix region. The ASIC Add-to-ReLU copy is included in its second region; there is no CPU launch or CPU payload copy between these two operators. Both backends return the final INT32 output to DRAM. Configuration, staging, conversions, computation and final visibility are timed; numerical checks and printing occur after the timed interval.

CGRA ReLU occupies control slots 0–4 and AddReLU slots 5–9. Both static programs are retained. Each job captures configuration and launch packets only, as in the earlier split experiment. The two run profiles are captured on the first inference and reused during warm measurements. ASIC jobs retain their native descriptor configuration in the adapter.

The medium fixture has an input of 16×16×8 and output of 8×8×16, giving 1024 checked outputs per inference. Each backend runs one cold and three warm inferences with inputs 0,1,0,1. No CPU cache flush separates samples; these are deterministic repeated executions, not independent statistical trials.

`run.py` saves commands, simulator/compiler-input/ELF hashes, exit codes, logs and parsed records. A passing backend requires all four samples, all 4096 outputs checked, zero numerical/counter errors, and exactly two regional submissions per sample. ASIC counters additionally require zero native CPU compute starts, one Add start and two ReLU starts per inference.

Run from the repository root with `bash build/experiments/asic/hybrid/split-onchip/compile.sh medium`, then `python3 build/experiments/asic/hybrid/split-onchip/run.py`. The runner replaces this experiment's run logs/results; archive them before a new experiment revision.

The simulation uses the same nominal 500 MHz clock for both endpoints. This does not establish physical timing closure. Area and timing figures for the later general ASIC must not be attached to this split implementation.
