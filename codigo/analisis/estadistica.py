"""Estadísticas de muestra para intervalos t de Student de la media."""

import math
from collections.abc import Iterable
from statistics import NormalDist, stdev

T_95 = {
    1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 7: 2.365,
    8: 2.306, 9: 2.262, 10: 2.228, 11: 2.201, 12: 2.179, 13: 2.160, 14: 2.145,
    15: 2.131, 16: 2.120, 17: 2.110, 18: 2.101, 19: 2.093, 20: 2.086, 21: 2.080,
    22: 2.074, 23: 2.069, 24: 2.064, 25: 2.060, 26: 2.056, 27: 2.052, 28: 2.048,
    29: 2.045, 30: 2.042,
}


def intervalo_media_ic95(valores: Iterable[float]) -> tuple[int, float, float, float, float]:
    """Devuelve n, media, desviación estándar e IC95% de la media."""
    muestra = [float(valor) for valor in valores if math.isfinite(float(valor))]
    n = len(muestra)
    if not n:
        return 0, math.nan, math.nan, math.nan, math.nan

    media = sum(muestra) / n
    if n < 2:
        return n, media, math.nan, math.nan, math.nan

    desviacion = stdev(muestra)
    df = n - 1
    if df in T_95:
        critico = T_95[df]
    else:
        z = NormalDist().inv_cdf(0.975)
        critico = (
            z
            + (z**3 + z) / (4 * df)
            + (5 * z**5 + 16 * z**3 + 3 * z) / (96 * df**2)
            + (3 * z**7 + 19 * z**5 + 17 * z**3 - 15 * z) / (384 * df**3)
        )
    margen = critico * desviacion / math.sqrt(n)
    return n, media, desviacion, media - margen, media + margen
