# Uso de KT1

[English](../en/usage.md) | **Español** · [← Volver al README](../../README.es.md)

## Navegación

| Gesto | Acción |
|---|---|
| Deslizar en horizontal | Página anterior / siguiente (los puntos de abajo indican la posición) |
| Deslizar en vertical | Subir / bajar el brillo |
| Pulsación larga (en la mayoría de páginas) | Cambiar el color de acento |
| Pulsación larga en **Cara** | Abrir **Calma** (SOS respiración) |
| Pulsación larga en **Calma** | Terminar la sesión |
| Dedo apoyado sobre la cara | Los ojos siguen tu dedo |
| Inclinar KT1 (con GY-521) | Los ojos miran hacia ese lado |
| Agitarlo | Ojos en espiral, mareados, durante unos segundos |

Tras 90 s sin tocarlo vuelve a la **Cara** (*Face*), salvo que haya un pomodoro, una sesión
de Calma, un aviso o una pausa activa en curso.

### Sensor táctil (TTP223)

Su acción depende de lo que esté pasando (en este orden de prioridad):
Chat → pausa activa → aviso de postura → alerta de Vital → pomodoro → fase de Vital →
medición de planta → si no, **caricias**.

- **Toque corto** (< 0,4 s): acción principal (confirmar, empezar, pausar…) o un saludo.
- **Mantener** (≥ 0,9 s): acción fuerte (terminar, salir…) o mimos + ronroneo.
- En la página **Chat**: mantén pulsado para hablar (pulsar para hablar), toca para repetir la respuesta.
- En la página **Calma**: toca para pausar/avanzar, mantén pulsado para terminar.

## Páginas

| # | Página | Qué hace |
|---|---|---|
| 1 | **Cara** (*Face*) | Ojos animados con 10 estados de ánimo, parpadeo, gestos en reposo. Barra superior: hora, fase del ciclo y minutos restantes, temperatura exterior, Wi-Fi. Toque = siguiente estado de ánimo. Muestra los avisos de postura y las alertas de Vital |
| 2 | **Ejercicio** (*Exercise*) | Biblioteca del entrenador: plan del día pierna / espalda / brazos + coach guiado (3-2-1, pitido por repetición, cambio de lado, descanso). El ciclo del día se lleva desde Vital |
| — | *Entrenador* (superpuesto) | Guía cada pausa activa: cuenta atrás 3-2-1, un pitido por repetición, cambio de lado, descanso. Botones **Otro** (*Other*) / **Sin pesas** (*No weights*) / **Saltar** (*Skip*) |
| 3 | **Pomodoro** | *Trabajo* fijo + *Escritura/Ocio* en flow ascendente, tiempos por modo (25/5, 50/10, 15/15) editables inline, tonos propios por modo, tarea por voz (`me pongo con X`: la ata, arranca el foco y da un consejo que se ve en el descanso), auto-ciclo x4 + descanso largo, puntitos + contador hoy por modo, modo tranqui, Pausa/Fin (Fin pide 2 toques), avisos en línea propia que nunca tapan los botones |
| 4 | **Plantas** (*Plants*) | Catálogo con dibujo pixel-art. Modo explorador en vivo: luz cada ~0,4 s y temperatura/humedad (DHT11) cada 2 s con barras de rango ideal que se vuelven verdes + consejo (`Acerca a la ventana`...). Toca arriba o el sensor para la medición oficial de ~8 s (promediada) con nota 0-100 |
| 5 | **Vital** | Director del día: SENTADO / DE PIE / RELAJATE con 3 presets de 1 toque (Suave sin mesa 45/3, Clásico Cornell 20/8/2, Foco 50/10), cuenta atrás grande, barra de día Buckley 2h, línea de siguiente. Abre el coach de Ejercicio en las pausas |
| 6 | **Calma** (*Calm*) | SOS meditación/relajación (ver abajo): **Respirar 60 s** (suspiro cíclico), **Surfear impulso 3 min** (observar el craving como una ola) y **Anclar 5-4-3-2-1**. Contador hoy/total en el menú, sin rachas |
| 7 | **Tiempo** (*Weather*) | Open-Meteo (sin clave): icono, temperatura, sensación térmica, humedad, viento, máx./mín., probabilidad de lluvia, amanecer/atardecer. Cada 15 min; toque = actualizar |
| 8 | **Reloj** (*Clock*) | Hora grande, fecha, franja de la semana, día del año y semana ISO |
| 9 | **Chat** | Asistente de voz (ver más abajo) |
| 10 | **Sonido** (*Sound*) | Sonido activado/desactivado, volumen (toca un segmento para probarlo) |
| 11 | **Pantalla** (*Screen*) | Brillo (5 niveles), color de acento (Cian / Ambar / Verde / Rosa — *Cyan / Amber / Green / Pink*) e **Idioma** (*Language*) (English / Espanol) |
| 12 | **Tiempos** (*Timers*) | Concentración del pomodoro (15/25/35/45), descanso corto (5/10/15), descanso largo (15/20/30, auto cada 4º) y recordatorio antisedentarismo (OFF/15/30/45/60 min) |

Todos los ajustes se guardan en flash (NVS) y se conservan tras reiniciar.

## Idioma

Página **Pantalla** → fila **Idioma** → toca *English* o *Espanol*. Cambia al instante:
textos, nombres de las páginas, ejercicios, fechas, el idioma del asistente de voz y el idioma de
los textos que devuelve el bridge del PC. El primer arranque usa `DEFAULT_LANG` (inglés).

## LED RGB de placa (diagnóstico: está dentro de la carcasa y no se ve)

| Color | Significado |
|---|---|
| Azul intermitente | Conectando a la Wi-Fi |
| Rojo fijo | Sin Wi-Fi |
| Ámbar intermitente | Compilación de OpenCode en curso |
| Rojo intermitente | La compilación de OpenCode ha fallado |
| Morado fijo | Actualización de firmware (OTA) |
| Apagado | Todo bien |

## Módulo LED externo (opcional, ver hardware §2)

| Comportamiento | Significado |
|---|---|
| Encendido fijo | Actividad: fase del ciclo, pomodoro, pausa activa |
| Strobe rápido | Aviso: postura, Vital, build roto, sin Wi-Fi |
| Apagado | Reposo |

## Asistente de voz (página Chat)

Toca **HABLAR** (*TALK*) (graba 4 s) o mantén pulsado el sensor táctil. Los ojos muestran el estado: muy abiertos =
escuchando, ámbar = pensando, contentos = hablando. Pulsación larga = repetir la última respuesta.
El asistente responde en el idioma del dispositivo.

Con `pc-agent` en marcha en tu PC también puedes pedirle acciones:

| Dices (ejemplos) | Comando | Efecto en el PC |
|---|---|---|
| «silencia el PC» | `MUTE` | Activa/desactiva el silencio |
| «volumen al 40» | `VOL:40` | Volumen general |
| «pausa / reanuda la música» | `PLAY` | Tecla multimedia reproducir/pausa |
| «abre el bloc de notas» | `OPEN:<app>` | Abre una app solo si está en la **lista de permitidas** |
| «bloquea el PC» | `LOCK` → `CONFIRM` | Win+L (antes pregunta «¿seguro?») |
| «suspende el PC» | `SLEEP` → `CONFIRM` | Suspender (antes pregunta) |
| «¿qué hay en mi pantalla?» | `SCREEN` / captura | Describe la ventana activa / la captura |
| «pon el vídeo de X» | `PLAY:<text or URL>` | Abre la URL o una búsqueda en YouTube |
| «apunta comprar pan» | `TODO:<text>` | Lo añade a tu archivo TODO |
| «avísame en 5 minutos de …» | `REMIND:<s>:<text>` | Notificación de Windows + alerta en KT1 |
| «¿qué avisos tengo?» | `REMINDLIST` | Lee los recordatorios pendientes |
| «recuerda que me gusta …» | `MEMORY:<fact>` | Guarda un dato en la memoria local |
| «¿qué recuerdas?» | `MEMLIST` | Lee los datos guardados |
| «dame el parte» | `BRIEF` | Recordatorios + TODO + memoria + pantalla |
| «no molestar» | `DND` | Activa/desactiva las notificaciones de Windows |
| «empareja el PC» | `PAIR` | Obtiene el token del bridge sin editar `secrets.h` (los primeros 15 min tras arrancar el bridge) |
| «manda a la sesión que revise X» | `PROMPT:<text>` | Envía un prompt a una sesión de OpenCode |
| «resume la sesión» | `RESUME` | Lee los últimos mensajes de OpenCode |
| «necesito calmarme / tengo craving» | `PAGE:Calma` | Abre la ventana Calma desde el principio (menú SOS) |
| «pon el pomodoro» | `PAGE:` | Cambia la página del dispositivo |
| «me pongo con el informe» | `FOCUS:<modo>:<tarea>` | Ata la tarea al pomodoro (clasifica trabajo/escritura/ocio), arranca el foco y da un consejo de voz para ese bloque |

## Ventana Calma (página Calma + PC)

Para momentos de tensión, estrés o impulso/craving. Acceso SOS: mantén pulsado en la
**Cara**, di «necesito calmarme», o abre `Calma` en el PC (`python calm_window.py`).

| Modo | Qué hace | Base |
|---|---|---|
| **Respirar 60 s** | Suspiro cíclico guiado: doble inhalación + exhalación larga, 6 ciclos | Balban et al. 2023 (*Cell Reports Medicine*): 5 min/día mejora ánimo y baja activación más que mindfulness |
| **Surfear impulso 3 min** | Puntúas de 0-10, observas la «ola» sin actuar, vuelves a puntuar | Bowen & Marlatt 2009: urge surfing no quita el craving al momento, pero cambia la respuesta (menos consumo a 7 días) |
| **Anclar 5-4-3-2-1** | Ver 5 → tocar 4 → oír 3 → sentir 2 → respirar 1 | Anclaje sensorial de consenso clínico para pánico (evidencia directa limitada) |

La primera vez muestra un aviso (no es terapia) que se acepta con OK. Solo guarda
contadores (hoy/total, última puntuación) en flash; nada sensible sale del dispositivo.
En el PC, `calm_window.py` añade respiración en grande, botón **DND 5 min** (silencia
notificaciones y las restaura solo) y apertura remota de Calma en KT1.

> ⚠️ **Esto no es tratamiento médico ni de adicciones.** Si el impulso es fuerte o hay
> malestar persistente, pide ayuda profesional (p. ej., 024 en España).

## Notas de salud

El ciclo 20-8-2 sigue las pautas de Cornell University Ergonomics (*Sitting and Standing at Work*);
el objetivo diario de 2 h de pie/actividad ligera sigue a Buckley et al. 2015 (*Br J Sports
Med*); el trabajo de fuerza de todos los grupos musculares principales ≥ 2 días/semana sigue las
directrices de la OMS de 2020. Los ejercicios están en `firmware/kt1-deskpet/exercises.h`.

> ⚠️ **Esto no es un programa médico.** Usa un peso que te deje 2–3 repeticiones en reserva, calienta
> con la versión *sin pesas*, mantén la espalda neutra, no te apoyes nunca en una mesa elevable subida
> ni en una silla con ruedas, deja las mancuernas en el suelo y para si notas dolor agudo,
> mareo o falta de aire. Si tienes lesiones o problemas de salud, consulta antes a tu
> médico.
