"""Corre el diseño factorial (movilidad de clúster x movilidad de nodo x enrutamiento) con réplicas.

Cada combinación se corre con --RngRun = 1..R (réplicas independientes) y en paralelo. Para cada corrida:
  - localizar.analizar()  -> localización de referencia, fuera de línea, con todas las lecturas
  - red.resumir_red()     -> entrega, latencia y consultas a través de la MANET
Todo se junta en resultados/<nombre>/experimento.csv (una fila por corrida y método) y
resultados/<nombre>/experimento_red.csv (una fila por corrida).

Uso (desde codigo/):
  python analisis/experimento.py --nombre factorial --reps 10 [--estimador python] \
      --routing aodv olsr dsdv [-- --simTime=600 ...]

Lo que va después de "--" se pasa tal cual a collar-sim (mismos parámetros para todas las combinaciones).
"""

import argparse
import csv
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

import numpy as np

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
from localizar import METODOS, analizar  # noqa: E402
from red import resumir_red  # noqa: E402
from rutas import BINARIO, CODIGO, NS3_DIR, absolutizar  # noqa: E402
from estadistica import intervalo_media_ic95  # noqa: E402

COMBINACIONES = [
    # (etiqueta, clusterMobility, nodeMobility)
    ("estatico", 0, 0),
    ("solo-nodo", 0, 1),
    ("solo-cluster", 1, 0),
    ("cluster+nodo", 1, 1),
]


def correr(routing: str, etiqueta: str, cm: int, nm: int, rep: int, base: Path, extra: list[str],
           paso: float, ventana: float, estimador: str) -> tuple[list[dict], dict]:
    carpeta = base / routing / etiqueta / f"run{rep:02d}"
    # Los factores van después de `extra`: si un --conf trae clusterMobility, routing, etc., no los pisa.
    sim = [*extra, f"--clusterMobility={cm}", f"--nodeMobility={nm}", f"--routing={routing}",
           f"--RngRun={rep}", f"--outDir={carpeta}"]
    if estimador == "python":
        # Cada corrida crea su propia memoria compartida (nombre único), así que pueden ir en paralelo.
        cmd = [sys.executable, "-I", str(AQUI / "ns3ai_estimador.py"), "--", *sim]
    else:
        cmd = [str(BINARIO), *sim]
    subprocess.run(cmd, cwd=NS3_DIR, check=True, stdout=subprocess.DEVNULL)
    clave = {"routing": routing, "combinacion": etiqueta, "clusterMobility": cm, "nodeMobility": nm,
             "rep": rep, "estimador": estimador}
    loc = [{**clave, **r} for r in analizar(carpeta, paso, ventana)]
    red = {**clave, **resumir_red(carpeta)}
    return loc, red


def tabla(filas: list[dict], metrica: str, columnas: list[str], col_clave: str, filtro: dict) -> None:
    print(f"{'':<14}" + "".join(f"{c:>28}" for c in columnas))
    for etiqueta, _, _ in COMBINACIONES:
        celdas = []
        for c in columnas:
            v = np.array([f[metrica] for f in filas if f["combinacion"] == etiqueta and f[col_clave] == c
                          and all(f[k] == x for k, x in filtro.items())], dtype=float)
            n, media, _, inferior, superior = intervalo_media_ic95(v)
            celdas.append(
                f"{media:.2f} [{inferior:.2f}, {superior:.2f}]"
                if n > 1 else f"{media:.2f} [IC no disp.; n={n}]" if n else "—"
            )
        print(f"{etiqueta:<14}" + "".join(f"{c:>28}" for c in celdas))


def main() -> None:
    argv = sys.argv[1:]
    extra = []
    if "--" in argv:
        extra = absolutizar(argv[argv.index("--") + 1:])
        argv = argv[:argv.index("--")]
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--nombre", default="factorial", help="subcarpeta de resultados/")
    ap.add_argument("--reps", type=int, default=10, help="réplicas por combinación")
    ap.add_argument("--routing", nargs="+", default=["aodv"], choices=["aodv", "olsr", "dsdv"])
    ap.add_argument("--paso", type=float, default=5.0)
    ap.add_argument("--ventana", type=float, default=10.0)
    ap.add_argument("--estimador", default="cpp", choices=["cpp", "python"],
                    help="estimador de las cabezas de zona: cpp, o python por ns3-ai")
    ap.add_argument("--jobs", type=int, default=None,
                    help="corridas en paralelo (por defecto núcleos - 1; con python, la mitad: cada "
                         "corrida ocupa dos núcleos en espera activa)")
    ap.add_argument("--sin-compilar", action="store_true", help="no llamar ./ns3 build antes")
    args = ap.parse_args(argv)
    if args.jobs is None:
        nucleos = os.cpu_count() or 2
        args.jobs = max(1, (nucleos - 1) // (2 if args.estimador == "python" else 1))

    if not args.sin_compilar:
        subprocess.run(["./ns3", "build", "scratch/manets/collar-sim"], cwd=NS3_DIR, check=True,
                       stdout=subprocess.DEVNULL)

    base = CODIGO / "resultados" / args.nombre
    trabajos = [(rt, et, cm, nm, rep) for rt in args.routing for et, cm, nm in COMBINACIONES
                for rep in range(1, args.reps + 1)]
    filas_loc, filas_red = [], []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futuros = [pool.submit(correr, *t, base, extra, args.paso, args.ventana, args.estimador)
                   for t in trabajos]
        for i, fut in enumerate(as_completed(futuros), 1):
            loc, red = fut.result()
            filas_loc.extend(loc)
            filas_red.append(red)
            if i % 10 == 0 or i == len(trabajos):
                print(f"  {i}/{len(trabajos)} corridas", flush=True)

    orden = lambda f: (f["routing"], f["combinacion"], f["rep"])  # noqa: E731
    filas_loc.sort(key=lambda f: (*orden(f), f["metodo"]))
    filas_red.sort(key=orden)
    for nombre, filas in (("experimento.csv", filas_loc), ("experimento_red.csv", filas_red)):
        with open(base / nombre, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(filas[0].keys()))
            w.writeheader()
            w.writerows(filas)
    (base / "comando.txt").write_text(" ".join(sys.argv) + "\n")

    print(f"\n{base}  ({args.reps} réplicas; media [IC95% t de Student])")
    for rt in args.routing:
        print(f"\n[{rt}] error mediano de localización fuera de línea (m)")
        tabla(filas_loc, "error_mediana_m", list(METODOS), "metodo", {"routing": rt})
    for metrica, nombre in (("lecturas_entrega", "entrega lectura -> cabeza de zona"),
                            ("lecturas_latencia_s_mediana", "latencia mediana lectura -> cabeza de zona (s)"),
                            ("consultas_exito", "consultas contestadas con estimación"),
                            ("consultas_rtt_s_mediana", "rtt mediano de la consulta (s)"),
                            ("respuesta_edad_s_mediana", "edad mediana de la estimación recibida (s)"),
                            ("respuesta_error_m_mediana", "error mediano de la respuesta al dueño (m)")):
        print(f"\n{nombre}")
        tabla(filas_red, metrica, args.routing, "routing", {})


if __name__ == "__main__":
    main()
