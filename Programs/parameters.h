/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2014 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version. Please see the file LICENSE-GPL for details.
 *
 * Web Page: http://mielke.cc/brltty/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

#ifndef BRLTTY_INCLUDED_PARAMETERS
#define BRLTTY_INCLUDED_PARAMETERS

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define PROGRAM_TERMINATION_REQUEST_COUNT_THRESHOLD 3
#define PROGRAM_TERMINATION_REQUEST_RESET_SECONDS 5

#define BRAILLE_DRIVER_START_RETRY_INTERVAL 5000
#define BRAILLE_INPUT_POLL_INTERVAL 40

#define SPEECH_DRIVER_START_RETRY_INTERVAL 5000
#define SPEECH_DRIVER_START_AUTOSPEAK_DELAY 4000

#define SPEECH_DRIVER_THREAD_START_TIMEOUT 15000
#define SPEECH_DRIVER_THREAD_STOP_TIMEOUT 5000

#define SPEECH_REQUEST_WAIT_DURATION 1000000
#define SPEECH_RESPONSE_WAIT_TIMEOUT 5000

#define SCREEN_DRIVER_START_RETRY_INTERVAL 5000
#define SCREEN_UPDATE_POLL_INTERVAL 40

#define KEYBOARD_MONITOR_START_RETRY_INTERVAL 5000

#define PID_FILE_CREATE_RETRY_INTERVAL 5000

#define UPDATE_SCHEDULE_DELAY 15

#define TUNE_DEVICE_CLOSE_DELAY 2000
#define TUNE_TOGGLE_REPEAT_DELAY 100

#define MESSAGE_HOLD_TIMEOUT 4000

#define LEARN_MODE_TIMEOUT 10000

#define MOUNT_TABLE_UPDATE_RETRY_INTERVAL 5000

#define GPM_CONNECTION_RESET_DELAY 5000

#define GIO_USB_INPUT_MONITOR_DISABLE 0

#define SERIAL_DEVICE_RESTART_DELAY 500

#define USB_INPUT_AWAIT_RETRY_INTERVAL_MINIMUM 10
#define USB_INPUT_READ_INITIAL_TIMEOUT_DEFAULT 20
#define USB_INPUT_URB_RESUBMIT_DELAY 40
#define USB_INPUT_INTERRUPT_URB_COUNT 8

#define BLUETOOTH_DEVICE_NAME_OBTAIN_TIMEOUT 5000
#define BLUETOOTH_CHANNEL_BUSY_RETRY_TIMEOUT 2000
#define BLUETOOTH_CHANNEL_BUSY_RETRY_INTERVAL 100
#define BLUETOOTH_CHANNEL_CONNECT_TIMEOUT 15000

#define LINUX_INPUT_DEVICE_OPEN_DELAY 1000
#define LINUX_USB_INPUT_PIPE_DISABLE 0
#define LINUX_USB_INPUT_TREAT_INTERRUPT_AS_BULK 0
#define LINUX_BLUETOOTH_NAME_OBTAIN_ASYNCHRONOUS 1
#define LINUX_BLUETOOTH_CHANNEL_DISCOVER_ASYNCHRONOUS 1
#define LINUX_BLUETOOTH_CHANNEL_CONNECT_ASYNCHRONOUS 1

#define WINDOWS_FILE_LOCK_RETRY_INTERVAL 1000

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BRLTTY_INCLUDED_PARAMETERS */
