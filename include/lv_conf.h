/**
 * @file lv_conf.h
 * Configuration file for LVGL v8.4.0 - Optimized for M5Stack TAB5
 * Display: 1280x720 IPS, ESP32-P4, 32MB PSRAM
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 16 (RGB565) per display TAB5 */
#define LV_COLOR_DEPTH 16

/* Swap bytes per M5GFX (gestito da M5.Display.setSwapBytes) */
#define LV_COLOR_16_SWAP 0

/* Disable transparency per performance */
#define LV_COLOR_SCREEN_TRANSP 0

/* GPU blending optimization */
#define LV_COLOR_MIX_ROUND_OFS 128

/* Chroma key */
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

/*=========================
   MEMORY SETTINGS
 *=========================*/

/* Use custom malloc per PSRAM */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM == 1
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif

/* Buffer intermedi aumentati per risoluzione HD */
#define LV_MEM_BUF_MAX_NUM 32

/* Usa memcpy/memset ottimizzate */
#define LV_MEMCPY_MEMSET_STD 1

/*====================
   HAL SETTINGS
 *====================*/

/* Refresh veloce: 16ms (~60fps) */
#define LV_DISP_DEF_REFR_PERIOD 33

/* Input device read: 10ms per reattività touch */
#define LV_INDEV_DEF_READ_PERIOD 10

/* Custom tick da millis() */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/* DPI per display 5" HD */
#define LV_DPI_DEF 150

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/

/*-------------
 * Drawing
 *-----------*/

/* Complex draw: necessario per UI moderne */
#define LV_DRAW_COMPLEX 1
#if LV_DRAW_COMPLEX != 0
    /* Shadow cache ridotta per performance */
    #define LV_SHADOW_CACHE_SIZE 0
    
    /* Circle cache minima */
    #define LV_CIRCLE_CACHE_SIZE 2
#endif

/* Layer buffers ottimizzati per HD */
#define LV_LAYER_SIMPLE_BUF_SIZE          (64 * 1024)
#define LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE (8 * 1024)

/* Disable image cache per risparmiare RAM */
#define LV_IMG_CACHE_DEF_SIZE 0

/* Gradient stops minimi */
#define LV_GRADIENT_MAX_STOPS 2

/* Disable gradient cache */
#define LV_GRAD_CACHE_DEF_SIZE 0

/* Disable dithering per performance */
#define LV_DITHER_GRADIENT 0

/* Rotation buffer (se necessario) */
#define LV_DISP_ROT_MAX_BUF (10*1024)

/*-------------
 * GPU
 *-----------*/

/* Nessuna GPU specifica - usa DMA di M5GFX */
#define LV_USE_GPU_ARM2D 0
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_RA6M3_G2D 0
#define LV_USE_GPU_SWM341_DMA2D 0
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL 0

/*-------------
 * Logging
 *-----------*/

/* Disable logging in produzione per performance */
#define LV_USE_LOG 0

/*-------------
 * Asserts
 *-----------*/

/* Assert solo essenziali */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER while(1);

/*-------------
 * Others
 *-----------*/

/* Performance monitor: disabilitato in produzione */
#define LV_USE_PERF_MONITOR 1

/* Memory monitor: disabilitato */
#define LV_USE_MEM_MONITOR 1

/* Refresh debug: disabilitato */
#define LV_USE_REFR_DEBUG 0

/* sprintf standard */
#define LV_SPRINTF_CUSTOM 0
#if LV_SPRINTF_CUSTOM == 0
    #define LV_SPRINTF_USE_FLOAT 0
#endif

/* User data enabled */
#define LV_USE_USER_DATA 1

/* Garbage collector disabled */
#define LV_ENABLE_GC 0

/*=====================
 *  COMPILER SETTINGS
 *====================*/

#define LV_BIG_ENDIAN_SYSTEM 0

#define LV_ATTRIBUTE_TICK_INC
#define LV_ATTRIBUTE_TIMER_HANDLER
#define LV_ATTRIBUTE_FLUSH_READY

/* Alignment per DMA (32 byte per ESP32-P4) */
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 32
#define LV_ATTRIBUTE_MEM_ALIGN __attribute__((aligned(32)))

#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY

/* Fast mem per funzioni critiche */
#define LV_ATTRIBUTE_FAST_MEM __attribute__((section(".iram0.text")))

/* DMA accessible memory */
#define LV_ATTRIBUTE_DMA

#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/* Extended coordinates per display HD */
#define LV_USE_LARGE_COORD 1

/*==================
 *   FONT USAGE
 *===================*/

/* Font essenziali */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1

#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK            0

#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

#define LV_FONT_CUSTOM_DECLARE

#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_FONT_FMT_TXT_LARGE 1

#define LV_USE_FONT_COMPRESSED 0

#define LV_USE_FONT_SUBPX 0

#define LV_USE_FONT_PLACEHOLDER 1

/*=================
 *  TEXT SETTINGS
 *=================*/

#define LV_TXT_ENC LV_TXT_ENC_UTF8

#define LV_TXT_BREAK_CHARS " ,.;:-_"

#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

#define LV_TXT_COLOR_CMD "#"

#define LV_USE_BIDI 0

#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==================
 *  WIDGET USAGE
 *================*/

#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1

#define LV_USE_LABEL      1
#if LV_USE_LABEL
    #define LV_LABEL_TEXT_SELECTION 1
    #define LV_LABEL_LONG_TXT_HINT 1
#endif

#define LV_USE_LINE       1

#define LV_USE_ROLLER     1
#if LV_USE_ROLLER
    #define LV_ROLLER_INF_PAGES 7
#endif

#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1

#define LV_USE_TEXTAREA   1
#if LV_USE_TEXTAREA != 0
    #define LV_TEXTAREA_DEF_PWD_SHOW_TIME 1500
#endif

#define LV_USE_TABLE      1

/*==================
 * EXTRA COMPONENTS
 *==================*/

#define LV_USE_ANIMIMG    1

#define LV_USE_CALENDAR   1
#if LV_USE_CALENDAR
    #define LV_CALENDAR_WEEK_STARTS_MONDAY 0
    #define LV_CALENDAR_DEFAULT_DAY_NAMES {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"}
    #define LV_CALENDAR_DEFAULT_MONTH_NAMES {"January", "February", "March",  "April", "May",  "June", "July", "August", "September", "October", "November", "December"}
    #define LV_USE_CALENDAR_HEADER_ARROW 1
    #define LV_USE_CALENDAR_HEADER_DROPDOWN 1
#endif

#define LV_USE_CHART      1
#define LV_USE_COLORWHEEL 1
#define LV_USE_IMGBTN     1
#define LV_USE_KEYBOARD   1
#define LV_USE_LED        1
#define LV_USE_LIST       1
#define LV_USE_MENU       1
#define LV_USE_METER      1
#define LV_USE_MSGBOX     1

#define LV_USE_SPAN       1
#if LV_USE_SPAN
    #define LV_SPAN_SNIPPET_STACK_SIZE 64
#endif

#define LV_USE_SPINBOX    1
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    1
#define LV_USE_TILEVIEW   1
#define LV_USE_WIN        1

/*-----------
 * Themes
 *----------*/

#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK 0
    #define LV_THEME_DEFAULT_GROW 1
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

#define LV_USE_THEME_BASIC 1
#define LV_USE_THEME_MONO 1

/*-----------
 * Layouts
 *----------*/

#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/*---------------------
 * 3rd party libraries
 *--------------------*/

#define LV_USE_FS_STDIO 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0
#define LV_USE_FS_FATFS 0
#define LV_USE_FS_LITTLEFS 0

/* Image decoders: disabilitati per performance */
#define LV_USE_PNG 0
#define LV_USE_BMP 0
#define LV_USE_SJPG 0
#define LV_USE_GIF 0

#define LV_USE_QRCODE 0
#define LV_USE_FREETYPE 0
#define LV_USE_TINY_TTF 0
#define LV_USE_RLOTTIE 0
#define LV_USE_FFMPEG 0

/*-----------
 * Others
 *----------*/

#define LV_USE_SNAPSHOT 0
#define LV_USE_MONKEY 0
#define LV_USE_GRIDNAV 0
#define LV_USE_FRAGMENT 0
#define LV_USE_IMGFONT 0
#define LV_USE_MSG 0
#define LV_USE_IME_PINYIN 0

/*==================
* EXAMPLES
*==================*/

#define LV_BUILD_EXAMPLES 0

/*===================
 * DEMO USAGE
 ====================*/

#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_STRESS 0
#define LV_USE_DEMO_MUSIC 0

#endif /*LV_CONF_H*/