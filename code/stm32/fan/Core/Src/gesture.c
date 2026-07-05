#include "gesture.h"

volatile GestureResult g_gesture = {0};
volatile uint8_t       g_gesture_ready = 0;

const char *GESTURE_NAMES[] = {
    "grabbing",    "grip",          "holy",         "point",
    "call",        "three3",        "timeout",      "xsign",
    "hand_heart",  "hand_heart2",   "little_finger","middle_finger",
    "take_picture","dislike",       "fist",         "four",
    "like",        "mute",          "ok",           "one",
    "palm",        "peace",         "peace_inverted","rock",
    "stop",        "stop_inverted", "three",        "three2",
    "two_up",      "two_up_inverted","three_gun",   "thumb_index",
    "thumb_index2","no_gesture"
};
