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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <hardware/hardware.h>
#include <hardware/boot_control.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <external/zlib/zlib.h>
#include "bootctrl.h"

static uint32_t gCurrentSlot;
static bool gInit;
static const char *gSlotSuffix[SLOT_NUM] = { "_a", "_b" };

#define NX_DEBUG "[NX_DEBUG]"

#define INIT_CHECK \
                       do \
                       {  \
                           if (!gInit) \
                           {               \
                               ALOGE("%s %s, module not inited", NX_DEBUG, __FUNCTION__); \
                               return -EINVAL; \
                           } \
                       }while(0)
#define SLOT_CHECK(slot, retval) \
                       do \
                       {  \
                           if (slot >= SLOT_NUM) \
                           {                     \
                               ALOGE("%s %s, slot %d too large", NX_DEBUG, __FUNCTION__, slot); \
                               return retval; \
                           } \
                       }while(0)

#define NX_TRACE ALOGI("%s %s enter", NX_DEBUG, __FUNCTION__)

void bootctrl_init(struct boot_control_module *module);
unsigned bootctrl_getNumberSlots(struct boot_control_module *module);
unsigned bootctrl_getCurrentSlot(struct boot_control_module *module);
int bootctrl_markBootSuccessful(struct boot_control_module *module);
int bootctrl_setActiveBootSlot(struct boot_control_module *module, unsigned slot);
int bootctrl_setSlotAsUnbootable(struct boot_control_module *module, unsigned slot);
int bootctrl_isSlotBootable(struct boot_control_module *module, unsigned slot);
const char* bootctrl_getSuffix(struct boot_control_module *module, unsigned slot);

static int crcCheck(nxBootctrl_t *ptBootctrl) {
    uint32_t crc = 0;
    uint8_t *pData = NULL;
    uint32_t len = 0;

    NX_TRACE;

    if(ptBootctrl == NULL)
        return -EINVAL;

    pData = (uint8_t *)ptBootctrl;
    len = sizeof(nxBootctrl_t) - sizeof(ptBootctrl->crc32_le);
    crc = crc32(0, pData, len);

    if(crc != ptBootctrl->crc32_le) {
        ALOGE("%s crcCheck failed, calcuated 0x%x, read 0x%x\n", NX_DEBUG, crc, ptBootctrl->crc32_le);
        return -EINVAL;
    }
    return 0;
}

static int crcSet(nxBootctrl_t *ptBootctrl) {
    uint32_t crc = 0;
    uint8_t *pData = NULL;
    uint32_t len = 0;

    NX_TRACE;
    if(ptBootctrl == NULL)
        return -EINVAL;

    pData = (uint8_t *)ptBootctrl;
    len = sizeof(nxBootctrl_t) - sizeof(ptBootctrl->crc32_le);

    crc = crc32(0, pData, len);
    ptBootctrl->crc32_le = crc;

    ALOGI("%s set crc = 0x%x\n", NX_DEBUG, crc);

    return 0;
}

void bootctrl_init(struct boot_control_module *module)
{
    int i;

    NX_TRACE;

    if(gInit) {
        ALOGI("%s bootctrl_init already called\n", NX_DEBUG);
        return;
    }

    char propbuf[PROPERTY_VALUE_MAX];
    // Get the suffix from the kernel commandline
    property_get("ro.boot.slot_suffix", propbuf, "");
    if (propbuf[0] == '\0') {
        ALOGE("%s property_get ro.boot.slot_suffix failed\n", NX_DEBUG);
        return;
    }

    ALOGI("slot suffix is %s\n", propbuf);
    for(i = 0; i < SLOT_NUM; i++) {
        if(strcmp(propbuf, gSlotSuffix[i]) == 0)
            break;
    }

    if(i >= SLOT_NUM) {
        ALOGE("%s slot suffix %s not valid\n", NX_DEBUG, propbuf);
        return;
    }

    gCurrentSlot = i;
    ALOGE("%s %s init gCurrentSlot set %d\n", NX_DEBUG, __FUNCTION__, i);
    gInit = true;

    return;
}

unsigned bootctrl_getNumberSlots(struct boot_control_module *module)
{
    NX_TRACE;
    return  SLOT_NUM;
}

unsigned bootctrl_getCurrentSlot(struct boot_control_module *module)
{
    NX_TRACE;
    INIT_CHECK;
    return  gCurrentSlot;
}

//return fd
static int bootctrl_load(nxBootctrl_t *ptBootctrl, bool read_only) {
    int fd = -1;
    int ret = 0;
    uint32_t nMagic = 0;

    NX_TRACE;
    if(ptBootctrl == NULL)
        return -EINVAL;

    fd = open(MISC_PARTITION, read_only ? O_RDONLY : O_RDWR);
    if(fd < 0) {
        ALOGE("%s %s, open %s failed, fd %d, errno %d", NX_DEBUG, __FUNCTION__, MISC_PARTITION, fd, errno);
        return -errno;
    }

    ALOGD("%s %s, BOOTCTRL_OFFSET = 0x%x", NX_DEBUG, __FUNCTION__, BOOTCTRL_OFFSET);

    lseek(fd, BOOTCTRL_OFFSET, SEEK_SET);

    do {
        ret = read(fd, ptBootctrl, sizeof(nxBootctrl_t));
    } while (ret == -1 && errno == EAGAIN);

    if (ret == -1) {
        close(fd);
        ALOGE("%s %s, read failed, errno %d", NX_DEBUG, __FUNCTION__, errno);
        return -errno;
    }
    // Read on block devices on Linux are never partial so fine to assert everything was read.
    assert(ret == sizeof(nxBootctrl_t));
    nMagic = ptBootctrl->magic;

    if (nMagic != BOOT_CTRL_MAGIC)
    {
        close(fd);
        ALOGE("%s %s, magic error, read Magic = 0x%x ", NX_DEBUG, __FUNCTION__, nMagic);
        ALOGE("%s %s, magic error, real Magic = 0x%x ", NX_DEBUG, __FUNCTION__, BOOT_CTRL_MAGIC_BIG_ENDIAN);
        return -EINVAL;
    }

    ret = crcCheck(ptBootctrl);
    if(ret) {
        close(fd);
        ALOGE("%s, crcCheck failed", __FUNCTION__);
        return ret;
    }

    return fd;
}

static int bootctrl_store(int fd, nxBootctrl_t *ptBootctrl) {
    int ret = 0;

    NX_TRACE;
    // ALOGI("%s fd = %d", NX_DEBUG, fd);
    if((fd < 0) || (ptBootctrl == NULL))
        return -EINVAL;

    crcSet(ptBootctrl);

    lseek(fd, BOOTCTRL_OFFSET, SEEK_SET);
    // ALOGI("%s BOOTCTRL_OFFSET = 0x%x", NX_DEBUG, BOOTCTRL_OFFSET);

    do {
        ret = write(fd, ptBootctrl, sizeof(nxBootctrl_t));
    } while (ret == -1 && errno == EAGAIN);

    if (ret == -1) {
        close(fd);
        ALOGE("%s, write failed, errno %d", __FUNCTION__, errno);
        return -errno;
    }

    // Writes on block devices on Linux are never partial so fine to assert everything was written.
    assert(ret == sizeof(nxBootctrl_t));
    ret = fsync(fd);

    if (ret == -1) {
       close(fd);
       ALOGE("%s, fsync failed, errno %d", __FUNCTION__, errno);
       return -errno;
    }

    if (ptBootctrl != NULL) {
        ALOGI("%s %s ptBootctrl = 0x%x", NX_DEBUG, __FUNCTION__, (uint32_t)ptBootctrl);
        ALOGI("%s %s ptBootctrl->slot_suffix = %s", NX_DEBUG, __FUNCTION__, ptBootctrl->slot_suffix);
        ALOGI("%s %s ptBootctrl->magic     = 0x%x", NX_DEBUG, __FUNCTION__, ptBootctrl->magic      );
        ALOGI("%s %s ptBootctrl->version   = 0x%x", NX_DEBUG, __FUNCTION__, ptBootctrl->version    );
        ALOGI("%s %s ptBootctrl->nb_slot   = 0x%x", NX_DEBUG, __FUNCTION__, ptBootctrl->nb_slot    );
        ALOGI("%s %s ptBootctrl->crc32_le  = 0x%x", NX_DEBUG, __FUNCTION__, ptBootctrl->crc32_le);
        ALOGI("%s %s ptBootctrl->slot_info[0].priority = %d", NX_DEBUG, __FUNCTION__, (uint32_t)ptBootctrl->slot_info[0].priority);
        ALOGI("%s %s ptBootctrl->slot_info[0].tries_re = %d", NX_DEBUG, __FUNCTION__, (uint32_t)ptBootctrl->slot_info[0].tries_remaining);
        ALOGI("%s %s ptBootctrl->slot_info[0].success_bo= %d", NX_DEBUG, __FUNCTION__, (uint32_t)ptBootctrl->slot_info[0].successful_boot);
        ALOGI("%s %s ptBootctrl->slot_info[1].priority = %d", NX_DEBUG, __FUNCTION__, (uint32_t)ptBootctrl->slot_info[1].priority);
        ALOGI("%s %s ptBootctrl->slot_info[1].tries_re = %d", NX_DEBUG, __FUNCTION__, (uint32_t)ptBootctrl->slot_info[1].tries_remaining);
        ALOGI("%s %s ptBootctrl->slot_info[1].success_bo= %d", NX_DEBUG, __FUNCTION__, (uint32_t)ptBootctrl->slot_info[1].successful_boot);
    }

    close(fd);

    return 0;
}

int bootctrl_markBootSuccessful(struct boot_control_module *module)
{
    int ret = 0;
    int fd = 0;
    nxBootctrl_t tBootctrl;

    NX_TRACE;
    INIT_CHECK;

    ALOGI("%s %s curSlot %d", NX_DEBUG, __FUNCTION__, gCurrentSlot);
    fd = bootctrl_load(&tBootctrl, false);

    if (fd < 0)
        ALOGE("%s %s failed", NX_DEBUG, __FUNCTION__);

    tBootctrl.slot_info[gCurrentSlot].successful_boot = 1;
    ALOGI("%s %s successful_boot slot[%d] : set 1", NX_DEBUG, __FUNCTION__, gCurrentSlot);

    ret = bootctrl_store(fd, &tBootctrl);

    return ret;
}

int bootctrl_setActiveBootSlot(struct boot_control_module *module, unsigned slot)
{
    int ret = 0;
    int fd = 0;
    nxBootctrl_t tBootctrl;

    NX_TRACE;
    INIT_CHECK;
    SLOT_CHECK(slot, -EINVAL);

    if(slot == gCurrentSlot) {
        ALOGI("%s %s, !!! warning, active slot is same as curSlot %d", NX_DEBUG, __FUNCTION__, slot);
    }

    fd = bootctrl_load(&tBootctrl, false);

    if (fd < 0)
        ALOGE("%s %s failed", NX_DEBUG, __FUNCTION__);

    tBootctrl.slot_info[slot].successful_boot = 0;
    tBootctrl.slot_info[slot].priority = 15;
    tBootctrl.slot_info[slot].tries_remaining = 7;

    ALOGI("%s %s successful_boot slot[%d] : set 0", NX_DEBUG, __FUNCTION__, slot);

    // lower other slot priority
    for(uint32_t i = 0; i < SLOT_NUM; i++) {
        if(i == slot)
            continue;

        if(tBootctrl.slot_info[i].priority >= 15)
            tBootctrl.slot_info[i].priority = 14;
    }
    ret = bootctrl_store(fd, &tBootctrl);

    return ret;
}

int bootctrl_setSlotAsUnbootable(struct boot_control_module *module, unsigned slot)
{
    // int retVal = 0;
    int ret = 0;
    int fd = 0;
    nxBootctrl_t tBootctrl;

    NX_TRACE;
    INIT_CHECK;
    SLOT_CHECK(slot, -EINVAL);

    if(slot == gCurrentSlot) {
        ALOGI("%s %s, !!! warning, set current slot %d unbootable", NX_DEBUG, __FUNCTION__, slot);
    }

    fd = bootctrl_load(&tBootctrl, false);

    if (fd < 0)
        ALOGE("%s %s failed", NX_DEBUG, __FUNCTION__);

    ALOGI("%s %s slot = %d", NX_DEBUG, __FUNCTION__, slot);

    tBootctrl.slot_info[slot].successful_boot = 0;
    tBootctrl.slot_info[slot].priority = 0;
    tBootctrl.slot_info[slot].tries_remaining = 0;
    ALOGI("%s %s successful_boot slot[%d] : set 0", NX_DEBUG, __FUNCTION__, slot);

    ret = bootctrl_store(fd, &tBootctrl);

    return ret;
}

int bootctrl_isSlotBootable(struct boot_control_module *module, unsigned slot)
{
    int ret = 0;
    int fd = 0;
    nxBootctrl_t tBootctrl;

    NX_TRACE;
    INIT_CHECK;
    SLOT_CHECK(slot, -EINVAL);
    fd = bootctrl_load(&tBootctrl, true);

    if (fd < 0)
        ALOGE("%s %s failed", NX_DEBUG, __FUNCTION__);

    if((tBootctrl.slot_info[slot].successful_boot > 0) || (tBootctrl.slot_info[slot].tries_remaining > 0)) {
        ret = 1;
    }

    close(fd);

    return ret;
}

const char* bootctrl_getSuffix(struct boot_control_module *module, unsigned slot)
{
    NX_TRACE;

    SLOT_CHECK(slot, NULL);
    return gSlotSuffix[slot];
}

static int boot_control_check_slot_sanity(struct boot_control_module *module, unsigned slot)
{
    if (!module)
        return -1;

    uint32_t num_slots = bootctrl_getNumberSlots(module);
    if ((num_slots < 1) || (slot > num_slots - 1)) {
        ALOGE("Invalid slot number");
        return -1;
    }
    return 0;
}

int bootctrl_isSlotMarkedSuccessful(struct boot_control_module *module, unsigned slot)
{
    int fd = 0;
    int ret = -1;
    nxBootctrl_t tBootctrl;

    NX_TRACE;

    if (boot_control_check_slot_sanity(module, slot) != 0) {
        ALOGE("%s: Argument check failed", __func__);
        goto error;
    }

    fd = bootctrl_load(&tBootctrl, true);

    if (fd < 0)
        ALOGE("%s %s failed", NX_DEBUG, __FUNCTION__);

    ret = tBootctrl.slot_info[slot].successful_boot;

    close(fd);

    return ret;

error:
        return -1;
}


static struct hw_module_methods_t bootctrl_module_methods = {
        .open = NULL
};

boot_control_module_t HAL_MODULE_INFO_SYM = {
        .common = {
                .tag = HARDWARE_MODULE_TAG,
                .module_api_version = BOOT_CONTROL_MODULE_API_VERSION_0_1,
                .hal_api_version = 0,
                .id = BOOT_CONTROL_HARDWARE_MODULE_ID,
                .name =  "Nexell Boot Control",
                .author = "Nexell Corporation.",
                .methods = &bootctrl_module_methods,
                .dso = NULL,
                .reserved = {0}
        },
        .init = bootctrl_init,
        .getNumberSlots = bootctrl_getNumberSlots,
        .getCurrentSlot = bootctrl_getCurrentSlot,
        .markBootSuccessful = bootctrl_markBootSuccessful,
        .setActiveBootSlot = bootctrl_setActiveBootSlot,
        .setSlotAsUnbootable = bootctrl_setSlotAsUnbootable,
        .isSlotBootable = bootctrl_isSlotBootable,
        .getSuffix = bootctrl_getSuffix,
        .isSlotMarkedSuccessful = bootctrl_isSlotMarkedSuccessful,
};
