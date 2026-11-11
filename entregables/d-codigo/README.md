# d. Código fuente

Los lineamientos piden el código fuente completo, con las versiones de cualquier librería que no venga en la
instalación estándar. El código está en [`../../codigo/`](../../codigo/). Esta carpeta solo guarda el zip que
va en el correo y la tabla de versiones.

Para armar el zip sin resultados ni compilados, desde la raíz del repositorio:

```bash
zip -r entregables/d-codigo/Taller01_MANETs_d_codigo.zip \
    codigo instalar.sh INSTALACION.md \
    -x '*/resultados/*' '*.so' '*/__pycache__/*' '*.pyc' '*.DS_Store'
```

Versiones con las que se desarrolló, medidas el 29 de octubre de 2026 en un Mac con Apple silicon:

| Pieza | Versión | Para qué |
|---|---|---|
| ns-3 | 3.46.1, del paquete ns-allinone-3.46.1 | el simulador |
| ns3-ai | v1.45.2 del proyecto DEFIANCE, la que trae ns-allinone-3.46.1 en `contrib/ai` | el estimador en Python durante la simulación |
| NetAnim | 3.110, con Qt 5.15.19 | ver la animación |
| Compilador | Apple clang 21.0.0, macOS 27.0.1 | |
| CMake, Ninja | 4.4.5, 1.13.2 | compilar |
| Boost, protobuf, pybind11 | 1.92.0, 36.2, 3.1.0 | los necesita ns3-ai |
| Python | 3.13.5 | análisis y estimador en Python |
| numpy, matplotlib | 2.5.4, 3.11.2 | análisis y gráficas |

ns-3 se usa sin cambios. `instalar.sh` solo compila un subconjunto de módulos y desactiva el enlazador rápido
`lld`, que en macOS no sabe enlazar contra la libc++ del sistema.

En `codigo/analisis/` están los scripts para estimación fuera de línea (`localizar.py`), métricas de
red (`red.py`), escenas de corridas individuales (`graficar.py`), diseño factorial (`experimento.py`)
y comparaciones con IC95% (`graficas_comparativas.py`, que comparte el cálculo con `estadistica.py`).
El [informe de experimentación](../f-experimentacion/experimentacion.md) documenta sus comandos,
archivos generados y figuras.

Cómo instalar y correr todo va en el manual técnico, literal e. [INSTALACION.md](../../INSTALACION.md) sirve
de base.
