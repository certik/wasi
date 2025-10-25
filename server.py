#!/usr/bin/env python3
"""Simple HTTP server for serving static files."""
import http.server
import socketserver
import os

PORT = 0  # Bind to 0 to auto-select a free port

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # Add CORS headers to allow cross-origin requests
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET')
        self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate')
        super().end_headers()

def main():
    with socketserver.TCPServer(("", PORT), MyHTTPRequestHandler) as httpd:
        actual_port = httpd.server_address[1]  # Get the auto-assigned port
        print(f"Server running at http://localhost:{actual_port}/")
        print(f"Serving files from: {os.getcwd()}")
        print()
        print(f"To view the game, open:")
        print()
        print(f"http://localhost:{actual_port}/gm.html")
        print()
        print("Press Ctrl+C to stop the server")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")
            httpd.shutdown()  # Explicit shutdown for cleaner exit

if __name__ == "__main__":
    main()
