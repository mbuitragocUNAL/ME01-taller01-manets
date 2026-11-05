<div align="center">

# Experimentación y análisis de resultados

**Taller 01: Simulación de MANETs**

**ME01 — Modelos estocásticos y simulación en computación y comunicaciones**

**Universidad Nacional de Colombia**
</div>

Este informe presenta los siete niveles experimentales ejecutados, las estadísticas descriptivas y
los intervalos de confianza calculados entre réplicas. Los CSV agregados por réplica y los comandos
efectivamente usados se adjuntan en [`datos/`](./datos/README.md).

## 1. Diseño del experimento

El experimento responde la pregunta del literal b: cómo afecta el patrón de movilidad al error y a la
frescura de la localización por RSSI. Se usa un diseño factorial con réplicas, descrito en el
literal c (Sección 8) y automatizado en `analisis/experimento.py`.

**Factores y niveles.**

| Factor | Niveles | Cómo se controla |
|---|---|---|
| Movilidad de clúster | sí / no (`clusterMobility`) | lo cruza `experimento.py` |
| Movilidad de nodo | sí / no (`nodeMobility`) | lo cruza `experimento.py` |
| Protocolo de enrutamiento | `aodv`, `olsr`, `dsdv` | opción `--routing` |
| Densidad | 4, 8 (base), 16 nodos por clúster | `nodesPerCluster` en el `.conf` |
| Sombreado | 0, 4 (base), 8 dB | `shadowSigmaDb` en el `.conf` |
| Fusión de zonas | `none`, `raw`, `merge` | `zoneFusion` en el `.conf` |

La combinación de movilidad de clúster y de nodo da los cuatro escenarios de movilidad: estático,
solo-nodo, solo-clúster y clúster+nodo.

**Réplicas y semillas.** Cada combinación de movilidad y protocolo se corrió 10 veces con
`--RngRun` distinto. Los intervalos de confianza del 95% de la media se calcularon entre réplicas.

**Duración.** Cada corrida simula 600 s. Los siete niveles suman 7 × 4 combinaciones de movilidad ×
3 protocolos × 10 réplicas = **840 corridas**.

**Variables de respuesta.** Error de localización (m), frescura o edad de la estimación (s) y tasa de
consultas respondidas; su origen en los CSV está en el literal b (Sección 5.2).

**Fundamento de los factores.** Se comparan AODV, protocolo reactivo bajo demanda [6]; OLSR, protocolo
proactivo que usa relevos multipunto [7]; y DSDV, vector de distancias con números de secuencia [8].
La movilidad se varía porque la elección del modelo de movilidad puede alterar el rendimiento
observado de los protocolos MANET [10]. Para el canal, el modelo de referencia es pérdida log-distancia
con sombreado aleatorio en dB [9]; ns-3 documenta el modelo de propagación y sus parámetros [12].
Los valores concretos usados aquí (`pathLossExp=2.7`, `shadowSigmaDb=0`, 4 u 8 dB) son niveles de
este experimento, no valores universales ni una calibración de campo.

## 2. Escenarios

Se estudian cuatro escenarios; E2, E3 y E4 tienen dos niveles alternativos además del nivel base E1.
Esto permite comprobar si la respuesta cambia al variar densidad, sombreado o fusión de zonas. En
cada nivel, `experimento.py` cruza las cuatro combinaciones de movilidad con los tres protocolos.

| # | Nombre | Archivo(s) de parámetros | Qué cambia frente a `base.conf` | Qué prueba |
|---|---|---|---|---|
| E1 | Caso base | `E1.conf` | nada (valores por defecto) | el efecto de la movilidad en condiciones por defecto |
| E2 | Densidad | `E2-densidad-baja.conf` (`nodesPerCluster=4`), `E2-densidad-alta.conf` (`=16`) | observadores por clúster (base: 8) | si más observadores compensan la mala geometría del bus |
| E3 | Sombreado | `E3-sombreado-cero.conf` (`shadowSigmaDb=0`), `E3-sombreado-alto.conf` (`=8`) | ruido del canal (base: 4 dB) | si el patrón de movilidad que gana cambia con el ruido del canal |
| E4 | Fusión de zonas | `E4-fusion-raw.conf`, `E4-fusion-merge.conf` (el nivel `none` es E1) | `zoneFusion` (base: none) | si la forma de combinar las dos zonas altera la conclusión |

Los archivos `.conf` están en `codigo/escenarios/`. Cada uno trae un comentario con su propósito y el
comando para correrlo. E2, E3 y E4 tienen más de un nivel, de modo que cada nivel es una corrida
aparte; `experimento.py` fija un solo valor del `.conf` por corrida y lo cruza con las movilidades y
los protocolos.

### Cómo generar los datos

Desde `codigo/`, con el entorno cargado (`source entorno.sh`):

```bash
# E1 — caso base
python analisis/experimento.py --nombre E1 --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E1.conf

# E2 — densidad (dos niveles)
python analisis/experimento.py --nombre E2-densidad-baja --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E2-densidad-baja.conf
python analisis/experimento.py --nombre E2-densidad-alta --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E2-densidad-alta.conf

# E3 — sombreado (dos niveles)
python analisis/experimento.py --nombre E3-sombreado-cero --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E3-sombreado-cero.conf
python analisis/experimento.py --nombre E3-sombreado-alto --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E3-sombreado-alto.conf

# E4 — fusión de zonas (none ya es E1; raw y merge son niveles adicionales)
python analisis/experimento.py --nombre E4-fusion-raw   --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E4-fusion-raw.conf
python analisis/experimento.py --nombre E4-fusion-merge --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E4-fusion-merge.conf
```

Cada comando deja en `codigo/resultados/<nombre>/`: `experimento.csv` (error de localización por
método y réplica), `experimento_red.csv` (métricas de red y de consultas por réplica) y `comando.txt`
(el comando exacto, para reproducir). Para la figura de una corrida suelta:

```bash
python analisis/graficar.py resultados/<carpeta_de_una_corrida>
```

Cada comando ejecuta 120 corridas de 600 s. Los siete conjuntos de resultados se incluyen en
[`datos/`](./datos/README.md); cada uno contiene las filas por réplica, los intervalos calculados y
el comando guardado durante la ejecución.

## 3. Resultados por escenario

Las tablas presentan la media entre réplicas y, entre corchetes, su IC95% (límite inferior–superior).
En particular, los errores y edades medianos son primero calculados dentro de cada corrida y luego
promediados entre réplicas; no son la mediana de todas las ventanas agrupadas.
Para comparar los niveles secundarios se fija AODV, movilidad `cluster+nodo`, estimador C++ y
trilateración; E1 es el nivel base común. Las tablas de localización usan `experimento.csv`; las de
red, `experimento_red.csv`. Los datos completos por réplica y todos los intervalos están en
[`datos/`](./datos/README.md).

### 3.1 E1 — Caso base

Resultados de localización con AODV y trilateración:

| Movilidad | Error mediano (m), media [IC95%] | Disponibilidad, % [IC95%] | Frescura media (s) [IC95%] |
|---|---:|---:|---:|
| Estático | 121.33 [98.17–144.50] | 97.6 [95.3–99.9] | 1.15 [0.93–1.37] |
| Solo nodo | 119.35 [95.84–142.86] | 97.0 [94.3–99.7] | 1.09 [0.83–1.35] |
| Solo clúster | 48.92 [39.28–58.56] | 88.6 [80.5–96.7] | 1.70 [1.01–2.39] |
| Clúster + nodo | 52.95 [44.39–61.50] | 96.3 [93.9–98.7] | 0.99 [0.76–1.21] |

Resultados de red y consultas para las mismas combinaciones (medias [IC95%]):

| Movilidad | Consultas exitosas, % | Error de respuesta (m) | Edad de respuesta (s) | Bytes de aplicación |
|---|---:|---:|---:|---:|
| Estático | 81.4 [73.5–89.2] | 101.02 [83.29–118.75] | 3.14 [2.62–3.66] | 37 597 [30 847–44 346] |
| Solo nodo | 84.7 [80.0–89.3] | 104.03 [89.90–118.16] | 2.83 [2.58–3.08] | 43 057 [33 278–52 835] |
| Solo clúster | 81.8 [73.8–89.8] | 78.07 [72.02–84.11] | 3.20 [2.72–3.69] | 137 956 [117 287–158 625] |
| Clúster + nodo | 73.8 [64.8–82.8] | 84.05 [70.67–97.43] | 2.90 [2.56–3.25] | 142 034 [127 276–156 792] |

Con movilidad `cluster+nodo` y trilateración, comparación descriptiva de protocolos (medias [IC95%]):

| Protocolo | Error mediano (m) | Frescura media (s) | Consultas exitosas, % | Error de respuesta (m) |
|---|---:|---:|---:|---:|
| AODV | 52.95 [44.39–61.50] | 0.99 [0.76–1.21] | 73.8 [64.8–82.8] | 84.05 [70.67–97.43] |
| OLSR | 50.23 [43.13–57.33] | 0.72 [0.60–0.83] | 95.3 [92.0–97.0] | 76.79 [70.08–83.50] |
| DSDV | 50.44 [42.85–58.04] | 0.79 [0.65–0.92] | 96.0 [94.0–98.0] | 85.43 [75.11–95.75] |

![Figura F1. Error mediano de localización por movilidad, protocolo y método de estimación en el caso base.](figuras/E1-localizacion.png)

![Figura F2. Entrega de lecturas, éxito de consultas y error de respuesta en el caso base.](figuras/E1-red.png)

En esta muestra, permitir la movilidad del clúster coincide con una reducción marcada del error
mediano de localización respecto a los tratamientos sin movilidad de clúster. La combinación de
ambos niveles móviles mantiene un error bajo y una frescura cercana a 1 s, aunque genera más tráfico
de aplicación que los tratamientos con clúster fijo. Los IC95% de error de los tratamientos móviles
no se solapan con los de los tratamientos sin movilidad de clúster; esto es descriptivo y no
sustituye una prueba formal.

![Figura F6. Ejemplo de trayectorias y estimaciones en una corrida E1 con AODV, movilidad de clúster y nodo, réplica 1.](figuras/E1-escena.png)

### 3.2 E2 — Densidad (baja vs alta)

Comparación con E1 (8 nodos por clúster); valores de localización como media [IC95%]:

| Nodos por clúster | Error mediano (m) | Disponibilidad, % | Frescura media (s) |
|---:|---:|---:|---:|
| 4 | 57.08 [51.45–62.71] | 76.0 [65.0–86.9] | 2.77 [1.20–4.33] |
| 8 (base) | 52.95 [44.39–61.50] | 96.3 [93.9–98.7] | 0.99 [0.76–1.21] |
| 16 | 51.77 [43.60–59.94] | 87.6 [81.7–93.5] | 2.19 [1.52–2.86] |

| Nodos por clúster | Consultas exitosas, % | Error de respuesta (m) | Edad de respuesta (s) | Bytes de aplicación |
|---:|---:|---:|---:|---:|
| 4 | 90.1 [86.1–94.1] | 92.90 [83.65–102.14] | 3.87 [2.89–4.85] | 66 986 [57 142–76 830] |
| 8 (base) | 73.8 [64.8–82.8] | 84.05 [70.67–97.43] | 2.90 [2.56–3.25] | 142 034 [127 276–156 792] |
| 16 | 61.2 [52.1–70.3] | 80.45 [69.72–91.19] | 3.27 [2.62–3.92] | 212 296 [177 324–247 268] |

Al aumentar la densidad de 4 a 16 nodos, el error mediano cambia poco frente a la amplitud de sus
intervalos, pero la disponibilidad no crece de forma monótona y el tráfico de aplicación sí aumenta.
En estos resultados, el nivel base de 8 supera a ambos niveles en disponibilidad y frescura; por
tanto, más observadores no garantizan por sí solos una mejor estimación o una respuesta más oportuna.

![Figura F3. Efecto de la densidad de nodos en el error y la disponibilidad de trilateración.](figuras/E2-densidad.png)

![Figura F7. Escena representativa E2 con densidad baja: trayectorias y estimaciones de una corrida.](figuras/E2-densidad-baja-escena.png)

![Figura F8. Escena representativa E2 con densidad alta: trayectorias y estimaciones de una corrida.](figuras/E2-densidad-alta-escena.png)

### 3.3 E3 — Sombreado (cero vs alto)

Comparar `E3-sombreado-cero` (0 dB) con `E3-sombreado-alto` (8 dB) y con el caso base (4 dB, de E1).

| Sombreado (dB) | Error mediano (m) | Disponibilidad, % | Frescura media (s) |
|---:|---:|---:|---:|
| 0 | 7.94 [3.18–12.70] | 44.2 [36.6–51.8] | 41.74 [31.10–52.38] |
| 4 (base) | 52.95 [44.39–61.50] | 96.3 [93.9–98.7] | 0.99 [0.76–1.21] |
| 8 | 60.15 [55.68–64.62] | 100.0 [100.0–100.0] | 0.65 [0.58–0.73] |

| Sombreado (dB) | Consultas exitosas, % | Error de respuesta (m) | Edad de respuesta (s) | Bytes de aplicación |
|---:|---:|---:|---:|---:|
| 0 | 83.5 [77.0–90.0] | 32.49 [11.55–53.42] | 11.56 [1.80–21.31] | 115 270 [95 830–134 710] |
| 4 (base) | 73.8 [64.8–82.8] | 84.05 [70.67–97.43] | 2.90 [2.56–3.25] | 142 034 [127 276–156 792] |
| 8 | 66.0 [57.8–74.3] | 110.01 [101.03–118.99] | 2.74 [2.39–3.09] | 202 072 [191 513–212 631] |

Sin sombreado se observa un error menor entre las estimaciones disponibles, pero la disponibilidad
baja a 44.2% y la frescura media sube a 41.7 s; el error aislado no describe la utilidad total del
sistema. Con 8 dB las estimaciones están disponibles en todas las ventanas de esta combinación,
pero el error de localización y el de respuesta son mayores. El sombreado altera conjuntamente
precisión, disponibilidad y edad, por lo que conviene leer estas métricas como un compromiso y no
elegir un nivel solo por el error mediano.

![Figura F4. Efecto del sombreado del canal en el error de trilateración y la frescura.](figuras/E3-sombreado.png)

![Figura F9. Escena representativa E3 sin sombreado: trayectorias y estimaciones de una corrida.](figuras/E3-sombreado-cero-escena.png)

![Figura F10. Escena representativa E3 con sombreado alto: trayectorias y estimaciones de una corrida.](figuras/E3-sombreado-alto-escena.png)

### 3.4 E4 — Fusión de zonas (none vs raw vs merge)

Comparar `none` (de E1), `E4-fusion-raw` y `E4-fusion-merge`.

| `zoneFusion` | Error de respuesta (m) | Consultas exitosas, % | Edad de respuesta (s) | Bytes de aplicación |
|---|---:|---:|---:|---:|
| `none` (base) | 84.05 [70.67–97.43] | 73.8 [64.8–82.8] | 2.90 [2.56–3.25] | 142 034 [127 276–156 792] |
| `raw` | 50.78 [41.84–59.72] | 79.2 [72.5–85.8] | 2.70 [2.49–2.91] | 246 250 [209 626–282 873] |
| `merge` | 65.07 [59.72–70.41] | 74.2 [65.2–83.2] | 3.08 [2.80–3.35] | 139 564 [122 934–156 195] |

En la combinación seleccionada, `raw` presenta menor error de respuesta y una proporción de
consultas exitosas algo mayor que `none`, a costa de aproximadamente 73% más bytes de aplicación.
`merge` reduce el error respecto a `none` con un volumen de bytes parecido. Sus IC95% del error
apenas dejan de solaparse (por unos 0.26 m); esta separación descriptiva no sustituye una prueba
formal.

![Figura F5. Comparación de error de respuesta, consultas exitosas y bytes intercambiados para cada modo de fusión.](figuras/E4-fusion.png)

![Figura F11. Escena representativa E4 con fusión raw: trayectorias y estimaciones de una corrida.](figuras/E4-fusion-raw-escena.png)

![Figura F12. Escena representativa E4 con fusión merge: trayectorias y estimaciones de una corrida.](figuras/E4-fusion-merge-escena.png)

Las gráficas comparativas se generan desde `codigo/` con:

```bash
python analisis/graficas_comparativas.py
```

El script lee los siete CSV agregados de `resultados/` y guarda las figuras en
`entregables/f-experimentacion/figuras/`. Cada punto muestra la media entre réplicas y cada barra
el intervalo de confianza del 95% para la media, calculado con t de Student; en las comparaciones
de densidad y sombreado, E1 sirve como nivel base. También guarda
`resultados/<escenario>/intervalos_confianza.csv`, con el tamaño de muestra válido, media,
desviación estándar e intervalo inferior/superior para cada métrica y combinación. Los valores no
disponibles se excluyen y el número `n` refleja cuántas réplicas válidas entraron en cada cálculo.
Con más de 30 grados de libertad se usa una aproximación de Cornish-Fisher al valor crítico de t.
El solapamiento de intervalos es descriptivo y no reemplaza una prueba formal de hipótesis.

Las Figuras F6–F12 son visualizaciones cualitativas de la salida de una sola corrida por nivel.
Para mantener la comparación controlada, se escogió en todos los casos AODV, `cluster+nodo` y
`run01`; no sustituyen los promedios ni los intervalos calculados sobre todas las réplicas. Se
generan desde `codigo/` con `python analisis/graficar.py <corrida> --salida <archivo.png> --titulo
<título>`.

### Animaciones NetAnim

Se adjuntan siete trazas XML, una para cada nivel ejecutado: E1, los dos niveles de E2, los dos de
E3 y los dos niveles adicionales de E4. Todas usan AODV, movilidad `cluster+nodo`, `RngRun=1` y
120 s de simulación para facilitar una inspección visual breve y comparable. Las genera
`ns3::AnimationInterface`; NetAnim carga estas trazas para animar la topología y las trayectorias
de los nodos [13]. El collar rojo representa la posición real; los marcadores magenta y cian
representan, respectivamente, las estimaciones válidas de las cabezas de zona sur y norte. El
marcador gris en el centro de una zona indica que aún no hay estimación válida. El registro de
paquetes está desactivado, y las filas completas de cada estimación se conservan en
`estimaciones_red.csv`. Los siete XML están en [`animaciones/`](./animaciones/README.md):
`E1.xml`, `E2-densidad-baja.xml`, `E2-densidad-alta.xml`, `E3-sombreado-cero.xml`,
`E3-sombreado-alto.xml`, `E4-fusion-raw.xml` y `E4-fusion-merge.xml`. Son corridas ilustrativas,
no nuevas réplicas estadísticas ni evidencia de promedios; las estadísticas del informe siguen
basadas en las corridas de 600 s.

Los GIF de esa carpeta permiten ver las mismas corridas sin abrir NetAnim. En su leyenda, **Collar:
posición real** es la ubicación verdadera del perro; **Estimación zona sur/norte** es la posición
estimada por la cabeza de esa zona; y **Estimación no disponible** indica que todavía no existe una
estimación válida. Los demás colores identifican los clústeres: bus-norte (azul), plaza (verde),
bus-sur (violeta) y paseador (naranja); cada punto pequeño representa un celular. Las etiquetas
`HZ-S` y `HZ-N` marcan las cabezas de zona, `D` al dueño, y la línea horizontal separa las zonas
norte y sur. Los diamantes magenta y cian son las estimaciones sur y norte; el gris indica que falta
una estimación válida.

## 4. Análisis

Las comparaciones de E2–E4 se controlan en AODV, movilidad `cluster+nodo`, estimador C++ y
trilateración; en cada tabla de sensibilidad se cambia un factor frente a E1. Por ello, describen
ese tratamiento y no necesariamente el promedio de todas las movilidades, protocolos o métodos.
Además, `disponibilidad` en `experimento.csv` es la fracción de ventanas en que el método produjo
una estimación válida; `estimaciones_red_disponibilidad` es la fracción de estimaciones válidas en
las cabezas de zona, y `consultas_exito` es la fracción de consultas del dueño que recibieron una
estimación. No son métricas intercambiables. El error y la edad de respuesta se calculan sobre las
consultas contestadas con una estimación, por lo que deben leerse junto con la tasa de éxito y no
como si describieran todas las consultas.

### 4.1 Movilidad, métodos de localización y enrutamiento

En E1, la movilidad de clúster coincide con una reducción grande del error de trilateración en la
comparación controlada: el error mediano pasa de 121.33 m en estático a 48.92 m con solo clúster y
52.95 m con clúster+nodo. La movilidad de los nodos por sí sola apenas cambia el error respecto al
caso estático (119.35 m). No obstante, `solo-clúster` tiene 88.6% de disponibilidad y 1.70 s de
frescura, frente a 96.3% y 0.99 s con `cluster+nodo`; reducir el error entre las estimaciones
disponibles no garantiza obtenerlas con la misma frecuencia ni actualidad. En las consultas, además,
`cluster+nodo` tiene menor éxito que el caso estático (73.8% frente a 81.4%). La movilidad produce,
por tanto, un compromiso entre geometría útil, disponibilidad y oportunidad de comunicación, no una
mejora uniforme.

Los métodos también presentan perfiles distintos. Al promediar las métricas por corrida de las 120
corridas E1 (4 movilidades × 3 protocolos × 10 réplicas), el error mediano medio por corrida fue
68.7 m para centroide, 84.3 m para trilateración y 89.2 m para vecino más cercano. En cambio, el
percentil 90 medio fue 102.1 m, 236.1 m y 120.1 m, respectivamente; la disponibilidad media fue
99.2% para centroide y vecino más cercano y 96.1% para trilateración. Es un resumen descriptivo
sin intervalo de confianza y no una comparación ajustada por tratamiento. La diferencia entre
mediana y percentil 90 de trilateración señala una cola de errores grandes en algunas corridas. En
el tratamiento AODV/`cluster+nodo`, por ejemplo, su error mediano (52.95 m) es menor que el del
centroide (58.50 m), pero su percentil 90 es mayor (169.39 m frente a 99.44 m). No hay un método
que domine todas las métricas y condiciones; la mediana sola puede ocultar errores extremos.

El protocolo muestra un efecto más claro sobre la entrega que sobre la precisión de localización.
En E1/`cluster+nodo`, OLSR y DSDV contestan con estimación el 94.8% y 96.4% de las consultas,
respectivamente, frente al 73.8% con AODV; sus errores medianos de respuesta son 76.79 m, 85.43 m
y 84.05 m. Los intervalos de éxito de OLSR y DSDV no se solapan con el de AODV en esta
comparación, pero los errores de respuesta sí presentan solapamiento. Como comprobación más amplia,
el promedio descriptivo por protocolo sobre las 40 corridas E1 de cada uno (todas las movilidades)
da:

| Protocolo | Consultas con estimación, media por corrida | Entrega de lecturas, media por corrida | Bytes de aplicación, media por corrida |
|---|---:|---:|---:|
| AODV | 80.4% | 92.6% | 90 161 |
| OLSR | 96.9% | 97.5% | 109 119 |
| DSDV | 97.9% | 98.4% | 107 153 |

En estos promedios, OLSR y DSDV aumentan el éxito de consultas en 16.5 y 17.5 puntos porcentuales
frente a AODV, con aproximadamente 21% y 19% más bytes de aplicación. Es una tendencia agregada
descriptiva: el comportamiento de cada protocolo depende de la movilidad, y los bytes no incluyen
un costo energético ni una valoración del retardo extremo a extremo. La latencia de un salto de
aplicación tampoco equivale a la edad de la estimación que recibe el dueño.

### 4.2 Densidad de observadores

La respuesta a la densidad no es monótona. Con 16 nodos por clúster, el error mediano de localización
(51.77 m) es parecido al nivel base de 8 (52.95 m), con intervalos ampliamente solapados; la
disponibilidad cae de 96.3% a 87.6% y la frescura empeora de 0.99 s a 2.19 s. El nivel bajo de 4
tiene mayor error (57.08 m), 76.0% de disponibilidad y 2.77 s de frescura. Así, en este tratamiento
8 nodos es el mejor de los tres en disponibilidad y frescura, pero no se establece un óptimo general
ni una diferencia estadísticamente probada de precisión.

La carga de red evidencia el costo de añadir nodos: el promedio de bytes por corrida crece de
66 986 con 4 nodos a 142 034 con 8 y 212 296 con 16. Sin embargo, el éxito de consultas disminuye
de 90.1% a 73.8% y 61.2%, respectivamente; al mismo tiempo, el error de respuesta baja de 92.90 m
a 84.05 m y 80.45 m. Esto último se refiere solo a respuestas con estimación, y no compensa por sí
solo el aumento de consultas sin respuesta. El resultado sugiere que la cantidad de observadores
afecta simultáneamente la cobertura, la carga y la entrega, y que no basta con maximizar la densidad.

### 4.3 Sombreado del canal

E3 muestra el mayor compromiso entre calidad condicional y disponibilidad. A 0 dB, el error mediano
de localización es 7.94 m y el error de respuesta 32.49 m, los menores de los tres niveles; pero
solo 44.2% de las ventanas locales producen una estimación válida, la disponibilidad de estimaciones
en red es 26.8%, y la frescura media de localización sube a 41.74 s. El éxito de consultas es 83.5%,
pero la edad de las respuestas con estimación promedia 11.56 s. Estos errores pequeños describen
solo la fracción que sí se pudo estimar y no deben interpretarse como desempeño global superior.

Con 8 dB, la disponibilidad local llega a 100% y la de estimaciones en red a 96.0%, mientras que el
error mediano de localización aumenta a 60.15 m y el de respuesta a 110.01 m; el éxito de consultas
baja a 66.0%. Los bytes de aplicación aumentan de 115 270 a 202 072 entre 0 y 8 dB. El nivel base
de 4 dB queda entre los extremos en varios indicadores (96.3% de disponibilidad local, 52.95 m de
error mediano y 73.8% de éxito). En este modelo, por tanto, un canal más ruidoso no se traduce
simplemente en menos estimaciones: cambian disponibilidad, error y tráfico en direcciones distintas.
Los datos describen la respuesta observada del simulador y no aíslan por sí solos el mecanismo que
la produce.

La cautela estadística es especialmente importante para 0 dB. Aunque la tabla controlada
`cluster+nodo` tiene 10 réplicas válidas, en otros grupos de movilidad y protocolo los errores y la
frescura de localización se calcularon con apenas 1–5 réplicas; en ciertos grupos de consultas
también hay solo 1–2. La comparación de esos grupos tiene mucha menos precisión y no debe
generalizarse a partir de sus medias.

### 4.4 Fusión de estimaciones de zona

Frente a `none`, `raw` reduce el error mediano de respuesta de 84.05 m a 50.78 m (cerca de 40%) y
mejora el éxito de consultas de 73.8% a 79.2%, pero aumenta los bytes de aplicación de 142 034 a
246 250 (cerca de 73%). `merge` reduce el error a 65.07 m con un volumen de bytes similar a `none`
(139 564), pero el éxito de consultas apenas cambia (74.2%). Para `none` y `merge`, los IC95% del
error de respuesta apenas no se solapan; esa separación de unos 0.26 m es descriptiva y no prueba
una diferencia estadística.

La mejora de `raw` se observa principalmente en la respuesta consultada. En la métrica independiente
de error mediano de localización local, sus resultados (50.64 m) son similares a `none` (52.95 m),
con IC95% solapados; la disponibilidad local también es parecida (96.7% frente a 96.3%). Así, no
debe describirse `raw` como un método que mejora toda localización por igual: su intercambio más
visible es menor error de respuesta y algo más de éxito, a cambio de más comunicación. `merge`
ofrece una alternativa de menor tráfico, con una mejora intermedia del error de respuesta, sin una
ventaja clara en el éxito.

### 4.5 Alcance de la evidencia

Cada IC95% estima la media de un tratamiento entre réplicas, no la variabilidad instantánea dentro
de una corrida. Las tablas de sensibilidad usan `n=10`; los agregados descriptivos de métodos y
protocolos indicados arriba resumen promedios por corrida y no llevan IC. El solapamiento o la
separación de IC sirve como orientación visual, pero no reemplaza contrastes formales. No se
ejecutaron pruebas de hipótesis ni ajustes por comparaciones múltiples, por lo que las diferencias
numéricas no se presentan como significativas.

También deben considerarse los artefactos del modelo que pueden afectar la interpretación: sesgo de
lecturas lejanas, sombreado independiente por paquete, cola ARP, caducidad de rutas en AODV y sesgo
de `zoneFusion=none`, descritos en la sección "Qué declarar en el informe" del README del
repositorio y en la Sección 7 del literal b. Las simulaciones usan configuraciones específicas y no
incluyen una calibración con mediciones de campo; las escenas y GIF son ilustraciones de corridas
cortas, no evidencia estadística adicional.

## 5. Conclusiones

1. **La movilidad de clúster mejora la precisión condicional en el caso base, pero no garantiza más
   respuestas útiles.** Con AODV y trilateración, el error mediano de las ventanas estimables baja
   de 121.33 m en estático a 48.92–52.95 m con movilidad de clúster. La combinación
   `cluster+nodo` mantiene 96.3% de disponibilidad y 0.99 s de frescura, mientras que `solo-clúster`
   reduce algo más el error pero cae a 88.6% de disponibilidad y 1.70 s. El éxito de consultas de
   `cluster+nodo` (73.8%) también es menor que el estático (81.4%).

2. **El efecto de densidad tiene rendimientos decrecientes y costos de comunicación.** Pasar de 8
   a 16 nodos aumenta los bytes de aplicación en cerca de 50% y no produce una reducción clara del
   error de localización; el éxito de consultas baja 12.6 puntos porcentuales. Cuatro nodos reducen
   el tráfico, pero también reducen disponibilidad y empeoran la frescura. El nivel base de 8 es el mejor
   compromiso observado en la combinación controlada para esas dos métricas, no un óptimo universal.

3. **La precisión entre casos estimables no basta para valorar el sombreado.** A 0 dB se observan
   errores bajos, pero disponibilidad local de 44.2%, disponibilidad en red de 26.8% y estimaciones
   mucho más antiguas. A 8 dB hay más estimaciones disponibles, pero con errores mayores y menos
   consultas exitosas. Además, los resultados a 0 dB para varios tratamientos tienen pocas réplicas
   válidas; deben considerarse exploratorios.

4. **La fusión cambia el compromiso entre calidad de respuesta y carga.** `raw` alcanza el menor
   error de respuesta y el mayor éxito de consultas entre los modos comparados, pero con cerca de
   73% más bytes que `none`. `merge` obtiene un error de respuesta intermedio con un volumen de
   bytes parecido a `none`; no se ha probado que sus diferencias sean estadísticamente
   concluyentes.

5. **No hay un único protocolo o estimador ganador para todos los objetivos.** OLSR y DSDV elevan
   el éxito de consultas respecto a AODV en E1, con más bytes de aplicación. Trilateración puede
   lograr una mediana competitiva en el tratamiento móvil, pero su cola de error es mayor; centroide
   tiene menor percentil 90 promedio en el resumen de E1. La selección depende de si se priorizan
   precisión típica, errores extremos, disponibilidad, actualidad, éxito de consulta o carga.

En conjunto, las 840 corridas permiten comparar tendencias dentro de este modelo y estos parámetros,
pero no demostrar que el mismo orden de desempeño se conserve en una red física. Las conclusiones
más firmes son los compromisos observados entre métricas y la necesidad de reportarlas juntas; las
diferencias causales y la generalización a otros entornos requieren pruebas y validación adicionales.

## 6. Recomendaciones y trabajo futuro

- Definir antes de una selección de protocolo o fusión qué métrica es prioritaria y reportar a la
  vez precisión, disponibilidad, actualidad, éxito de consultas y bytes; evitar optimizar el error
  solo sobre respuestas disponibles.
- Repetir o ampliar las réplicas a 0 dB, en particular para movilidad estática y solo-nodo, hasta
  conseguir muestras suficientes y comparables para estimar incertidumbre.
- Añadir contrastes formales entre tratamientos con un plan de comparaciones e interacciones
  definido de antemano; considerar modelos que respeten el diseño factorial y corregir por
  comparaciones múltiples.
- Reportar por separado métricas condicionales (error y edad de respuestas con estimación) y tasas
  de disponibilidad/éxito que indican cuántos casos quedaron fuera de esas métricas.
- Como extensiones del modelo, estudiar sombreado correlacionado espacial y temporalmente, medir
  distancias con tecnologías como Wi-Fi FTM y evaluar estimadores temporales (por ejemplo, un filtro
  de Kalman integrado mediante ns3-ai).

## 7. Referencias

- **[6]** C. Perkins, E. Belding-Royer, y S. Das, “Ad hoc On-Demand Distance Vector (AODV)
  Routing,” IETF, RFC 3561, jul. 2003. [En línea]. Disponible:
  https://www.rfc-editor.org/rfc/rfc3561.html.
- **[7]** T. Clausen y P. Jacquet, “Optimized Link State Routing Protocol (OLSR),” IETF, RFC 3626,
  oct. 2003. [En línea]. Disponible: https://www.rfc-editor.org/rfc/rfc3626.html.
- **[8]** C. E. Perkins y P. Bhagwat, “Highly dynamic Destination-Sequenced Distance-Vector routing
  (DSDV) for mobile computers,” en *Proc. ACM SIGCOMM*, 1994, pp. 234–244,
  doi: 10.1145/190314.190336.
- **[9]** T. S. Rappaport, *Wireless Communications: Principles and Practice*, 2.ª ed. Upper Saddle
  River, NJ, EE. UU.: Prentice Hall PTR, 2002.
- **[10]** T. Camp, J. Boleng, y V. Davies, “A survey of mobility models for ad hoc network
  research,” *Wireless Communications and Mobile Computing*, vol. 2, n.º 5, pp. 483–502, 2002,
  doi: 10.1002/wcm.72.
- **[11]** M. E. Payton, M. H. Greenstone, y N. Schenker, “Overlapping confidence intervals or
  standard error intervals: What do they mean in terms of statistical significance?,” *Journal of
  Insect Science*, vol. 3, n.º 1, art. 34, 2003, doi: 10.1093/jis/3.1.34.
- **[12]** nsnam, “ns-3 Model Library: Propagation, Mobility and Routing,” versión 3.46, 2025.
  [En línea]. Disponible: https://www.nsnam.org/docs/release/3.46/models/html/.
- **[13]** nsnam, “Animation,” en *ns-3 Model Library*, versión 3.46, 2025. [En línea]. Disponible:
  https://www.nsnam.org/docs/release/3.46/models/html/animation.html.
