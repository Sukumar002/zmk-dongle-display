/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
 
 #include <lvgl.h>

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_CONTROL
#define LV_ATTRIBUTE_IMG_CONTROL
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_CONTROL uint8_t control_map[] = {
  0xff, 0xff, 0xff, 0xff, 	/*Color of index 0*/
  0x00, 0x00, 0x00, 0xff, 	/*Color of index 1*/

  0x00, 0x00, 
  0x00, 0x00, 
  0x03, 0x00, 
  0x07, 0x80, 
  0x0c, 0xc0, 
  0x18, 0x60, 
  0x30, 0x30, 
  0x20, 0x10, 
  0x00, 0x00, 
  0x00, 0x00, 
  0x00, 0x00, 
  0x00, 0x00, 
  0x00, 0x00, 
  0x00, 0x00, 
};

const lv_img_dsc_t control_icon = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 14,
  .header.h = 14,
  .data_size = 36,
  .data = control_map,
};

#ifndef LV_ATTRIBUTE_IMG_SHIFT
#define LV_ATTRIBUTE_IMG_SHIFT
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_SHIFT uint8_t shift_map[] = {
  0xff, 0xff, 0xff, 0xff, 	/*Color of index 0*/
  0x00, 0x00, 0x00, 0xff, 	/*Color of index 1*/

  0x00, 0x00, 
  0x00, 0x00, 
  0x03, 0x00, 
  0x07, 0x80, 
  0x0c, 0xc0, 
  0x18, 0x60, 
  0x30, 0x30, 
  0x78, 0x78, 
  0x08, 0x40, 
  0x08, 0x40, 
  0x08, 0x40, 
  0x0f, 0xc0, 
  0x00, 0x00, 
  0x00, 0x00, 
};

const lv_img_dsc_t shift_icon = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 14,
  .header.h = 14,
  .data_size = 36,
  .data = shift_map,
};


#ifndef LV_ATTRIBUTE_IMG_WIN
#define LV_ATTRIBUTE_IMG_WIN
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_WIN uint8_t win_map[] = {
  0xff, 0xff, 0xff, 0xff, 	/*Color of index 0*/
  0x00, 0x00, 0x00, 0xff, 	/*Color of index 1*/

  0x00, 0x00, 
  0x00, 0x00, 
  0x00, 0xf0, 
  0x0b, 0xf0, 
  0x3b, 0xf0, 
  0x3b, 0xf0, 
  0x3b, 0xf0, 
  0x00, 0x00, 
  0x3b, 0xf0, 
  0x3b, 0xf0, 
  0x3b, 0xf0, 
  0x03, 0xf0, 
  0x00, 0x30, 
  0x00, 0x00, 
};

const lv_img_dsc_t win_icon = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 14,
  .header.h = 14,
  .data_size = 36,
  .data = win_map,
};

#ifndef LV_ATTRIBUTE_IMG_CMD
#define LV_ATTRIBUTE_IMG_CMD
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_CMD uint8_t cmd_map[] = {
  0xff, 0xff, 0xff, 0xff, 	/*Color of index 0*/
  0x00, 0x00, 0x00, 0xff, 	/*Color of index 1*/

  0x00, 0x00,
  0x00, 0x00,
  0x18, 0x60,
  0x24, 0x90,
  0x24, 0x90,
  0x1f, 0xe0,
  0x04, 0x80,
  0x04, 0x80,
  0x1f, 0xe0,
  0x24, 0x90,
  0x24, 0x90,
  0x18, 0x60,
  0x00, 0x00,
  0x00, 0x00,
};

const lv_img_dsc_t cmd_icon = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 14,
  .header.h = 14,
  .data_size = 36,
  .data = cmd_map,
};

#ifndef LV_ATTRIBUTE_IMG_OPT
#define LV_ATTRIBUTE_IMG_OPT
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_OPT uint8_t opt_map[] = {
  0xff, 0xff, 0xff, 0xff, 	/*Color of index 0*/
  0x00, 0x00, 0x00, 0xff, 	/*Color of index 1*/

  0x00, 0x00,
  0x00, 0x00,
  0x3c, 0xe0,
  0x3c, 0xe0,
  0x06, 0x00,
  0x06, 0x00,
  0x06, 0x00,
  0x03, 0x00,
  0x03, 0x00,
  0x03, 0x00,
  0x01, 0xe0,
  0x01, 0xe0,
  0x00, 0x00,
  0x00, 0x00,
};

const lv_img_dsc_t opt_icon = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 14,
  .header.h = 14,
  .data_size = 36,
  .data = opt_map,
};


/* 9x9 framed modifier badges: inactive is black-on-white; active is white-on-black. */

#ifndef LV_ATTRIBUTE_IMG_MODIFIER_CONTROL_OFF
#define LV_ATTRIBUTE_IMG_MODIFIER_CONTROL_OFF
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_MODIFIER_CONTROL_OFF uint8_t modifier_control_off_map[] = {
  0xff 
  0xff,
  0xff 
  0xff,
  0x00 
  0x00,
  0x00 
  0xff,
  0xff 
  0x80,
  0x80 
  0x80,
  0x9c 
  0x80,
  0xb6 
  0x80,
  0xa2 
  0x80,
  0x80 
  0x80,
  0x80 
  0x80,
  0x80 
  0x80,
  0xff 
  0x80,
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
  0x00 
  0x00,
  0x00 
  0xff,
  0xff 
  0xff,
  0xff 
  0xff,
  0xff 
  0x80,
  0x80 
  0x80,
  0x9c 
  0x80,
  0xb6 
  0x80,
  0xa2 
  0x80,
  0x80 
  0x80,
  0x80 
  0x80,
  0x80 
  0x80,
  0xff 
  0x80,
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
  0xff 
  0xff,
  0xff 
  0xff,
  0x00 
  0x00,
  0x00 
  0xff,
  0xff 
  0x80,
  0x80 
  0x80,
  0x9c 
  0x80,
  0xb6 
  0x80,
  0xf7 
  0x80,
  0x94 
  0x80,
  0x9c 
  0x80,
  0x80 
  0x80,
  0xff 
  0x80,
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
  0x00 
  0x00,
  0x00 
  0xff,
  0xff 
  0xff,
  0xff 
  0xff,
  0xff 
  0x80,
  0x80 
  0x80,
  0x9c 
  0x80,
  0xb6 
  0x80,
  0xf7 
  0x80,
  0x94 
  0x80,
  0x9c 
  0x80,
  0x80 
  0x80,
  0xff 
  0x80,
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
  0xff 
  0xff,
  0xff 
  0xff,
  0x00 
  0x00,
  0x00 
  0xff,
  0xff 
  0x80,
  0x80 
  0x80,
  0x9e 
  0x80,
  0xbe 
  0x80,
  0xbe 
  0x80,
  0xbe 
  0x80,
  0xbe 
  0x80,
  0x82 
  0x80,
  0xff 
  0x80,
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
  0x00 
  0x00,
  0x00 
  0xff,
  0xff 
  0xff,
  0xff 
  0xff,
  0xff 
  0x80,
  0x80 
  0x80,
  0x9e 
  0x80,
  0xbe 
  0x80,
  0xbe 
  0x80,
  0xbe 
  0x80,
  0xbe 
  0x80,
  0x82 
  0x80,
  0xff 
  0x80,
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
  0xff 
  0xff,
  0xff 
  0xff,
  0x00 
  0x00,
  0x00 
  0xff,
  0xff 
  0x80,
  0x80 
  0x80,
  0xb6 
  0x80,
  0xbe 
  0x80,
  0x94 
  0x80,
  0xbe 
  0x80,
  0xb6 
  0x80,
  0x80 
  0x80,
  0xff 
  0x80,
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
  0x00 
  0x00,
  0x00 
  0xff,
  0xff 
  0xff,
  0xff 
  0xff,
  0xff 
  0x80,
  0x80 
  0x80,
  0xb6 
  0x80,
  0xbe 
  0x80,
  0x94 
  0x80,
  0xbe 
  0x80,
  0xb6 
  0x80,
  0x80 
  0x80,
  0xff 
  0x80,
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
  0xff 
  0xff,
  0xff 
  0xff,
  0x00 
  0x00,
  0x00 
  0xff,
  0xff 
  0x80,
  0x80 
  0x80,
  0xb6 
  0x80,
  0x98 
  0x80,
  0x98 
  0x80,
  0x88 
  0x80,
  0x8e 
  0x80,
  0x80 
  0x80,
  0xff 
  0x80,
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
  0x00 
  0x00,
  0x00 
  0xff,
  0xff 
  0xff,
  0xff 
  0xff,
  0xff 
  0x80,
  0x80 
  0x80,
  0xb6 
  0x80,
  0x98 
  0x80,
  0x98 
  0x80,
  0x88 
  0x80,
  0x8e 
  0x80,
  0x80 
  0x80,
  0xff 
  0x80,
};
const lv_img_dsc_t modifier_opt_on = {
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.w = 9,
  .header.h = 9,
  .data_size = 26,
  .data = modifier_opt_on_map,
};
