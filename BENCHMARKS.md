# Benchmarks

No evaluations were rerun. Ranges are observed minima/maxima, not confidence intervals.

## Physical Vita evidence

|Configuration / protocol|Model bytes|TTFT s|Prompt tokens/s|Decode pieces/s|
|---|---:|---:|---:|---:|
|Selected adapted LFM Q8, 3 prompts × 3 repeats|244,215,040|3.708–4.736|3.80–4.31|1.662–1.906|
|Optimized Woozy/SmolLM2-135M packed Q8, fixed five prompts|Not stated in selected report excerpt|6.382–8.672|5.476–5.941|2.514–2.630|
|Historical aligned Smol Q8, same fixed five control protocol|Not stated here|57.783–76.762|0.638–0.640|0.554–0.601|

LFM and Smol prompt sets/tokenizers differ: these are contextual comparisons, not paired identical-work speedups. Smol has 134,515,008 parameters; an exact packed file size is not inferred. Historical LFM Q4 was 129,395,968 bytes, TTFT 4.228–4.837 s, decode 2.779–3.162 pieces/s. Mixed Q8-head/Q4-core was 162,950,400 bytes, TTFT 4.084–4.825 s, decode 2.314–2.639 pieces/s. Neither is deployed in 03.05.

LFM load: 18.866 s. Sampled live heap: 29.87 MiB excluding model block, not peak RSS. The older report described about 28 MiB KV across all 14 layers; final runtime statistics specify fixed 12 MiB attention KV. Do not apply the old allocation to the final executable. Final UI additionally uses a bounded 2 MiB theme cache. No fresh final-build peak-memory measurement exists.

TTFT excludes initialization/loading and ends at first output. Decode excludes the first emitted piece produced during prefill and includes terminal EOS latency; pieces are not words. Prompt rate includes the final prompt position producing the first piece. LFM actual output IDs/total time was 1.02–1.69/s, separate from decode. Calculator microseconds are not neural throughput.

## PC and provisional agent quality evidence

Frozen 110-item development suite: greedy FP32 masters, 96-new-token cap, native templates; Smol retained its established system message. Provisional agent scores: upstream LFM 45/110, SmolLM2-135M 35/110, adapted LFM FP32 70/110. Deployment adapted Q8 also scored 70/110, including arithmetic 17/20 and instruction following 10/15. Only 44 complete Q8 answers matched the FP32 master; equal totals do not imply identical answers. Adapted Q4 scored 60/110, mixed precision 62/110. These are not human/blind final validation scores.

A separate Q8 factual audit flagged 21/60 science/history/knowledge/uncertainty answers for materially false/unsupported assertions. This is not an all-task hallucination rate.

Later conversation/application suites are separate: Phase12 clean agent-only acceptance remained 4/5 checks, 3/4 cases. Earlier 80-item suites and physical host/device retention controls must not be pooled with the 110-item model screen. Retention controls test implementation correspondence, not intelligence. Eval80-075 actual-input tracing remains unrun.

## Preserved record provenance

Values derive from VITALM_PRETRAINED_FIRST_FINAL_REPORT.md sections 1, 4, 6–10 and 17, ADAPTATION_REPORT.md, VITA_SMOLLM2_OPTIMIZATION_REPORT.md, Phase12 review and Phase13 source/release notes. These summaries replace raw experimental dumps. Selected model/runtime measurements predate branding; 03.05 retained them without a new speed campaign. Energy, thermal behavior, endurance and broad human acceptance remain unmeasured.
