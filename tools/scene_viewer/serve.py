#!/usr/bin/env python3
"""Serves the scene viewer, or renders a scene to a PNG.

The viewer (index.html + viewer.js) shows a .scene file with the engine's
shading, without building or running the game. Browsers do not let a page
read local files, so the repository is served over HTTP.

Usage (from anywhere):
  python3 tools/scene_viewer/serve.py                 # open it in the browser
  python3 tools/scene_viewer/serve.py --no-browser    # only serve
  python3 tools/scene_viewer/serve.py --shot out.png --view player
      # headless Firefox renders the scene and saves out.png, then exits

Options:
  --scene PATH   scene file, relative to the repo root
                 (default assets/scenes/desert.scene)
  --port N       HTTP port (default 8000; 0 = any free port)
  --view V       orbit | top | player   initial camera
  --size WxH     size of the --shot image (default 1280x720)

Only the standard library is needed. three.js is loaded from a CDN, so the
browser needs internet access.
"""
import argparse
import functools
import http.server
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib.parse
import webbrowser

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


class Handler(http.server.SimpleHTTPRequestHandler):
    shot_path = None
    shot_done = None
    shot_error = ''

    def end_headers(self):
        # Always serve the latest version of the scene and the assets
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def do_POST(self):
        if self.path != '/__shot' or not Handler.shot_path:
            self.send_error(404)
            return
        data = self.rfile.read(int(self.headers.get('Content-Length', 0)))
        with open(Handler.shot_path, 'wb') as f:
            f.write(data)
        Handler.shot_error = urllib.parse.unquote(self.headers.get('X-Error', ''))
        self.send_response(204)
        self.end_headers()
        Handler.shot_done.set()

    def log_request(self, code='-', size='-'):
        # Only show failed requests (missing assets, typos in the scene)
        if Handler.shot_path is None and not str(int(code)).startswith(('2', '3')):
            super().log_request(code, size)


def take_shot(url, size, timeout=120):
    firefox = shutil.which('firefox')
    if not firefox:
        sys.exit('--shot needs firefox in the PATH')
    width, height = size.split('x')
    profile = tempfile.mkdtemp(prefix='scene_viewer_')
    proc = subprocess.Popen(
        [firefox, '--headless', '--no-remote', '--profile', profile,
         f'--width={width}', f'--height={height}', url],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        if not Handler.shot_done.wait(timeout):
            sys.exit(f'timed out after {timeout}s waiting for the render')
    finally:
        proc.terminate()
        try:
            proc.wait(10)
        except subprocess.TimeoutExpired:
            proc.kill()
        shutil.rmtree(profile, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--scene', default='assets/scenes/desert.scene')
    parser.add_argument('--port', type=int, default=8000)
    parser.add_argument('--view', choices=['orbit', 'top', 'player'], default='orbit')
    parser.add_argument('--no-browser', action='store_true')
    parser.add_argument('--shot', metavar='PNG')
    parser.add_argument('--size', default='1280x720')
    args = parser.parse_args()

    if not os.path.isfile(os.path.join(ROOT, args.scene)):
        sys.exit(f'{args.scene}: no such scene file under {ROOT}')

    if args.shot:
        Handler.shot_path = os.path.abspath(args.shot)
        Handler.shot_done = threading.Event()
        args.port = 0 if args.port == 8000 else args.port

    server = http.server.ThreadingHTTPServer(
        ('127.0.0.1', args.port), functools.partial(Handler, directory=ROOT))
    port = server.server_address[1]
    query = urllib.parse.urlencode({'scene': args.scene, 'view': args.view})
    url = f'http://127.0.0.1:{port}/tools/scene_viewer/index.html?{query}'

    if args.shot:
        threading.Thread(target=server.serve_forever, daemon=True).start()
        take_shot(url + '&shot=1', args.size)
        server.shutdown()
        if Handler.shot_error:
            sys.exit(f'scene error: {Handler.shot_error}')
        print(f'saved {Handler.shot_path}')
        return

    print(f'Scene viewer: {url}\n(Ctrl+C to stop)')
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
