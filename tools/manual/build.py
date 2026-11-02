"""batrider360 instruction booklet: manual_en.html / manual_ja.html + manual.css -> release/manual/*.pdf

    py -3 tools/manual/build.py [en|ja ...] [--preview]

Needs (Windows host): Microsoft Edge (prints the PDF, as the kit's N64 booklet was printed by headless Chrome),
the fonts C:/Windows/Fonts/bahnschrift.ttf and NotoSansJP-VF.ttf (embedded in the PDF), and PyMuPDF
(`pip install pymupdf`) to resolve the page references and to write the --preview page sheets.

What it does:
  - generates the running heads / page numbers (@page rules per section) into out/sections.css;
  - fills each picture slot <div class="slot" data-file="work/x.png" ...> with that picture when the file exists
    (paths relative to the project root, or tools/manual/shots/<name>), else draws a marked placeholder;
  - fills every <span class="pg" data-to="id"></span> with the page of the element id="id": it prints with an
    invisible marker in each target, reads the pages back, and repeats until they settle; the final print has no
    markers.
Regenerate after the GADGETS pictures (work/gadgets_*.png) change.
"""
import os, re, subprocess, sys, tempfile, shutil, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(HERE, "out")
REL = os.path.join(ROOT, "release", "manual")
EDGE = [r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe", r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"]

TITLE = "ARMED POLICE BATRIDER CHASE!"
SECTIONS = {  # page name: (colour, English head, Japanese head)
	"gs": ("#0d1b4c", "GETTING STARTED", "はじめに"),
	"play": ("#0a6fa8", "HOW TO PLAY", "あそびかた"),
	"chase": ("#c9102e", "CHASE! MODE", "CHASE! モード"),
	"menus": ("#6a2c91", "MODES AND MENUS", "モードとメニュー"),
	"host": ("#2b2f77", "STATES AND REPLAYS", "ステートとリプレイ"),
	"gad": ("#2b2f77", "GADGETS", "ガジェット"),
	"ref": ("#4a5066", "REFERENCE", "資料"),
}


PAD_PAGE = '<section class="s-ref newpage notes">' + '<div class="line"></div>' * 21 + "</section>\n"


def url(p):
	return "file:///" + os.path.abspath(p).replace("\\", "/")


def sections_css(lang):
	head = "font-family:'BR Sans','BR JP',sans-serif; font-size:5.4pt; vertical-align:bottom; padding-bottom:2.2mm; border-bottom:0.3mm solid #c4c9d6;"
	num = "font-family:'BR Sans',sans-serif; font-weight:700; font-size:7pt; color:#fff; background:#0d1b4c; text-align:center; vertical-align:middle; width:7.6mm; height:4.4mm;"
	css = []
	for name, (col, en, ja) in SECTIONS.items():
		sec = ja if lang == "ja" else en
		t = f"font-weight:500; letter-spacing:0.24em; color:#8a90a4; {head}"
		s = f"font-weight:700; letter-spacing:{'0.12em' if lang == 'ja' else '0.24em'}; color:{col}; {head}"
		mid = f"content:''; {head}"
		css.append(f"""@page {name}:right {{ @top-left {{ content:"{TITLE}"; {t} text-align:left; }} @top-center {{ {mid} }}
 @top-right {{ content:"{sec}"; {s} text-align:right; }} @bottom-right {{ content:counter(page); {num} }} }}
@page {name}:left {{ @top-left {{ content:"{sec}"; {s} text-align:left; }} @top-center {{ {mid} }}
 @top-right {{ content:"{TITLE}"; {t} text-align:right; }} @bottom-left {{ content:counter(page); {num} }} }}""")
	return "\n".join(css)


def find_pic(rel):
	for p in (os.path.join(HERE, "shots", os.path.basename(rel)), os.path.join(ROOT, rel)):
		if os.path.isfile(p):
			return p
	return None


def fill_slots(html, lang, missing):
	def rep(m):
		attrs = dict(re.findall(r'data-(\w+)="([^"]*)"', m.group(1)))
		cls = m.group(1)
		p = find_pic(attrs["file"])
		if p:
			return f'<img class="shot" src="{url(p)}" alt="">'
		missing.append(attrs["file"])
		label = attrs.get("label", "")
		todo = "画像は後日" if lang == "ja" else "PICTURE TO COME"
		return (f'<div class="slot missing" style="aspect-ratio:{attrs.get("ar", "4/3")}">{todo}<br>{label}<br>'
			f'<span style="font-weight:400">{attrs["file"]}</span></div>')
	return re.sub(r'<div class="slot"([^>]*)></div>', rep, html)


def edge():
	for e in EDGE:
		if os.path.isfile(e):
			return e
	sys.exit("Microsoft Edge not found")


def print_pdf(html_path, pdf_path, _prof):
	# a fresh profile per print: a second Edge started on a profile still in use hands its job over and returns
	prof = tempfile.mkdtemp(prefix="br-manual-edge-")
	if os.path.exists(pdf_path):
		os.remove(pdf_path)
	subprocess.run([edge(), "--headless=new", "--disable-gpu", "--no-first-run", "--no-default-browser-check",
		f"--user-data-dir={prof}", "--no-pdf-header-footer", "--generate-pdf-document-outline",
		"--allow-file-access-from-files", "--virtual-time-budget=8000", f"--print-to-pdf={pdf_path}", url(html_path)],
		check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=300)
	for _ in range(600):  # Edge may finish writing after the launcher returns
		if os.path.isfile(pdf_path) and os.path.getsize(pdf_path) > 0:
			break
		time.sleep(0.1)
	time.sleep(0.5)
	shutil.rmtree(prof, ignore_errors=True)
	if not os.path.isfile(pdf_path):
		sys.exit("Edge wrote no PDF")


def marker_pages(pdf_path):
	import pymupdf
	pages = {}
	with pymupdf.open(pdf_path) as d:
		for i, pg in enumerate(d):
			for m in re.findall(r"QQ(\w+?)QQ", pg.get_text().replace("\n", "")):
				pages.setdefault(m, i + 1)
	return pages


def build(lang, preview):
	src = os.path.join(HERE, f"manual_{lang}.html")
	html = open(src, encoding="utf-8").read()
	missing = []
	html = fill_slots(html, lang, missing)
	html = html.replace('href="manual.css"', f'href="{url(os.path.join(HERE, "manual.css"))}"')
	html = html.replace("</head>", f"<style>\n{sections_css(lang)}\n.mk{{font-size:0.5pt;color:#fff;letter-spacing:0;text-transform:none;font-stretch:100%}}\n</style>\n</head>")
	html = re.sub(r'src="art/', f'src="{url(os.path.join(HERE, "art"))}/', html)
	ids = re.findall(r'<span class="pg" data-to="(\w+)"></span>', html)
	os.makedirs(OUT, exist_ok=True)
	os.makedirs(REL, exist_ok=True)
	prof = tempfile.mkdtemp(prefix="br-manual-edge-")
	work_html = os.path.join(OUT, f"manual_{lang}.build.html")
	work_pdf = os.path.join(OUT, f"manual_{lang}.pass.pdf")
	final = os.path.join(REL, f"batrider360_manual_{lang}.pdf")

	def render(pages, markers, pdf):
		h = html
		if markers:
			h = re.sub(r'(<(?:h1|h2|h3|div|section|p)\b[^>]*\bid="(\w+)"[^>]*>)', r'\1<span class="mk">QQ\2QQ</span>', h)
		h = re.sub(r'<span class="pg" data-to="(\w+)"></span>', lambda m: f'<span class="pg">{pages.get(m.group(1), 88)}</span>', h)
		open(work_html, "w", encoding="utf-8").write(h)
		print_pdf(work_html, pdf, prof)

	try:
		pages = {}
		for it in range(4):
			render(pages, True, work_pdf)
			new = marker_pages(work_pdf)
			lost = sorted(set(ids) - set(new))
			if lost:
				print(f"  {lang}: no target for page references {lost}")
			if new == pages:
				break
			pages = new
		render(pages, False, final)
		# a booklet has a multiple of four pages and its back cover on an even page: pad with ruled note pages
		import pymupdf
		with pymupdf.open(final) as d:
			pad = -len(d) % 4
		if pad:
			html = html.replace("<!--PAD-->", PAD_PAGE * pad)
			render(pages, False, final)
	finally:
		shutil.rmtree(prof, ignore_errors=True)
	import pymupdf
	with pymupdf.open(final) as d:
		n = len(d)
		if preview:
			from PIL import Image
			ims = []
			for pg in d:
				p = pg.get_pixmap(dpi=80)
				ims.append(Image.frombytes("RGB", (p.width, p.height), p.samples))
			for s in range(0, n, 8):
				part = ims[s:s + 8]
				w, h = part[0].size
				sheet = Image.new("RGB", (4 * w + 18, 2 * h + 6), "#777")
				for k, im in enumerate(part):
					sheet.paste(im, ((k % 4) * (w + 6), (k // 4) * (h + 6)))
				sheet.save(os.path.join(OUT, f"preview_{lang}_{s // 8 + 1}.png"))
	print(f"{final}: {n} pages; references {dict(sorted(pages.items(), key=lambda kv: kv[1]))}")
	if missing:
		print(f"  placeholders (picture not found yet): {', '.join(sorted(set(missing)))}")


if __name__ == "__main__":
	args = [a for a in sys.argv[1:] if not a.startswith("--")]
	for lang in args or ["en", "ja"]:
		build(lang, "--preview" in sys.argv)
