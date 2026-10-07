# Uso de KT1

[English](../en/usage.md) | **Español** · [← Volver al README](../../README.es.md)

## Navegación

| Gesto | Acción |
|---|---|
| Deslizar en horizontal | Página anterior / siguiente (los puntos de abajo indican la posición) |
| Deslizar en vertical | Subir / bajar el brillo |
| Pulsación larga (en la mayoría de páginas) | Cambiar el color de acento |
| Dedo apoyado sobre la cara | Los ojos siguen tu dedo |
| Inclinar KT1 (con GY-521) | Los ojos miran hacia ese lado |
| Agitarlo | Ojos en espiral, mareados, durante unos segundos |

Tras 90 s sin tocarlo vuelve a la **Cara** (*Face*), salvo que haya un pomodoro, un aviso o una
pausa activa en curso.

### Sensor táctil (TTP223)

Su acción depende de lo que esté pasando (en este orden de prioridad):
Chat → pausa activa → aviso de postura → alerta de Vital → pomodoro → fase de Vital →
medición de planta → si no, **caricias**.

- **Toque corto** (< 0,4 s): acción principal (confirmar, empezar, pausar…) o un saludo.
- **Mantener** (≥ 0,9 s): acción fuerte (terminar, salir…) o mimos + ronroneo.
- En la página **Chat**: mantén pulsado para hablar (pulsar para hablar), toca para repetir la respuesta.

## Páginas

| # | Página | Qué hace |
|---|---|---|
| 1 | **Cara** (*Face*) | Ojos animados con 10 estados de ánimo, parpadeo, gestos en reposo. Barra superior: hora, fase del ciclo y minutos restantes, temperatura exterior, Wi-Fi. Toque = siguiente estado de ánimo. Muestra los avisos de postura y las alertas de Vital |
| 2 | **Ejercicio** (*Exercise*) | Ciclo sentado/de pie **20 min sentado → 8 min de pie → 2 min en movimiento** (Cornell). Cada 4 ciclos, una pausa más larga en circuito. Grupo muscular del día (piernas / espalda / brazos) |
| — | *Entrenador* (superpuesto) | Guía cada pausa activa: cuenta atrás 3-2-1, un pitido por repetición, cambio de lado, descanso. Botones **Otro** (*Other*) / **Sin pesas** (*No weights*) / **Saltar** (*Skip*) |
| 3 | **Pomodoro** | *Trabajo* fijo + *Escritura/Ocio* en flow ascendente, tiempos por modo (25/5, 50/10, 15/15) editables inline, tonos propios por modo, tarea por voz (`me pongo con X`: la ata, arranca el foco y da un consejo que se ve en el descanso), auto-ciclo x4 + descanso largo, puntitos + contador hoy por modo, modo tranqui, Pausa/Fin (Fin pide 2 toques), avisos en línea propia que nunca tapan los botones |
| 4 | **Plantas** (*Plants*) | Elige una planta y toca **Medir** (*Measure*): ~8 s de luz (LDR), temperatura y humedad (DHT11) comparadas con sus rangos ideales → **Bajo / Ideal / Alto** (*Low / Ideal / High*) |
| 5 | **Vital** | Cambios de postura alternativos SENTADO / DE PIE / RELAJATE (*SITTING / STANDING / RELAX*) (solo movilidad). Intervalos configurables. Excluyente con *Ejercicio* |
| 6 | **Tiempo** (*Weather*) | Open-Meteo (sin clave): icono, temperatura, sensación térmica, humedad, viento, máx./mín., probabilidad de lluvia, amanecer/atardecer. Cada 15 min; toque = actualizar |
| 7 | **Reloj** (*Clock*) | Hora grande, fecha, franja de la semana, día del año y semana ISO |
| 8 | **Chat** | Asistente de voz (ver más abajo) |
| 9 | **Sonido** (*Sound*) | Sonido activado/desactivado, volumen (toca un segmento para probarlo) |
| 10 | **Pantalla** (*Screen*) | Brillo (5 niveles), color de acento (Cian / Ambar / Verde / Rosa — *Cyan / Amber / Green / Pink*) e **Idioma** (*Language*) (English / Espanol) |
| 11 | **Tiempos** (*Timers*) | Concentración del pomodoro (15/25/35/45), descanso corto (5/10/15), descanso largo (15/20/30, auto cada 4º) y recordatorio antisedentarismo (OFF/15/30/45/60 min) |

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
| «pon el pomodoro» | `PAGE:` | Cambia la página del dispositivo |
| «me pongo con el informe» | `FOCUS:<modo>:<tarea>` | Ata la tarea al pomodoro (clasifica trabajo/escritura/ocio), arranca el foco y da un consejo de voz para ese bloque |

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
