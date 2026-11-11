# Taller 01: simulación de MANETs

Taller de ME01 Modelos Estocásticos, UNAL 2026-02. Vale el 20 % de la nota y se entrega el miércoles 4 de
noviembre de 2026 a las 11:59 pm.

El proyecto: un perro se pierde con un collar que emite una señal cada segundo. Los celulares que pasan cerca
saben dónde están por GPS y miden con qué fuerza oyen el collar. Con esas lecturas la red estima dónde está el
perro, y el dueño consulta esa estimación a través de la misma red ad hoc. Los celulares se mueven en grupos,
como pasajeros de un bus o gente en una plaza, y eso es lo que estudiamos: cómo cambia la localización según
se muevan los grupos y las personas dentro de ellos.

La simulación está en NS-3 3.46.1, en C++, y el análisis en Python.

## Qué falta

| Tarea | Quién | Estado |
|---|---|---|
| Pregunta de investigación e hipótesis finales | el grupo | pendiente; hay un borrador más abajo |
| Escenarios y parámetros del experimento | el grupo, probando con archivos `.conf` | pendiente |
| Correr el experimento final | quien tenga el entorno instalado | pendiente |
| Literales a–f | el grupo | pendiente; la estructura está en [`entregables/`](entregables/README.md) |
| Nombres del SIA y roles | cada integrante, en [`entregables/autores.md`](entregables/autores.md) | pendiente |
| Correo de entrega | un integrante | pendiente; lista en [`entregables/correo-entrega.md`](entregables/correo-entrega.md) |
| Código de la simulación y del análisis | hecho | listo para usar |

Mejoras posibles al código, ninguna obligatoria:
- medir cuántos paquetes de control gasta cada protocolo de enrutamiento;
- el escenario del paseador con varios perros, cada uno con su collar;
- un protocolo de inundación pura, que ns-3 no trae.

## Instalar

Ver [INSTALACION.md](INSTALACION.md). En resumen, después de instalar las dependencias del sistema:

```bash
git clone https://github.com/mbuitragocUNAL/ME01-taller01-manets.git
cd ME01-taller01-manets
./instalar.sh
```

El script descarga ns-3, compila solo los módulos que usamos, crea el entorno de Python y corre una prueba.
En el Mac donde lo probamos tardó 2 minutos. En cada terminal nueva hay que cargar el entorno con
`source entorno.sh`.

## Correr

Todo se corre desde `codigo/`, con el entorno cargado:

```bash
source entorno.sh
cd codigo
./simular --outDir=resultados/demo --netanim=true
python analisis/localizar.py resultados/demo
python analisis/red.py resultados/demo
python analisis/graficar.py resultados/demo
```

`./simular` corre una simulación de 600 s y deja los resultados en `resultados/demo/`. Los otros tres
comandos leen esa carpeta:
- `localizar.py` estima la posición del perro con todas las lecturas e imprime el error de cada método;
- `red.py` imprime cuántos mensajes llegaron y cuánto tardaron;
- `graficar.py` dibuja las trayectorias y las estimaciones en `escena.png`.

`graficar.py` también acepta `--salida ruta.png` y `--titulo "texto"` para personalizar esa figura.
Después de generar los CSV de los escenarios E1–E4, ejecuta `python analisis/graficas_comparativas.py`
para crear las figuras del informe y calcular intervalos de confianza del 95% entre réplicas. Las
figuras quedan en `entregables/f-experimentacion/figuras/` y cada escenario recibe
`resultados/<escenario>/intervalos_confianza.csv`.

Para el experimento completo, con réplicas y en paralelo:

```bash
python analisis/experimento.py --nombre prueba --reps 10 --routing aodv olsr dsdv -- --simTime=600
```

Esto corre las 4 combinaciones de movilidad con cada protocolo, 10 veces cada una con semillas distintas,
y deja las tablas en `resultados/prueba/`. En el Mac de Miguel, las 120 corridas tardaron unos 5 minutos.

Si alguien cambia un `.cc` o un `.h`, hay que recompilar con `./compilar`. `experimento.py` lo hace solo.

## Probar con otros parámetros

Todos los parámetros están en [`codigo/escenarios/base.conf`](codigo/escenarios/base.conf), con su valor por
defecto y un comentario. Para probar un escenario, copien ese archivo, cambien lo que quieran y pásenlo con
`--conf`:

```bash
cp escenarios/base.conf escenarios/mi-prueba.conf
./simular --conf=escenarios/mi-prueba.conf --outDir=resultados/mi-prueba
python analisis/experimento.py --nombre mi-prueba --reps 10 -- --conf=escenarios/mi-prueba.conf
```

Un valor escrito en la línea de comandos después de `--conf` le gana al archivo. En `experimento.py`, los
factores del diseño los pone el script y siempre ganan: movilidad de clúster, movilidad de nodo, protocolo y
semilla. `./simular --help` muestra la lista completa.

Cada corrida guarda en su carpeta `parametros.txt` y `parametros_texto.txt`, con todos los valores que usó
y la semilla, así que cualquier resultado se puede repetir.

## Qué simula

Hay 32 celulares en un área de 500 × 300 m, repartidos en una jerarquía de dos niveles:

| Nivel | Qué es | Cuántos |
|---|---|---|
| 2 | Zonas: `sur` (y de 0 a 150 m) y `norte` (y de 150 a 300 m) | 2 |
| 1 | Clústeres: `bus-norte` y `plaza` en el norte, `bus-sur` y `paseador` en el sur | 4 |
| nodo | Celulares, 8 por clúster | 32 |

El perro es un nodo aparte que camina al azar a 0.3 m/s.

La posición de cada celular es la suma de tres capas:
- la zona, que no se mueve;
- el clúster, que se mueve como un bloque;
  - los buses siguen una ruta rectangular a 6 m/s, con paradas de 20 s;
  - los grupos a pie van de un punto al azar a otro;
- el celular, que camina al azar dentro de la caja de su clúster.

Las cajas miden 12 × 3 m en el bus, 20 × 20 m en la plaza y 6 × 6 m en el paseador.

El bus es el ejemplo más claro. El bus recorre su ruta como un bloque, y los 8 celulares que van adentro
se mueven con él: cuando el bus dobla una esquina, doblan todos. Al mismo tiempo, cada pasajero camina al
azar dentro del bus a 0.5 m/s y cambia de dirección cada 5 s, sin salirse de la caja de 12 × 3 m. En
ns-3, el bus es el padre de un `HierarchicalMobilityModel` y cada celular es un hijo. Lo medimos en una
corrida de 120 s: el bus llegó a 6 m/s y recorrió 480 m, mientras los pasajeros se movían en promedio a
0.4 m/s respecto al bus. Con `--nodeMobility=false` los pasajeros quedan quietos en su puesto, como
sentados, y solo se mueve el bus. El modelo no tiene paradas donde la gente suba o baje: los mismos 8
celulares van todo el recorrido.

`--clusterMobility` y `--nodeMobility` prenden o apagan el movimiento de cada nivel. Sus cuatro combinaciones
son las del experimento: estático, solo-nodo, solo-clúster y clúster+nodo.

El collar emite cada segundo por 802.11b a 0 dBm. La pérdida de señal sigue el modelo log-distancia, con
exponente 2.7, más un sombreado aleatorio normal de 4 dB. Esa es la parte estocástica principal: dos lecturas
a la misma distancia dan RSSI distintos. Cada celular que decodifica el beacon anota el RSSI y su posición
GPS, que es la real más un error normal de 4 m por eje.

Con esas lecturas hay tres formas de estimar dónde está el perro:
- `cercano` toma la posición del celular que oyó más fuerte. Es lo que hace Find My de Apple y nos sirve de
  línea base;
- `centroide` promedia las posiciones de los celulares, pesando más a los que oyeron más fuerte;
- `trilateracion` convierte cada RSSI en una distancia y busca el punto que mejor cuadra con todas, por
  mínimos cuadrados. Necesita al menos 3 celulares distintos.

### Los mensajes en la red

Los celulares enrutan con AODV, OLSR o DSDV (`--routing`). La cabeza de cada clúster es su celular 0. Las
cabezas de zona son la de `plaza` en el norte y la de `paseador` en el sur.

| Mensaje | De quién a quién | Cuándo |
|---|---|---|
| R reporte | celular a la cabeza de su clúster | cada lectura, tras una espera al azar de 0 a 100 ms |
| V relevo | cabeza de clúster a cabeza de zona | cada reporte que le llega |
| E intercambio | cabeza de zona a la otra cabeza de zona | cada estimación |
| Q consulta | el dueño a la cabeza de su zona | cada 5 a 15 s, al azar |
| A respuesta | cabeza de zona al dueño | cada consulta |
| X cruda | cabeza de zona a la otra cabeza de zona | cada lectura, solo con `--zoneFusion=raw` |

Cada 5 s, cada cabeza de zona estima con las lecturas de los últimos 10 s. El dueño viaja en `bus-norte`.

`--zoneFusion` decide qué hacer con las dos zonas. Las dos alternativas a `none` nos parecieron igual de
válidas, así que quedaron las dos como opción:
- `none`, el valor por defecto: cada cabeza estima solo con su zona. Al dueño le llega la estimación con más
  celulares, o la más reciente si una de las dos ya tiene más de 10 s;
- `raw`: cada cabeza le reenvía a la otra las lecturas crudas de su zona, y las dos estiman con todo;
- `merge`: cada una estima por su lado y, al contestar, se promedian las dos, pesando por número de celulares.

### El estimador en Python con ns3-ai

Con `--estimator=python`, cada vez que una cabeza de zona va a estimar, la simulación se detiene, le pasa las
lecturas a Python por memoria compartida y sigue con la posición que Python devuelve. Python usa la misma
función que `localizar.py`. Para probar otro estimador, como un filtro de Kalman o un modelo entrenado, basta
con cambiar `estimar_peticion()` en `analisis/ns3ai_estimador.py`, sin tocar el C++.

```bash
python analisis/ns3ai_estimador.py -- --outDir=resultados/demo-py
python analisis/experimento.py --nombre prueba-py --reps 10 --estimador python
```

Comparamos una misma corrida de 300 s con el estimador en C++ y en Python. Todos los demás archivos salieron
idénticos, y las 118 estimaciones difirieron a lo sumo en 3 cm. Cada corrida con Python ocupa dos núcleos al
100 % mientras espera, por eso `experimento.py` corre la mitad de corridas a la vez en ese modo.

Necesita que ns-3 se haya compilado con ns3-ai. `instalar.sh` lo hace si encuentra Boost, protobuf y pybind11.

## Archivos de salida

Cada corrida deja en su carpeta:

| Archivo | Qué trae |
|---|---|
| `rssi.csv` | cada lectura: quién la tomó, dónde estaba, dónde estaba el perro, el RSSI y la distancia real |
| `posiciones.csv` | la posición de cada nodo cada segundo |
| `nodos.csv` | clúster, zona y papel de cada nodo |
| `mensajes.csv` | cada mensaje de la tabla de arriba, con la hora de envío y la de llegada; vacía si se perdió |
| `reportes.csv` | cada lectura, con la hora en que llegó a la cabeza de su zona |
| `estimaciones_red.csv` | cada estimación de las cabezas de zona, con su error contra la posición real |
| `consultas.csv` | cada consulta del dueño: si tuvo respuesta, cuánto tardó, qué tan vieja y qué tan errada estaba la estimación |
| `parametros.txt`, `parametros_texto.txt` | los parámetros y la semilla de la corrida |
| `netanim.xml` | la animación para NetAnim, solo con `--netanim=true` |

## Qué declarar en el informe

Estos efectos los vimos al probar el código. Son supuestos o artefactos del modelo, y el análisis tiene que
tenerlos en cuenta. No son conclusiones.

- Lejos del collar solo se decodifican los beacons que tuvieron sombreado favorable. Las lecturas débiles se
  pierden por debajo de unos −90 dBm, así que las lecturas lejanas salen más fuertes de lo que deberían. Al
  ajustar RSSI contra distancia da un exponente de 1.6 en vez de 2.7, y la trilateración subestima las
  distancias. Sin sombreado, el ajuste da exactamente 2.7.
- El sombreado se sortea independiente en cada paquete. En la realidad está correlacionado en el espacio y en
  el tiempo, así que promediar lecturas ayuda más en la simulación que en la vida real.
- ns-3 guarda solo 3 paquetes por vecino mientras resuelve su dirección con ARP. Los relevos salen en ráfagas,
  y con AODV se perdía el 40 %. Subimos esa cola a 64 con `--arpQueue`, cerca de los 100 que usa Linux, y la
  entrega con AODV pasó de 57 % a 93 %.
- Con `zoneFusion=none`, cada cabeza de zona solo ve las lecturas de su lado y su estimación se carga hacia su
  zona. Se nota en `escena.png`.
- Con AODV, cerca del 22 % de las consultas del dueño quedaron sin respuesta en las pruebas. Las rutas de AODV
  vencen a los 3 s y las consultas llegan cada 10 s, así que casi cada consulta tiene que buscar la ruta de
  nuevo. Con OLSR y DSDV fue entre 1 % y 7 %.
- `zoneFusion=raw` duplica las lecturas que tiene cada cabeza, pero el tráfico extra bajó la entrega de relevos
  de 99 % a 67 % en una corrida de prueba con DSDV.
- 802.11b reemplaza a Bluetooth, que es lo que usaría un collar real. ns-3 no trae Bluetooth.

## Reglas del taller

- Grupos de máximo 7 personas.
- Hay que responder una pregunta de investigación sobre modelos de movilidad en MANET con el método científico.
- El sistema tiene que ser jerárquico, con al menos 3 clústeres en el primer nivel y 2 en el segundo, y debe
  haber movimiento tanto de clústeres como de nodos.
- Se entrega lo mismo que en una tarea automatizada, literales a–f. El correo del profesor insiste en el
  manual técnico y el de usuario.
- El profesor autorizó usar IA para el código y la simulación en NS-3. Los documentos y el correo de entrega
  los tenemos que escribir nosotros.
- Probablemente haya sustentación en horario extraclase. El monitor asigna la hora.

El enunciado completo es el correo del profesor del 5 de octubre. No está en el repositorio porque trae los
enlaces al Drive del curso.

## Borrador de la pregunta de investigación

Escrito el 24 de octubre como punto de partida. El grupo decide la versión final.

> ¿Cómo afecta el patrón de movilidad de los celulares que observan al error y a la frescura de la posición
> estimada de un collar por RSSI?

Hay dos hipótesis que tiran en sentidos opuestos. Los clústeres que se mueven recorren más área y oyen el collar
más seguido, lo que daría estimaciones más frescas. Pero los celulares de un mismo bus están casi en el mismo
punto, una mala geometría para trilaterar, lo que daría más error aunque haya muchas lecturas.

La movilidad es la variable independiente. Lo que se mide es la localización: el error en metros, qué tan
vieja está la estimación y cuántas consultas reciben respuesta. Otros factores posibles son la densidad de
celulares, el sombreado, el protocolo y la forma de combinar las zonas.

Lo que dejamos por fuera: medir distancia por tiempo de vuelo, que necesita Wi-Fi FTM y ns-3 no lo modela bien.
Puede ir en trabajo futuro.

## Material del curso

No está en el repositorio porque tiene derechos de autor. Todo viene del Drive que enlaza el correo del
profesor:

- El libro de Zárate Ceballos y otros, *Wireless Network Simulation: A Guide using Ad Hoc Networks and the
  ns-3 Simulator*. Ortiz Triviño es coautor. Los capítulos que más sirven: 3, diseño de experimentos; 4,
  ns-3; 5, análisis de resultados con réplicas e intervalos de confianza; 6, MANETs en ns-3.
- Ospina-López y Ortiz-Triviño, 2017, "Estimation of a growth factor to achieve scalable ad hoc networks",
  *Ingeniería y Universidad* 21(1), 49–70, DOI 10.11144/Javeriana.iyu21-1.egfa. Es el ejemplo de indagación
  que dio el profesor. Su tabla de parámetros de la página 13 sirve de modelo para la nuestra.
- Sarkar, *Ad Hoc Mobile Wireless Networks*, capítulo 8, sobre modelos de movilidad. Base del marco teórico.
- Las 5 animaciones de MANETs planas y jerárquicas de la carpeta de NS-3, que muestran lo que se espera ver.

## Calendario

| Fecha | Qué |
|---|---|
| Jueves 29 de octubre a domingo 1 de noviembre | Pregunta final, escenarios, corridas |
| Lunes 2 y martes 3 | Análisis de resultados, marco teórico, diseño, manuales |
| Miércoles 4 | Revisión final y correo antes de las 11:59 pm |
