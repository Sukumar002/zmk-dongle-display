/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */
#include <lvgl.h>
#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif
#ifndef LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_CONST
#endif
#ifndef LV_ATTRIBUTE_IMG_MODIFIER_CONTROL_OFF
#define LV_ATTRIBUTE_IMG_MODIFIER_CONTROL_OFF
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_CONTROL_OFF uint8_t modifier_control_off_map[] = {
  0xff,
  0xff,
  0xff,
  0xff,
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0x80,
  0x80,
  0x80,
  0x9c,
  0x80,
  0xb6,
  0x80,
  0xa2,
  0x80,
  0x80,
  0x80,
  0x80,
  0x80,
  0x80,
  0x80,
  0xff,
  0x80
};
const lv_img_dsc_t modifier_control_off = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_control_off_map,
};

#ifndef LV_ATTRIBUTE_IMG_MODIFIER_CONTROL_ON
#define LV_ATTRIBUTE_IMG_MODIFIER_CONTROL_ON
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_CONTROL_ON uint8_t modifier_control_on_map[] = {
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0x80,
  0x80,
  0x80,
  0x9c,
  0x80,
  0xb6,
  0x80,
  0xa2,
  0x80,
  0x80,
  0x80,
  0x80,
  0x80,
  0x80,
  0x80,
  0xff,
  0x80
};
const lv_img_dsc_t modifier_control_on = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_control_on_map,
};

#ifndef LV_ATTRIBUTE_IMG_MODIFIER_SHIFT_OFF
#define LV_ATTRIBUTE_IMG_MODIFIER_SHIFT_OFF
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_SHIFT_OFF uint8_t modifier_shift_off_map[] = {
  0xff,
  0xff,
  0xff,
  0xff,
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0x80,
  0x80,
  0x80,
  0x9c,
  0x80,
  0xb6,
  0x80,
  0xf7,
  0x80,
  0x94,
  0x80,
  0x9c,
  0x80,
  0x80,
  0x80,
  0xff,
  0x80
};
const lv_img_dsc_t modifier_shift_off = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_shift_off_map,
};

#ifndef LV_ATTRIBUTE_IMG_MODIFIER_SHIFT_ON
#define LV_ATTRIBUTE_IMG_MODIFIER_SHIFT_ON
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_SHIFT_ON uint8_t modifier_shift_on_map[] = {
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0x80,
  0x80,
  0x80,
  0x9c,
  0x80,
  0xb6,
  0x80,
  0xf7,
  0x80,
  0x94,
  0x80,
  0x9c,
  0x80,
  0x80,
  0x80,
  0xff,
  0x80
};
const lv_img_dsc_t modifier_shift_on = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_shift_on_map,
};

#ifndef LV_ATTRIBUTE_IMG_MODIFIER_WIN_OFF
#define LV_ATTRIBUTE_IMG_MODIFIER_WIN_OFF
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_WIN_OFF uint8_t modifier_win_off_map[] = {
  0xff,
  0xff,
  0xff,
  0xff,
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0x80,
  0x80,
  0x80,
  0x9e,
  0x80,
  0xbe,
  0x80,
  0xbe,
  0x80,
  0xbe,
  0x80,
  0xbe,
  0x80,
  0x82,
  0x80,
  0xff,
  0x80
};
const lv_img_dsc_t modifier_win_off = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_win_off_map,
};

#ifndef LV_ATTRIBUTE_IMG_MODIFIER_WIN_ON
#define LV_ATTRIBUTE_IMG_MODIFIER_WIN_ON
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_WIN_ON uint8_t modifier_win_on_map[] = {
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0x80,
  0x80,
  0x80,
  0x9e,
  0x80,
  0xbe,
  0x80,
  0xbe,
  0x80,
  0xbe,
  0x80,
  0xbe,
  0x80,
  0x82,
  0x80,
  0xff,
  0x80
};
const lv_img_dsc_t modifier_win_on = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_win_on_map,
};

#ifndef LV_ATTRIBUTE_IMG_MODIFIER_CMD_OFF
#define LV_ATTRIBUTE_IMG_MODIFIER_CMD_OFF
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_CMD_OFF uint8_t modifier_cmd_off_map[] = {
  0xff,
  0xff,
  0xff,
  0xff,
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0x80,
  0x80,
  0x80,
  0xb6,
  0x80,
  0xbe,
  0x80,
  0x94,
  0x80,
  0xbe,
  0x80,
  0xb6,
  0x80,
  0x80,
  0x80,
  0xff,
  0x80
};
const lv_img_dsc_t modifier_cmd_off = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_cmd_off_map,
};

#ifndef LV_ATTRIBUTE_IMG_MODIFIER_CMD_ON
#define LV_ATTRIBUTE_IMG_MODIFIER_CMD_ON
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_CMD_ON uint8_t modifier_cmd_on_map[] = {
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0x80,
  0x80,
  0x80,
  0xb6,
  0x80,
  0xbe,
  0x80,
  0x94,
  0x80,
  0xbe,
  0x80,
  0xb6,
  0x80,
  0x80,
  0x80,
  0xff,
  0x80
};
const lv_img_dsc_t modifier_cmd_on = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_cmd_on_map,
};

#ifndef LV_ATTRIBUTE_IMG_MODIFIER_OPT_OFF
#define LV_ATTRIBUTE_IMG_MODIFIER_OPT_OFF
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_OPT_OFF uint8_t modifier_opt_off_map[] = {
  0xff,
  0xff,
  0xff,
  0xff,
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0x80,
  0x80,
  0x80,
  0xb6,
  0x80,
  0x98,
  0x80,
  0x98,
  0x80,
  0x88,
  0x80,
  0x8e,
  0x80,
  0x80,
  0x80,
  0xff,
  0x80
};
const lv_img_dsc_t modifier_opt_off = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_opt_off_map,
};

#ifndef LV_ATTRIBUTE_IMG_MODIFIER_OPT_ON
#define LV_ATTRIBUTE_IMG_MODIFIER_OPT_ON
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_OPT_ON uint8_t modifier_opt_on_map[] = {
  0x00,
  0x00,
  0x00,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0x80,
  0x80,
  0x80,
  0xb6,
  0x80,
  0x98,
  0x80,
  0x98,
  0x80,
  0x88,
  0x80,
  0x8e,
  0x80,
  0x80,
  0x80,
  0xff,
  0x80
};
const lv_img_dsc_t modifier_opt_on = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_opt_on_map,
};
