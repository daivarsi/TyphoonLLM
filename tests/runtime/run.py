"""Host-only session control-flow checks; no model or VitaSDK is loaded."""
from pathlib import Path
import os,shlex,subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="typhoon-prefill-") as temporary:
    binary=Path(temporary)/"prefill-test"
    compiler=shlex.split(os.environ.get("HOST_CC","cc"))
    subprocess.run(compiler+["-std=c11","-O2","-I"+str(repo/"tests/runtime/include"),"-I"+str(repo/"src/src"),str(repo/"src/src/session.c"),str(repo/"tests/runtime/prefill_head_test.c"),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
