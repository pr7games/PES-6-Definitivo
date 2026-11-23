# PES 6 Definitivo

Port para PC de PES 6 Xbox 360, por PR7.

## Actualizaciones

Descarga los paquetes desde **Releases** o utiliza **Acerca de → Buscar actualizaciones** en el launcher. El acceso es publico: no necesitas Git, GitHub CLI ni una cuenta de GitHub.

Si tienes el launcher de prueba 0.1/0.2, su actualizacion puente 0.2.1 habilita el canal publico. Tambien puedes copiar el contenido de `PES6-Launcher-0.2.1.zip` sobre tu instalacion existente, con el juego y launcher cerrados. Despues actualiza a 0.3 desde el launcher.

El launcher descarga todas las partes necesarias, comprueba sus SHA-256 y aplica la actualizacion al cerrarse. Incluye recursos de juego y mods activos; conserva partidas y preferencias. Las bases de archivos del juego deben coincidir con una version soportada: si no coinciden, la instalacion se detiene antes de reemplazar archivos.

La version 0.3 es una entrega preliminar. No incluye el editor externo. El editor integrado del juego permanece disponible.

## Fuentes

`source/launcher-src`, `source/native`, `source/runtime` y `source/scripts` contienen el codigo seleccionado del port. `source/installer-src` conserva el codigo del instalador, sin sus datos de juego. El codigo generado desde el ejecutable original y los compiladores no se incluyen.

El launcher se compila en Windows con `powershell -File build-launcher.ps1`; `-UpdatePackage` genera la version publicada. El motor requiere ReXGlue SDK 0.10.0, Clang, CMake, Ninja, Node.js y la generacion local desde una copia compatible del juego. Los scripts nativos conservan rutas de herramientas que deben ajustarse al entorno de desarrollo.

`updates/latest.json` es el manifiesto publico del actualizador. Los ZIP descargables estan en Releases; no se guardan los mods como historial de Git.

Para preparar futuras entregas, `source/scripts/package-public-update.ps1` selecciona el runtime y admite un inventario anterior para retirar recursos obsoletos. `source/scripts/build-game-update.cjs` genera parches por bloques y admite una carpeta de juego de la version anterior. Cada nueva entrega debe publicar una version superior y sus paquetes antes de actualizar el manifiesto.

No se incluyen ISO/GOD, partidas, credenciales, archivos maestros de edicion, respaldos, pruebas ni documentacion interna. Los recursos originales de PES y las marcas pertenecen a sus respectivos titulares. Las dependencias conservan sus avisos y licencias. La publicacion de estas fuentes no concede derechos sobre recursos de terceros; aun no se ha definido una licencia general para el codigo propio.