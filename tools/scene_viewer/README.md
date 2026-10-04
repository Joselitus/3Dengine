# Visor de escenas

Muestra un fichero `.scene` en el navegador con el mismo sombreado que el juego, sin compilar ni ejecutar el motor. Usa three.js y contiene una copia de `animatedshader.vert`/`shader.frag`.

## Uso

```bash
python3 tools/scene_viewer/serve.py            # abre http://127.0.0.1:8000/tools/scene_viewer/
python3 tools/scene_viewer/serve.py --scene assets/scenes/otra.scene --view top
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
- **Opciones de visualización:**
  - Activar o desactivar la niebla, el cielo y la animación.
  - Rejilla de 1 m con ejes.
  - "Luz de día", para ver mejor la escena nocturna.
  - Captura PNG.

## Mantenerlo sincronizado con el motor

Hay tres puntos que deben coincidir con el motor:

| Visor (`viewer.js`) | Motor |
|---|---|
| `parseScene` | `SceneFile::load` (`src/SceneFile.cpp`) |
| `vertexShader` / `fragmentShader` | `src/animatedshader.vert` / `src/shader.frag` |
| `FOV`, `SENSIVILITY`, `PLAYER_HEIGHT`, `BREATH_AMPLITUDE`… | `Camera`, `Controller`, `AnimatedModel::computeFit`, `Scene.cpp` |

El pingüino se carga con el `FBXLoader` de three.js, no con Assimp. Su postura en cada momento puede variar un poco respecto al juego, pero el ajuste de tamaño y posición es el mismo.
