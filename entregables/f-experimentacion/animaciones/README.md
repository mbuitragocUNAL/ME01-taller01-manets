# Trazas de animación para NetAnim

Se incluyen siete trazas XML compatibles con NetAnim y sus GIF correspondientes. Cada traza cubre
120 segundos simulados y usa la réplica visual `RngRun=1`, AODV y movilidad `cluster+nodo`. El nivel
del escenario cambia según el nombre del archivo:

| Traza XML | Escenario | Animación GIF |
|---|---|---|
| [`E1.xml`](./E1.xml) | Valores base | [`E1.gif`](./E1.gif) |
| [`E2-densidad-baja.xml`](./E2-densidad-baja.xml) | 4 nodos por clúster | [`E2-densidad-baja.gif`](./E2-densidad-baja.gif) |
| [`E2-densidad-alta.xml`](./E2-densidad-alta.xml) | 16 nodos por clúster | [`E2-densidad-alta.gif`](./E2-densidad-alta.gif) |
| [`E3-sombreado-cero.xml`](./E3-sombreado-cero.xml) | Sombreado de 0 dB | [`E3-sombreado-cero.gif`](./E3-sombreado-cero.gif) |
| [`E3-sombreado-alto.xml`](./E3-sombreado-alto.xml) | Sombreado de 8 dB | [`E3-sombreado-alto.gif`](./E3-sombreado-alto.gif) |
| [`E4-fusion-raw.xml`](./E4-fusion-raw.xml) | Fusión `raw` | [`E4-fusion-raw.gif`](./E4-fusion-raw.gif) |
| [`E4-fusion-merge.xml`](./E4-fusion-merge.xml) | Fusión `merge` | [`E4-fusion-merge.gif`](./E4-fusion-merge.gif) |

Para ver una traza, abre NetAnim, selecciona **File > Open**, y elige uno de estos archivos. Luego
pulsa **Play**. El collar rojo muestra la ubicación real; el marcador magenta corresponde a la
estimación de la zona sur y el cian, a la de la zona norte. Los marcadores de estimación aparecen
en gris en el centro de su zona hasta que haya una estimación válida; entonces siguen la posición
estimada y cambian a magenta o cian. También se muestran la topología y el movimiento de los nodos.
El registro de paquetes está desactivado; las métricas de localización por instante se guardan en
`estimaciones_red.csv` [13].
Si NetAnim aún no está instalado, sigue las instrucciones de instalación de la parte II del
[manual de usuario y técnico](../../e-manuales/manuales.md).

Los GIF permiten reproducir cada escena sin abrir NetAnim. Para regenerarlos desde WSL:

```bash
cd /mnt/c/Proyectos/ME01-taller01-manets/codigo
source /home/juanj/ME01-taller01-manets/entorno.sh
python analisis/animar_netanim.py
```

Estas animaciones son ejemplos cualitativos de una corrida corta y controlada; no representan los
promedios ni los IC95% de las 10 réplicas de 600 s del informe.
