# PES 6 Definitivo

Repositorio de desarrollo de PR7. Estado: preparacion privada, sin Release
y sin version estable publicada.

## Contenido de este borrador

- `source/launcher-src`: fuentes del launcher actual.
- `source/native`: fuentes nativas, configuracion y soporte CMake seleccionados.
- `source/scripts`: parcheador del codigo generado y sus dependencias locales.
- `updates/test`: manifiesto activo de prueba para actualizar el launcher 0.1 a 0.2.

No incluye el juego, mods, caras, kits, estadios, texturas, partidas,
recursos visuales del launcher, instaladores ni credenciales.
Tampoco incluye el codigo generado a partir del ejecutable del juego.

## Compilacion y alcance

Esta seleccion inicial no es todavia un checkout autonomo compilable.
El proyecto utiliza ReXGlue SDK 0.10.0, C++23, Clang, CMake, Ninja y Node.js.
El launcher utiliza C# y .NET Framework para Windows.
Faltan preparar instrucciones reproducibles, generacion local desde una copia
del juego aportada por el desarrollador y revision de componentes auxiliares
del runtime/extension. No ejecutar el parcheador contra el proyecto jugable
para probar esta copia.

## Actualizaciones

El launcher normal se compila como 0.1; `build-launcher.ps1 -UpdatePackage`
genera 0.2. El feed de prueba en main apunta a un ZIP en la rama privada
`update-test-packages`, fijado a un commit concreto y verificado por SHA-256.
No se ha creado ninguna Release ni etiqueta estable.

Este primer paquete solo reemplaza las cuatro copias del launcher, en la raiz
y en bin. No instala mods ni modifica game, save o configuraciones.
El helper espera el cierre del launcher, bloquea actualizaciones simultaneas
y restaura los archivos anteriores si falla la copia.

Para el repo privado se utiliza GitHub CLI ya autenticado. En este PC su ruta
se configura localmente en `data/config/updater-local.json` con el campo
`ghExe`. Esa configuracion no contiene tokens y no se sube al repositorio.
La sesion de Chrome no autentica al launcher. Esta modalidad es para pruebas
del desarrollador; el acceso para jugadores se debe definir por separado.
Nunca distribuir un token compartido de GitHub dentro del launcher.

Compilar el launcher: `powershell -File ./build-launcher.ps1`.
Comprobar el flujo aislado: `powershell -File ./test-updater.ps1 -GitHubCli RUTA_A_GH`.

## Antes de publicar

Revisar procedencia y licencias de cada componente y decidir la licencia
del codigo propio. No se concede una licencia open source en este borrador.
Las fuentes nativas contienen implementaciones de efectos graficos; mantener
el repositorio privado hasta decidir cuales se abriran. Excluir `mods` solo
mantiene fuera los recursos, no las implementaciones de esos efectos.
