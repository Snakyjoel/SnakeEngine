import http.server
import socketserver
import os
import json
import urllib.parse
import webbrowser

PORT = 8123

# Get the path to assets/preload
tools_dir = os.path.dirname(os.path.abspath(__file__))
project_dir = os.path.dirname(tools_dir)
preload_dir = os.path.join(project_dir, 'assets', 'preload')

class EditorHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=tools_dir, **kwargs)

    def do_GET(self):
        parsed_path = urllib.parse.urlparse(self.path)
        
        # API Endpoints
        if parsed_path.path == '/api/workspace_info':
            self.send_response(200)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            
            # List stages
            stages = []
            stages_dir = os.path.join(preload_dir, 'stages')
            if os.path.exists(stages_dir):
                for f in os.listdir(stages_dir):
                    if f.endswith('.json'):
                        stages.append(f[:-5])
            
            # List songs (charts)
            songs = []
            data_dir = os.path.join(preload_dir, 'data')
            if os.path.exists(data_dir):
                for d in os.listdir(data_dir):
                    if os.path.isdir(os.path.join(data_dir, d)):
                        songs.append(d)
                        
            resp = {'stages': sorted(stages), 'songs': sorted(songs)}
            self.wfile.write(json.dumps(resp).encode('utf-8'))
            return
            
        elif parsed_path.path.startswith('/api/load_file'):
            qs = urllib.parse.parse_qs(parsed_path.query)
            if 'path' not in qs:
                self.send_error(400)
                return
            
            rel_path = qs['path'][0]
            # prevent directory traversal
            if '..' in rel_path:
                self.send_error(403)
                return
                
            full_path = os.path.join(preload_dir, rel_path)
            if not os.path.exists(full_path):
                shared_dir = os.path.join(project_dir, 'assets', 'shared')
                full_path = os.path.join(shared_dir, rel_path)
                if not os.path.exists(full_path):
                    self.send_error(404)
                    return
                
            self.send_response(200)
            self.send_header('Access-Control-Allow-Origin', '*')
            if full_path.endswith('.json'): self.send_header('Content-Type', 'application/json')
            elif full_path.endswith('.lua'): self.send_header('Content-Type', 'text/plain')
            elif full_path.endswith('.xml'): self.send_header('Content-Type', 'application/xml')
            elif full_path.endswith('.png'): self.send_header('Content-Type', 'image/png')
            else: self.send_header('Content-Type', 'application/octet-stream')
            self.end_headers()
            
            with open(full_path, 'rb') as f:
                self.wfile.write(f.read())
            return
            
        # Default handling (serves stage_editor.html)
        if self.path == '/':
            self.path = '/stage_editor.html'
        super().do_GET()

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()

    def do_POST(self):
        parsed_path = urllib.parse.urlparse(self.path)
        if parsed_path.path == '/api/save_stage':
            qs = urllib.parse.parse_qs(parsed_path.query)
            if 'name' not in qs:
                self.send_error(400)
                return
            
            stage_name = qs['name'][0]
            if '..' in stage_name:
                self.send_error(403)
                return
                
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            
            save_path = os.path.join(preload_dir, 'stages', f'{stage_name}.json')
            
            try:
                with open(save_path, 'wb') as f:
                    f.write(post_data)
                self.send_response(200)
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(b'{"status":"ok"}')
            except Exception as e:
                self.send_error(500, str(e))
            return
            
        self.send_error(404)

if __name__ == '__main__':
    with socketserver.TCPServer(("", PORT), EditorHandler) as httpd:
        print(f"Serving stage editor at http://localhost:{PORT}")
        webbrowser.open(f"http://localhost:{PORT}")
        httpd.serve_forever()
