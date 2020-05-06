/*
 * CXD56XX
 *
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

/***************/

typedef struct {
    MemoryRegion iomem;
} cxd56_topreg_state;

static uint64_t cxd56_topreg_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    switch (offset) {
    case 0x058c:
        return 0x48040000;
    }

    fprintf(stderr,
                  "TOPREG: read at bad offset 0x%x\n", (int)offset);
    return 0;
}

static void cxd56_topreg_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    fprintf(stderr,
                  "TOPREG: write at bad offset 0x%x\n", (int)offset);
}

static const MemoryRegionOps cxd56_topreg_ops = {
    .read = cxd56_topreg_read,
    .write = cxd56_topreg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

typedef struct {
    MemoryRegion iomem;
} cxd56_topreg_sub_state;

static uint64_t cxd56_topreg_sub_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    switch (offset) {
    case 0x0418:
        return 0x00000101;
    }

    fprintf(stderr,
                  "TOPREG_SUB: read at bad offset 0x%x\n", (int)offset);
    return 0;
}

static void cxd56_topreg_sub_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    fprintf(stderr,
                  "TOPREG_SUB: write at bad offset 0x%x\n", (int)offset);
}

static const MemoryRegionOps cxd56_topreg_sub_ops = {
    .read = cxd56_topreg_sub_read,
    .write = cxd56_topreg_sub_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

typedef struct {
    MemoryRegion iomem;
} cxd56_bkup_sram_state;

static uint64_t cxd56_bkup_sram_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    switch (offset) {
    case 0x000c:
        return 0x2020450f;
    }

    fprintf(stderr,
                  "BKUP_SRAM: read at bad offset 0x%x\n", (int)offset);
    return 0;
}

static void cxd56_bkup_sram_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    fprintf(stderr,
                  "BKUP_SRAM: write at bad offset 0x%x\n", (int)offset);
}

static const MemoryRegionOps cxd56_bkup_sram_ops = {
    .read = cxd56_bkup_sram_read,
    .write = cxd56_bkup_sram_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

typedef struct {
    MemoryRegion iomem;
} cxd56_crg_state;

static uint64_t cxd56_crg_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    switch (offset) {
    case 0x0000:
        return 0x00010001;
    }

    fprintf(stderr,
                  "CRG: read at bad offset 0x%x\n", (int)offset);
    return 0;
}

static void cxd56_crg_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    fprintf(stderr,
                  "CRG: write at bad offset 0x%x\n", (int)offset);
}

static const MemoryRegionOps cxd56_crg_ops = {
    .read = cxd56_crg_read,
    .write = cxd56_crg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

static
void do_sys_reset(void *opaque, int n, int level)
{
    if (level) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
    }
}

/***************/

static void cxd56_init(MachineState *ms)
{
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


    {
    cxd56_topreg_state *topreg;
    cxd56_topreg_sub_state *topreg_sub;
    cxd56_crg_state *crg;
    cxd56_bkup_sram_state *bkup_sram;

    topreg = g_new0(cxd56_topreg_state, 1);
    memory_region_init_io(&topreg->iomem, NULL, &cxd56_topreg_ops, topreg, "topreg", 0x3000);
    memory_region_add_subregion(get_system_memory(), 0x04100000, &topreg->iomem);
    topreg_sub = g_new0(cxd56_topreg_sub_state, 1);
    memory_region_init_io(&topreg_sub->iomem, NULL, &cxd56_topreg_sub_ops, topreg_sub, "topreg_sub", 0x3000);
    memory_region_add_subregion(get_system_memory(), 0x04103000, &topreg_sub->iomem);

    bkup_sram = g_new0(cxd56_bkup_sram_state, 1);
    memory_region_init_io(&bkup_sram->iomem, NULL, &cxd56_bkup_sram_ops, bkup_sram, "bkup_sram", 0x10000);
    memory_region_add_subregion(get_system_memory(), 0x04400000, &bkup_sram->iomem);

    crg = g_new0(cxd56_crg_state, 1);
    memory_region_init_io(&crg->iomem, NULL, &cxd56_crg_ops, crg, "crg", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0x4e011000, &crg->iomem);
    }

    /* Add dummy regions for the devices we don't implement yet,
     * so guest accesses don't cause unlogged crashes.
     */
    create_unimplemented_device("i2c-0", 0x40002000, 0x1000);
    create_unimplemented_device("i2c-2", 0x40021000, 0x1000);

    system_clock_scale = NANOSECONDS_PER_SECOND / 160000000;

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
