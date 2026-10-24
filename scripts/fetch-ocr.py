#!/usr/bin/env python3
"""拉取 MaaCommonAssets 固定的 PP-OCR v4 中文模型（Apache-2.0），带 sha256 校验。"""
import hashlib
import sys
import urllib.request
from pathlib import Path

REVISION = "dabcd4681ac990dc4361de26416d986abd80e4aa"
FILES = {
    "det.onnx": "767e663dded50813dca0abc9a49859e345e0f74a48b387661188f096a9211d74",
    "rec.onnx": "da4815ea386ee8083be319da8d20e70f4fdb0b6404ba0d2da7f675831e1b37fa",
    "keys.txt": "28b2362ad4ab2dc38769aa72feb535e3a9ddb3fd2a7585a05920e6393b1dc7f7",
}
ROOT = Path(__file__).resolve().parent.parent / "assets" / "model" / "ocr"


def main() -> int:
    ROOT.mkdir(parents=True, exist_ok=True)
    for name, digest in FILES.items():
        path = ROOT / name
        if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == digest:
            print(f"已就绪 {name}")
            continue
        url = f"https://raw.githubusercontent.com/MaaXYZ/MaaCommonAssets/{REVISION}/OCR/ppocr_v4/zh_cn/{name}"
        print(f"下载 {name}")
        data = urllib.request.urlopen(url, timeout=60).read()
        if hashlib.sha256(data).hexdigest() != digest:
            print(f"校验失败 {name}", file=sys.stderr)
            return 1
        temp = path.with_suffix(path.suffix + ".part")
        temp.write_bytes(data)
        temp.replace(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
