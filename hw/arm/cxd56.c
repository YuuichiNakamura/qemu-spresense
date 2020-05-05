/*
 * CXD56XX
 *
 * Copyright (c) 2006 CodeSourcery.
 * Written by Paul Brook
 *
 * This code is licensed under the GPL.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/arm/boot.h"
#include "qemu/timer.h"
#include "hw/i2c/i2c.h"
#include "net/net.h"
#include "hw/boards.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "sysemu/runstate.h"
#include "sysemu/sysemu.h"
#include "hw/arm/armv7m.h"
#include "hw/char/pl011.h"
#include "hw/irq.h"
#include "hw/watchdog/cmsdk-apb-watchdog.h"
#include "migration/vmstate.h"
#include "hw/misc/unimp.h"
#include "cpu.h"


#define NUM_IRQ_LINES 128

static
void do_sys_reset(void *opaque, int n, int level)
{
    if (level) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
    }
}

static void cxd56_init(MachineState *ms)
{
    /* Memory map of SoC devices, from
     * cxd56 LM3S6965 Microcontroller Data Sheet (rev I)
     * http://www.ti.com/lit/ds/symlink/lm3s6965.pdf
     *
     * 40000000 wdtimer
     * 40002000 i2c (unimplemented)
     * 40004000 GPIO
     * 40005000 GPIO
     * 40006000 GPIO
     * 40007000 GPIO
     * 40008000 SSI
     * 4000c000 UART
     * 4000d000 UART
     * 4000e000 UART
     * 40020000 i2c
     * 40021000 i2c (unimplemented)
     * 40024000 GPIO
     * 40025000 GPIO
     * 40026000 GPIO
     * 40028000 PWM (unimplemented)
     * 4002c000 QEI (unimplemented)
     * 4002d000 QEI (unimplemented)
     * 40030000 gptimer
     * 40031000 gptimer
     * 40032000 gptimer
     * 40033000 gptimer
     * 40038000 ADC
     * 4003c000 analogue comparator (unimplemented)
     * 40048000 ethernet
     * 400fc000 hibernation module (unimplemented)
     * 400fd000 flash memory control (unimplemented)
     * 400fe000 system control
     */

    DeviceState *nvic;

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();

#if 0
    memory_region_init_rom(flash, NULL, "cxd56.flash", 65536,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0, flash);
#endif

    memory_region_init_ram(sram, NULL, "cxd56.sram", 0x00180000,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0x0d000000, sram);

    memory_region_init_alias(flash, NULL, "cxd56.flash", sram, 0, 0x00180000);
    memory_region_add_subregion(system_memory, 0x00000000, flash);

    nvic = qdev_create(NULL, TYPE_ARMV7M);
    qdev_prop_set_uint32(nvic, "num-irq", NUM_IRQ_LINES);
    qdev_prop_set_string(nvic, "cpu-type", ms->cpu_type);
    qdev_prop_set_bit(nvic, "enable-bitband", false);
    object_property_set_link(OBJECT(nvic), OBJECT(get_system_memory()),
                                     "memory", &error_abort);
    /* This will exit with an error if the user passed us a bad cpu_type */
    qdev_init_nofail(nvic);

    qdev_connect_gpio_out_named(nvic, "SYSRESETREQ", 0,
                                qemu_allocate_irq(&do_sys_reset, NULL, 0));

    pl011_create(0x041ac000, qdev_get_gpio_in(nvic, 11), serial_hd(0));

    /* Add dummy regions for the devices we don't implement yet,
     * so guest accesses don't cause unlogged crashes.
     */
    create_unimplemented_device("i2c-0", 0x40002000, 0x1000);
    create_unimplemented_device("i2c-2", 0x40021000, 0x1000);
    create_unimplemented_device("PWM", 0x40028000, 0x1000);
    create_unimplemented_device("QEI-0", 0x4002c000, 0x1000);
    create_unimplemented_device("QEI-1", 0x4002d000, 0x1000);
    create_unimplemented_device("analogue-comparator", 0x4003c000, 0x1000);
    create_unimplemented_device("hibernation", 0x400fc000, 0x1000);
    create_unimplemented_device("flash-control", 0x400fd000, 0x1000);

    armv7m_load_kernel(ARM_CPU(first_cpu), ms->kernel_filename, 0x00180000);
}

static void spresense_init(MachineState *machine)
{
    cxd56_init(machine);
}

static void spresense_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "SPRESENSE";
    mc->init = spresense_init;
    mc->ignore_memory_transaction_failures = true;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m4");
}

static const TypeInfo spresense_type = {
    .name = MACHINE_TYPE_NAME("spresense"),
    .parent = TYPE_MACHINE,
    .class_init = spresense_class_init,
};

static void cxd56_machine_init(void)
{
    type_register_static(&spresense_type);
}

type_init(cxd56_machine_init)
