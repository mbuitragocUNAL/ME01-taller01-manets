# Datos y estadísticas de los experimentos

Estos archivos son los resultados agregados de las simulaciones ejecutadas para el informe. Cada
carpeta corresponde a un nivel experimental:

- `E1`: configuración base.
- `E2-densidad-baja` y `E2-densidad-alta`: 4 y 16 nodos por clúster (el nivel base, 8, está en E1).
- `E3-sombreado-cero` y `E3-sombreado-alto`: sombreado de 0 y 8 dB (el nivel base, 4 dB, está en E1).
- `E4-fusion-raw` y `E4-fusion-merge`: `zoneFusion=raw` y `zoneFusion=merge` (el nivel base, `none`, está en E1).

Cada carpeta contiene:

| Archivo | Contenido |
|---|---|
| `experimento.csv` | Una fila por réplica y método de localización; incluye movilidad, protocolo, disponibilidad, error y frescura. Son 360 filas por nivel. |
| `experimento_red.csv` | Una fila por réplica; incluye entregas y latencias de mensajes, consultas, edad/error de respuesta y bytes de aplicación. Son 120 filas por nivel. |
| `intervalos_confianza.csv` | Media, desviación estándar, IC95% y `n` válido para cada métrica y tratamiento. Son 708 filas por nivel. |
| `comando.txt` | Comando almacenado durante la ejecución para reproducir ese nivel. |

Se ejecutaron 10 réplicas para cada una de las 4 combinaciones de movilidad y los 3 protocolos,
con `simTime=600` s: 120 corridas por nivel, 840 corridas en total. Los CSV conservan las métricas
agregadas de cada réplica; no contienen los registros de paquetes y posiciones generados dentro de
cada corrida.

Los IC95% de `intervalos_confianza.csv` se calculan sobre réplicas válidas. Las filas con `n=0`
identifican tratamientos sin valores válidos para esa métrica; esto ocurre con más frecuencia en
algunos grupos de `E3-sombreado-cero`. En las tablas del informe se usa una comparación controlada
(AODV, `cluster+nodo`, C++ y trilateración), con `n=10` para cada cifra.

Para definiciones de columnas, comandos de generación, gráficos e interpretación, consultar
[`../experimentacion.md`](../experimentacion.md).
