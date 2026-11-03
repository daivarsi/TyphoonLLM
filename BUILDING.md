# Building

The final source ZIP was extracted to `src/`. Only the Inter font provenance documentation was sanitized; functional source and CMake layout are retained.

Use CMake 3.16+, a C build backend and VitaSDK in a suitable host shell. The recorded build used Ubuntu 20.04 under WSL with an installed SDK; a fully reproducible host image is not supplied. Set `VITASDK` to your SDK installation and expose its tools on PATH.

From the repository root, portable equivalents of the recorded production commands are:

```sh
cmake -S src -B build -DVITALM_TITLE_ID=VLMCHAT01 -DVITALM_APP_NAME=TyphoonLLM -DVITALM_APP_VERSION=03.06
cmake --build build -j4
```

Use all three overrides: the preserved CMake defaults target an isolated preview. `vita_create_self` and `vita_create_vpk` generate `build/eboot.bin` and `build/VitaLMChat.vpk`; the latter is the historical output name. A rebuild is not the frozen final package, and byte-identical reproducibility is not established.

Keep `src/common/`, `src/src/`, generated font/art includes, `src/fonts/`, references, notices and `src/sce_sys/` intact. CMake records C11, `-O3 -Wall -Wextra`, prefetch distance 2 and the actual linked Vita stubs.

Model assets are not needed to compile. Running requires the exact `model.vlp` and `tokenizer.bin` from Releases, installed according to [INSTALLATION.md](INSTALLATION.md). No model conversion/training is required. Historical compiler warnings remain in inherited helpers. This handoff did not rebuild, re-quantize, retrain or rerun evaluations.
