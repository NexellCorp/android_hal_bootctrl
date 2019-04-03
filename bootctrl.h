/*
 * Copyright 2015 The Android Open Source Project
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BOOTCTRLH
#define BOOTCTRLH

#include <stdint.h>
#include <bootable/recovery/bootloader.h>

#define SLOT_NUM 2

#define BOOT_CTRL_MAGIC_LITTLE_ENDIAN   BOOT_CTRL_MAGIC //0x42414342
#define BOOT_CTRL_MAGIC_BIG_ENDIAN      0x42434142

#define BOOTCTRL_OFFSET             offsetof(struct bootloader_message_ab, slot_suffix)
#define CRC_DATA_OFFSET             (uint32_t)(&(((nxBootctrl_t *)0)->slot_info[0]))

typedef struct bootloader_control nxBootctrl_t;

#endif //BOOTCTRLH
