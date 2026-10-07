# Visor de escenas

Muestra los mapas del juego en el navegador con el mismo sombreado, sin compilar ni ejecutar el motor: **Desierto de día**, **Bosque** y **Desierto de noche** (selector arriba a la izquierda). Cada uno es un fichero `.scene` (`assets/scenes/desert_day.scene`, `forest.scene`, `desert.scene`). Los dos primeros se construyen en código en el juego (`TestStage`, `ForestStage`) y su `.scene` es solo una descripción para el visor. Usa three.js y contiene una copia de `animatedshader.vert`/`shader.frag`.

## Uso

```bash
python3 tools/scene_viewer/serve.py            # abre http://127.0.0.1:8000/tools/scene_viewer/
python3 tools/scene_viewer/serve.py --scene assets/scenes/forest.scene --view top --hour 18
python3 tools/scene_viewer/serve.py --shot captura.png --view player   # PNG sin ventana
```

- Solo necesita la biblioteca estándar de Python. three.js se descarga de un CDN, así que hace falta conexión a internet.
- `--shot` lanza Firefox en modo headless, espera a que todo cargue, guarda el PNG y termina. Sirve para revisar la escena desde la terminal, o para que Claude la vea.
- La página tiene que servirse por HTTP, porque si se abre como `file://` el navegador bloquea la carga de los assets.

## Qué permite

- **Cámaras:**
  - Órbita (vista libre).
  - Planta (vista desde arriba).
  - Jugador: la misma vista que al empezar el juego. Al arrastrar, se mira alrededor como con el ratón en el juego.
- **Seleccionar objetos:** haz clic en un objeto o en la tabla. Se muestran su línea en el `.scene`, su posición, yaw, escala y efecto.
- **Cursor sobre las dunas:** muestra la posición del mundo y la `y` que hay que poner a un objeto en ese punto.
- **Recarga automática:** al guardar el `.scene`, el visor se actualiza solo.
- **Hora del día** (mapas de día y bosque): un deslizador (o "dejar pasar el día") mueve el sol, la luz y el cielo como en el juego (`VehicleStage::onTimeChanged`), con las dunas o los árboles del horizonte. `--hour H` la fija en una captura.
- **Árboles detallados** (`object ... sway`/`lit`): el viento (efecto `sway`) los mece en el visor igual que en el juego; las hojas son tarjetas con alfa, que se recortan. `forest.scene` solo lista los árboles de los primeros 500 m de carretera y con los modelos de menos detalle (`_lod1` y `_lod2`): el visor no cambia de nivel con la distancia, y el bosque entero (14 000 árboles) lo ahogaría.
- **Opciones de visualización:**
  - Activar o desactivar el cielo y la animación (el juego ya no tiene niebla y el visor tampoco).
  - Rejilla de 1 m con ejes.
  - "Luz de día", para ver mejor la escena nocturna.
  - Captura PNG.

## Mantenerlo sincronizado con el motor

Hay tres puntos que deben coincidir con el motor:

| Visor (`viewer.js`) | Motor |
|---|---|
| `parseScene` | `SceneFile::load` (`src/world/SceneFile.cpp`) |
| `vertexShader` / `fragmentShader` | `src/shaders/animatedshader.vert` / `src/shaders/shader.frag` |
| `updateClock` | `VehicleStage::onTimeChanged` |
| `proceduralSky` (parte del shader) | `unlit` = 3 de `shader.frag` (cielo, dunas, árboles del horizonte) |
| `FOV`, `SENSIVILITY`, `PLAYER_HEIGHT`, `BREATH_AMPLITUDE`, `PROP_SINK`… | `Camera`, `Controller`, `AnimatedModel::computeFit`, `SceneStage` |

Los mapas hechos en código se describen aparte, así que si cambia `TestStage` (sus adornos, el RV...) hay que actualizar `desert_day.scene` a mano; `forest.scene` lo escribe `assets/forest/generate_forest.py`. Pingu y la criatura (modelos animados) no se muestran.

El pingüino se carga con el `FBXLoader` de three.js, no con Assimp. Su postura en cada momento puede variar un poco respecto al juego, pero el ajuste de tamaño y posición es el mismo.
