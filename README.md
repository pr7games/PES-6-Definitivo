# PES 6 Definitivo

Repositorio de desarrollo de PR7. Estado: preparacion privada, sin Release
y sin version estable publicada.

## Contenido de este borrador

- `source/launcher-src`: fuentes del launcher actual.
- `source/native`: fuentes nativas, configuracion y soporte CMake seleccionados.
- `source/scripts`: parcheador del codigo generado y sus dependencias locales.
- `updates/test`: propuesta de manifiesto de prueba, desactivada y sin paquetes.

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

El launcher incluido todavia tiene desactivado el actualizador.
`updates/test/manifest.json` no es un feed funcional ni una Release:
su formato es provisional, `enabled` es `false` y no contiene paquetes.
La autenticacion privada y la instalacion segura quedan por implementar.
Nunca distribuir un token compartido de GitHub dentro del launcher.

## Antes de publicar

Revisar procedencia y licencias de cada componente y decidir la licencia
del codigo propio. No se concede una licencia open source en este borrador.
Las fuentes nativas contienen implementaciones de efectos graficos; mantener
el repositorio privado hasta decidir cuales se abriran. Excluir `mods` solo
mantiene fuera los recursos, no las implementaciones de esos efectos.
