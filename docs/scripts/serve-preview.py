"""Serve the built tutorial locally, including VitePress clean URLs."""
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
import argparse

root = Path(__file__).resolve().parents[1] / '.vitepress/dist'
parser = argparse.ArgumentParser()
parser.add_argument('--port', type=int, default=5173)
args = parser.parse_args()
if not (root / 'index.html').is_file():
    raise SystemExit('Run npm run build in docs/ first.')

class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=str(root), **kw)
    def list_directory(self, path):
        self.send_error(404)
        return None
    def translate_path(self, path):
        resolved = super().translate_path(path)
        p = Path(resolved)
        if not p.exists() and not p.suffix and p.with_suffix('.html').is_file():
            return str(p.with_suffix('.html'))
        return resolved
    def end_headers(self):
        self.send_header('X-Content-Type-Options', 'nosniff')
        super().end_headers()

print(f'OSRCORE preview: http://localhost:{args.port} (LAN enabled)', flush=True)
ThreadingHTTPServer(('0.0.0.0', args.port), Handler).serve_forever()
