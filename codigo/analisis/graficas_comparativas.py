"""Genera gráficas comparativas a partir de los CSV agregados de los escenarios.

Uso desde codigo/:
  python analisis/graficas_comparativas.py
  python analisis/graficas_comparativas.py --resultados resultados --salida ../entregables/f-experimentacion/figuras

Cada punto muestra la media entre réplicas y cada barra su intervalo de confianza del 95%.
También se escribe intervalos_confianza.csv dentro de cada escenario.
"""

import argparse
import csv
import math
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

CODIGO = Path(__file__).resolve().parents[1]
from estadistica import intervalo_media_ic95  # noqa: E402

COMBINACIONES = ("estatico", "solo-nodo", "solo-cluster", "cluster+nodo")
ETIQUETAS_MOVILIDAD = ("Estático", "Solo nodo", "Solo clúster", "Clúster + nodo")
RUTAS = ("aodv", "olsr", "dsdv")
COLORES_MOVILIDAD = ("#4c78a8", "#f58518", "#54a24b", "#e45756")
COLORES_METODO = {
    "cercano": "#4c78a8",
    "centroide": "#f58518",
    "trilateracion": "#54a24b",
}
ESCENARIOS = (
    "E1", "E2-densidad-baja", "E2-densidad-alta", "E3-sombreado-cero",
    "E3-sombreado-alto", "E4-fusion-raw", "E4-fusion-merge",
)
CLAVES_LOCALIZACION = (
    "routing", "combinacion", "clusterMobility", "nodeMobility", "estimador", "metodo",
)
CLAVES_RED = ("routing", "combinacion", "clusterMobility", "nodeMobility", "estimador")


def leer_csv(ruta: Path) -> list[dict[str, str]]:
    if not ruta.is_file():
        raise FileNotFoundError(f"No se encontró el archivo requerido: {ruta}")
    with ruta.open(newline="", encoding="utf-8") as archivo:
        return list(csv.DictReader(archivo))


def valores(
    filas: list[dict[str, str]],
    metrica: str,
    **filtros: str,
) -> np.ndarray:
    muestra = []
    for fila in filas:
        if any(fila.get(clave) != valor for clave, valor in filtros.items()):
            continue
        valor = float(fila[metrica])
        if math.isfinite(valor):
            muestra.append(valor)
    return np.asarray(muestra, dtype=float)


def guardar_intervalos(resultados: Path) -> None:
    for escenario in ESCENARIOS:
        carpeta = resultados / escenario
        resumen = []
        for nombre_archivo, claves, origen in (
            ("experimento.csv", CLAVES_LOCALIZACION, "localizacion"),
            ("experimento_red.csv", CLAVES_RED, "red"),
        ):
            filas = leer_csv(carpeta / nombre_archivo)
            grupos: dict[tuple[str, ...], list[dict[str, str]]] = {}
            for fila in filas:
                clave = tuple(fila[columna] for columna in claves)
                grupos.setdefault(clave, []).append(fila)
            metricas = []
            for columna in filas[0]:
                if columna in claves or columna == "rep":
                    continue
                if any(_es_numero(fila[columna]) for fila in filas):
                    metricas.append(columna)
            for clave, grupo in grupos.items():
                for metrica in metricas:
                    muestra = valores(grupo, metrica)
                    n, media, desviacion, inferior, superior = intervalo_media_ic95(muestra)
                    resumen.append({
                        "origen": origen,
                        "metrica": metrica,
                        **dict(zip(claves, clave)),
                        "n": n,
                        "media": media,
                        "desviacion_estandar": desviacion,
                        "ic95_inferior": inferior,
                        "ic95_superior": superior,
                    })

        columnas_salida = (
            "origen", "metrica", *CLAVES_LOCALIZACION,
            "n", "media", "desviacion_estandar", "ic95_inferior", "ic95_superior",
        )
        with (carpeta / "intervalos_confianza.csv").open(
            "w", newline="", encoding="utf-8"
        ) as archivo:
            writer = csv.DictWriter(archivo, fieldnames=columnas_salida)
            writer.writeheader()
            writer.writerows(resumen)
        print(carpeta / "intervalos_confianza.csv")


def _es_numero(valor: str) -> bool:
    try:
        float(valor)
    except ValueError:
        return False
    return True


def errorbar(
    ax: plt.Axes,
    x: float,
    muestra: np.ndarray,
    *,
    etiqueta: str | None = None,
    color: str | None = None,
    marcador: str = "o",
    desplazamiento: float = 0.0,
) -> None:
    n, media, _, inferior, superior = intervalo_media_ic95(muestra)
    if math.isfinite(media):
        yerr = None
        if n > 1:
            yerr = np.asarray([[media - inferior], [superior - media]])
        ax.errorbar(
            x + desplazamiento,
            media,
            yerr=yerr,
            label=etiqueta,
            color=color,
            marker=marcador,
            capsize=3,
            linewidth=1.5,
        )


def terminar_figura(fig: plt.Figure, ruta: Path, titulo: str) -> None:
    fig.suptitle(
        f"{titulo}\nMedia e IC del 95% (t de Student); valores no disponibles omitidos",
        fontsize=13,
    )
    fig.tight_layout(rect=(0, 0.08, 1, 0.91))
    fig.legend(*fig.axes[0].get_legend_handles_labels(), loc="lower center", ncol=4, frameon=False)
    fig.savefig(ruta, dpi=170, bbox_inches="tight")
    plt.close(fig)
    print(ruta)


def grafica_e1_localizacion(resultados: Path, salida: Path) -> None:
    filas = leer_csv(resultados / "E1" / "experimento.csv")
    fig, ejes = plt.subplots(1, 3, figsize=(14, 5), sharey=True)
    offsets = {"cercano": -0.18, "centroide": 0.0, "trilateracion": 0.18}
    for ax, routing in zip(ejes, RUTAS):
        for metodo, color in COLORES_METODO.items():
            for indice, combinacion in enumerate(COMBINACIONES):
                muestra = valores(
                    filas, "error_mediana_m", routing=routing, combinacion=combinacion, metodo=metodo
                )
                errorbar(
                    ax,
                    indice,
                    muestra,
                    etiqueta=metodo if indice == 0 else None,
                    color=color,
                    marcador="o",
                    desplazamiento=offsets[metodo],
                )
        ax.set_title(routing.upper())
        ax.set_xticks(range(len(ETIQUETAS_MOVILIDAD)), ETIQUETAS_MOVILIDAD, rotation=25, ha="right")
        ax.grid(axis="y", alpha=0.25)
        ax.set_xlabel("Patrón de movilidad")
    ejes[0].set_ylabel("Error mediano de localización (m)")
    terminar_figura(fig, salida / "E1-localizacion.png", "E1 — Error por movilidad, protocolo y estimador")


def grafica_e1_red(resultados: Path, salida: Path) -> None:
    filas = leer_csv(resultados / "E1" / "experimento_red.csv")
    metricas = (
        ("lecturas_entrega", "Entrega de lecturas", (0, 1)),
        ("consultas_exito", "Consultas contestadas con estimación", (0, 1)),
        ("respuesta_error_m_mediana", "Error mediano de respuesta (m)", None),
    )
    fig, ejes = plt.subplots(1, 3, figsize=(14, 5))
    for ax, (metrica, titulo, limites) in zip(ejes, metricas):
        for indice_ruta, (routing, color) in enumerate(
            zip(RUTAS, ("#4c78a8", "#f58518", "#54a24b"))
        ):
            for indice, combinacion in enumerate(COMBINACIONES):
                muestra = valores(filas, metrica, routing=routing, combinacion=combinacion)
                errorbar(
                    ax,
                    indice,
                    muestra,
                    etiqueta=routing.upper() if indice == 0 else None,
                    color=color,
                    desplazamiento=(indice_ruta - 1) * 0.12,
                )
        ax.set_title(titulo)
        ax.set_xticks(range(len(ETIQUETAS_MOVILIDAD)), ETIQUETAS_MOVILIDAD, rotation=25, ha="right")
        ax.set_xlabel("Patrón de movilidad")
        if limites:
            ax.set_ylim(*limites)
        ax.grid(axis="y", alpha=0.25)
    terminar_figura(fig, salida / "E1-red.png", "E1 — Desempeño de red y consultas")


def grafica_densidad(resultados: Path, salida: Path) -> None:
    escenarios = ((4, "E2-densidad-baja"), (8, "E1"), (16, "E2-densidad-alta"))
    datos = {densidad: leer_csv(resultados / nombre / "experimento.csv")
             for densidad, nombre in escenarios}
    metricas = (
        ("error_mediana_m", "Error de trilateración (m)"),
        ("disponibilidad", "Disponibilidad"),
    )
    fig, ejes = plt.subplots(2, 3, figsize=(15, 8), sharex=True)
    for columna, routing in enumerate(RUTAS):
        for fila, (metrica, titulo) in enumerate(metricas):
            ax = ejes[fila, columna]
            for indice_mov, combinacion in enumerate(COMBINACIONES):
                for indice_densidad, (densidad, _) in enumerate(escenarios):
                    muestra = valores(
                        datos[densidad],
                        metrica,
                        routing=routing,
                        combinacion=combinacion,
                        metodo="trilateracion",
                    )
                    errorbar(
                        ax,
                        indice_densidad,
                        muestra,
                        etiqueta=ETIQUETAS_MOVILIDAD[indice_mov]
                        if indice_densidad == 0 else None,
                        color=COLORES_MOVILIDAD[indice_mov],
                        desplazamiento=(indice_mov - 1.5) * 0.12,
                    )
            ax.set_title(f"{routing.upper()} — {titulo}" if fila == 0 else routing.upper())
            ax.set_xticks(range(3), ("4", "8 (base)", "16"))
            ax.grid(axis="y", alpha=0.25)
            if fila == 1:
                ax.set_xlabel("Nodos por clúster")
                ax.set_ylim(0, 1)
            if columna == 0:
                ax.set_ylabel(titulo)
    terminar_figura(fig, salida / "E2-densidad.png", "E2 — Efecto de la densidad")


def grafica_sombreado(resultados: Path, salida: Path) -> None:
    escenarios = ((0, "E3-sombreado-cero"), (4, "E1"), (8, "E3-sombreado-alto"))
    datos = {sigma: leer_csv(resultados / nombre / "experimento.csv")
             for sigma, nombre in escenarios}
    metricas = (
        ("error_mediana_m", "Error trilateración (m)"),
        ("frescura_media_s", "Frescura (s; escala log)"),
    )
    fig, ejes = plt.subplots(2, 3, figsize=(15, 8), sharex=True)
    for columna, routing in enumerate(RUTAS):
        for fila, (metrica, titulo) in enumerate(metricas):
            ax = ejes[fila, columna]
            for indice_mov, combinacion in enumerate(COMBINACIONES):
                for indice_sigma, (sigma, _) in enumerate(escenarios):
                    muestra = valores(
                        datos[sigma],
                        metrica,
                        routing=routing,
                        combinacion=combinacion,
                        metodo="trilateracion",
                    )
                    errorbar(
                        ax,
                        indice_sigma,
                        muestra,
                        etiqueta=ETIQUETAS_MOVILIDAD[indice_mov]
                        if indice_sigma == 0 else None,
                        color=COLORES_MOVILIDAD[indice_mov],
                        desplazamiento=(indice_mov - 1.5) * 0.12,
                    )
            ax.set_title(f"{routing.upper()} — {titulo}" if fila == 0 else routing.upper())
            ax.set_xticks(range(3), ("0", "4 (base)", "8"))
            ax.grid(axis="y", alpha=0.25)
            if fila == 1:
                ax.set_yscale("log")
                ax.set_ylim(bottom=0.1)
            if fila == 1:
                ax.set_xlabel("Desviación del sombreado (dB)")
            if columna == 0:
                ax.set_ylabel(titulo)
    terminar_figura(fig, salida / "E3-sombreado.png", "E3 — Efecto del sombreado")


def grafica_fusion(resultados: Path, salida: Path) -> None:
    escenarios = (("none", "E1"), ("raw", "E4-fusion-raw"), ("merge", "E4-fusion-merge"))
    datos = {fusion: leer_csv(resultados / nombre / "experimento_red.csv")
             for fusion, nombre in escenarios}
    metricas = (
        ("respuesta_error_m_mediana", "Error mediano de respuesta (m)"),
        ("consultas_exito", "Consultas con estimación"),
        ("bytes_aplicacion", "Bytes de aplicación intercambiados"),
    )
    fig, ejes = plt.subplots(3, 3, figsize=(15, 10), sharex=True)
    for columna, routing in enumerate(RUTAS):
        for fila, (metrica, titulo) in enumerate(metricas):
            ax = ejes[fila, columna]
            for indice_mov, combinacion in enumerate(COMBINACIONES):
                for indice_fusion, (fusion, _) in enumerate(escenarios):
                    muestra = valores(
                        datos[fusion], metrica, routing=routing, combinacion=combinacion
                    )
                    errorbar(
                        ax,
                        indice_fusion,
                        muestra,
                        etiqueta=ETIQUETAS_MOVILIDAD[indice_mov]
                        if indice_fusion == 0 else None,
                        color=COLORES_MOVILIDAD[indice_mov],
                        desplazamiento=(indice_mov - 1.5) * 0.12,
                    )
            ax.set_title(f"{routing.upper()} — {titulo}" if fila == 0 else routing.upper())
            ax.set_xticks(range(3), ("none", "raw", "merge"))
            ax.grid(axis="y", alpha=0.25)
            if fila == 2:
                ax.set_xlabel("Fusión de zonas")
            if fila == 1:
                ax.set_ylim(0, 1)
            if columna == 0:
                ax.set_ylabel(titulo)
    terminar_figura(fig, salida / "E4-fusion.png", "E4 — Calidad y costo de la fusión de zonas")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resultados", type=Path, default=CODIGO / "resultados",
                        help="carpeta con los resultados agregados E1–E4")
    parser.add_argument(
        "--salida",
        type=Path,
        default=CODIGO.parent / "entregables" / "f-experimentacion" / "figuras",
        help="carpeta de salida para los PNG",
    )
    args = parser.parse_args()
    args.salida.mkdir(parents=True, exist_ok=True)

    guardar_intervalos(args.resultados)
    grafica_e1_localizacion(args.resultados, args.salida)
    grafica_e1_red(args.resultados, args.salida)
    grafica_densidad(args.resultados, args.salida)
    grafica_sombreado(args.resultados, args.salida)
    grafica_fusion(args.resultados, args.salida)


if __name__ == "__main__":
    main()
