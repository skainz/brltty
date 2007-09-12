/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2007 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
 *
 * Web Page: http://mielke.cc/brltty/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

/*
 * config.c - Everything configuration related.
 */

#include "prologue.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif /* HAVE_SYS_WAIT_H */

#ifdef ENABLE_PREFERENCES_MENU
#ifdef ENABLE_TABLE_SELECTION
#ifdef HAVE_GLOB_H
#include <glob.h>
#endif /* HAVE_GLOB_H */
#endif /* ENABLE_TABLE_SELECTION */
#endif /* ENABLE_PREFERENCES_MENU */

#include "cmd.h"
#include "brl.h"
#include "spk.h"
#include "scr.h"
#include "tbl.h"
#include "ctb.h"
#include "tunes.h"
#include "message.h"
#include "misc.h"
#include "system.h"
#include "async.h"
#include "program.h"
#include "options.h"
#include "brltty.h"
#include "defaults.h"
#include "io_serial.h"

#ifdef ENABLE_USB_SUPPORT
#include "io_usb.h"
#endif /* ENABLE_USB_SUPPORT */

#ifdef ENABLE_BLUETOOTH_SUPPORT
#include "io_bluetooth.h"
#endif /* ENABLE_BLUETOOTH_SUPPORT */

#ifdef __MINGW32__
#include "sys_windows.h"

static int opt_installService;
static int opt_removeService;
#endif /* __MINGW32__ */

#ifdef __MSDOS__
#include "sys_msdos.h"
#endif /* __MSDOS__ */

static int opt_version;
static int opt_verify;
static int opt_quiet;
static int opt_noDaemon;
static int opt_standardError;
static char *opt_logLevel;
static int opt_bootParameters = 1;
static int opt_environmentVariables;
static char *opt_updateInterval;
static char *opt_messageDelay;

static char *opt_configurationFile;
static char *opt_pidFile;
static char *opt_writableDirectory;
static char *opt_dataDirectory;
static char *opt_libraryDirectory;

static char *opt_brailleDevice;
int opt_releaseDevice;
static char **brailleDevices;
static const char *brailleDevice = NULL;
static int brailleConstructed;

static char *opt_brailleDriver;
static char **brailleDrivers;
static const BrailleDriver *brailleDriver = NULL;
static void *brailleObject;
static char *opt_brailleParameters;
static char **brailleParameters = NULL;
static char *preferencesFile = NULL;

static char *opt_tablesDirectory;
static char *opt_textTable;
static char *opt_attributesTable;

#ifdef ENABLE_CONTRACTED_BRAILLE
static char *opt_contractionsDirectory;
static char *opt_contractionTable;
#endif /* ENABLE_CONTRACTED_BRAILLE */

#ifdef ENABLE_API
static int opt_noApi;
static char *opt_apiParameters;
static char **apiParameters = NULL;
int apiStarted;
#endif /* ENABLE_API */

#ifdef ENABLE_SPEECH_SUPPORT
static char *opt_speechDriver;
static char **speechDrivers = NULL;
static const SpeechDriver *speechDriver = NULL;
static void *speechObject;
static char *opt_speechParameters;
static char **speechParameters = NULL;
static char *opt_speechFifo;
#endif /* ENABLE_SPEECH_SUPPORT */

static char *opt_screenDriver;
static char **screenDrivers;
static const ScreenDriver *screenDriver = NULL;
static void *screenObject;
static char *opt_screenParameters;
static char **screenParameters = NULL;

#ifdef ENABLE_PCM_SUPPORT
char *opt_pcmDevice;
#endif /* ENABLE_PCM_SUPPORT */

#ifdef ENABLE_MIDI_SUPPORT
char *opt_midiDevice;
#endif /* ENABLE_MIDI_SUPPORT */

static const char *const optionStrings_LogLevel[] = {
  "0-7 [5]",
  "emergency alert critical error warning [notice] information debug",
  NULL
};

static const char *const optionStrings_BrailleDriver[] = {
  BRAILLE_DRIVER_CODES,
  NULL
};

static const char *const optionStrings_ScreenDriver[] = {
  SCREEN_DRIVER_CODES,
  NULL
};

#ifdef ENABLE_SPEECH_SUPPORT
static const char *const optionStrings_SpeechDriver[] = {
  SPEECH_DRIVER_CODES,
  NULL
};
#endif /* ENABLE_SPEECH_SUPPORT */

BEGIN_OPTION_TABLE
  { .letter = 'a',
    .word = "attributes-table",
    .flags = OPT_Config | OPT_Environ,
    .argument = strtext("file"),
    .setting.string = &opt_attributesTable,
    .description = strtext("Path to attributes translation table file.")
  },

  { .letter = 'b',
    .word = "braille-driver",
    .bootParameter = 1,
    .flags = OPT_Config | OPT_Environ,
    .argument = strtext("driver"),
    .setting.string = &opt_brailleDriver,
    .defaultSetting = "auto",
    .description = strtext("Braille driver: one of {%s}"),
    .strings = optionStrings_BrailleDriver
  },

#ifdef ENABLE_CONTRACTED_BRAILLE
  { .letter = 'c',
    .word = "contraction-table",
    .flags = OPT_Config | OPT_Environ,
    .argument = strtext("file"),
    .setting.string = &opt_contractionTable,
    .description = strtext("Path to contraction table file.")
  },
#endif /* ENABLE_CONTRACTED_BRAILLE */

  { .letter = 'd',
    .word = "braille-device",
    .bootParameter = 2,
    .flags = OPT_Config | OPT_Environ,
    .argument = strtext("device"),
    .setting.string = &opt_brailleDevice,
    .defaultSetting = BRAILLE_DEVICE,
    .description = strtext("Path to device for accessing braille display.")
  },

  { .letter = 'e',
    .word = "standard-error",
    .setting.flag = &opt_standardError,
    .description = strtext("Log to standard error rather than to the system log.")
  },

  { .letter = 'f',
    .word = "configuration-file",
    .flags = OPT_Environ,
    .argument = strtext("file"),
    .setting.string = &opt_configurationFile,
    .defaultSetting = CONFIGURATION_DIRECTORY "/" CONFIGURATION_FILE,
    .description = strtext("Path to default settings file.")
  },

  { .letter = 'l',
    .word = "log-level",
    .argument = strtext("level"),
    .setting.string = &opt_logLevel,
    .description = strtext("Diagnostic logging level: %s, or one of {%s}"),
    .strings = optionStrings_LogLevel
  },

#ifdef ENABLE_MIDI_SUPPORT
  { .letter = 'm',
    .word = "midi-device",
    .flags = OPT_Config | OPT_Environ,
    .argument = strtext("device"),
    .setting.string = &opt_midiDevice,
    .description = strtext("Device specifier for the Musical Instrument Digital Interface.")
  },
#endif /* ENABLE_MIDI_SUPPORT */

  { .letter = 'n',
    .word = "no-daemon",
    .setting.flag = &opt_noDaemon,
    .description = strtext("Remain a foreground process.")
  },

#ifdef ENABLE_PCM_SUPPORT
  { .letter = 'p',
    .word = "pcm-device",
    .flags = OPT_Config | OPT_Environ,
    .argument = strtext("device"),
    .setting.string = &opt_pcmDevice,
    .description = strtext("Device specifier for soundcard digital audio.")
  },
#endif /* ENABLE_PCM_SUPPORT */

  { .letter = 'q',
    .word = "quiet",
    .setting.flag = &opt_quiet,
    .description = strtext("Suppress start-up messages.")
  },

  { .letter = 'r',
    .word = "release-device",
    .flags = OPT_Config | OPT_Environ,
    .setting.flag = &opt_releaseDevice,
#ifdef WINDOWS
    .defaultSetting = FLAG_TRUE_WORD,
#else /* WINDOWS */
    .defaultSetting = FLAG_FALSE_WORD,
#endif /* WINDOWS */
    .description = strtext("Release braille device when screen or window is unreadable.")
  },

#ifdef ENABLE_SPEECH_SUPPORT
  { .letter = 's',
    .word = "speech-driver",
    .flags = OPT_Config | OPT_Environ,
    .argument = strtext("driver"),
    .setting.string = &opt_speechDriver,
    .defaultSetting = "auto",
    .description = strtext("Speech driver: one of {%s}"),
    .strings = optionStrings_SpeechDriver
  },
#endif /* ENABLE_SPEECH_SUPPORT */

  { .letter = 't',
    .word = "text-table",
    .bootParameter = 3,
    .flags = OPT_Config | OPT_Environ,
    .argument = strtext("file"),
    .setting.string = &opt_textTable,
    .description = strtext("Path to text translation table file.")
  },

  { .letter = 'v',
    .word = "verify",
    .setting.flag = &opt_verify,
    .description = strtext("Print start-up messages and exit.")
  },

  { .letter = 'x',
    .word = "screen-driver",
    .flags = OPT_Config | OPT_Environ,
    .argument = strtext("driver"),
    .setting.string = &opt_screenDriver,
    .defaultSetting = SCREEN_DRIVER,
    .description = strtext("Screen driver: one of {%s}"),
    .strings = optionStrings_ScreenDriver
  },

#ifdef ENABLE_API
  { .letter = 'A',
    .word = "api-parameters",
    .flags = OPT_Extend | OPT_Config | OPT_Environ,
    .argument = strtext("arg,..."),
    .setting.string = &opt_apiParameters,
    .defaultSetting = API_PARAMETERS,
    .description = strtext("Parameters for the application programming interface.")
  },
#endif /* ENABLE_API */

  { .letter = 'B',
    .word = "braille-parameters",
    .flags = OPT_Extend | OPT_Config | OPT_Environ,
    .argument = strtext("arg,..."),
    .setting.string = &opt_brailleParameters,
    .defaultSetting = BRAILLE_PARAMETERS,
    .description = strtext("Parameters for the braille driver.")
  },

#ifdef ENABLE_CONTRACTED_BRAILLE
  { .letter = 'C',
    .word = "contractions-directory",
    .flags = OPT_Hidden | OPT_Config | OPT_Environ,
    .argument = strtext("directory"),
    .setting.string = &opt_contractionsDirectory,
    .defaultSetting = DATA_DIRECTORY,
    .description = strtext("Path to directory for contractions tables.")
  },
#endif /* ENABLE_CONTRACTED_BRAILLE */

  { .letter = 'D',
    .word = "data-directory",
    .flags = OPT_Hidden | OPT_Config | OPT_Environ,
    .argument = strtext("directory"),
    .setting.string = &opt_dataDirectory,
    .defaultSetting = DATA_DIRECTORY,
    .description = strtext("Path to directory for driver help and configuration files.")
  },

  { .letter = 'E',
    .word = "environment-variables",
    .setting.flag = &opt_environmentVariables,
    .description = strtext("Recognize environment variables.")
  },

#ifdef ENABLE_SPEECH_SUPPORT
  { .letter = 'F',
    .word = "speech-fifo",
    .flags = OPT_Config | OPT_Environ,
    .argument = strtext("file"),
    .setting.string = &opt_speechFifo,
    .description = strtext("Path to speech pass-through FIFO.")
  },
#endif /* ENABLE_SPEECH_SUPPORT */

#ifdef __MINGW32__
  { .letter = 'I',
    .word = "install-service",
    .setting.flag = &opt_installService,
    .description = strtext("Install Windows service.")
  },
#endif /* __MINGW32__ */


  { .letter = 'L',
    .word = "library-directory",
    .flags = OPT_Hidden | OPT_Config | OPT_Environ,
    .argument = strtext("directory"),
    .setting.string = &opt_libraryDirectory,
    .defaultSetting = LIBRARY_DIRECTORY,
    .description = strtext("Path to directory for loading drivers.")
  },

  { .letter = 'M',
    .word = "message-delay",
    .argument = strtext("csecs"),
    .setting.string = &opt_messageDelay,
    .description = strtext("Message hold time [400].")
  },

#ifdef ENABLE_API
  { .letter = 'N',
    .word = "no-api",
    .setting.flag = &opt_noApi,
    .description = strtext("Disable the application programming interface.")
  },
#endif /* ENABLE_API */

  { .letter = 'P',
    .word = "pid-file",
    .argument = strtext("file"),
    .setting.string = &opt_pidFile,
    .description = strtext("Path to process identifier file.")
  },

#ifdef __MINGW32__
  { .letter = 'R',
    .word = "remove-service",
    .setting.flag = &opt_removeService,
    .description = strtext("Remove Windows service.")
  },
#endif /* __MINGW32__ */

#ifdef ENABLE_SPEECH_SUPPORT
  { .letter = 'S',
    .word = "speech-parameters",
    .flags = OPT_Extend | OPT_Config | OPT_Environ,
    .argument = strtext("arg,..."),
    .setting.string = &opt_speechParameters,
    .defaultSetting = SPEECH_PARAMETERS,
    .description = strtext("Parameters for the speech driver.")
  },
#endif /* ENABLE_SPEECH_SUPPORT */

  { .letter = 'T',
    .word = "tables-directory",
    .flags = OPT_Hidden | OPT_Config | OPT_Environ,
    .argument = strtext("directory"),
    .setting.string = &opt_tablesDirectory,
    .defaultSetting = DATA_DIRECTORY,
    .description = strtext("Path to directory for text and attributes tables.")
  },

  { .letter = 'U',
    .word = "update-interval",
    .argument = strtext("csecs"),
    .setting.string = &opt_updateInterval,
    .description = strtext("Braille window update interval [4].")
  },

  { .letter = 'V',
    .word = "version",
    .setting.flag = &opt_version,
    .description = strtext("Print the versions of the core, API, and built-in drivers, and then exit.")
  },

  { .letter = 'W',
    .word = "writable-directory",
    .flags = OPT_Hidden | OPT_Config | OPT_Environ,
    .argument = strtext("directory"),
    .setting.string = &opt_writableDirectory,
    .defaultSetting = WRITABLE_DIRECTORY,
    .description = strtext("Path to directory which can be written to.")
  },

  { .letter = 'X',
    .word = "screen-parameters",
    .flags = OPT_Extend | OPT_Config | OPT_Environ,
    .argument = strtext("arg,..."),
    .setting.string = &opt_screenParameters,
    .defaultSetting = SCREEN_PARAMETERS,
    .description = strtext("Parameters for the screen driver.")
  },
END_OPTION_TABLE

static void
parseParameters (
  char **values,
  const char *const *names,
  const char *qualifier,
  const char *parameters
) {
  if (parameters && *parameters) {
    char *copy = strdupWrapper(parameters);
    char *name = copy;

    while (1) {
      char *end = strchr(name, ',');
      int done = end == NULL;
      if (!done) *end = 0;

      if (*name) {
        char *value = strchr(name, '=');
        if (!value) {
          LogPrint(LOG_ERR, "%s: %s", gettext("missing parameter value"), name);
        } else if (value == name) {
        noName:
          LogPrint(LOG_ERR, "%s: %s", gettext("missing parameter name"), name);
        } else {
          int nameLength = value++ - name;
          int eligible = 1;

          if (qualifier) {
            char *colon = memchr(name, ':', nameLength);
            if (colon) {
              int qualifierLength = colon - name;
              int nameAdjustment = qualifierLength + 1;
              eligible = 0;
              if (!qualifierLength) {
                LogPrint(LOG_ERR, "%s: %s", gettext("missing parameter qualifier"), name);
              } else if (!(nameLength -= nameAdjustment)) {
                goto noName;
              } else if ((qualifierLength == strlen(qualifier)) &&
                         (memcmp(name, qualifier, qualifierLength) == 0)) {
                name += nameAdjustment;
                eligible = 1;
              }
            }
          }

          if (eligible) {
            unsigned int index = 0;
            while (names[index]) {
              if (strncasecmp(name, names[index], nameLength) == 0) {
                free(values[index]);
                values[index] = strdupWrapper(value);
                break;
              }
              ++index;
            }

            if (!names[index]) {
              LogPrint(LOG_ERR, "%s: %s", gettext("unsupported parameter"), name);
            }
          }
        }
      }

      if (done) break;
      name = end + 1;
    }

    free(copy);
  }
}

static char **
processParameters (
  const char *const *names,
  const char *qualifier,
  const char *parameters
) {
  char **values;

  if (!names) {
    static const char *const noNames[] = {NULL};
    names = noNames;
  }

  {
    unsigned int count = 0;
    while (names[count]) ++count;
    values = mallocWrapper((count + 1) * sizeof(*values));
    values[count] = NULL;
    while (count--) values[count] = strdupWrapper("");
  }

  parseParameters(values, names, qualifier, parameters);
  return values;
}

static void
logParameters (const char *const *names, char **values, char *description) {
  if (names && values) {
    while (*names) {
      LogPrint(LOG_INFO, "%s: %s=%s", description, *names, *values);
      ++names;
      ++values;
    }
  }
}

static int
replaceTranslationTable (TranslationTable table, const char *file) {
  int ok = 0;
  char *path = makePath(opt_tablesDirectory, file);
  if (path) {
    if (loadTranslationTable(path, NULL, table, 0)) ok = 1;
    free(path);
  }
  if (!ok) LogPrint(LOG_ERR, "%s: %s", gettext("cannot load translation table"), file);
  return ok;
}

static int
replaceTextTable (const char *file) {
  if (!replaceTranslationTable(textTable, file)) return 0;
  makeUntextTable();
  return 1;
}

static int
replaceAttributesTable (const char *file) {
  return replaceTranslationTable(attributesTable, file);
}

int
readCommand (BRL_DriverCommandContext context) {
  int command = readBrailleCommand(&brl, context);
  if (command != EOF) {
    LogPrint(LOG_DEBUG, "command: %06X", command);
    if (IS_DELAYED_COMMAND(command)) command = BRL_CMD_NOOP;
    command &= BRL_MSK_CMD;
  }
  return command;
}

#ifdef ENABLE_CONTRACTED_BRAILLE
static void
exitContractionTable (void) {
  if (contractionTable) {
    destroyContractionTable(contractionTable);
    contractionTable = NULL;
  }
}

static int
loadContractionTable (const char *file) {
  void *table = NULL;
  if (*file) {
    char *path = makePath(opt_contractionsDirectory, file);
    LogPrint(LOG_DEBUG, "compiling contraction table: %s", file);
    if (path) {
      if (!(table = compileContractionTable(path))) {
        LogPrint(LOG_ERR, "%s: %s", gettext("cannot compile contraction table"), path);
      }
      free(path);
    }
    if (!table) return 0;
  }
  if (contractionTable) destroyContractionTable(contractionTable);
  contractionTable = table;
  return 1;
}
#endif /* ENABLE_CONTRACTED_BRAILLE */

static void
applyBraillePreferences (void) {
  if (braille->firmness) braille->firmness(&brl, prefs.brailleFirmness);
  if (braille->sensitivity) braille->sensitivity(&brl, prefs.brailleSensitivity);
}

#ifdef ENABLE_SPEECH_SUPPORT
static void
applySpeechPreferences (void) {
  if (speech->rate) setSpeechRate(prefs.speechRate, 0);
  if (speech->volume) setSpeechVolume(prefs.speechVolume, 0);
}
#endif /* ENABLE_SPEECH_SUPPORT */

static void
dimensionsChanged (int infoLevel, int rows, int columns) {
  fwinshift = MAX(columns-prefs.windowOverlap, 1);
  hwinshift = columns / 2;
  vwinshift = (rows > 1)? rows: 5;

  LogPrint(LOG_DEBUG, "shifts: fwin=%d hwin=%d vwin=%d",
           fwinshift, hwinshift, vwinshift);
}

static int
changedWindowAttributes (void) {
  dimensionsChanged(LOG_INFO, brl.y, brl.x);
  return 1;
}

static void
changedPreferences (void) {
  changedWindowAttributes();
  setTuneDevice(prefs.tuneDevice);
  applyBraillePreferences();
#ifdef ENABLE_SPEECH_SUPPORT
  applySpeechPreferences();
#endif /* ENABLE_SPEECH_SUPPORT */
}

int
loadPreferences (int change) {
  int ok = 0;
  FILE *file = openDataFile(preferencesFile, "rb");
  if (file) {
    Preferences newPreferences;
    size_t length = fread(&newPreferences, 1, sizeof(newPreferences), file);
    if (ferror(file)) {
      LogPrint(LOG_ERR, "%s: %s: %s",
               gettext("cannot read preferences file"), preferencesFile, strerror(errno));
    } else if ((length < 40) ||
               (newPreferences.magic[0] != (PREFS_MAGIC_NUMBER & 0XFF)) ||
               (newPreferences.magic[1] != (PREFS_MAGIC_NUMBER >> 8))) {
      LogPrint(LOG_ERR, "%s: %s", gettext("invalid preferences file"), preferencesFile);
    } else {
      prefs = newPreferences;
      ok = 1;

      if (prefs.version == 0) {
        prefs.version++;
        prefs.pcmVolume = DEFAULT_PCM_VOLUME;
        prefs.midiVolume = DEFAULT_MIDI_VOLUME;
        prefs.fmVolume = DEFAULT_FM_VOLUME;
      }

      if (prefs.version == 1) {
        prefs.version++;
        prefs.sayLineMode = DEFAULT_SAY_LINE_MODE;
        prefs.autospeak = DEFAULT_AUTOSPEAK;
      }

      if (prefs.version == 2) {
        prefs.version++;
        prefs.autorepeat = DEFAULT_AUTOREPEAT;
        prefs.autorepeatDelay = DEFAULT_AUTOREPEAT_DELAY;
        prefs.autorepeatInterval = DEFAULT_AUTOREPEAT_INTERVAL;

        prefs.cursorVisibleTime *= 4;
        prefs.cursorInvisibleTime *= 4;
        prefs.attributesVisibleTime *= 4;
        prefs.attributesInvisibleTime *= 4;
        prefs.capitalsVisibleTime *= 4;
        prefs.capitalsInvisibleTime *= 4;
      }

      if (length == 40) {
        length++;
        prefs.speechRate = SPK_DEFAULT_RATE;
      }

      if (length == 41) {
        length++;
        prefs.speechVolume = SPK_DEFAULT_VOLUME;
      }

      if (length == 42) {
        length++;
        prefs.brailleFirmness = DEFAULT_BRAILLE_FIRMNESS;
      }

      if (prefs.version == 3) {
        prefs.version++;
        prefs.autorepeatPanning = DEFAULT_AUTOREPEAT_PANNING;
      }

      if (prefs.version == 4) {
        prefs.version++;
        prefs.brailleSensitivity = DEFAULT_BRAILLE_SENSITIVITY;
      }

      if (change) changedPreferences();
    }
    fclose(file);
  }
  return ok;
}

static void
resetPreferences (void) {
  memset(&prefs, 0, sizeof(prefs));

  prefs.magic[0] = PREFS_MAGIC_NUMBER & 0XFF;
  prefs.magic[1] = PREFS_MAGIC_NUMBER >> 8;
  prefs.version = 4;

  prefs.autorepeat = DEFAULT_AUTOREPEAT;
  prefs.autorepeatPanning = DEFAULT_AUTOREPEAT_PANNING;
  prefs.autorepeatDelay = DEFAULT_AUTOREPEAT_DELAY;
  prefs.autorepeatInterval = DEFAULT_AUTOREPEAT_INTERVAL;

  prefs.showCursor = DEFAULT_SHOW_CURSOR;
  prefs.cursorStyle = DEFAULT_CURSOR_STYLE;
  prefs.blinkingCursor = DEFAULT_BLINKING_CURSOR;
  prefs.cursorVisibleTime = DEFAULT_CURSOR_VISIBLE_TIME;
  prefs.cursorInvisibleTime = DEFAULT_CURSOR_INVISIBLE_TIME;

  prefs.showAttributes = DEFAULT_SHOW_ATTRIBUTES;
  prefs.blinkingAttributes = DEFAULT_BLINKING_ATTRIBUTES;
  prefs.attributesVisibleTime = DEFAULT_ATTRIBUTES_VISIBLE_TIME;
  prefs.attributesInvisibleTime = DEFAULT_ATTRIBUTES_INVISIBLE_TIME;

  prefs.blinkingCapitals = DEFAULT_BLINKING_CAPITALS;
  prefs.capitalsVisibleTime = DEFAULT_CAPITALS_VISIBLE_TIME;
  prefs.capitalsInvisibleTime = DEFAULT_CAPITALS_INVISIBLE_TIME;

  prefs.windowFollowsPointer = DEFAULT_WINDOW_FOLLOWS_POINTER;
  prefs.highlightWindow = DEFAULT_HIGHLIGHT_WINDOW;

  prefs.textStyle = DEFAULT_TEXT_STYLE;
  prefs.brailleFirmness = DEFAULT_BRAILLE_FIRMNESS;
  prefs.brailleSensitivity = DEFAULT_BRAILLE_SENSITIVITY;

  prefs.windowOverlap = DEFAULT_WINDOW_OVERLAP;
  prefs.slidingWindow = DEFAULT_SLIDING_WINDOW;
  prefs.eagerSlidingWindow = DEFAULT_EAGER_SLIDING_WINDOW;

  prefs.skipIdenticalLines = DEFAULT_SKIP_IDENTICAL_LINES;
  prefs.skipBlankWindows = DEFAULT_SKIP_BLANK_WINDOWS;
  prefs.blankWindowsSkipMode = DEFAULT_BLANK_WINDOWS_SKIP_MODE;

  prefs.alertMessages = DEFAULT_ALERT_MESSAGES;
  prefs.alertDots = DEFAULT_ALERT_DOTS;
  prefs.alertTunes = DEFAULT_ALERT_TUNES;
  prefs.tuneDevice = getDefaultTuneDevice();
  prefs.pcmVolume = DEFAULT_PCM_VOLUME;
  prefs.midiVolume = DEFAULT_MIDI_VOLUME;
  prefs.midiInstrument = DEFAULT_MIDI_INSTRUMENT;
  prefs.fmVolume = DEFAULT_FM_VOLUME;

  prefs.sayLineMode = DEFAULT_SAY_LINE_MODE;
  prefs.autospeak = DEFAULT_AUTOSPEAK;
  prefs.speechRate = SPK_DEFAULT_RATE;
  prefs.speechVolume = SPK_DEFAULT_VOLUME;

  prefs.statusStyle = braille->statusStyle;
}

static void
getPreferences (void) {
  if (!loadPreferences(0)) resetPreferences();
  setTuneDevice(prefs.tuneDevice);
}

#ifdef ENABLE_PREFERENCES_MENU
int 
savePreferences (void) {
  int ok = 0;
  FILE *file = openDataFile(preferencesFile, "w+b");
  if (file) {
    size_t length = fwrite(&prefs, 1, sizeof(prefs), file);
    if (length == sizeof(prefs)) {
      ok = 1;
    } else {
      if (!ferror(file)) errno = EIO;
      LogPrint(LOG_ERR, "%s: %s: %s",
               gettext("cannot write to preferences file"), preferencesFile, strerror(errno));
    }
    fclose(file);
  }
  if (!ok) message(gettext("not saved"), 0);
  return ok;
}

static int
testBrailleFirmness (void) {
  return braille->firmness != NULL;
}

static int
testBrailleSensitivity (void) {
  return braille->sensitivity != NULL;
}

static int
changedBrailleFirmness (unsigned char setting) {
  setBrailleFirmness(&brl, setting);
  return 1;
}

static int
changedBrailleSensitivity (unsigned char setting) {
  setBrailleSensitivity(&brl, setting);
  return 1;
}

#ifdef ENABLE_SPEECH_SUPPORT
static int
testSpeechRate (void) {
  return speech->rate != NULL;
}

static int
changedSpeechRate (unsigned char setting) {
  setSpeechRate(setting, 1);
  return 1;
}

static int
testSpeechVolume (void) {
  return speech->volume != NULL;
}

static int
changedSpeechVolume (unsigned char setting) {
  setSpeechVolume(setting, 1);
  return 1;
}
#endif /* ENABLE_SPEECH_SUPPORT */

static int
testTunes (void) {
  return prefs.alertTunes;
}

static int
changedTuneDevice (unsigned char setting) {
  return setTuneDevice(setting);
}

#ifdef ENABLE_PCM_SUPPORT
static int
testTunesPcm (void) {
  return testTunes() && (prefs.tuneDevice == tdPcm);
}
#endif /* ENABLE_PCM_SUPPORT */

#ifdef ENABLE_MIDI_SUPPORT
static int
testTunesMidi (void) {
  return testTunes() && (prefs.tuneDevice == tdMidi);
}
#endif /* ENABLE_MIDI_SUPPORT */

#ifdef ENABLE_FM_SUPPORT
static int
testTunesFm (void) {
  return testTunes() && (prefs.tuneDevice == tdFm);
}
#endif /* ENABLE_FM_SUPPORT */

#ifdef ENABLE_TABLE_SELECTION
typedef struct {
  const char *directory;
  const char *pattern;
  const char *initial;
  char *current;
  unsigned none:1;
#ifdef HAVE_GLOB_H
  glob_t glob;
#endif /* HAVE_GLOB_H */
  const char **paths;
  int count;
  unsigned char setting;
  const char *pathsArea[3];
} GlobData;
static GlobData glob_textTable;
static GlobData glob_attributesTable;

static void
globPrepare (GlobData *data, const char *directory, const char *pattern, const char *initial, int none) {
  memset(data, 0, sizeof(*data));
  data->directory = directory;
  data->pattern = pattern;
  if (!initial) initial = "";
  data->current = strdupWrapper(data->initial = initial);
  data->none = (none != 0);
}

static void
globBegin (GlobData *data) {
  int index;

  data->paths = data->pathsArea;
  data->count = ARRAY_COUNT(data->pathsArea) - 1;
  data->paths[data->count] = NULL;
  index = data->count;

#ifdef HAVE_GLOB_H
  memset(&data->glob, 0, sizeof(data->glob));
  data->glob.gl_offs = data->count;
#endif /* HAVE_GLOB_H */

  {
#ifdef HAVE_FCHDIR
    int originalDirectory = open(".", O_RDONLY);
    if (originalDirectory != -1) {
#else /* HAVE_FCHDIR */
    char *originalDirectory = getWorkingDirectory();
    if (originalDirectory) {
#endif /* HAVE_FCHDIR */
      if (chdir(data->directory) != -1) {
#ifdef HAVE_GLOB_H
        if (glob(data->pattern, GLOB_DOOFFS, NULL, &data->glob) == 0) {
          data->paths = (const char **)data->glob.gl_pathv;
          /* The behaviour of gl_pathc is inconsistent. Some implementations
           * include the leading NULL pointers and some don't. Let's just
           * figure it out the hard way by finding the trailing NULL.
           */
          while (data->paths[data->count]) ++data->count;
        }
#endif /* HAVE_GLOB_H */

#ifdef HAVE_FCHDIR
        if (fchdir(originalDirectory) == -1) LogError("fchdir");
#else /* HAVE_FCHDIR */
        if (chdir(originalDirectory) == -1) LogError("chdir");
#endif /* HAVE_FCHDIR */
      } else {
        LogPrint(LOG_ERR, "%s: %s: %s",
                 gettext("cannot set working directory"), data->directory, strerror(errno));
      }

#ifdef HAVE_FCHDIR
      close(originalDirectory);
#else /* HAVE_FCHDIR */
      free(originalDirectory);
#endif /* HAVE_FCHDIR */
    } else {
#ifdef HAVE_FCHDIR
      LogPrint(LOG_ERR, "%s: %s",
               gettext("cannot open working directory"), strerror(errno));
#else /* HAVE_FCHDIR */
      LogPrint(LOG_ERR, "%s", gettext("cannot determine working directory"));
#endif /* HAVE_FCHDIR */
    }
  }

  if (data->none) data->paths[--index] = "";
  data->paths[--index] = data->initial;
  data->paths += index;
  data->count -= index;
  data->setting = 0;

  for (index=1; index<data->count; ++index) {
    if (strcmp(data->paths[index], data->initial) == 0) {
      data->paths += 1;
      data->count -= 1;
      break;
    }
  }

  for (index=0; index<data->count; ++index) {
    if (strcmp(data->paths[index], data->current) == 0) {
      data->setting = index;
      break;
    }
  }
}

static void
globEnd (GlobData *data) {
#ifdef HAVE_GLOB_H
  if (data->glob.gl_pathc) {
    int index;
    for (index=0; index<data->glob.gl_offs; ++index)
      data->glob.gl_pathv[index] = NULL;
    globfree(&data->glob);
  }
#endif /* HAVE_GLOB_H */
}

static const char *
globChanged (GlobData *data) {
  char *path = strdup(data->paths[data->setting]);
  if (path) {
    free(data->current);
    return data->current = path;
  } else {
    LogError("strdup");
  }
  return NULL;
}

static int
changedTextTable (unsigned char setting) {
  return replaceTextTable(globChanged(&glob_textTable));
}

static int
changedAttributesTable (unsigned char setting) {
  return replaceAttributesTable(globChanged(&glob_attributesTable));
}

#ifdef ENABLE_CONTRACTED_BRAILLE
static GlobData glob_contractionTable;

static int
changedContractionTable (unsigned char setting) {
  return loadContractionTable(globChanged(&glob_contractionTable));
}
#endif /* ENABLE_CONTRACTED_BRAILLE */
#endif /* ENABLE_TABLE_SELECTION */

static int
testSkipBlankWindows (void) {
  return prefs.skipBlankWindows;
}

static int
testSlidingWindow (void) {
  return prefs.slidingWindow;
}

static int
changedWindowOverlap (unsigned char setting) {
  return changedWindowAttributes();
}

static int
testAutorepeat (void) {
  return prefs.autorepeat;
}

static int
changedAutorepeat (unsigned char setting) {
  if (setting) resetAutorepeat();
  return 1;
}

static int
testShowCursor (void) {
  return prefs.showCursor;
}

static int
testBlinkingCursor (void) {
  return testShowCursor() && prefs.blinkingCursor;
}

static int
testShowAttributes (void) {
  return prefs.showAttributes;
}

static int
testBlinkingAttributes (void) {
  return testShowAttributes() && prefs.blinkingAttributes;
}

static int
testBlinkingCapitals (void) {
  return prefs.blinkingCapitals;
}

typedef struct {
  unsigned char *setting;                 /* pointer to current value */
  int (*changed) (unsigned char setting); /* called when value changes */
  int (*test) (void);                     /* returns true if item should be presented */
  const char *label;                      /* item name for presentation */
  const char *const *names;               /* symbolic names of values */
  unsigned char minimum;                  /* lowest valid value */
  unsigned char maximum;                  /* highest valid value */
  unsigned char divisor;                  /* present only multiples of this value */
} MenuItem;

static void
previousSetting (MenuItem *item) {
  if ((*item->setting)-- <= item->minimum) *item->setting = item->maximum;
}

static void
nextSetting (MenuItem *item) {
  if ((*item->setting)++ >= item->maximum) *item->setting = item->minimum;
}

void
updatePreferences (void) {
#ifdef ENABLE_TABLE_SELECTION
  globBegin(&glob_textTable);
  globBegin(&glob_attributesTable);
#ifdef ENABLE_CONTRACTED_BRAILLE
  globBegin(&glob_contractionTable);
#endif /* ENABLE_CONTRACTED_BRAILLE */
#endif /* ENABLE_TABLE_SELECTION */

  {
    static unsigned char exitSave = 0;                /* 1 == save preferences on exit */

    static const char *booleanValues[] = {
      strtext("No"),
      strtext("Yes")
    };

    static const char *cursorStyles[] = {
      strtext("Underline"),
      strtext("Block")
    };

    static const char *firmnessLevels[] = {
      strtext("Minimum"),
      strtext("Low"),
      strtext("Medium"),
      strtext("High"),
      strtext("Maximum")
    };

    static const char *sensitivityLevels[] = {
      strtext("Minimum"),
      strtext("Low"),
      strtext("Medium"),
      strtext("High"),
      strtext("Maximum")
    };

    static const char *skipBlankWindowsModes[] = {
      strtext("All"),
      strtext("End of Line"),
      strtext("Rest of Line")
    };

    static const char *statusStyles[] = {
      strtext("None"),
      "Alva",
      "Tieman",
      "PowerBraille 80",
      strtext("Generic"),
      "MDV",
      "Voyager"
    };

    static const char *textStyles[] = {
      "8-dot",
      "6-dot"
    };

    static const char *tuneDevices[] = {
      "Beeper"
        " ("
#ifdef ENABLE_BEEPER_SUPPORT
        strtext("console tone generator")
#else /* ENABLE_BEEPER_SUPPORT */
        strtext("unsupported")
#endif /* ENABLE_BEEPER_SUPPORT */
        ")",

      "PCM"
        " ("
#ifdef ENABLE_PCM_SUPPORT
        strtext("soundcard digital audio")
#else /* ENABLE_PCM_SUPPORT */
        strtext("unsupported")
#endif /* ENABLE_PCM_SUPPORT */
        ")",

      "MIDI"
        " ("
#ifdef ENABLE_MIDI_SUPPORT
        strtext("Musical Instrument Digital Interface")
#else /* ENABLE_MIDI_SUPPORT */
        strtext("unsupported")
#endif /* ENABLE_MIDI_SUPPORT */
        ")",

      "FM"
        " ("
#ifdef ENABLE_FM_SUPPORT
        strtext("soundcard synthesizer")
#else /* ENABLE_FM_SUPPORT */
        strtext("unsupported")
#endif /* ENABLE_FM_SUPPORT */
        ")"
    };

#ifdef ENABLE_SPEECH_SUPPORT
    static const char *sayModes[] = {
      strtext("Immediate"),
      strtext("Enqueue")
    };
#endif /* ENABLE_SPEECH_SUPPORT */

    #define MENU_ITEM(setting, changed, test, label, values, minimum, maximum, divisor) {&setting, changed, test, label, values, minimum, maximum, divisor}
    #define NUMERIC_ITEM(setting, changed, test, label, minimum, maximum, divisor) MENU_ITEM(setting, changed, test, label, NULL, minimum, maximum, divisor)
    #define TIME_ITEM(setting, changed, test, label) NUMERIC_ITEM(setting, changed, test, label, 1, 100, updateInterval/10)
    #define VOLUME_ITEM(setting, changed, test, label) NUMERIC_ITEM(setting, changed, test, label, 0, 100, 5)
    #define TEXT_ITEM(setting, changed, test, label, names, count) MENU_ITEM(setting, changed, test, label, names, 0, count-1, 1)
    #define SYMBOLIC_ITEM(setting, changed, test, label, names) TEXT_ITEM(setting, changed, test, label, names, ARRAY_COUNT(names))
    #define BOOLEAN_ITEM(setting, changed, test, label) SYMBOLIC_ITEM(setting, changed, test, label, booleanValues)
    #define GLOB_ITEM(data, changed, test, label) TEXT_ITEM(data.setting, changed, test, label, data.paths, data.count)
    MenuItem menu[] = {
      BOOLEAN_ITEM(exitSave, NULL, NULL, strtext("Save on Exit")),
      SYMBOLIC_ITEM(prefs.textStyle, NULL, NULL, strtext("Text Style"), textStyles),
      BOOLEAN_ITEM(prefs.skipIdenticalLines, NULL, NULL, strtext("Skip Identical Lines")),
      BOOLEAN_ITEM(prefs.skipBlankWindows, NULL, NULL, strtext("Skip Blank Windows")),
      SYMBOLIC_ITEM(prefs.blankWindowsSkipMode, NULL, testSkipBlankWindows, strtext("Which Blank Windows"), skipBlankWindowsModes),
      BOOLEAN_ITEM(prefs.slidingWindow, NULL, NULL, strtext("Sliding Window")),
      BOOLEAN_ITEM(prefs.eagerSlidingWindow, NULL, testSlidingWindow, strtext("Eager Sliding Window")),
      NUMERIC_ITEM(prefs.windowOverlap, changedWindowOverlap, NULL, strtext("Window Overlap"), 0, 20, 1),
      BOOLEAN_ITEM(prefs.autorepeat, changedAutorepeat, NULL, strtext("Autorepeat")),
      BOOLEAN_ITEM(prefs.autorepeatPanning, NULL, testAutorepeat, strtext("Autorepeat Panning")),
      TIME_ITEM(prefs.autorepeatDelay, NULL, testAutorepeat, strtext("Autorepeat Delay")),
      TIME_ITEM(prefs.autorepeatInterval, NULL, testAutorepeat, strtext("Autorepeat Interval")),
      BOOLEAN_ITEM(prefs.showCursor, NULL, NULL, strtext("Show Cursor")),
      SYMBOLIC_ITEM(prefs.cursorStyle, NULL, testShowCursor, strtext("Cursor Style"), cursorStyles),
      BOOLEAN_ITEM(prefs.blinkingCursor, NULL, testShowCursor, strtext("Blinking Cursor")),
      TIME_ITEM(prefs.cursorVisibleTime, NULL, testBlinkingCursor, strtext("Cursor Visible Time")),
      TIME_ITEM(prefs.cursorInvisibleTime, NULL, testBlinkingCursor, strtext("Cursor Invisible Time")),
      BOOLEAN_ITEM(prefs.showAttributes, NULL, NULL, strtext("Show Attributes")),
      BOOLEAN_ITEM(prefs.blinkingAttributes, NULL, testShowAttributes, strtext("Blinking Attributes")),
      TIME_ITEM(prefs.attributesVisibleTime, NULL, testBlinkingAttributes, strtext("Attributes Visible Time")),
      TIME_ITEM(prefs.attributesInvisibleTime, NULL, testBlinkingAttributes, strtext("Attributes Invisible Time")),
      BOOLEAN_ITEM(prefs.blinkingCapitals, NULL, NULL, strtext("Blinking Capitals")),
      TIME_ITEM(prefs.capitalsVisibleTime, NULL, testBlinkingCapitals, strtext("Capitals Visible Time")),
      TIME_ITEM(prefs.capitalsInvisibleTime, NULL, testBlinkingCapitals, strtext("Capitals Invisible Time")),
      SYMBOLIC_ITEM(prefs.brailleFirmness, changedBrailleFirmness, testBrailleFirmness, strtext("Braille Firmness"), firmnessLevels),
      SYMBOLIC_ITEM(prefs.brailleSensitivity, changedBrailleSensitivity, testBrailleSensitivity, strtext("Braille Sensitivity"), sensitivityLevels),
#ifdef HAVE_LIBGPM
      BOOLEAN_ITEM(prefs.windowFollowsPointer, NULL, NULL, strtext("Window Follows Pointer")),
#endif /* HAVE_LIBGPM */
      BOOLEAN_ITEM(prefs.highlightWindow, NULL, NULL, strtext("Highlight Window")),
      BOOLEAN_ITEM(prefs.alertTunes, NULL, NULL, strtext("Alert Tunes")),
      SYMBOLIC_ITEM(prefs.tuneDevice, changedTuneDevice, testTunes, strtext("Tune Device"), tuneDevices),
#ifdef ENABLE_PCM_SUPPORT
      VOLUME_ITEM(prefs.pcmVolume, NULL, testTunesPcm, strtext("PCM Volume")),
#endif /* ENABLE_PCM_SUPPORT */
#ifdef ENABLE_MIDI_SUPPORT
      VOLUME_ITEM(prefs.midiVolume, NULL, testTunesMidi, strtext("MIDI Volume")),
      TEXT_ITEM(prefs.midiInstrument, NULL, testTunesMidi, strtext("MIDI Instrument"), midiInstrumentTable, midiInstrumentCount),
#endif /* ENABLE_MIDI_SUPPORT */
#ifdef ENABLE_FM_SUPPORT
      VOLUME_ITEM(prefs.fmVolume, NULL, testTunesFm, strtext("FM Volume")),
#endif /* ENABLE_FM_SUPPORT */
      BOOLEAN_ITEM(prefs.alertDots, NULL, NULL, strtext("Alert Dots")),
      BOOLEAN_ITEM(prefs.alertMessages, NULL, NULL, strtext("Alert Messages")),
#ifdef ENABLE_SPEECH_SUPPORT
      SYMBOLIC_ITEM(prefs.sayLineMode, NULL, NULL, strtext("Say-Line Mode"), sayModes),
      BOOLEAN_ITEM(prefs.autospeak, NULL, NULL, strtext("Autospeak")),
      NUMERIC_ITEM(prefs.speechRate, changedSpeechRate, testSpeechRate, strtext("Speech Rate"), 0, SPK_MAXIMUM_RATE, 1),
      NUMERIC_ITEM(prefs.speechVolume, changedSpeechVolume, testSpeechVolume, strtext("Speech Volume"), 0, SPK_MAXIMUM_VOLUME, 1),
#endif /* ENABLE_SPEECH_SUPPORT */
      SYMBOLIC_ITEM(prefs.statusStyle, NULL, NULL, strtext("Status Style"), statusStyles),
#ifdef ENABLE_TABLE_SELECTION
      GLOB_ITEM(glob_textTable, changedTextTable, NULL, strtext("Text Table")),
      GLOB_ITEM(glob_attributesTable, changedAttributesTable, NULL, strtext("Attributes Table")),
#ifdef ENABLE_CONTRACTED_BRAILLE
      GLOB_ITEM(glob_contractionTable, changedContractionTable, NULL, strtext("Contraction Table"))
#endif /* ENABLE_CONTRACTED_BRAILLE */
#endif /* ENABLE_TABLE_SELECTION */
    };
    int menuSize = ARRAY_COUNT(menu);
    static int menuIndex = 0;                        /* current menu item */

    int lineIndent = 0;                                /* braille window pos in buffer */
    int indexChanged = 1;
    int settingChanged = 0;                        /* 1 when item's value has changed */

    Preferences oldPreferences = prefs;        /* backup preferences */
    int command = EOF;                                /* readbrl() value */

    /* status cells */
    setStatusText(&brl, "prf");
    message(gettext("Preferences Menu"), 0);

    if (prefs.autorepeat) resetAutorepeat();

    while (1) {
      MenuItem *item = &menu[menuIndex];
      char valueBuffer[0X10];
      const char *value;

      testProgramTermination();
      closeTuneDevice(0);

      if (!item->names) {
        snprintf(valueBuffer, sizeof(valueBuffer), "%d", *item->setting);
        value = valueBuffer;
      } else {
        if (!*(value = item->names[*item->setting - item->minimum])) value = strtext("<off>");
        value = gettext(value);
      }

      {
        const char *label = gettext(item->label);
        const char *delimiter = ": ";
        int settingIndent = strlen(label) + strlen(delimiter);
        int valueLength = strlen(value);
        int lineLength = settingIndent + valueLength;
        char line[lineLength + 1];

        /* First we draw the current menu item in the buffer */
        snprintf(line,  sizeof(line), "%s%s%s",
                 label, delimiter, value);

#ifdef ENABLE_SPEECH_SUPPORT
        if (prefs.autospeak) {
          if (indexChanged) {
            sayString(line, 1);
          } else if (settingChanged) {
            sayString(value, 1);
          }
        }
#endif /* ENABLE_SPEECH_SUPPORT */

        /* Next we deal with the braille window position in the buffer.
         * This is intended for small displays and/or long item descriptions 
         */
        if (settingChanged) {
          settingChanged = 0;
          /* make sure the updated value is visible */
          if ((lineLength-lineIndent > brl.x*brl.y) && (lineIndent < settingIndent))
            lineIndent = settingIndent;
        }
        indexChanged = 0;

        /* Then draw the braille window */
        writeBrailleText(&brl, &line[lineIndent], MAX(0, lineLength-lineIndent));
        drainBrailleOutput(&brl, updateInterval);

        /* Now process any user interaction */
        command = readBrailleCommand(&brl, BRL_CTX_PREFS);
        handleAutorepeat(&command, NULL);
        if (command != EOF) {
          switch (command) {
            case BRL_CMD_NOOP:
              continue;

            case BRL_CMD_HELP:
              /* This is quick and dirty... Something more intelligent 
               * and friendly needs to be done here...
               */
              message( 
                  "Press UP and DOWN to select an item, "
                  "HOME to toggle the setting. "
                  "Routing keys are available too! "
                  "Press PREFS again to quit.", MSG_WAITKEY|MSG_NODELAY);
              break;

            case BRL_BLK_PASSKEY+BRL_KEY_HOME:
            case BRL_CMD_PREFLOAD:
              prefs = oldPreferences;
              changedPreferences();
              message(gettext("changes discarded"), 0);
              break;
            case BRL_BLK_PASSKEY+BRL_KEY_ENTER:
            case BRL_CMD_PREFSAVE:
              exitSave = 1;
              goto exitMenu;

            case BRL_CMD_TOP:
            case BRL_CMD_TOP_LEFT:
            case BRL_BLK_PASSKEY+BRL_KEY_PAGE_UP:
            case BRL_CMD_MENU_FIRST_ITEM:
              menuIndex = lineIndent = 0;
              indexChanged = 1;
              break;
            case BRL_CMD_BOT:
            case BRL_CMD_BOT_LEFT:
            case BRL_BLK_PASSKEY+BRL_KEY_PAGE_DOWN:
            case BRL_CMD_MENU_LAST_ITEM:
              menuIndex = menuSize - 1;
              lineIndent = 0;
              indexChanged = 1;
              break;

            case BRL_CMD_LNUP:
            case BRL_CMD_PRDIFLN:
            case BRL_BLK_PASSKEY+BRL_KEY_CURSOR_UP:
            case BRL_CMD_MENU_PREV_ITEM:
              do {
                if (menuIndex == 0) menuIndex = menuSize;
                --menuIndex;
              } while (menu[menuIndex].test && !menu[menuIndex].test());
              lineIndent = 0;
              indexChanged = 1;
              break;
            case BRL_CMD_LNDN:
            case BRL_CMD_NXDIFLN:
            case BRL_BLK_PASSKEY+BRL_KEY_CURSOR_DOWN:
            case BRL_CMD_MENU_NEXT_ITEM:
              do {
                if (++menuIndex == menuSize) menuIndex = 0;
              } while (menu[menuIndex].test && !menu[menuIndex].test());
              lineIndent = 0;
              indexChanged = 1;
              break;

            case BRL_CMD_FWINLT:
              if (lineIndent > 0)
                lineIndent -= MIN(brl.x*brl.y, lineIndent);
              else
                playTune(&tune_bounce);
              break;
            case BRL_CMD_FWINRT:
              if (lineLength-lineIndent > brl.x*brl.y)
                lineIndent += brl.x*brl.y;
              else
                playTune(&tune_bounce);
              break;

            {
              void (*adjust) (MenuItem *item);
              int count;
            case BRL_CMD_WINUP:
            case BRL_CMD_CHRLT:
            case BRL_BLK_PASSKEY+BRL_KEY_CURSOR_LEFT:
            case BRL_CMD_BACK:
            case BRL_CMD_MENU_PREV_SETTING:
              adjust = previousSetting;
              goto adjustSetting;
            case BRL_CMD_WINDN:
            case BRL_CMD_CHRRT:
            case BRL_BLK_PASSKEY+BRL_KEY_CURSOR_RIGHT:
            case BRL_CMD_HOME:
            case BRL_CMD_RETURN:
            case BRL_CMD_MENU_NEXT_SETTING:
              adjust = nextSetting;
            adjustSetting:
              count = item->maximum - item->minimum + 1;
              do {
                adjust(item);
                if (!--count) break;
              } while ((*item->setting % item->divisor) || (item->changed && !item->changed(*item->setting)));
              if (count)
                settingChanged = 1;
              else
                playTune(&tune_command_rejected);
              break;
            }

    #ifdef ENABLE_SPEECH_SUPPORT
            case BRL_CMD_SAY_LINE:
              speech->say((unsigned char *)line, lineLength);
              break;
            case BRL_CMD_MUTE:
              speech->mute();
              break;
    #endif /* ENABLE_SPEECH_SUPPORT */

            default:
              if (command >= BRL_BLK_ROUTE && command < BRL_BLK_ROUTE+brl.x) {
                unsigned char oldSetting = *item->setting;
                int key = command - BRL_BLK_ROUTE;
                if (item->names) {
                  *item->setting = key % (item->maximum + 1);
                } else {
                  *item->setting = rescaleInteger(key, brl.x-1, item->maximum-item->minimum) + item->minimum;
                }
                if (item->changed && !item->changed(*item->setting)) {
                  *item->setting = oldSetting;
                  playTune(&tune_command_rejected);
                } else if (*item->setting != oldSetting) {
                  settingChanged = 1;
                }
                break;
              }

              /* For any other keystroke, we exit */
              playTune(&tune_command_rejected);
            case BRL_BLK_PASSKEY+BRL_KEY_ESCAPE:
            case BRL_BLK_PASSKEY+BRL_KEY_END:
            case BRL_CMD_PREFMENU:
            exitMenu:
              if (exitSave) {
                if (savePreferences()) {
                  playTune(&tune_command_done);
                }
              }

    #ifdef ENABLE_TABLE_SELECTION
              globEnd(&glob_textTable);
              globEnd(&glob_attributesTable);
    #ifdef ENABLE_CONTRACTED_BRAILLE
              globEnd(&glob_contractionTable);
    #endif /* ENABLE_CONTRACTED_BRAILLE */
    #endif /* ENABLE_TABLE_SELECTION */

              return;
          }
        }
      }
    }
  }
}
#endif /* ENABLE_PREFERENCES_MENU */

typedef struct {
  const char *driverType;
  const char *const *requestedDrivers;
  const char *const *autodetectableDrivers;
  const char * (*getDefaultDriver) (void);
  int (*haveDriver) (const char *code);
  int (*initializeDriver) (const char *code, int verify);
} DriverActivationData;

static int
activateDriver (const DriverActivationData *data, int verify) {
  int oneDriver = data->requestedDrivers[0] && !data->requestedDrivers[1];
  int autodetect = oneDriver && (strcmp(data->requestedDrivers[0], "auto") == 0);
  const char *const defaultDrivers[] = {data->getDefaultDriver(), NULL};
  const char *const *driver;

  if (!oneDriver || autodetect) verify = 0;

  if (!autodetect) {
    driver = data->requestedDrivers;
  } else if (defaultDrivers[0]) {
    driver = defaultDrivers;
  } else if (*(driver = data->autodetectableDrivers)) {
    LogPrint(LOG_DEBUG, "performing %s driver autodetection", data->driverType);
  } else {
    LogPrint(LOG_DEBUG, "no autodetectable %s drivers", data->driverType);
  }

  if (!*driver) {
    static const char *const fallbackDrivers[] = {"no", NULL};
    driver = fallbackDrivers;
    autodetect = 0;
  }

  while (*driver) {
    if (!autodetect || data->haveDriver(*driver)) {
      LogPrint(LOG_DEBUG, "checking for %s driver: %s", data->driverType, *driver);
      if (data->initializeDriver(*driver, verify)) return 1;
    }

    ++driver;
  }

  LogPrint(LOG_DEBUG, "%s driver not found", data->driverType);
  return 0;
}

void
initializeBraille (void) {
  initializeBrailleDisplay(&brl);
  brl.bufferResized = &dimensionsChanged;
  brl.dataDirectory = opt_dataDirectory;
}

int
constructBrailleDriver (void) {
  initializeBraille();

  if (braille->construct(&brl, brailleParameters, brailleDevice)) {
    if (ensureBrailleBuffer(&brl, LOG_INFO)) {
      brailleConstructed = 1;
      return 1;
    }

    braille->destruct(&brl);
  } else {
    LogPrint(LOG_DEBUG, "%s: %s -> %s",
             gettext("braille driver initialization failed"),
             braille->definition.code, brailleDevice);
  }

  return 0;
}

void
destructBrailleDriver (void) {
  brailleConstructed = 0;
  drainBrailleOutput(&brl, 0);
  braille->destruct(&brl);
}

static int
initializeBrailleDriver (const char *code, int verify) {
  if ((braille = loadBrailleDriver(code, &brailleObject, opt_libraryDirectory))) {
    brailleParameters = processParameters(braille->parameters,
                                          braille->definition.code,
                                          opt_brailleParameters);
    if (brailleParameters) {
      int constructed = verify;

      if (!constructed) {
        LogPrint(LOG_DEBUG, "initializing braille driver: %s -> %s",
                 braille->definition.code, brailleDevice);

        if (constructBrailleDriver()) {
#ifdef ENABLE_API
          if (apiStarted) api_link(&brl);
#endif /* ENABLE_API */

          brailleDriver = braille;
          constructed = 1;
        }
      }

      if (constructed) {
        LogPrint(LOG_INFO, "%s: %s [%s]",
                 gettext("Braille Driver"), braille->definition.code, braille->definition.name);
        identifyBrailleDriver(braille, 0);
        logParameters(braille->parameters, brailleParameters,
                      gettext("Braille Parameter"));
        LogPrint(LOG_INFO, "%s: %s", gettext("Braille Device"), brailleDevice);

        /* Initialize the braille driver's help screen. */
        LogPrint(LOG_INFO, "%s: %s", gettext("Help File"),
                 braille->helpFile? braille->helpFile: gettext("none"));
        {
          char *path = makePath(opt_dataDirectory, braille->helpFile);
          if (path) {
            if (verify || constructHelpScreen(path)) {
              LogPrint(LOG_INFO, "%s: %s[%d]", gettext("Help Page"), path, getHelpPageNumber());
            } else {
              LogPrint(LOG_WARNING, "%s: %s", gettext("cannot open help file"), path);
            }
            free(path);
          }
        }

        {
          const char *part1 = CONFIGURATION_DIRECTORY "/brltty-";
          const char *part2 = braille->definition.code;
          const char *part3 = ".prefs";
          char *path = mallocWrapper(strlen(part1) + strlen(part2) + strlen(part3) + 1);
          sprintf(path, "%s%s%s", part1, part2, part3);
          preferencesFile = path;
          fixInstallPath(&preferencesFile);
          if (path != preferencesFile) free(path);
        }
        LogPrint(LOG_INFO, "%s: %s", gettext("Preferences File"), preferencesFile);

        return 1;
      }

      deallocateStrings(brailleParameters);
      brailleParameters = NULL;
    }

    if (brailleObject) {
      unloadSharedObject(brailleObject);
      brailleObject = NULL;
    }
  } else {
    LogPrint(LOG_ERR, "%s: %s", gettext("braille driver not loadable"), code);
  }

  braille = &noBraille;
  return 0;
}

static int
activateBrailleDriver (int verify) {
  int oneDevice = brailleDevices[0] && !brailleDevices[1];
  const char *const *device = (const char *const *)brailleDevices;

  if (!oneDevice) verify = 0;

  while (*device) {
    const char *const *autodetectableDrivers;

    brailleDevice = *device;
    LogPrint(LOG_DEBUG, "checking braille device: %s", brailleDevice);

    {
      const char *type;
      const char *dev = brailleDevice;

      if (isSerialDevice(&dev)) {
        static const char *const serialDrivers[] = {
          "md", "pm", "ts", "ht", "bn", "al", "bm",
          NULL
        };
        autodetectableDrivers = serialDrivers;
        type = "serial";
      } else

#ifdef ENABLE_USB_SUPPORT
      if (isUsbDevice(&dev)) {
        static const char *const usbDrivers[] = {
          "al", "bm", "fs", "ht", "pm", "vo",
          NULL
        };
        autodetectableDrivers = usbDrivers;
        type = "USB";
      } else
#endif /* ENABLE_USB_SUPPORT */

#ifdef ENABLE_BLUETOOTH_SUPPORT
      if (isBluetoothDevice(&dev)) {
        static const char *bluetoothDrivers[] = {
          "ht", "bm",
          NULL
        };
        autodetectableDrivers = bluetoothDrivers;
        type = "bluetooth";
      } else
#endif /* ENABLE_BLUETOOTH_SUPPORT */

      {
        static const char *noDrivers[] = {NULL};
        autodetectableDrivers = noDrivers;
      }
    }

    {
      const DriverActivationData data = {
        .driverType = "braille",
        .requestedDrivers = (const char *const *)brailleDrivers,
        .autodetectableDrivers = autodetectableDrivers,
        .getDefaultDriver = getDefaultBrailleDriver,
        .haveDriver = haveBrailleDriver,
        .initializeDriver = initializeBrailleDriver
      };
      if (activateDriver(&data, verify)) return 1;
    }

    ++device;
  }

  brailleDevice = NULL;
  return 0;
}

static void
deactivateBrailleDriver (void) {
  destructHelpScreen();

  if (brailleDriver) {
#ifdef ENABLE_API
    if (apiStarted) api_unlink(&brl);
#endif /* ENABLE_API */

    if (brailleConstructed) destructBrailleDriver();
    braille = &noBraille;
    brailleDevice = NULL;
    brailleDriver = NULL;
  }

  if (brailleObject) {
    unloadSharedObject(brailleObject);
    brailleObject = NULL;
  }

  if (brailleParameters) {
    deallocateStrings(brailleParameters);
    brailleParameters = NULL;
  }

  if (preferencesFile) {
    free(preferencesFile);
    preferencesFile = NULL;
  }
}

static int
startBrailleDriver (void) {
#ifdef ENABLE_BLUETOOTH_SUPPORT
  btForgetConnectErrors();
#endif /* ENABLE_BLUETOOTH_SUPPORT */

  if (!activateBrailleDriver(0)) return 0;
  getPreferences();
  applyBraillePreferences();
  clearStatusCells(&brl);
  setHelpPageNumber(brl.helpPage);
  playTune(&tune_braille_on);

  if (!opt_quiet) {
    char banner[0X100];
    makeProgramBanner(banner, sizeof(banner));
    message(banner, 0);
  }

  return 1;
}

static int tryBrailleDriver (void);

static void
retryBrailleDriver (void *data) {
  if (!brailleDriver) tryBrailleDriver();
}

static int
tryBrailleDriver (void) {
  if (startBrailleDriver()) return 1;
  asyncRelativeAlarm(5000, retryBrailleDriver, NULL);
  initializeBraille();
  ensureBrailleBuffer(&brl, LOG_DEBUG);
  return 0;
}

static void
stopBrailleDriver (void) {
  deactivateBrailleDriver();
  playTune(&tune_braille_off);
}

void
restartBrailleDriver (void) {
  stopBrailleDriver();
  LogPrint(LOG_INFO, gettext("reinitializing braille driver"));
  tryBrailleDriver();
}

static void
exitBrailleDriver (void) {
  if (brailleConstructed) {
    clearStatusCells(&brl);
    message(gettext("BRLTTY terminated"), MSG_NODELAY|MSG_SILENT);
  }

  stopBrailleDriver();
}

#ifdef ENABLE_API
static void
exitApi (void) {
  api_stop(&brl);
  apiStarted = 0;
}
#endif /* ENABLE_API */

#ifdef ENABLE_SPEECH_SUPPORT
void
initializeSpeech (void) {
}

int
constructSpeechDriver (void) {
  initializeSpeech();

  if (speech->construct(speechParameters)) {
    return 1;
  } else {
    LogPrint(LOG_DEBUG, "speech driver initialization failed: %s",
             speech->definition.code);
  }

  return 0;
}

void
destructSpeechDriver (void) {
  speech->destruct();
}

static int
initializeSpeechDriver (const char *code, int verify) {
  if ((speech = loadSpeechDriver(code, &speechObject, opt_libraryDirectory))) {
    speechParameters = processParameters(speech->parameters,
                                         speech->definition.code,
                                         opt_speechParameters);
    if (speechParameters) {
      int constructed = verify;

      if (!constructed) {
        LogPrint(LOG_DEBUG, "initializing speech driver: %s",
                 speech->definition.code);

        if (constructSpeechDriver()) {
          constructed = 1;
          speechDriver = speech;
        }
      }

      if (constructed) {
        LogPrint(LOG_INFO, "%s: %s [%s]",
                 gettext("Speech Driver"), speech->definition.code, speech->definition.name);
        identifySpeechDriver(speech, 0);
        logParameters(speech->parameters, speechParameters,
                      gettext("Speech Parameter"));

        return 1;
      }

      deallocateStrings(speechParameters);
      speechParameters = NULL;
    }

    if (speechObject) {
      unloadSharedObject(speechObject);
      speechObject = NULL;
    }
  } else {
    LogPrint(LOG_ERR, "%s: %s", gettext("speech driver not loadable"), code);
  }

  speech = &noSpeech;
  return 0;
}

static int
activateSpeechDriver (int verify) {
  static const char *const autodetectableDrivers[] = {
    NULL
  };

  const DriverActivationData data = {
    .driverType = "speech",
    .requestedDrivers = (const char *const *)speechDrivers,
    .autodetectableDrivers = autodetectableDrivers,
    .getDefaultDriver = getDefaultSpeechDriver,
    .haveDriver = haveSpeechDriver,
    .initializeDriver = initializeSpeechDriver
  };

  return activateDriver(&data, verify);
}

static void
deactivateSpeechDriver (void) {
  if (speechDriver) {
    destructSpeechDriver();

    speech = &noSpeech;
    speechDriver = NULL;
  }

  if (speechObject) {
    unloadSharedObject(speechObject);
    speechObject = NULL;
  }

  if (speechParameters) {
    deallocateStrings(speechParameters);
    speechParameters = NULL;
  }
}

static int
startSpeechDriver (void) {
  if (!activateSpeechDriver(0)) return 0;
  applySpeechPreferences();
  return 1;
}

static int trySpeechDriver (void);

static void
retrySpeechDriver (void *data) {
  if (!speechDriver) trySpeechDriver();
}

static int
trySpeechDriver (void) {
  if (startSpeechDriver()) return 1;
  asyncRelativeAlarm(5000, retrySpeechDriver, NULL);
  return 0;
}

static void
stopSpeechDriver (void) {
  speech->mute();
  deactivateSpeechDriver();
}

void
restartSpeechDriver (void) {
  stopSpeechDriver();
  LogPrint(LOG_INFO, gettext("reinitializing speech driver"));
  trySpeechDriver();
}

static void
exitSpeechDriver (void) {
  stopSpeechDriver();
}
#endif /* ENABLE_SPEECH_SUPPORT */

static int
initializeScreenDriver (const char *code, int verify) {
  if ((screen = loadScreenDriver(code, &screenObject, opt_libraryDirectory))) {
    screenParameters = processParameters(getScreenParameters(screen),
                                         getScreenDriverDefinition(screen)->code,
                                         opt_screenParameters);
    if (screenParameters) {
      int constructed = verify;

      if (!constructed) {
        LogPrint(LOG_DEBUG, "initializing screen driver: %s",
                 getScreenDriverDefinition(screen)->code);

        if (constructScreenDriver(screenParameters)) {
          constructed = 1;
          screenDriver = screen;
        }
      }

      if (constructed) {
        LogPrint(LOG_INFO, "%s: %s [%s]",
                 gettext("Screen Driver"),
                 getScreenDriverDefinition(screen)->code,
                 getScreenDriverDefinition(screen)->name);
        identifyScreenDriver(screen, 0);
        logParameters(getScreenParameters(screen),
                      screenParameters,
                      gettext("Screen Parameter"));

        return 1;
      }

      deallocateStrings(screenParameters);
      screenParameters = NULL;
    }

    if (screenObject) {
      unloadSharedObject(screenObject);
      screenObject = NULL;
    }
  } else {
    LogPrint(LOG_ERR, "%s: %s", gettext("screen driver not loadable"), code);
  }

  screen = &noScreen;
  return 0;
}

static int
activateScreenDriver (int verify) {
  static const char *const autodetectableDrivers[] = {
    NULL
  };

  const DriverActivationData data = {
    .driverType = "screen",
    .requestedDrivers = (const char *const *)screenDrivers,
    .autodetectableDrivers = autodetectableDrivers,
    .getDefaultDriver = getDefaultScreenDriver,
    .haveDriver = haveScreenDriver,
    .initializeDriver = initializeScreenDriver
  };

  return activateDriver(&data, verify);
}

static void
deactivateScreenDriver (void) {
  if (screenDriver) {
    destructScreenDriver();

    screen = &noScreen;
    screenDriver = NULL;
  }

  if (screenObject) {
    unloadSharedObject(screenObject);
    screenObject = NULL;
  }

  if (screenParameters) {
    deallocateStrings(screenParameters);
    screenParameters = NULL;
  }
}

static int
startScreenDriver (void) {
  if (!activateScreenDriver(0)) return 0;
  return 1;
}

static int tryScreenDriver (void);

static void
retryScreenDriver (void *data) {
  if (!screenDriver) tryScreenDriver();
}

static int
tryScreenDriver (void) {
  if (startScreenDriver()) return 1;
  asyncRelativeAlarm(5000, retryScreenDriver, NULL);
  initializeScreen();
  return 0;
}

static void
stopScreenDriver (void) {
  deactivateScreenDriver();
}

static void
exitScreen (void) {
  stopScreenDriver();
  destructSpecialScreens();
}

static void
exitTunes (void) {
  closeTuneDevice(1);
}

static void
exitPidFile (void) {
  unlink(opt_pidFile);
}

static void createPidFile (void);

static void
retryPidFile (void *data) {
  createPidFile();
}

static void
createPidFile (void) {
  FILE *stream = fopen(opt_pidFile, "w");
  if (stream) {
    long int pid = getpid();
    fprintf(stream, "%ld\n", pid);
    fclose(stream);
    atexit(exitPidFile);
  } else {
    LogPrint(LOG_WARNING, "%s: %s: %s",
             gettext("cannot open process identifier file"),
             opt_pidFile, strerror(errno));
    asyncRelativeAlarm(5000, retryPidFile, NULL);
  }
}

#if defined(__MINGW32__)
static void
background (void) {
  LPTSTR cmdline = GetCommandLine();
  int len = strlen(cmdline);
  char newcmdline[len+4];
  STARTUPINFO startupinfo;
  PROCESS_INFORMATION processinfo;
  
  memset(&startupinfo, 0, sizeof(startupinfo));
  startupinfo.cb = sizeof(startupinfo);

  memcpy(newcmdline, cmdline, len);
  memcpy(newcmdline+len, " -n", 4);

  if (!CreateProcess(NULL, newcmdline, NULL, NULL, TRUE, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &startupinfo, &processinfo)) {
    LogWindowsError("CreateProcess");
    exit(10);
  }
  ExitProcess(0);
}

#elif defined(__MSDOS__)

#else /* Unix */
static void
background (void) {
  fflush(stdout);
  fflush(stderr);

  {
    pid_t child = fork();

    if (child == -1) {
      LogError("fork");
      exit(10);
    }

    if (child) _exit(0);
  }

  if (setsid() == -1) {                        
    LogError("setsid");
    exit(13);
  }
}
#endif /* background() */

static int
validateInterval (int *value, const char *string) {
  if (!string || !*string) return 1;

  {
    static const int minimum = 1;
    int ok = validateInteger(value, string, &minimum, NULL);
    if (ok) *value *= 10;
    return ok;
  }
}

void
startup (int argc, char *argv[]) {
  int problemCount = processOptions(optionTable, optionCount,
                                    "brltty", &argc, &argv,
                                    &opt_bootParameters,
                                    &opt_environmentVariables,
                                    &opt_configurationFile,
                                    NULL);

  {
    char **const paths[] = {
      &opt_libraryDirectory,
      &opt_writableDirectory,
      &opt_dataDirectory,
      &opt_tablesDirectory,

#ifdef ENABLE_CONTRACTED_BRAILLE
      &opt_contractionsDirectory,
#endif /* ENABLE_CONTRACTED_BRAILLE */

      NULL
    };
    fixInstallPaths(paths);
  }

  if (argc) {
    LogPrint(LOG_ERR, "%s: %s", gettext("excess argument"), argv[0]);
    ++problemCount;
  }

  if (!validateInterval(&updateInterval, opt_updateInterval)) {
    LogPrint(LOG_ERR, "%s: %s", gettext("invalid update interval"), opt_updateInterval);
    ++problemCount;
  }

  if (!validateInterval(&messageDelay, opt_messageDelay)) {
    LogPrint(LOG_ERR, "%s: %s", gettext("invalid message delay"), opt_messageDelay);
    ++problemCount;
  }

  /* Set logging levels. */
  {
    int level = LOG_NOTICE;

    if (opt_logLevel && *opt_logLevel) {
      static const char *const words[] = {
        "emergency", "alert", "critical", "error",
        "warning", "notice", "information", "debug"
      };
      static unsigned int count = ARRAY_COUNT(words);

      {
        int length = strlen(opt_logLevel);
        int index;
        for (index=0; index<count; ++index) {
          const char *word = words[index];
          if (strncasecmp(opt_logLevel, word, length) == 0) {
            level = index;
            goto setLevel;
          }
        }
      }

      {
        int value;
        if (isInteger(&value, opt_logLevel) && (value >= 0) && (value < count)) {
          level = value;
          goto setLevel;
        }
      }

      LogPrint(LOG_ERR, "%s: %s", gettext("invalid log level"), opt_logLevel);
      ++problemCount;
    }
  setLevel:

    setLogLevel(level);
    setPrintLevel((opt_version || opt_verify)?
                    (opt_quiet? LOG_NOTICE: LOG_INFO):
                    (opt_quiet? LOG_WARNING: LOG_NOTICE));
    if (opt_standardError) LogClose();
  }

  {
    const char *prefix = setPrintPrefix(NULL);
    char banner[0X100];
    makeProgramBanner(banner, sizeof(banner));
    LogPrint(LOG_NOTICE, "%s [%s]", banner, PACKAGE_URL);
    setPrintPrefix(prefix);
  }

  if (opt_version) {
    LogPrint(LOG_INFO, "%s", PACKAGE_COPYRIGHT);
    identifyScreenDrivers(1);

#ifdef ENABLE_API
    api_identify(1);
#endif /* ENABLE_API */

    identifyBrailleDrivers(1);

#ifdef ENABLE_SPEECH_SUPPORT
    identifySpeechDrivers(1);
#endif /* ENABLE_SPEECH_SUPPORT */

    exit(0);
  }

#ifdef __MINGW32__
  {
    const char *name = "BrlAPI";
    const char *description = "Braille API (BrlAPI)";
    int stop = 0;

    if (opt_installService) {
      installService(name, description);
      stop = 1;
    }

    if (opt_removeService) {
      removeService(name);
      stop = 1;
    }

    if (stop) exit(0);
  }
#endif /* __MINGW32__ */

  if (opt_verify) opt_noDaemon = 1;
  if (!opt_noDaemon
#ifdef __MINGW32__
      && !isWindowsService
#endif
  ) background();

  if (!opt_standardError) {
    LogClose();
    LogOpen(1);
  }

  if (!opt_noDaemon) {
    setPrintOff();

    {
      const char *nullDevice = "/dev/null";

      freopen(nullDevice, "r", stdin);
      freopen(nullDevice, "a", stdout);

      if (opt_standardError) {
        fflush(stderr);
      } else {
        freopen(nullDevice, "a", stderr);
      }
    }

#ifdef __MINGW32__
    {
      HANDLE h = CreateFile("NUL", GENERIC_READ|GENERIC_WRITE,
                            FILE_SHARE_READ|FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, 0, NULL);

      if (!h) {
        LogWindowsError("CreateFile[NUL]");
      } else {
        SetStdHandle(STD_INPUT_HANDLE, h);
        SetStdHandle(STD_OUTPUT_HANDLE, h);

        if (opt_standardError) {
          fflush(stderr);
        } else {
          SetStdHandle(STD_ERROR_HANDLE, h);
        }
      }
    }
#endif /* __MINGW32__ */
  }

  /*
   * From this point, all IO functions as printf, puts, perror, etc. can't be
   * used anymore since we are a daemon.  The LogPrint facility should 
   * be used instead.
   */

  atexit(exitTunes);
  suppressTuneDeviceOpenErrors();

  /* Create the process identifier file. */
  if (opt_pidFile) createPidFile();

  {
    const char *directories[] = {opt_dataDirectory, "/", NULL};
    const char **directory = directories;

    while (*directory) {
      if (setWorkingDirectory(*directory)) break;                /* * change to directory containing data files  */
      ++directory;
    }
  }

  {
    char *directory;
    if ((directory = getWorkingDirectory())) {
      LogPrint(LOG_INFO, "%s: %s", gettext("Working Directory"), directory);
      free(directory);
    } else {
      LogPrint(LOG_ERR, "%s: %s", gettext("cannot determine working directory"), strerror(errno));
    }
  }

  LogPrint(LOG_INFO, "%s: %s", gettext("Writable Directory"), opt_writableDirectory);
  writableDirectory = opt_writableDirectory;

  LogPrint(LOG_INFO, "%s: %s", gettext("Configuration File"), opt_configurationFile);
  LogPrint(LOG_INFO, "%s: %s", gettext("Data Directory"), opt_dataDirectory);
  LogPrint(LOG_INFO, "%s: %s", gettext("Library Directory"), opt_libraryDirectory);
  LogPrint(LOG_INFO, "%s: %s", gettext("Tables Directory"), opt_tablesDirectory);

  if (opt_textTable) {
    fixTextTablePath(&opt_textTable);
    if (!replaceTextTable(opt_textTable)) opt_textTable = NULL;
  }
  if (!opt_textTable) {
    opt_textTable = TEXT_TABLE;
    makeUntextTable();
  }
  LogPrint(LOG_INFO, "%s: %s", gettext("Text Table"), opt_textTable);
#ifdef ENABLE_PREFERENCES_MENU
#ifdef ENABLE_TABLE_SELECTION
  globPrepare(&glob_textTable, opt_tablesDirectory,
              TEXT_TABLE_PREFIX "*" TRANSLATION_TABLE_EXTENSION,
              opt_textTable, 0);
#endif /* ENABLE_TABLE_SELECTION */
#endif /* ENABLE_PREFERENCES_MENU */

  if (opt_attributesTable) {
    fixAttributesTablePath(&opt_attributesTable);
    replaceAttributesTable(opt_attributesTable);
  } else {
    opt_attributesTable = ATTRIBUTES_TABLE;
  }
  LogPrint(LOG_INFO, "%s: %s", gettext("Attributes Table"), opt_attributesTable);
#ifdef ENABLE_PREFERENCES_MENU
#ifdef ENABLE_TABLE_SELECTION
  globPrepare(&glob_attributesTable, opt_tablesDirectory,
              "attr*" TRANSLATION_TABLE_EXTENSION,
              opt_attributesTable, 0);
#endif /* ENABLE_TABLE_SELECTION */
#endif /* ENABLE_PREFERENCES_MENU */

#ifdef ENABLE_CONTRACTED_BRAILLE
  LogPrint(LOG_INFO, "%s: %s", gettext("Contractions Directory"), opt_contractionsDirectory);
  if (opt_contractionTable) {
    fixContractionTablePath(&opt_contractionTable);
    loadContractionTable(opt_contractionTable);
  }
  atexit(exitContractionTable);
  LogPrint(LOG_INFO, "%s: %s", gettext("Contraction Table"),
           opt_contractionTable? opt_contractionTable: gettext("none"));
#ifdef ENABLE_PREFERENCES_MENU
#ifdef ENABLE_TABLE_SELECTION
  globPrepare(&glob_contractionTable, opt_contractionsDirectory,
              CONTRACTION_TABLE_PREFIX "*" CONTRACTION_TABLE_EXTENSION,
              opt_contractionTable, 1);
#endif /* ENABLE_TABLE_SELECTION */
#endif /* ENABLE_PREFERENCES_MENU */
#endif /* ENABLE_CONTRACTED_BRAILLE */

  /* initialize screen driver */
  atexit(exitScreen);
  constructSpecialScreens();
  screenDrivers = splitString(opt_screenDriver? opt_screenDriver: "", ',', NULL);
  if (opt_verify) {
    if (activateScreenDriver(1)) deactivateScreenDriver();
  } else {
    tryScreenDriver();
  }
  
#ifdef ENABLE_API
  apiStarted = 0;
  if (!opt_noApi) {
    api_identify(0);
    apiParameters = processParameters(api_parameters,
                                      NULL,
                                      opt_apiParameters);
    logParameters(api_parameters, apiParameters,
                  gettext("API Parameter"));
    if (!opt_verify) {
      if (api_start(&brl, apiParameters)) {
        atexit(exitApi);
        apiStarted = 1;
      }
    }
  }
#endif /* ENABLE_API */

  /* The device(s) the braille display might be connected to. */
  if (!*opt_brailleDevice) {
    LogPrint(LOG_ERR, gettext("braille device not specified"));
    exit(4);
  }
  brailleDevices = splitString(opt_brailleDevice, ',', NULL);

  /* Activate the braille display. */
  brailleDrivers = splitString(opt_brailleDriver? opt_brailleDriver: "", ',', NULL);
  brailleConstructed = 0;
  if (opt_verify) {
    if (activateBrailleDriver(1)) deactivateBrailleDriver();
  } else {
    resetPreferences();
    atexit(exitBrailleDriver);
    tryBrailleDriver();
  }

#ifdef ENABLE_SPEECH_SUPPORT
  /* Activate the speech synthesizer. */
  speechDrivers = splitString(opt_speechDriver? opt_speechDriver: "", ',', NULL);
  if (opt_verify) {
    if (activateSpeechDriver(1)) deactivateSpeechDriver();
  } else {
    atexit(exitSpeechDriver);
    trySpeechDriver();
  }

  /* Create the speech pass-through FIFO. */
  LogPrint(LOG_INFO, "%s: %s", gettext("Speech FIFO"),
           opt_speechFifo? opt_speechFifo: gettext("none"));
  if (!opt_verify) {
    if (opt_speechFifo) openSpeechFifo(opt_dataDirectory, opt_speechFifo);
  }
#endif /* ENABLE_SPEECH_SUPPORT */

  if (opt_verify) exit(0);

  if (problemCount) {
    char buffer[0X40];
    snprintf(buffer, sizeof(buffer),
             ngettext("%d startup problem", "%d startup problems", problemCount),
             problemCount);
    message(buffer, MSG_WAITKEY);
  }
}
