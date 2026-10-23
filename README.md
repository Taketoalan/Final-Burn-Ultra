# FB Neo Ultra v.1.56

Frontend GTK3 en C++ para **FinalBurn Neo**. Navega tu biblioteca de ROMs, consulta
metadatos de los juegos, marca favoritos, filtra por sistema y lanza los títulos
directamente en el emulador.

## Características

- Interfaz oscura con estética arcade/neón.
- Detección automática del ejecutable de FBNeo, sus carpetas de ROMs y los DAT.
- Escaneo recursivo de ROMs `.zip` y `.7z` (sin extraerlas).
- Metadatos desde los DAT de FBNeo: título, año, fabricante y sistema.
- Búsqueda por título, nombre de ROM, sistema, año o fabricante.
- Filtros por sistema, favoritos y biblioteca completa.
- Panel de previsualización con artwork (`previews`, `titles`, `marquees`, `flyers`, `snap`).
- Lanzamiento directo con `fbneo <rom>` y restauración de la ventana al salir.
- Sincronización automática de las carpetas de ROMs con `fbneo.ini`.
- Generación de metadatos DAT desde la propia aplicación (`fbneo -dat`).
- Preferencias persistentes y atajos de teclado.

## Requisitos

- Linux con GTK 3.
- Compilador C++17 (GCC o Clang).
- CMake 3.16 o superior.
- `pkg-config`.

En Debian/Ubuntu/Linux Mint:

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev
```

También necesitas una instalación funcional de **FinalBurn Neo** y tus propias ROMs.
La aplicación detecta el ejecutable en ubicaciones habituales, entre ellas:

```text
/usr/bin/fbneo
/usr/local/bin/fbneo
~/.var/Org.Final_Burn_Neo/fbneo
```

## Compilar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## Ejecutar

```bash
./build/fbneo-ultra
```

## Instalar

Para instalarlo solo para tu usuario:

```bash
cmake --install build --prefix "$HOME/.local"
```

O de forma global:

```bash
sudo cmake --install build
```

Esto instala el binario, el icono y la entrada de escritorio. Después puedes abrir
**FB Neo Ultra** desde el menú de aplicaciones.

## Configuración

La configuración se guarda en:

```text
~/.config/fbneo-ultra/config.ini
```

Desde **Preferencias** (`Ctrl+,`) puedes ajustar:

- Ruta del ejecutable de FBNeo.
- Carpeta de metadatos DAT.
- Carpeta de artwork.
- Carpetas de ROMs.
- Pantalla completa.
- Sincronización de rutas con la configuración de FBNeo.
- Generación automática de DAT si faltan.
- Argumentos extra para el emulador.

### Metadatos DAT

FBNeo puede generar los DAT con:

```bash
fbneo -dat
```

Normalmente se crean en:

```text
~/.local/share/fbneo/support/lists/dat/
```

FB Neo Ultra puede generarlos automáticamente si activas la opción correspondiente
o desde el menú **Generar metadatos DAT**.

### Rutas de ROMs en FBNeo

La línea de comandos de FBNeo espera el **nombre corto** de la ROM, no la ruta
completa. Por ejemplo:

```bash
fbneo 1942
```

Por eso, al lanzar un juego, FB Neo Ultra sincroniza tus carpetas con
`szAppRomPaths` en:

```text
~/.local/share/fbneo/config/fbneo.ini
```

Antes de modificar el archivo se crea una copia de seguridad:

```text
fbneo.ini.fbneo-ultra.bak
```

FBNeo identifica cada juego por ese nombre corto. Si el mismo nombre de archivo
existe en varios sistemas, la biblioteca lo mostrará una sola vez. La
sincronización escribe como máximo las 20 rutas que admite `szAppRomPaths`.

## Atajos

| Atajo | Acción |
| --- | --- |
| `F5` / `Ctrl+R` | Actualizar biblioteca |
| `Ctrl+F` | Enfocar búsqueda |
| `Enter` | Jugar con el juego seleccionado |
| `Ctrl+,` | Preferencias |
| `Esc` | Limpiar la búsqueda |
| `Ctrl+Q` | Salir |

## Estructura del proyecto

```text
FBNeo-Ultra/
├── CMakeLists.txt
├── data/
│   └── fbneo-ultra.desktop
├── resources/
│   ├── fbneo-ultra.svg
│   ├── resources.gresource.xml
│   └── style.css
└── src/
    ├── config.*
    ├── database.*
    ├── main.cpp
    ├── main_window.*
    └── scanner.*
```

## Aviso legal

Este proyecto no incluye ROMs, BIOS ni el emulador FinalBurn Neo. Usa únicamente
copias de juegos que poseas legalmente y respeta la legislación de tu país.
