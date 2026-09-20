# Sonic Doctor

Analizador de audio en tiempo real que diagnostica un sonido y da
recomendaciones concretas — tanto generales como específicas para tus
propios plugins (KickForge EQ, Surgical De-Esser, y los que hagas después).

## Qué mide (todo con DSP real, no adivina nada)

- **Loudness aproximado** (Short-term LUFS, ventana ~3s, K-weighting simplificado)
- **Pico, RMS y Crest Factor** (indicador de dinámica/compresión)
- **Correlación de fase** (problemas de fase entre canales L/R)
- **Balance espectral en 7 bandas**, comparado contra una curva de referencia
  típica según el tipo de fuente que elijas (Genérico, Voz, Kick, Bajo, Mezcla)
- **Sibilancia** (energía 4-10kHz) y cuánto excede lo típico
- **Rumble** (ruido bajo 40Hz)
- **Detección de resonancias**: picos angostos que sobresalen del promedio local
- **Clipping, offset de DC, piso de ruido**

## Qué NO hace (y por qué, para que no haya sorpresas)

- **No detecta qué efectos ya tiene aplicado un sonido.** Eso es un problema
  de investigación en Machine Learning (requiere modelos entrenados con
  miles de ejemplos), no algo que se resuelva con análisis de señal
  tradicional. Ningún plugin comercial serio promete esto de forma confiable.
- **No carga ni controla plugins de terceros automáticamente.** Hacer que un
  plugin "hostee" otros plugins dentro de sí mismo es un proyecto de
  ingeniería del tamaño de un mini-DAW, y muchos hosts (incluyendo
  probablemente Studio One) no lo soportan de forma estable. En vez de eso,
  Sonic Doctor te dice **exactamente qué valores poner** en tus propios
  plugins — tú los aplicas con un clic, pero la decisión de qué ajustar la
  toma el análisis, no tu oído a ciegas.

## Arquitectura (para que sea fácil agregar tu próximo plugin)

```
SoundProfile          <- todo lo medido, en un solo lugar
      |
      v
RecommendationEngine  <- reparte el SoundProfile a cada recomendador
      |
      +--> KickForgeEQRecommender
      +--> SurgicalDeEsserRecommender
      +--> (tu próximo plugin aquí)
```

### Cómo agregar soporte para un plugin nuevo tuyo

1. Copia `Source/Recommendations/SurgicalDeEsserRecommender.h` y `.cpp` como
   plantilla, y renómbralos con el nombre de tu plugin nuevo.
2. Dentro de `generate()`, lee lo que necesites de `SoundProfile` (ver
   `Source/SoundProfile.h` — ahí está TODO lo disponible: loudness, bandas,
   resonancias, sibilancia, fase, etc.) y arma tu lista de `Recommendation`.
3. En `Source/Recommendations/RecommendationEngine.cpp`, agrega una sola
   línea en el constructor:
   ```cpp
   recommenders.push_back (std::make_unique<TuNuevoPluginRecommender>());
   ```
4. Agrega tu `.cpp` nuevo a `CMakeLists.txt` (`target_sources`).
5. Listo — aparece automáticamente como una pestaña nueva en la interfaz,
   sin tocar el motor de análisis ni la interfaz para nada más.

## Compilar

```bash
git clone <tu-repo>
cd SonicDoctor
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
El VST3 queda en `build/SonicDoctor_artefacts/Release/VST3/`.

También puedes usar el workflow de GitHub Actions incluido
(`.github/workflows/build.yml`, solo Windows por estabilidad).

## Instalar en Studio One

Copia el `.vst3` a `C:\Program Files\Common Files\VST3\` y reescanea plugins
en Studio One (Opciones > Ubicaciones > VST Plug-Ins > Reescanear).

## Nota sobre precisión

Los valores de LUFS, curvas de referencia y umbrales de "lo típico" son
aproximaciones prácticas para guiar decisiones de mezcla — no son mediciones
certificadas para broadcast (EBU R128 / ATSC A/85) ni sustituyen tu propio
criterio. Úsalo como punto de partida, no como verdad absoluta.
