# f. Experimentación y análisis de resultados

El PDF del informe, su fuente editable, los datos agregados y las figuras de los siete niveles
experimentales están incluidos en esta carpeta. Consulta el [informe PDF](./Taller01_MANETs_f_experimentacion.pdf)
o [`experimentacion.md`](./experimentacion.md); los CSV por réplica, intervalos y comandos exactos
están en [`datos/`](./datos/README.md). Las siete trazas XML y sus GIF con la posición real y las
estimaciones de NetAnim están en [`animaciones/`](./animaciones/README.md).

Los experimentos cubren movilidad, densidad, sombreado y fusión de zonas:

| # | Nombre | Archivo de parámetros | Qué cambia frente a `base.conf` | Qué prueba |
|---|---|---|---|---|
| E1 | Caso base | `codigo/escenarios/E1.conf` | — | efecto de la movilidad con valores por defecto |
| E2 | Densidad | `E2-densidad-baja.conf`, `E2-densidad-alta.conf` | 4 y 16 nodos por clúster (base: 8) | efecto de la cantidad de observadores |
| E3 | Sombreado | `E3-sombreado-cero.conf`, `E3-sombreado-alto.conf` | 0 y 8 dB (base: 4 dB) | efecto del ruido del canal |
| E4 | Fusión de zonas | `E4-fusion-raw.conf`, `E4-fusion-merge.conf` | `raw` y `merge` (base: `none`) | efecto de combinar las estimaciones de zona |

`analisis/experimento.py` cruza la movilidad de clúster con la de nodo (4 combinaciones) y con los
protocolos solicitados. Los niveles de densidad, sombreado y fusión se fijan en los archivos `.conf`
de [`codigo/escenarios/`](../../codigo/escenarios/).

Para producir los datos de un escenario, desde `codigo/` y con el entorno cargado:

```bash
python analisis/experimento.py --nombre E1 --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E1.conf
```

Eso deja en `codigo/resultados/E1/`:
- `experimento.csv`, con métricas de localización por método y réplica;
- `experimento_red.csv`, con las métricas de red y de consultas por réplica;
- `comando.txt`, con el comando exacto.

`analisis/graficas_comparativas.py` genera las figuras del informe y
`intervalos_confianza.csv` por escenario. Las copias de los resultados generados se encuentran en
[`datos/`](./datos/README.md). Los intervalos son del 95% para la media entre réplicas;
los valores no disponibles se excluyen y la columna `n` informa cuántas réplicas válidas se usaron.
Para una corrida suelta, `analisis/graficar.py` dibuja trayectorias y estimaciones; acepta `--salida`
y `--titulo` para personalizar la figura.

El informe completo y las instrucciones de generación están en
[`experimentacion.md`](./experimentacion.md). Los siete niveles representan 840 corridas
(4 movilidades × 3 protocolos × 10 réplicas por nivel).

Antes de sacar conclusiones, lean la sección "Qué declarar en el informe" del [README](../../README.md). Ahí
están los efectos del modelo que ya vimos y que pueden confundir el análisis.

El encabezado lleva los nombres de [`../autores.md`](../autores.md).
