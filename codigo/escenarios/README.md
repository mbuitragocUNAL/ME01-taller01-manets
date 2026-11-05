# Escenarios de simulación

Los archivos `.conf` de esta carpeta fijan las condiciones de cada experimento. **E1** es la
referencia con los valores base; **E2**, **E3** y **E4** cambian un factor a la vez para medir su
efecto y comprobar si las conclusiones sobre movilidad se mantienen en otras condiciones.

En cada corrida, `analisis/experimento.py` combina las cuatro configuraciones de movilidad
(clúster estático o móvil y nodos estáticos o móviles) con los protocolos AODV, OLSR y DSDV.
Los demás parámetros permanecen en sus valores base, salvo el factor que el escenario estudia.
Cada archivo indica sus valores y propósito en los comentarios iniciales.

## Comparación

| Escenario | Archivos | Factor y niveles | Qué aporta y por qué se eligió |
|---|---|---|---|
| **E1 — Caso base** | `E1.conf` | Referencia: 8 nodos por clúster, sombreado de 4 dB y fusión `none`. | Permite medir el efecto de la movilidad y comparar los demás escenarios con una condición común, sin alterar los parámetros base. |
| **E2 — Densidad** | `E2-densidad-baja.conf`, `E2-densidad-alta.conf` | 4 y 16 nodos por clúster, frente a los 8 de E1. | Mide cómo cambia la localización al variar la cantidad de nodos que pueden aportar lecturas. El nivel bajo prueba la escasez de observaciones; el alto, si tener más lecturas ayuda pese a una geometría desfavorable, como la de los clústeres tipo bus. |
| **E3 — Sombreado** | `E3-sombreado-cero.conf`, `E3-sombreado-alto.conf` | Desviación del sombreado de 0 y 8 dB, frente a los 4 dB de E1. | Aísla la influencia de la variabilidad del canal sobre las lecturas RSSI. El nivel cero da una referencia sin sombreado; el alto permite comprobar si el patrón de movilidad que funciona mejor cambia cuando las mediciones son más inciertas. |
| **E4 — Fusión de zonas** | `E4-fusion-raw.conf`, `E4-fusion-merge.conf` | `raw` y `merge`; `none` de E1 es el tercer nivel de comparación. | Compara las formas de combinar la información de dos zonas y su posible balance entre error de localización y tráfico de red. `raw` comparte lecturas individuales; `merge` combina las estimaciones de las cabezas de zona. |

Así, cada escenario responde una pregunta distinta: E1 establece la referencia; E2 estudia la
cantidad de nodos; E3, el ruido del canal; y E4, el intercambio de información entre zonas. La
variación de un solo factor por comparación facilita atribuir los cambios observados a ese factor,
en lugar de confundirlos con varios cambios simultáneos.

## Diferencias entre los niveles

- **Densidad:** menos nodos pueden significar menos lecturas disponibles para estimar la posición;
  más nodos permiten probar si las lecturas adicionales mejoran el resultado.
- **Sombreado:** con `0 dB` no se agrega variación de sombreado; `8 dB` representa un canal más
  variable que el nivel base de `4 dB`.
- **Fusión:** `none` mantiene cada zona trabajando por separado; `raw` reenvía las lecturas crudas
  a la otra cabeza de zona; `merge` combina las estimaciones de ambas cabezas. E1 cubre `none`,
  por lo que E4 solo necesita los archivos para `raw` y `merge`.

## Ejecución

Desde `codigo/`, con el entorno de ns-3 preparado, se puede ejecutar cada nivel así:

```bash
python analisis/experimento.py --nombre E1 --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E1.conf

python analisis/experimento.py --nombre E2-densidad-baja --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E2-densidad-baja.conf
python analisis/experimento.py --nombre E2-densidad-alta --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E2-densidad-alta.conf

python analisis/experimento.py --nombre E3-sombreado-cero --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E3-sombreado-cero.conf
python analisis/experimento.py --nombre E3-sombreado-alto --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E3-sombreado-alto.conf

python analisis/experimento.py --nombre E4-fusion-raw --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E4-fusion-raw.conf
python analisis/experimento.py --nombre E4-fusion-merge --reps 10 --routing aodv olsr dsdv -- --conf=escenarios/E4-fusion-merge.conf
```

`--nombre` identifica la carpeta de resultados y `--reps` indica el número de réplicas. Las salidas
se guardan en `codigo/resultados/<nombre>/`. Para probar otro número de réplicas o protocolos,
ajuste esas opciones manteniendo constantes las condiciones que quiera comparar.
