# Install TyphoonLLM 03.05

Requires a homebrew-capable PS Vita and VitaShell.

1. Download TyphoonLLM-v03.05-install-kit.zip from this release and extract it on your computer.
2. Copy the CONTENTS of the extracted ux0 folder to the root of your Vita ux0 storage using your usual VitaShell transfer method. Merge the data folders if prompted. Keep the existing VitaLMChat folder name.
3. In VitaShell, open ux0:TyphoonLLM-v03.05-final.vpk and install it using the normal prompts.
4. Launch TyphoonLLM from LiveArea. Wait for the model to load (historically about 19 seconds) and for the app to become ready.

Your Vita must contain:

ux0:TyphoonLLM-v03.05-final.vpk
ux0:data/VitaLMChat/model.vlp
ux0:data/VitaLMChat/tokenizer.bin
ux0:data/VitaLMChat/MODEL_LICENSE.txt

Copy the CONTENTS of ux0, not an extra nested ux0 folder. The title ID is VLMCHAT01, so installation updates an existing app with that ID. No PC runtime, API account or internet inference connection is needed after installation.

The unchanged final VPK contains the app, not the model. Installing only the VPK is insufficient on a fresh Vita. This kit includes the exact model/tokenizer separately in the correct folder layout. It does not rebuild or modify the application.

Alternative: download the VPK, model.vlp and tokenizer.bin separately; copy the latter two to ux0:data/VitaLMChat/ and install the VPK with VitaShell.

Stop interrupts generation; New chat clears session state. Theme defaults Dark at launch. Missing-model errors usually mean the model files are absent, misnamed or nested under the wrong folder.
