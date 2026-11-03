# Runtime update 03.06

Prompt prefill previously computed a complete vocabulary projection after each four-token batch and each residual single position, although the session only samples the final prompt scores. The new prefill entry points omit those discarded intermediate projections. Every transformer layer, attention KV update and recurrent convolution update still runs. The last prompt position and every generated token still compute all vocabulary scores. Model weights, quantization, sampling, user prompts and numerical matrix kernels are unchanged.

The engine-owned projection flag is scoped to each prefill call and restored on every return, including an error. Session code explicitly requests scores on the final batch or final residual token. The default is enabled; existing preview-only test workloads can temporarily disable it with PREFILL_HEAD_SKIP 0 for matched controls. Production never reads the automatic test file.

## Physical measurements

The same PSVshellPlus profile was maintained: CPU 500 MHz, GPU 222 MHz, XBAR 166 MHz. The plugin Quick Menu was checked, rather than treating ordinary SDK frequency readbacks as physical-clock measurements. Existing clock profiles remain in charge; the app does not request 500 MHz or lower a user-selected clock. The original offline app already behaved passively, and its four-token prefix cache already worked. The online-only system-prefix cache correction is not added to the offline prompt template.

| Original offline runtime | Baseline | Optimized |
|---|---:|---:|
| Long prompt, first token | 29.26 s | 25.06 s |
| Long prompt, complete answer | 49.10 s | 45.48 s |
| Short prompt, first token | 1.73 s | 1.59 s |
| Short prompt, complete answer | 60.47 s | 60.45 s |

The long prompt improved about 14.4% to first token and 7.4% overall. The short prompt saved about 0.14 seconds to first token; total-time change was negligible. This is a prompt-processing optimization, not a decoding-speed improvement. Each offline pair used identical prompt IDs and emitted identical generated token IDs and stop reasons. Long prefill forwarded 121 positions in both cases; required vocabulary evaluations changed from 31 to 1. Two repeated online controls also preserved exact IDs and improved mean long-prompt first-token time by about 13.8% and total time by 8.2%.

These are limited controlled samples on one physical Vita using LFM2.5-230M Q8, a 512-position session, three native inference participants, four-position prefill and greedy generation. They are not a universal speed guarantee. Short requests benefit little. Generated-answer quality and the model's known factual/wording mistakes are unchanged. Test orchestration and rendering/instrumentation are included in the end-to-end timings; model load is excluded.

## Verification

Run `python3 tests/runtime/run.py` on a host with a C compiler. It compiles the actual session implementation against a small deterministic fake engine and checks 54 prompt-boundary, final-score, prefix-reuse, mismatch and cancellation cases. It does not run or numerically validate the LLM. Separate physical paired runs verify the real model's emitted token IDs. The production 03.06 package is built from this repository source; isolated device testing used a separate test title/data directory while reading the same unchanged model/tokenizer assets.

The default offline prompt and layout are retained; the misleading colored marker beside Offline is removed. No online retrieval dependency is added. Keep the existing model assets. The immutable 03.05 release and its rollback package remain available; this is a new 03.06 runtime update.

## Clock-reading caveat

PSVshellPlus handles 500 MHz through a special clock path. Its own getter checks a plugin key that ordinary app SDK getters need not reflect. See the [plugin clock implementation](https://github.com/GrapheneCt/PSVshellPlus/blob/main/PSVshellPlus_Kernel/clock.c). Earlier SDK-only 333/444 labels must not be treated as independently verified physical clocks, and this update does not claim a measured overclock speedup. It measures avoided prefill work at the same verified profile.

The offline header has no colored connection dot. Its former cyan marker could be mistaken for online status and was removed.
