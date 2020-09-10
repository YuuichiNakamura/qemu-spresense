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

//#define DEBUG

#ifdef DEBUG
#define REGERR(...)     fprintf(stderr, __VA_ARGS__)
#else
#define REGERR(...)
#endif

#define NUM_IRQ_LINES 128

typedef struct {
    MemoryRegion topreg;
    MemoryRegion topreg_sub;
    MemoryRegion scu;
    MemoryRegion bkup_sram;
    MemoryRegion crg;
    MemoryRegion cpuid;
    MemoryRegion cpuid2;
    MemoryRegion swint;
    MemoryRegion cpufifo;
    MemoryRegion nvic_sysreg;
    MemoryRegion nvic_systick;

    qemu_irq swint_irq[8];
    MemoryRegion *real_nvic_sysreg[8];
    MemoryRegion *real_nvic_systick[8];

    qemu_irq cpufifo_from_irq;
    uint32_t cpufifo_wrd0;
} cxd56_device_state;

/***************/

static uint64_t cxd56_topreg_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    switch (offset) {
    case 0x0044:
        return 0x00000001;
    case 0x04c4:
        return 0xa0500500;
    case 0x04f8:
        return 0xffffffff;
    case 0x058c:
        return 0x48040000;
    }

    REGERR("TOPREG: read at bad offset 0x%x\n", (int)offset);
    return 0;
}

static void cxd56_topreg_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    switch (offset) {
    case 0x2168:
    case 0x216c:
    case 0x2170:
    case 0x2174:
        return; /* LED */
    }

    REGERR("TOPREG: write at bad offset 0x%x\n", (int)offset);
}

static const MemoryRegionOps cxd56_topreg_ops = {
    .read = cxd56_topreg_read,
    .write = cxd56_topreg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

static uint64_t cxd56_topreg_sub_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    switch (offset) {
    case 0x0418:
        return 0x00000101;
    case 0x1490:
        return 0x00000002;
    }

    REGERR("TOPREG_SUB: read at bad offset 0x%x\n", (int)offset);
    return 0;
}

static void cxd56_topreg_sub_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    REGERR("TOPREG_SUB: write at bad offset 0x%x\n", (int)offset);
}

static const MemoryRegionOps cxd56_topreg_sub_ops = {
    .read = cxd56_topreg_sub_read,
    .write = cxd56_topreg_sub_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

static uint64_t cxd56_scu_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    switch (offset) {
    case 0x0020:
        return 0x00000003;
    }

    REGERR("scu: read at bad offset 0x%x\n", (int)offset);
    return 0;
}

static void cxd56_scu_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    REGERR("scu: write at bad offset 0x%x\n", (int)offset);
}

static const MemoryRegionOps cxd56_scu_ops = {
    .read = cxd56_scu_read,
    .write = cxd56_scu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

static uint64_t cxd56_bkup_sram_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    switch (offset) {
    case 0x0000:        /* rcosc_clock */
        return 0x007e5000;
    case 0x000c:        /* sysfw_version */
//        return 0x2020450f;
        return 0x20204ea1;
    }

    REGERR("BKUP_SRAM: read at bad offset 0x%x\n", (int)offset);
    return 0;
}

static void cxd56_bkup_sram_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    REGERR("BKUP_SRAM: write at bad offset 0x%x\n", (int)offset);
}

static const MemoryRegionOps cxd56_bkup_sram_ops = {
    .read = cxd56_bkup_sram_read,
    .write = cxd56_bkup_sram_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

static uint64_t cxd56_crg_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    switch (offset) {
    case 0x0000:
        return 0x00010001;
    case 0x0030:
    case 0x0040:
        return 0;
    }

    REGERR("CRG: read at bad offset 0x%x\n", (int)offset);
    return 0;
}

static void cxd56_crg_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    switch (offset) {
    case 0x0030:
//        REGERR("CRG: reset 0x%x\n", (int)value);
        if (value != 0) {
            int cpu;
            for (cpu = 1; cpu < 6; cpu++) {
                if (value & (1 << (16 + cpu))) {
                    REGERR("CRG: boot cpu %d\n", cpu);
                    arm_set_cpu_on_and_reset(cpu);
                }
            }
        }
        return;

    case 0x0040:
//        REGERR("CRG: ck_gate_ahb 0x%x\n", (int)value);
        return;
    }

    REGERR("CRG: write at bad offset 0x%x\n", (int)offset);
}

static const MemoryRegionOps cxd56_crg_ops = {
    .read = cxd56_crg_read,
    .write = cxd56_crg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

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

static void cxd56_swint_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    cxd56_device_state *s = (cxd56_device_state *)opaque;

    int cpu = (offset / 4) - 2;
    qemu_set_irq(s->swint_irq[cpu], value);
//    REGERR("swint: write at bad offset 0x%x 0x%x\n", (int)offset, (int)value);
}

static const MemoryRegionOps cxd56_swint_ops = {
    .write = cxd56_swint_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

static uint64_t cxd56_cpufifo_read(void *opaque, hwaddr offset,
                              unsigned size)
{
    cxd56_device_state *s = (cxd56_device_state *)opaque;
    uint64_t value = 0;

    switch (offset) {
    case 0x0014:
        value = s->cpufifo_wrd0;
        break;
    }

    REGERR("cpufifo: read at bad offset 0x%x 0x%x\n", (int)offset, (int)value);
    return value;
}

static void cxd56_cpufifo_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    cxd56_device_state *s = (cxd56_device_state *)opaque;

    switch (offset) {
    case 0x0004:
        s->cpufifo_wrd0 = value;
        break;
    case 0x000c:
        switch (s->cpufifo_wrd0) {
            case 0x01400101:
                s->cpufifo_wrd0 = 0x03000007;
            case 0x0a000001:
                qemu_irq_pulse(s->cpufifo_from_irq);
                break;
        }
        break;
    }

    REGERR("cpufifo: write at bad offset 0x%x 0x%x\n", (int)offset, (int)value);
}

static const MemoryRegionOps cxd56_cpufifo_ops = {
    .read = cxd56_cpufifo_read,
    .write = cxd56_cpufifo_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

static MemTxResult cxd56_nvic_sysreg_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size,
                                    MemTxAttrs attrs)
{
    cxd56_device_state *s = (cxd56_device_state *)opaque;
    return memory_region_dispatch_write(s->real_nvic_sysreg[current_cpu->cpu_index], addr, value, size_memop(size) | MO_TE, attrs);
}

static MemTxResult cxd56_nvic_sysreg_read(void *opaque, hwaddr addr,
                                   uint64_t *data, unsigned size,
                                   MemTxAttrs attrs)
{
    cxd56_device_state *s = (cxd56_device_state *)opaque;
    return memory_region_dispatch_read(s->real_nvic_sysreg[current_cpu->cpu_index], addr, data, size_memop(size) | MO_TE, attrs);
}

static const MemoryRegionOps cxd56_nvic_sysreg_ops = {
    .read_with_attrs = cxd56_nvic_sysreg_read,
    .write_with_attrs = cxd56_nvic_sysreg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static MemTxResult cxd56_nvic_systick_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size,
                                    MemTxAttrs attrs)
{
    cxd56_device_state *s = (cxd56_device_state *)opaque;
    return memory_region_dispatch_write(s->real_nvic_systick[current_cpu->cpu_index], addr, value, size_memop(size) | MO_TE, attrs);
}

static MemTxResult cxd56_nvic_systick_read(void *opaque, hwaddr addr,
                                   uint64_t *data, unsigned size,
                                   MemTxAttrs attrs)
{
    cxd56_device_state *s = (cxd56_device_state *)opaque;
    return memory_region_dispatch_read(s->real_nvic_systick[current_cpu->cpu_index], addr, data, size_memop(size) | MO_TE, attrs);
}

static const MemoryRegionOps cxd56_nvic_systick_ops = {
    .read_with_attrs = cxd56_nvic_systick_read,
    .write_with_attrs = cxd56_nvic_systick_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/***************/

static void cxd56_devices(cxd56_device_state *s)
{
    memory_region_init_io(&s->topreg, NULL, &cxd56_topreg_ops, s, "topreg", 0x3000);
    memory_region_add_subregion(get_system_memory(), 0x04100000, &s->topreg);
    memory_region_init_io(&s->topreg_sub, NULL, &cxd56_topreg_sub_ops, s, "topreg_sub", 0x3000);
    memory_region_add_subregion(get_system_memory(), 0x04103000, &s->topreg_sub);

    memory_region_init_io(&s->scu, NULL, &cxd56_scu_ops, s, "scu", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0x04195000, &s->scu);

    memory_region_init_io(&s->bkup_sram, NULL, &cxd56_bkup_sram_ops, s, "bkup_sram", 0x10000);
    memory_region_add_subregion(get_system_memory(), 0x04400000, &s->bkup_sram);

    memory_region_init_io(&s->crg, NULL, &cxd56_crg_ops, s, "crg", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0x4e011000, &s->crg);

    memory_region_init_io(&s->cpuid, NULL, &cxd56_cpuid_ops, s, "cpuid", 4);
    memory_region_add_subregion(get_system_memory(), 0x0e002040, &s->cpuid);

    memory_region_init_alias(&s->cpuid2, NULL, "cpuid.mirror", &s->cpuid, 0, 4);
    memory_region_add_subregion(get_system_memory(), 0x4e002040, &s->cpuid2);

    memory_region_init_io(&s->swint, NULL, &cxd56_swint_ops, s, "swint", 0x0400);
    memory_region_add_subregion(get_system_memory(), 0x4600c000, &s->swint);

    memory_region_init_io(&s->cpufifo, NULL, &cxd56_cpufifo_ops, s, "cpufifo", 0x0400);
    memory_region_add_subregion(get_system_memory(), 0x4600c400, &s->cpufifo);

    memory_region_init_io(&s->nvic_sysreg, NULL, &cxd56_nvic_sysreg_ops, s, "nvic_sysreg", 0x1000);
    memory_region_add_subregion(get_system_memory(), 0xe000e000, &s->nvic_sysreg);
    memory_region_init_io(&s->nvic_systick, NULL, &cxd56_nvic_systick_ops, s, "nvic_systick", 0xe0);
    memory_region_add_subregion_overlap(get_system_memory(), 0xe000e010, &s->nvic_systick, 1);

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
    cxd56_device_state *s = g_new(cxd56_device_state, 1);
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();
    int n;
    unsigned int smp_cpus = ms->smp.cpus;

    memory_region_init_ram(sram, NULL, "cxd56.sram", 0x00180000,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0x0d000000, sram);

    memory_region_init_alias(flash, NULL, "cxd56.mirror", sram, 0, 0x10000);
    memory_region_add_subregion(system_memory, 0, flash);
    
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
#endif

    cxd56_devices(s);

    for (n = 0; n < smp_cpus; n++) {
        nvic = qdev_new(TYPE_ARMV7M);
        qdev_prop_set_uint32(nvic, "cpunum", n);
        qdev_prop_set_uint32(nvic, "num-irq", NUM_IRQ_LINES);
        qdev_prop_set_string(nvic, "cpu-type", ms->cpu_type);
        object_property_set_bool(OBJECT(nvic), "start-powered-off",
                                 true, &error_abort);

        /* CPU reset vector is loaded here. CPU0 must reload the vector again after loading kernel. */
        sysbus_realize_and_unref(SYS_BUS_DEVICE(nvic), &error_fatal);

        s->real_nvic_sysreg[n] = &(ARMV7M(nvic)->nvic.sysregmem);
        s->real_nvic_systick[n] = &(ARMV7M(nvic)->nvic.systickmem);
        s->swint_irq[n] = qdev_get_gpio_in(nvic, 96);

        if (n == 0) {
            s->cpufifo_from_irq = qdev_get_gpio_in(nvic, 79);
            pl011_create(0x041ac000, qdev_get_gpio_in(nvic, 11), serial_hd(0));
        }
    }

    system_clock_scale = NANOSECONDS_PER_SECOND / 160000000;

    armv7m_load_kernel(ARM_CPU(first_cpu), ms->kernel_filename, 0x00180000);
    arm_set_cpu_on_and_reset(0);
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
