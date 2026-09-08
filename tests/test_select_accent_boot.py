"""Exact built ROM cold-boot capture; no storage or typing claims."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import time
from PIL import Image
from Xlib import X, display

root = Path(__file__).resolve().parents[1]
qa = root / "tests/select-accent-evidence/boot"
qa.mkdir(parents=True, exist_ok=True)
rom = qa / (root.name + ".gba")
shutil.copy2(root / rom.name, rom)
digest = hashlib.sha256(rom.read_bytes()).hexdigest()
print("ROM SHA256", digest, flush=True)
r, w = os.pipe()
xvfb = subprocess.Popen(["Xvfb", "-displayfd", str(w), "-screen", "0", "800x600x24", "-nolisten", "tcp"], pass_fds=(w,), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
os.close(w)
with os.fdopen(r, "rb") as pipe:
    number = pipe.readline().decode().strip()
assert number.isdigit()
name = ":" + number
env = dict(os.environ, DISPLAY=name, HOME=str(qa / "home"), XDG_CONFIG_HOME=str(qa / "config"))
Path(env["HOME"]).mkdir(exist_ok=True)
Path(env["XDG_CONFIG_HOME"]).mkdir(exist_ok=True)
d = display.Display(name)
emu = None
try:
    with (qa / "emulator.log").open("w") as log:
        emu = subprocess.Popen(["mgba", "-2", "-C", "audioSync=0", "-C", "videoSync=1", "-C", "pauseOnFocusLost=0", str(rom)], env=env, cwd=qa, stdout=log, stderr=log)
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            titles = []
            for child in d.screen().root.query_tree().children:
                title = str(child.get_wm_name())
                titles.append(title)
                assert "Crash" not in title, titles
                if child.get_attributes().map_state != X.IsViewable or "mGBA" not in title:
                    continue
                geom = child.get_geometry()
                if geom.width < 480 or geom.height < 320:
                    continue
                raw = child.get_image(0, 0, geom.width, geom.height, X.ZPixmap, 0xffffffff)
                im = Image.frombytes("RGB", (geom.width, geom.height), raw.data, "raw", "BGRX")
                im = im.crop((0, im.height - 320, 480, im.height))
                # Pillow's ImagingCore is iterable at runtime; its stub omits it.
                pixels = list(im.getdata())  # type: ignore[arg-type]
                black = sum(r < 50 and g < 50 and b < 50 for r, g, b in pixels)
                blue = sum(b > 140 and b > r + 35 for r, g, b in pixels)
                if black > 100 and blue > 100:
                    im.save(qa / "home.png")
                    print("Rendered black/blue pixels", black, blue, "titles", titles, flush=True)
                    break
            else:
                assert emu.poll() is None, "emulator exited"
                time.sleep(0.5)
                continue
            break
        else:
            raise AssertionError("No rendered app framebuffer within 120 seconds")
        assert hashlib.sha256(rom.read_bytes()).hexdigest() == digest
        print("PASS boot framebuffer captured; image requires visual inspection", flush=True)
finally:
    d.close()
    if emu is not None:
        emu.terminate()
        try:
            emu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            emu.kill()
            emu.wait()
    xvfb.terminate()
    xvfb.wait(timeout=5)
