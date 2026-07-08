#ifndef __GESTURE_H
#define __GESTURE_H

#include <stdint.h>

/* 识别结果结构体 */
typedef struct {
    int32_t  class_id;
    char     class_name[24];
    float    score;
    /* bbox —— 无 bbox 时 has_bbox=0 */
    int32_t  x, y, w, h;
    uint8_t  has_bbox;
} GestureResult;

/* 全局最新结果，ISR 写入，主循环读取 */
extern volatile GestureResult g_gesture;
extern volatile uint8_t       g_gesture_ready;

/* 手势名称表（与 Python 端 NAMES 对齐） */
extern const char *GESTURE_NAMES[];

#endif
