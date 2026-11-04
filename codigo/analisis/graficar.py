"""Dibuja una corrida: trayectorias por clúster, el perro y las estimaciones de cada método.

Uso:
  python analisis/graficar.py resultados/<corrida> [--salida ruta.png] [--titulo texto]

Necesita estimaciones.csv. Por defecto escribe <corrida>/escena.png.
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

COLORES = {"0": "#1e6edc", "1": "#28aa5a", "2": "#7850c8", "3": "#dc9620"}
MARCAS = {"cercano": "x", "centroide": "^", "trilateracion": "o"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corrida", type=Path)
    parser.add_argument("--salida", type=Path, help="ruta del PNG (por defecto, corrida/escena.png)")
    parser.add_argument("--titulo", help="título alternativo para la figura")
    args = parser.parse_args()
    corrida = args.corrida
    salida = args.salida or corrida / "escena.png"

    nombres = {}
    with open(corrida / "nodos.csv") as f:
        for r in csv.DictReader(f):
            if r["cluster"] != "-1":
                nombres[r["cluster"]] = r["nombre_cluster"]

    trayectorias = defaultdict(list)
    with open(corrida / "posiciones.csv") as f:
        for r in csv.DictReader(f):
            trayectorias[(r["rol"], r["cluster"], r["nodo"])].append((float(r["x"]), float(r["y"])))

    fig, ax = plt.subplots(figsize=(11, 7))
    ax.axhline(150, color="0.6", lw=1, ls="--")
    ax.text(5, 290, "zona norte", color="0.4")
    ax.text(5, 5, "zona sur", color="0.4")
    vistos = set()
    for (rol, cluster, _), pts in trayectorias.items():
        xs, ys = zip(*pts)
        if rol == "collar":
            ax.plot(xs, ys, color="crimson", lw=2.5, label="perro (verdad)", zorder=5)
            ax.plot(xs[0], ys[0], "o", color="crimson", ms=8, zorder=6)
            continue
        etiqueta = nombres[cluster] if cluster not in vistos else None
        vistos.add(cluster)
        # marker: en las corridas estáticas la trayectoria es un solo punto
        ax.plot(xs, ys, color=COLORES.get(cluster, "0.5"), lw=0.6, alpha=0.5, label=etiqueta,
                marker=".", ms=3, markevery=[0])

    estimaciones = defaultdict(list)
    with open(corrida / "estimaciones.csv") as f:
        for r in csv.DictReader(f):
            if r["x"] != "nan":
                estimaciones[r["metodo"]].append((float(r["x"]), float(r["y"])))
    for metodo, pts in estimaciones.items():
        xs, ys = zip(*pts)
        ax.scatter(xs, ys, marker=MARCAS[metodo], s=18, alpha=0.6, label=f"estimación: {metodo}",
                   zorder=4)

    # Estimaciones hechas dentro de la red por cada cabeza de zona (si la corrida las tiene).
    if (corrida / "estimaciones_red.csv").exists():
        por_zona = defaultdict(list)
        with open(corrida / "estimaciones_red.csv") as f:
            for r in csv.DictReader(f):
                if r["valida"] == "1":
                    por_zona[r["zona"]].append((float(r["x"]), float(r["y"])))
        nombres_zona = {"0": "sur", "1": "norte"}
        for zona, pts in sorted(por_zona.items()):
            xs, ys = zip(*pts)
            ax.scatter(xs, ys, marker="s", s=22, facecolors="none", linewidths=1.0,
                       edgecolors="black" if zona == "0" else "dimgray",
                       label=f"red: cabeza zona {nombres_zona.get(zona, zona)}", zorder=4)

    ax.set_xlim(0, 500)
    ax.set_ylim(0, 300)
    ax.set_aspect("equal")
    ax.set_xlabel("x (m)")
    ax.set_ylabel("y (m)")
    ax.set_title(args.titulo or corrida.name)
    ax.legend(loc="upper left", bbox_to_anchor=(1.01, 1), fontsize=8)
    fig.tight_layout()
    salida.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(salida, dpi=130)
    plt.close(fig)
    print(salida)


if __name__ == "__main__":
    main()
