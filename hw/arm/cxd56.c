/*
 * CXD56XX
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/arm/boot.h"
#include "qemu/timer.h"
#include "hw/boards.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "sysemu/runstate.h"
#include "sysemu/sysemu.h"
#include "hw/arm/armv7m.h"
#include "hw/char/pl011.h"
#include "hw/irq.h"
#include "hw/loader.h"
#include "hw/misc/unimp.h"
#include "cpu.h"
#include "target/arm/arm-powerctl.h"


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
    switch (offset) {
    case 0x0030:
        fprintf(stderr, "CRG: reset 0x%x\n", (int)value);
        if (value != 0) {
            int cpu;
            for (cpu = 1; cpu < 6; cpu++) {
                if (value & (1 << (16 + cpu))) {
                    fprintf(stderr, "CRG: boot cpu %d\n", cpu);
                    arm_set_cpu_on_and_reset(cpu);
                }
            }
        }

        return;

    case 0x0040:
        fprintf(stderr, "CRG: ck_gate_ahb 0x%x\n", (int)value);
        return;
    }

    fprintf(stderr,
                  "CRG: write at bad offset 0x%x\n", (int)offset);
}

static const MemoryRegionOps cxd56_crg_ops = {
    .read = cxd56_crg_read,
    .write = cxd56_crg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

typedef struct {
    MemoryRegion iomem;
} cxd56_cpuid_state;

static uint64_t cxd56_cpuid_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    return current_cpu->cpu_index + 2;
}

static const MemoryRegionOps cxd56_cpuid_ops = {
    .read = cxd56_cpuid_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

typedef struct {
    MemoryRegion iomem;
} cxd56_swint_state;

static qemu_irq cxd56_swint[8];

static void cxd56_swint_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    int cpu = (offset / 4) - 2;
    qemu_set_irq(cxd56_swint[cpu], value);
#if 0
    fprintf(stderr,
                  "swint: write at bad offset 0x%x 0x%x\n", (int)offset, (int)value);
#endif
}

static const MemoryRegionOps cxd56_swint_ops = {
    .write = cxd56_swint_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

typedef struct {
    MemoryRegion iomem;
} cxd56_nvic_sysreg_state;

static MemoryRegion *cxd56_nvic_sysreg[8];

static MemTxResult cxd56_nvic_sysreg_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size,
                                    MemTxAttrs attrs)
{
    return memory_region_dispatch_write(cxd56_nvic_sysreg[current_cpu->cpu_index], addr, value, size_memop(size) | MO_TE, attrs);
}

static MemTxResult cxd56_nvic_sysreg_read(void *opaque, hwaddr addr,
                                   uint64_t *data, unsigned size,
                                   MemTxAttrs attrs)
{
    return memory_region_dispatch_read(cxd56_nvic_sysreg[current_cpu->cpu_index], addr, data, size_memop(size) | MO_TE, attrs);
}

static const MemoryRegionOps cxd56_nvic_sysreg_ops = {
    .read_with_attrs = cxd56_nvic_sysreg_read,
    .write_with_attrs = cxd56_nvic_sysreg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

typedef struct {
    MemoryRegion iomem;
} cxd56_nvic_systick_state;

static MemoryRegion *cxd56_nvic_systick[8];

static MemTxResult cxd56_nvic_systick_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size,
                                    MemTxAttrs attrs)
{
    return memory_region_dispatch_write(cxd56_nvic_systick[current_cpu->cpu_index], addr, value, size_memop(size) | MO_TE, attrs);
}

static MemTxResult cxd56_nvic_systick_read(void *opaque, hwaddr addr,
                                   uint64_t *data, unsigned size,
                                   MemTxAttrs attrs)
{
    return memory_region_dispatch_read(cxd56_nvic_systick[current_cpu->cpu_index], addr, data, size_memop(size) | MO_TE, attrs);
}

static const MemoryRegionOps cxd56_nvic_systick_ops = {
    .read_with_attrs = cxd56_nvic_systick_read,
    .write_with_attrs = cxd56_nvic_systick_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/
#if 0
static
void do_sys_reset(void *opaque, int n, int level)
{
    if (level) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
    }
}
#endif
/***************/

static void cxd56_devices(void)
{
    cxd56_topreg_state *topreg;
    cxd56_topreg_sub_state *topreg_sub;
    cxd56_bkup_sram_state *bkup_sram;
    cxd56_crg_state *crg;
    cxd56_cpuid_state *cpuid;
    cxd56_swint_state *swint;
    cxd56_nvic_sysreg_state *nvic_sysreg;
    cxd56_nvic_systick_state *nvic_systick;

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

    cpuid = g_new0(cxd56_cpuid_state, 1);
    memory_region_init_io(&cpuid->iomem, NULL, &cxd56_cpuid_ops, crg, "cpuid", 4);
    memory_region_add_subregion(get_system_memory(), 0x0e002040, &cpuid->iomem);

    swint = g_new0(cxd56_swint_state, 1);
    memory_region_init_io(&swint->iomem, NULL, &cxd56_swint_ops, swint, "swint", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0x4600c000, &swint->iomem);

    nvic_sysreg = g_new0(cxd56_nvic_sysreg_state, 1);
    memory_region_init_io(&nvic_sysreg->iomem, NULL, &cxd56_nvic_sysreg_ops, nvic_sysreg, "nvic_sysreg", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0xe000e000, &nvic_sysreg->iomem);
    nvic_systick = g_new0(cxd56_nvic_systick_state, 1);
    memory_region_init_io(&nvic_systick->iomem, NULL, &cxd56_nvic_systick_ops, nvic_systick, "nvic_systick", 0xe0);
    memory_region_add_subregion_overlap(get_system_memory(), 0xe000e010, &nvic_systick->iomem, 1);
#if 0
    /* Add dummy regions for the devices we don't implement yet,
     * so guest accesses don't cause unlogged crashes.
     */
    create_unimplemented_device("cxd56.pmu_sub", 0x04106000, 0x1000);
    create_unimplemented_device("cxd56.freqdisc", 0x04107000, 0x1000);
    create_unimplemented_device("cxd56.spiflash", 0x04110000, 0x1000);
    create_unimplemented_device("cxd56.dmac", 0x04120000, 0x4000);
#endif
}

static void cxd56_init(MachineState *ms)
{
    DeviceState *nvic = NULL;
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();
    int n;
    unsigned int smp_cpus = ms->smp.cpus;

#if 0
    memory_region_init_rom(flash, NULL, "cxd56.flash", 65536,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0, flash);

    /* Tiny boot loader */
    static uint16_t code[] = {
        0x0000, 0x0d00,         /* vector: initial SP */
        0x000d, 0x0000,         /* vector: initial PC */
        0xed08, 0xe000,         /* (NVIC_VECTAB) */
        0x2000,                 /* movs   r0, #0 */
        0x6801,                 /* ldr    r1, [r0, #0] */
        0x6880,                 /* ldr    r0, [r0, #8] */
        0x6001,                 /* str    r1, [r0, #0] */
        0x6808,                 /* ldr    r0, [r1, #0] */
        0x4685,                 /* mov    sp, r0 */
        0x6848,                 /* ldr    r0, [r1, #4] */
        0x4687                  /* mov    pc, r0 */
    };
    rom_add_blob_fixed("cxd56.flash", code, sizeof(code), 0x00000000);

    memory_region_init_ram(sram, NULL, "cxd56.sram", 0x00180000,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0x0d000000, sram);

    for (n = 0; n < smp_cpus; n++) {
        nvic = qdev_create(NULL, TYPE_ARMV7M);
        qdev_prop_set_uint32(nvic, "cpunum", n);
        qdev_prop_set_uint32(nvic, "num-irq", NUM_IRQ_LINES);
        qdev_prop_set_string(nvic, "cpu-type", ms->cpu_type);
        qdev_prop_set_bit(nvic, "enable-bitband", false);
        if (n > 0) {
            object_property_set_bool(OBJECT(nvic), true,
                                     "start-powered-off", &error_abort);
        }

        qdev_init_nofail(nvic);

        ARMv7MState *s = ARMV7M(nvic);
        cxd56_nvic_sysreg[n] = &s->nvic.sysregmem;
        cxd56_nvic_systick[n] = &s->nvic.systickmem;
#if 0
        qdev_connect_gpio_out_named(nvic, "SYSRESETREQ", 0,
                                    qemu_allocate_irq(&do_sys_reset, NULL, 0));
#endif
        cxd56_swint[n] = qdev_get_gpio_in(nvic, 96);
        if (n == 0) {
            pl011_create(0x041ac000, qdev_get_gpio_in(nvic, 11), serial_hd(0));
        }
    }

    cxd56_devices();

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
    mc->max_cpus = 6;
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
