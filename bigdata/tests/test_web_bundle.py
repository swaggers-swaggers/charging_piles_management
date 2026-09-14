from pathlib import Path
import re
import xml.etree.ElementTree as ET

from common import REPO


def test_embedded_dashboard_contains_all_runtime_assets():
    qrc = REPO / "code/server/server_web.qrc"
    aliases = {node.attrib["alias"] for node in ET.parse(qrc).findall(".//file")}
    html = (REPO / "code/web/index.html").read_text(encoding="utf-8")
    page_assets = set(re.findall(r'(?:src|href)="([^"#?]+)"', html))
    required = {"web/" + asset for asset in page_assets if not asset.startswith("data:")}
    required.update(
        "web/data/" + name + ".json"
        for name in (
            "overview",
            "stations",
            "station-ranking",
            "load-forecast",
            "user-demand",
            "data-quality",
            "pipeline-health",
        )
    )
    assert required <= aliases


def test_qrc_sources_exist():
    qrc = REPO / "code/server/server_web.qrc"
    for node in ET.parse(qrc).findall(".//file"):
        assert (qrc.parent / (node.text or "")).resolve().is_file()
