# PES 6 DEFINITIVO by Golazo Virtual

Port para Windows de PES 6 Xbox 360. Proyecto independiente, no oficial. Entrega preliminar.

## Instalacion limpia - 0.3.1

1. Descarga [PES6-DEFINITIVO-Setup-0.3.1.exe](https://github.com/pr7games/PES-6-Definitivo/releases/download/v0.3.1/PES6-DEFINITIVO-Setup-0.3.1.exe). Es un unico EXE.
2. Prepara tu copia original compatible de PES 6 para Xbox 360 (EUROPE): ISO, carpeta GOD o archivos originales extraidos. El instalador no proporciona esa copia.
3. Ejecuta el instalador, selecciona el origen y elige una **carpeta de destino vacia**. No uses la carpeta de tu juego original ni sobrescribas una instalacion existente.
4. Pulsa **INSTALAR PES 6** y espera a que termine. Puedes crear un acceso directo.
5. Abre **PES 6 Launcher.exe**, elige Vulkan, D3D12 o Nativo y configura resolucion, idiomas y controles. Las mejoras opcionales empiezan desactivadas; puedes activarlas en Graficos.

Incluye el runtime y los mods activos necesarios. No incluye partidas, editor externo, documentacion interna ni Neural Rendering. Su codigo se conserva para futuras versiones. La carpeta save se crea vacia para tus partidas nuevas.

El instalador comprueba la copia original y aplica parches para preparar los archivos del juego. Una imagen incompatible o previamente modificada puede ser rechazada. El contenido instalado depende de la copia compatible aportada por el usuario.

## Actualizaciones

En el launcher utiliza **Acerca de > Buscar actualizaciones**. El launcher muestra progreso, verifica SHA-256 y conserva partidas y preferencias. Cierra el juego antes de actualizar.

## Fuentes y avisos

Las fuentes seleccionadas estan en source. El codigo generado desde el ejecutable del juego y las herramientas de compilacion no se incluyen. Para compilar el launcher: `powershell -File build-launcher.ps1`. El motor requiere ReXGlue SDK 0.10.0, Clang, CMake, Ninja, Node.js y generacion local desde la copia compatible del juego; ajusta las rutas de herramientas de los scripts.

Consulta [LEGAL.md](LEGAL.md), [NOTICE.txt](NOTICE.txt) y las licencias conservadas junto a las dependencias. La publicacion de fuentes no equivale por si sola a conceder una licencia open source; aun no se ha elegido una licencia general para el codigo propio.
