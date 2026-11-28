"""Run the native parser HTTP probe against a local HTTP or HTTPS server."""

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import ssl
import subprocess
from tempfile import TemporaryDirectory
from threading import Thread


class DocumentHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def send_document(self, status, body):
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/binary":
            self.send_document(200, b"a\0b")
        elif self.path == "/empty":
            self.send_document(204, b"")
        else:
            self.send_document(404, b"m" * 32768)

    def do_POST(self):
        length = int(self.headers["Content-Length"])
        body = self.rfile.read(length)
        expected = b"x" * 13 + b"\0" + b"x" * (32768 - 14)
        if self.path == "/post" and body == expected:
            self.send_document(200, b"r" * 32768)
        elif self.path in ("/missing", "/legacy") and body == b"a\0b":
            self.send_document(404, b"missing")
        else:
            self.send_document(400, b"unexpected request")

    def log_message(self, *args):
        pass


def generate_certificate(executable, directory):
    cert = directory / "localhost.pem"
    key = directory / "localhost-key.pem"
    subprocess.run(
        [executable, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
         "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
         "-keyout", str(key), "-out", str(cert)],
        check=True, capture_output=True,
    )
    subject_hash = subprocess.check_output(
        [executable, "x509", "-in", str(cert), "-subject_hash", "-noout"], text=True,
    ).strip()
    (directory / f"{subject_hash}.0").write_bytes(cert.read_bytes())
    return cert, key


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("probe", type=Path)
    parser.add_argument("--openssl", help="OpenSSL executable for a disposable loopback TLS certificate")
    parser.add_argument("--work-dir", type=Path, default=Path.cwd())
    args = parser.parse_args()

    with TemporaryDirectory(dir=args.work_dir) as directory:
        env = dict(os.environ)
        with ThreadingHTTPServer(("127.0.0.1", 0), DocumentHandler) as server:
            if args.openssl:
                cert, key = generate_certificate(args.openssl, Path(directory))
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                context.load_cert_chain(cert, key)
                server.socket = context.wrap_socket(server.socket, server_side=True)
                env["SSL_CERT_FILE"] = str(cert.resolve())
                env["SSL_CERT_DIR"] = str(cert.parent.resolve())
            thread = Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                subprocess.run(
                    [str(args.probe.resolve()), str(server.server_port), str(int(bool(args.openssl)))],
                    env=env, check=True, timeout=30,
                )
            finally:
                server.shutdown()
                thread.join()
    print("HTTPS backend passed" if args.openssl else "HTTP backend passed")


if __name__ == "__main__":
    main()
