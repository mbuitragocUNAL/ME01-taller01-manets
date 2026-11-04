"""Convierte trazas XML de NetAnim en GIFs con ubicación real y estimaciones zonales.

Uso desde codigo/:
  python analisis/animar_netanim.py
  python analisis/animar_netanim.py --entrada ruta/a/trazas --salida ruta/a/gifs
"""

import argparse
import math
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
ANIMACIONES = ROOT / "entregables" / "f-experimentacion" / "animaciones"
ESCENARIOS = {
    "E1": "Caso base",
    "E2-densidad-baja": "Densidad baja",
    "E2-densidad-alta": "Densidad alta",
    "E3-sombreado-cero": "Sombreado 0 dB",
    "E3-sombreado-alto": "Sombreado 8 dB",
    "E4-fusion-raw": "Fusión raw",
    "E4-fusion-merge": "Fusión merge",
}
COLORS = {
    "collar": (230, 20, 20),
    "estimate-none": (130, 130, 130),
    "estimate-south": (255, 0, 255),
    "estimate-north": (0, 210, 210),
}
CLUSTER_NAMES = ("Bus norte", "Plaza", "Bus sur", "Paseador")

WIDTH, HEIGHT = 1120, 640
PLOT = (70, 105, 830, 561)
FRAME_STEP = 0.5
FRAME_DURATION_MS = 80


def load_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = (
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
        Path("C:/Windows/Fonts/arial.ttf"),
    )
    for candidate in candidates:
        if candidate.is_file():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


def color_of(event: ET.Element, fallback: tuple[int, int, int]) -> tuple[int, int, int]:
    return tuple(int(event.get(channel, fallback[index])) for index, channel in enumerate(("r", "g", "b")))


def meter_to_pixel(x: float, y: float) -> tuple[int, int]:
    left, top, right, bottom = PLOT
    px = left + x / 500.0 * (right - left)
    py = bottom - y / 300.0 * (bottom - top)
    return round(px), round(py)


def draw_marker(
    draw: ImageDraw.ImageDraw,
    x: float,
    y: float,
    color: tuple[int, int, int],
    label: str,
    font: ImageFont.FreeTypeFont | ImageFont.ImageFont,
) -> None:
    left, top, right, bottom = PLOT
    clipped_x = min(max(x, 0.0), 500.0)
    clipped_y = min(max(y, 0.0), 300.0)
    px, py = meter_to_pixel(clipped_x, clipped_y)
    radius = 8
    draw.polygon(
        ((px, py - radius), (px + radius, py), (px, py + radius), (px - radius, py)),
        fill=color,
        outline=(30, 30, 30),
    )
    text_x = px + 10 if px < right - 70 else px - 70
    text_y = max(top, min(py - 18, bottom - 18))
    draw.text((text_x, text_y), label, fill=(35, 35, 35), font=font)
    if x < 0 or x > 500 or y < 0 or y > 300:
        draw.text((text_x, text_y + 15), "fuera del área", fill=(110, 30, 30), font=font)


def render_frame(
    time_s: float,
    nodes: dict[int, dict[str, object]],
    title: str,
    font: ImageFont.FreeTypeFont | ImageFont.ImageFont,
    small_font: ImageFont.FreeTypeFont | ImageFont.ImageFont,
) -> Image.Image:
    image = Image.new("RGB", (WIDTH, HEIGHT), "white")
    draw = ImageDraw.Draw(image)
    left, top, right, bottom = PLOT

    draw.text((left, 24), title, fill=(24, 54, 93), font=font)
    draw.text((left, 62), f"Tiempo simulado: {time_s:05.1f} s", fill=(60, 60, 60), font=small_font)

    for x in range(0, 501, 100):
        px, _ = meter_to_pixel(x, 0)
        draw.line((px, top, px, bottom), fill=(220, 225, 230), width=1)
        draw.text((px - 8, bottom + 7), str(x), fill=(80, 80, 80), font=small_font)
    for y in range(0, 301, 50):
        _, py = meter_to_pixel(0, y)
        draw.line((left, py, right, py), fill=(220, 225, 230), width=1)
        draw.text((left - 34, py - 8), str(y), fill=(80, 80, 80), font=small_font)
    _, middle_y = meter_to_pixel(0, 150)
    draw.line((left, middle_y, right, middle_y), fill=(110, 120, 130), width=2)
    draw.rectangle((left, top, right, bottom), outline=(100, 110, 120), width=2)
    draw.text((left + 8, top + 8), "Zona norte", fill=(95, 105, 115), font=small_font)
    draw.text((left + 8, middle_y + 7), "Zona sur", fill=(95, 105, 115), font=small_font)

    for node_id, node in nodes.items():
        x, y = node["position"]
        description = str(node["description"])
        color = node["color"]
        if description == "REAL":
            px, py = meter_to_pixel(x, y)
            draw.ellipse((px - 8, py - 8, px + 8, py + 8), fill=color, outline=(30, 30, 30), width=2)
            draw.ellipse((px - 2, py - 2, px + 2, py + 2), fill="white")
            draw.text((px + 11, py - 10), "REAL", fill=(35, 35, 35), font=small_font)
        elif description in ("EST-S", "EST-N", "EST-S sin dato", "EST-N sin dato"):
            label = "EST-S" if description.startswith("EST-S") else "EST-N"
            draw_marker(draw, x, y, color, label, small_font)
        else:
            px, py = meter_to_pixel(x, y)
            radius = 5 if description in ("HZ-S", "HZ-N", "D") else 3
            draw.ellipse((px - radius, py - radius, px + radius, py + radius), fill=color, outline=(30, 30, 30))
            if description in ("HZ-S", "HZ-N", "D"):
                draw.text((px + 7, py - 8), description, fill=(35, 35, 35), font=small_font)

    legend_x, legend_y = 865, 116
    draw.text((legend_x, legend_y), "Leyenda", fill=(24, 54, 93), font=font)
    legend = (
        (COLORS["collar"], "Collar: posición real"),
        (COLORS["estimate-south"], "Estimación zona sur"),
        (COLORS["estimate-north"], "Estimación zona norte"),
        (COLORS["estimate-none"], "Estimación no disponible"),
    )
    for index, (color, label) in enumerate(legend):
        y = legend_y + 48 + index * 36
        draw.ellipse((legend_x, y, legend_x + 14, y + 14), fill=color, outline=(40, 40, 40))
        draw.text((legend_x + 22, y - 2), label, fill=(45, 45, 45), font=small_font)

    draw.text((legend_x, 330), "Clústeres", fill=(24, 54, 93), font=font)
    for index, name in enumerate(CLUSTER_NAMES):
        node = next((value for value in nodes.values() if value["cluster"] == index), None)
        color = node["color"] if node else (110, 110, 110)
        y = 374 + index * 32
        draw.ellipse((legend_x, y, legend_x + 12, y + 12), fill=color, outline=(40, 40, 40))
        draw.text((legend_x + 20, y - 3), name, fill=(45, 45, 45), font=small_font)

    return image


def convert(xml_path: Path, output_path: Path) -> tuple[int, int]:
    root = ET.parse(xml_path).getroot()
    if root.tag != "anim" or root.get("filetype") != "animation":
        raise ValueError(f"No es una traza NetAnim compatible: {xml_path}")

    nodes: dict[int, dict[str, object]] = {}
    for element in root.findall("node"):
        node_id = int(element.attrib["id"])
        nodes[node_id] = {
            "position": (float(element.attrib["locX"]), float(element.attrib["locY"])),
            "color": (255, 0, 0),
            "description": "",
        }

    events = sorted(root.findall("nu"), key=lambda event: float(event.get("t", "0")))
    for event in events:
        node_id = int(event.attrib["id"])
        description = event.get("descr", "")
        if event.get("p") == "d" and description in ("REAL", "EST-S", "EST-N"):
            nodes[node_id]["description"] = description
            if description == "REAL":
                nodes[node_id]["role"] = "collar"
            else:
                nodes[node_id]["role"] = "estimate"
                nodes[node_id]["cluster"] = -1

    # Infer cluster membership from node ordering and the two final estimate-marker nodes.
    markers = sorted(node_id for node_id, node in nodes.items() if node.get("role") == "estimate")
    collar = next(node_id for node_id, node in nodes.items() if node.get("role") == "collar")
    nodes[collar]["cluster"] = -1
    node_count = collar
    nodes_per_cluster, remainder = divmod(node_count, len(CLUSTER_NAMES))
    if remainder:
        raise ValueError(f"El número de nodos no permite inferir los clústeres en {xml_path}")
    for node_id, node in nodes.items():
        if node_id < collar:
            cluster = min(node_id // nodes_per_cluster, len(CLUSTER_NAMES) - 1)
            node["cluster"] = cluster
            node["color"] = (
                (30, 110, 220), (40, 170, 90), (120, 80, 200), (220, 150, 30)
            )[cluster]
        elif node_id == collar:
            node["color"] = COLORS["collar"]
        elif node_id in markers:
            node["cluster"] = -1
            node["color"] = COLORS["estimate-none"]

    updates = []
    for event in events:
        time_s = float(event.get("t", "0"))
        node_id = int(event.attrib["id"])
        if node_id not in nodes:
            continue
        kind = event.get("p")
        if kind == "p":
            updates.append((time_s, node_id, "position", (float(event.attrib["x"]), float(event.attrib["y"]))))
        elif kind == "c":
            updates.append((time_s, node_id, "color", color_of(event, nodes[node_id]["color"])))
        elif kind == "d":
            updates.append((time_s, node_id, "description", event.get("descr", "")))

    if not markers or len(markers) != 2 or not updates:
        raise ValueError(f"La traza no contiene los nodos y estimaciones esperados: {xml_path}")

    final_time = max(time_s for time_s, *_ in updates)
    times = [i * FRAME_STEP for i in range(math.ceil(final_time / FRAME_STEP) + 1)]
    font, small_font = load_font(23), load_font(15)
    frames = []
    update_index = 0
    title = f"{xml_path.stem} — {ESCENARIOS.get(xml_path.stem, xml_path.stem)}"
    for time_s in times:
        while update_index < len(updates) and updates[update_index][0] <= time_s:
            _, node_id, key, value = updates[update_index]
            nodes[node_id][key] = value
            update_index += 1
        frames.append(render_frame(time_s, nodes, title, font, small_font))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(
        output_path,
        save_all=True,
        append_images=frames[1:],
        duration=FRAME_DURATION_MS,
        loop=0,
        optimize=True,
        disposal=2,
    )
    return len(frames), final_time


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--entrada", type=Path, default=ANIMACIONES, help="carpeta con trazas XML")
    parser.add_argument("--salida", type=Path, default=ANIMACIONES, help="carpeta para los GIF")
    args = parser.parse_args()

    xml_files = sorted(args.entrada.glob("*.xml"))
    if not xml_files:
        raise FileNotFoundError(f"No se encontraron trazas XML en {args.entrada}")
    for xml_path in xml_files:
        output_path = args.salida / f"{xml_path.stem}.gif"
        frame_count, final_time = convert(xml_path, output_path)
        print(f"{output_path}: {frame_count} cuadros, {final_time:.1f} s simulados")


if __name__ == "__main__":
    main()
