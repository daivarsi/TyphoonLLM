# Architecture

## Upstream model

LiquidAI/LFM2.5-230M has 229,693,184 parameters, width 1024 and 14 blocks: eight double-gated short convolutions and six grouped-query attention blocks. FFNs use SwiGLU with width 2560. RMSNorm, per-head QK normalization, RoPE theta 1e6, 16 query/eight KV heads and head dimension 64 are preserved. Embeddings and output projection share weights. Configured vocabulary is 65,536; upstream tokenizer has 64,402 entries. Upstream context claims are not Vita validation.

## Native runtime

The specialized C engine descends from WoozyLLM/llama2.c, retaining optimized quantized operations and adding LFM topology. Deployment uses Q8 group64 weights/scales and dynamic Q8 activations, with FP32 norms and convolution kernels. The existing custom `.vlp` container is not GGUF. Loader interpretation and tensor layout live in `src/src/llama_engine.c`; `vitalm_format.h` also contains an older format validator and is not the sole LFM specification.

Byte-level BPE preserves upstream Unicode splitting, added tokens and pair ranks. The converted `tokenizer.bin` and compiled merge/Unicode tables support tokenization. Historical coverage matched 11,940/11,940 cases, not every possible Unicode input. Embedding rows are dequantized on demand.

Convolution blocks maintain short rolling state with gated operations. Attention maintains causal GQA key/value state. FFN and vocabulary projection matrices execute normally. ARMv7 NEON, four-row activation reuse, bounded prefetch and aligned workspace reduce cost. A persistent main-plus-two-worker pool partitions rows and synchronizes before workspace reuse.

Prefill processes known prompt positions, including causal batch4 paths where applicable; decode advances one new token at a time. Their rates measure different work. Application sessions bound usable context to 512 positions. Final runtime statistics specify fixed 12 MiB KV, unlike older larger allocations. Convolution state must be preserved/reset consistently with KV state.

## Application layer

`src/assistant.c` separately routes supported bounded deterministic arithmetic and dated offline reference requests. Calculator/reference successes are not neural-model capability. Accepted formatting includes ties-to-even cases; arithmetic is not arbitrary precision.

Session/worker/reply policy implement bounded multi-turn state, reuse/rebuild decisions, stop and automatic continuation. New chat clears state. Continuation is bounded; correction/ambiguity failures remain.

CPU framebuffer UI code, canvas/platform functions and generated fonts/art handle presentation, native IME, touch and buttons. A bounded 2 MiB theme texture cache falls back to flat rendering on allocation failure. It is separate from inference state. Historical unused renderer/probe sources remain in the snapshot; CMake’s executable list defines the release target.

Production `VLMCHAT01` retains `ux0:data/VitaLMChat/`. The isolated preview has a separate private path; production consumes no automatic test trigger. Inference uses no cloud/API.
