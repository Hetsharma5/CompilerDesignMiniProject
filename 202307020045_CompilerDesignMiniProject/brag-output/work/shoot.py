# usage: python shoot.py t1 t2 ...  -> stills/still_<t>.png
import sys, os, pathlib
from playwright.sync_api import sync_playwright
here = pathlib.Path(__file__).parent.resolve()
def open_page(p):
    b = p.chromium.launch()
    pg = b.new_page(viewport={"width":1920,"height":1080}, color_scheme="dark")
    pg.goto((here/"video.html").as_uri()); pg.evaluate("window.ready")
    offs = pg.frames[1].evaluate("['sym','icg'].map(id=>document.getElementById(id).offsetTop)")
    pg.evaluate("o=>window.REPO=o", offs)
    return b, pg
if __name__ == "__main__":
    os.makedirs(here/"stills", exist_ok=True)
    with sync_playwright() as p:
        b, pg = open_page(p)
        for t in sys.argv[1:]:
            pg.evaluate(f"render({t})"); pg.screenshot(path=str(here/"stills"/f"still_{t}.png"))
        b.close()
