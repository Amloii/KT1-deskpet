// ===========================================================================
//  lang.h — UI language (English / Spanish), switchable at runtime
//
//  - gLang is stored in NVS (key "lang") and changed on the SCREEN settings page.
//  - TR("English", "Spanish") picks the text for the current language.
//  - Tables use one column per language: TABLE[gLang][i].
//  - Texts WITHOUT accents: the built-in TFT_eSPI fonts are ASCII only.
//  - Keep translations about the same length: buttons have a fixed width.
// ===========================================================================
#pragma once
#include <stdint.h>

enum Lang : uint8_t { LANG_EN, LANG_ES, LANG_COUNT };

static uint8_t gLang = LANG_EN;

#define TR(en, es) (gLang == LANG_ES ? (es) : (en))

static const char* const LANG_NAMES[LANG_COUNT] = { "English", "Espanol" };
static const char* const LANG_CODES[LANG_COUNT] = { "en", "es" };   // TTS language
