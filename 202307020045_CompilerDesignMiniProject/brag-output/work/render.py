# Renders video.html frame by frame (30fps) and pipes JPEG frames into ffmpeg -> video_noaudio.mp4
import subprocess, sys
sys.path.insert(0, "py")
import imageio_ffmpeg
from shoot import open_page, sync_playwright
FPS, DUR = 30, 63.0
ff = imageio_ffmpeg.get_ffmpeg_exe()
enc = subprocess.Popen([ff, "-y", "-loglevel", "error", "-f", "image2pipe", "-framerate", str(FPS), "-c:v", "mjpeg", "-i", "-",
    "-c:v", "libx264", "-preset", "slow", "-crf", "17", "-pix_fmt", "yuv420p", "-movflags", "+faststart", "video_noaudio.mp4"], stdin=subprocess.PIPE)
with sync_playwright() as p:
    b, pg = open_page(p)
    for f in range(int(FPS * DUR)):
        pg.evaluate(f"render({f / FPS})")
        enc.stdin.write(pg.screenshot(type="jpeg", quality=95))
        if f % 300 == 0: print("frame", f, flush=True)
    b.close()
enc.stdin.close(); enc.wait(); print("done", enc.returncode)
