// ===========================================================================
//  exercises.h — Exercise library for KT1's active breaks
//
//  Designed for little space (~1.5 x 1.5 m), standing next to the desk with
//  dumbbells. Texts WITHOUT accents (the built-in TFT_eSPI fonts lack them).
//  Name: max ~21 chars (font 4) · Cue: max ~34 chars (font 2).
//  Each text has two columns: { "English", "Spanish" } (see lang.h).
//
//  Edit freely: add, remove or change reps. The firmware rotates the groups
//  LEG -> BACK -> ARM on each break and uses MOBILITY as the "no weights"
//  alternative (or when you are on a call).
// ===========================================================================
#pragma once
#include <stdint.h>
#include "lang.h"

enum Group : uint8_t { GR_LEG, GR_BACK, GR_ARM, GR_MOB, GR_COUNT };
static const char* const GROUP_NAMES_L[LANG_COUNT][GR_COUNT] = {
  { "Legs",   "Back",    "Arms",  "Mobility"  },
  { "Pierna", "Espalda", "Brazo", "Movilidad" },
};
#define GROUP_NAMES (GROUP_NAMES_L[gLang])

struct Exercise {
  const char* nameL[LANG_COUNT];   // on-screen name { en, es }
  const char* cueL[LANG_COUNT];    // technique cue, 1 line { en, es }
  uint8_t     group;     // Group
  uint8_t     reps;      // repetitions (0 = hold exercise)
  uint8_t     holdSec;   // seconds to hold if reps == 0
  bool        perSide;   // repeated with the other side / leg
  uint16_t    tempoMs;   // pace of each rep (guide beep)
  bool        weights;   // uses dumbbells
  const char* name() const { return nameL[gLang]; }
  const char* cue()  const { return cueL[gLang]; }
};

static const Exercise EXERCISES[] = {
  // ---------------- LEGS ----------------
  { { "Goblet squat",          "Sentadilla goblet"     }, { "Dumbbell at chest, knees out",       "Mancuerna al pecho, rodillas fuera"   }, GR_LEG,  10, 0, false, 3000, true  },
  { { "Romanian deadlift",     "Peso muerto rumano"    }, { "Flat back, push hips back",          "Espalda recta, lleva cadera atras"    }, GR_LEG,  10, 0, false, 3000, true  },
  { { "Reverse lunge",         "Zancada atras"         }, { "Upright torso, knee at 90 deg",      "Torso erguido, rodilla a 90 grados"   }, GR_LEG,   8, 0, true,  3000, true  },
  { { "Sumo squat",            "Sentadilla sumo"       }, { "Wide stance, toes pointing out",     "Pies anchos, puntas hacia fuera"      }, GR_LEG,  10, 0, false, 3000, true  },
  { { "Calf raises",           "Elevacion de talones"  }, { "Dumbbells at sides, rise slowly",    "Mancuernas a los lados, sube lento"   }, GR_LEG,  15, 0, false, 2000, true  },
  // ---------------- BACK ----------------
  { { "Bent-over row",         "Remo inclinado"        }, { "Torso at 45 deg, elbows back",       "Torso a 45 grados, codos atras"       }, GR_BACK, 10, 0, false, 3000, true  },
  { { "One-arm row",           "Remo a una mano"       }, { "Firm support: never a wheeled chair","Apoyo firme: nunca silla con ruedas"  }, GR_BACK, 10, 0, true,  3000, true  },
  { { "Reverse fly",           "Pajaros"               }, { "Light, squeeze shoulder blades","Peso ligero, junta las escapulas"   }, GR_BACK, 12, 0, false, 3000, true  },
  { { "Shrugs",                "Encogimientos"         }, { "Shoulders to ears, pause on top",    "Hombros a las orejas, pausa arriba"   }, GR_BACK, 12, 0, false, 2500, true  },
  // ---------------- ARMS / SHOULDERS ----------------
  { { "Biceps curl",           "Curl de biceps"        }, { "Elbows tucked, no swinging",         "Codos pegados, sin balancear"         }, GR_ARM,  10, 0, false, 3000, true  },
  { { "Hammer curl",           "Curl martillo"         }, { "Palms facing, lower slowly",         "Palmas enfrentadas, baja lento"       }, GR_ARM,  10, 0, false, 3000, true  },
  { { "Overhead press",        "Press militar"         }, { "Tight core, don't arch your back",   "Abdomen firme, no arquees la espalda" }, GR_ARM,  10, 0, false, 3000, true  },
  { { "Lateral raises",        "Elevaciones laterales" }, { "Light weight, only to shoulders",    "Peso ligero, solo hasta los hombros"  }, GR_ARM,  12, 0, false, 3000, true  },
  { { "Triceps extension",     "Extension de triceps"  }, { "One dumbbell behind your head",      "Una mancuerna detras de la cabeza"    }, GR_ARM,  10, 0, false, 3000, true  },
  // ---------------- MOBILITY (no weights) ----------------
  { { "Neck mobility",         "Movilidad de cuello"   }, { "Tilt and turn slowly, no pain",      "Inclina y gira despacio, sin dolor"   }, GR_MOB,   0, 20, true, 0,    false },
  { { "Chest opener",          "Apertura de pecho"     }, { "Hands behind, open your chest",      "Manos atras, abre el pecho"           }, GR_MOB,   0, 30, false,0,    false },
  { { "Wrist stretch",         "Estira las munecas"    }, { "Straight arm, pull hand gently",     "Brazo recto, tira suave de la mano"   }, GR_MOB,   0, 20, true, 0,    false },
  { { "Hip flexor stretch",    "Flexor de cadera"      }, { "Static lunge, hips forward",         "Zancada quieta, cadera al frente"     }, GR_MOB,   0, 30, true, 0,    false },
  { { "Shoulder circles",      "Circulos de hombros"   }, { "Big and slow, backwards",            "Grandes y lentos, hacia atras"        }, GR_MOB,  10, 0, false, 2000, false },
  { { "Air squat",             "Sentadilla sin peso"   }, { "Easy pace, no bouncing",             "Ritmo comodo, sin rebotar"            }, GR_MOB,  12, 0, false, 2500, false },
};
static const int EX_COUNT = sizeof(EXERCISES) / sizeof(EXERCISES[0]);
