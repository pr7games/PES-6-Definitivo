# PES 6 DEFINITIVO by Golazo Virtual

Port para Windows de PES 6 Xbox 360. Proyecto independiente, no oficial. Entrega preliminar.

## Instalacion limpia - 0.3.1

1. Descarga [PES6-DEFINITIVO-Setup-0.3.1.exe](https://github.com/pr7games/PES-6-Definitivo/releases/download/v0.3.1/PES6-DEFINITIVO-Setup-0.3.1.exe). Es un unico EXE; no requiere una carpeta payload aparte.
2. Prepara tu copia original compatible de PES 6 para Xbox 360: ISO, carpeta GOD o archivos originales extraidos. El instalador no proporciona esa copia.
3. Ejecuta el instalador, selecciona el origen y elige una **carpeta de destino vacia**. No uses la carpeta de tu juego original ni sobrescribas una instalacion existente.
4. Pulsa **INSTALAR PES 6** y espera a que termine. Puedes crear un acceso directo.
5. Abre **PES 6 Launcher.exe**, elige Vulkan, D3D12 o Nativo y configura resolucion, idiomas y controles. Las mejoras opcionales empiezan desactivadas; puedes activarlas en Graficos.

Incluye el runtime y los mods activos necesarios. No incluye partidas, editor externo, documentacion interna ni Neural Rendering. Su codigo se conserva para futuras versiones. La carpeta save se crea vacia para tus partidas nuevas.

El instalador comprueba la copia original y aplica parches para preparar los archivos del juego. Una imagen incompatible o previamente modificada puede ser rechazada. El contenido instalado depende de la copia compatible aportada por el usuario.

## Actualizaciones

En el launcher utiliza **Acerca de > Buscar actualizaciones**. Las descargas son publicas: no necesitas cuenta de GitHub, Git ni GitHub CLI. El launcher muestra progreso, verifica SHA-256 y conserva partidas y preferencias. Cierra el juego antes de actualizar.

El instalador 0.3.1 es para instalaciones limpias. El canal automatico conserva por ahora la actualizacion 0.3; no se cambiara a 0.3.1 hasta publicar un paquete compatible para instalaciones existentes. Un launcher 0.3.1 no baja a 0.3.

Los launchers antiguos 0.1/0.2 reciben primero el puente 0.2.1. Si un parche indica que cv.img u otro archivo no coincide, la instalacion se detiene: esa base no esta soportada. Puedes realizar una instalacion limpia en otra carpeta y conservar aparte tus partidas. No reemplaces archivos de juego al azar para saltarte la comprobacion.

Las partes ZIP de la release 0.3 son paquetes del actualizador, no el instalador inicial. Para instalar desde cero usa el EXE 0.3.1.

## Integridad

El archivo `.exe.sha256` de la misma release permite verificar la descarga. En PowerShell: `Get-FileHash -Algorithm SHA256 'PES6-DEFINITIVO-Setup-0.3.1.exe'`. Compara el resultado con el checksum publicado.

## Fuentes y avisos

Las fuentes seleccionadas estan en source. El codigo generado desde el ejecutable del juego y las herramientas de compilacion no se incluyen. Para compilar el launcher: `powershell -File build-launcher.ps1`. El motor requiere ReXGlue SDK 0.10.0, Clang, CMake, Ninja, Node.js y generacion local desde la copia compatible del juego; ajusta las rutas de herramientas de los scripts.

Consulta [LEGAL.md](LEGAL.md), [NOTICE.txt](NOTICE.txt) y las licencias conservadas junto a las dependencias. La publicacion de fuentes no equivale por si sola a conceder una licencia open source; aun no se ha elegido una licencia general para el codigo propio.