/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_H_
#define VM_H_
#include <bsp_extern.h>
#include <vpci.h>
#include <page.h>
#include <cpu_caps.h>
#include <e820.h>

#ifdef CONFIG_PARTITION_MODE
#include <mptable.h>
#endif

#define INVALID_VM_ID 0xffffU

#define PLUG_CPU(n)		(1U << (n))

struct vm_hw_info {
	/* vcpu array of this VM */
	struct acrn_vcpu vcpu_array[CONFIG_MAX_VCPUS_PER_VM];
	uint16_t created_vcpus;	/* Number of created vcpus */
	uint64_t gpa_lowtop;    /* top lowmem gpa of this VM */
} __aligned(PAGE_SIZE);

struct sw_linux {
	void *ramdisk_src_addr;		/* HVA */
	void *ramdisk_load_addr;	/* GPA */
	uint32_t ramdisk_size;
	void *bootargs_src_addr;	/* HVA */
	void *bootargs_load_addr;	/* GPA */
	uint32_t bootargs_size;
	void *dtb_src_addr;		/* HVA */
	void *dtb_load_addr;		/* GPA */
	uint32_t dtb_size;
};

struct sw_kernel_info {
	void *kernel_src_addr;		/* HVA */
	void *kernel_load_addr;		/* GPA */
	void *kernel_entry_addr;	/* GPA */
	uint32_t kernel_size;
};

struct vm_sw_info {
	int32_t kernel_type;	/* Guest kernel type */
	/* Kernel information (common for all guest types) */
	struct sw_kernel_info kernel_info;
	/* Additional information specific to Linux guests */
	struct sw_linux linux_info;
	/* HVA to IO shared page */
	void *io_shared_page;
	/* If enable IO completion polling mode */
	bool is_completion_polling;
};

struct vm_pm_info {
	uint8_t			px_cnt;		/* count of all Px states */
	struct cpu_px_data	px_data[MAX_PSTATE];
	uint8_t			cx_cnt;		/* count of all Cx entries */
	struct cpu_cx_data	cx_data[MAX_CSTATE];
	struct pm_s_state_data	*sx_state_data;	/* data for S3/S5 implementation */
};

/* VM guest types */
#define VM_LINUX_GUEST      0x02
#define VM_MONO_GUEST       0x01

enum vpic_wire_mode {
	VPIC_WIRE_INTR = 0,
	VPIC_WIRE_LAPIC,
	VPIC_WIRE_IOAPIC,
	VPIC_WIRE_NULL
};

/* Enumerated type for VM states */
enum vm_state {
	VM_STATE_UNKNOWN = 0,
	VM_CREATED,	/* VM created / awaiting start (boot) */
	VM_STARTED,	/* VM started (booted) */
	VM_PAUSED,	/* VM paused */
};

struct vm_arch {
	/* I/O bitmaps A and B for this VM, MUST be 4-Kbyte aligned */
	uint8_t io_bitmap[PAGE_SIZE*2];
	/* MSR bitmap region for this VM, MUST be 4-Kbyte aligned */
	uint8_t msr_bitmap[PAGE_SIZE];

	uint64_t guest_init_pml4;/* Guest init pml4 */
	/* EPT hierarchy for Normal World */
	void *nworld_eptp;
	/* EPT hierarchy for Secure World
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 */
	void *sworld_eptp;
	struct memory_ops ept_mem_ops;

	void *tmp_pg_array;	/* Page array for tmp guest paging struct */
	struct acrn_vioapic vioapic;	/* Virtual IOAPIC base address */
	struct acrn_vpic vpic;      /* Virtual PIC */
	struct vm_io_handler_desc emul_pio[EMUL_PIO_IDX_MAX];

	/* reference to virtual platform to come here (as needed) */
} __aligned(PAGE_SIZE);


#define CPUID_CHECK_SUBLEAF	(1U << 0U)
#define MAX_VM_VCPUID_ENTRIES	64U
struct vcpuid_entry {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t leaf;
	uint32_t subleaf;
	uint32_t flags;
	uint32_t padding;
};

struct acrn_vm {
	struct vm_arch arch_vm; /* Reference to this VM's arch information */
	struct vm_hw_info hw;	/* Reference to this VM's HW information */
	struct vm_sw_info sw;	/* Reference to SW associated with this VM */
	struct vm_pm_info pm;	/* Reference to this VM's arch information */
	uint32_t e820_entry_num;
	struct e820_entry *e820_entries;
	uint16_t vm_id;		    /* Virtual machine identifier */
	enum vm_state state;	/* VM state */
	struct acrn_vuart vuart;		/* Virtual UART */
	enum vpic_wire_mode wire_mode;
	struct iommu_domain *iommu;	/* iommu domain of this VM */
	spinlock_t spinlock;	/* Spin-lock used to protect VM modifications */

	uint16_t emul_mmio_regions; /* Number of emulated mmio regions */
	struct mem_io_node emul_mmio[CONFIG_MAX_EMULATED_MMIO_REGIONS];

	uint8_t GUID[16];
	struct secure_world_control sworld_control;

	/* Secure World's snapshot
	 * Currently, Secure World is only running on vcpu[0],
	 * so the snapshot only stores the vcpu0's run_context
	 * of secure world.
	 */
	struct cpu_context sworld_snapshot;

	uint32_t vcpuid_entry_nr, vcpuid_level, vcpuid_xlevel;
	struct vcpuid_entry vcpuid_entries[MAX_VM_VCPUID_ENTRIES];
	struct acrn_vpci vpci;
#ifdef CONFIG_PARTITION_MODE
	struct mptable_info mptable;
	uint8_t vrtc_offset;
#endif

	spinlock_t softirq_dev_lock;
	struct list_head softirq_dev_entry_list;
	uint64_t intr_inject_delay_delta; /* delay of intr injection */
	bool snoopy_mem;
} __aligned(PAGE_SIZE);

#ifdef CONFIG_PARTITION_MODE
struct vpci_vdev_array {
	int32_t num_pci_vdev;
	struct pci_vdev vpci_vdev_list[];
};
#endif

#define MAX_BOOTARGS_SIZE	1024U
#define MAX_CONFIG_NAME_SIZE	32U

/*
 * PRE_LAUNCHED_VM is launched by ACRN hypervisor, with LAPIC_PT;
 * SOS_VM is launched by ACRN hypervisor, without LAPIC_PT;
 * NORMAL_VM is launched by ACRN devicemodel, with/without LAPIC_PT depends on usecases.
 */
enum acrn_vm_type {
	UNDEFINED_VM = 0,
	PRE_LAUNCHED_VM,
	SOS_VM,
	NORMAL_VM	/* Post-launched VM */
};

struct acrn_vm_mem_config {
	uint64_t start_hpa;	/* the start HPA of VM memory configuration, for pre-launched VMs only */
	uint64_t size;		/* VM memory size configuration */
};

struct acrn_vm_os_config {
	char name[MAX_CONFIG_NAME_SIZE];		/* OS name, useful for debug */
	char bootargs[MAX_BOOTARGS_SIZE];		/* boot args/cmdline */
} __aligned(8);

struct acrn_vm_pci_ptdev_config {
	union pci_bdf vbdf;				/* virtual BDF of PCI PT device */
	union pci_bdf pbdf;				/* physical BDF of PCI PT device */
} __aligned(8);

struct acrn_vm_config {
	enum acrn_vm_type type;				/* specify the type of VM */
	char name[MAX_CONFIG_NAME_SIZE];		/* VM name identifier, useful for debug. */
	uint8_t GUID[16];				/* GUID of the VM */
	uint64_t pcpu_bitmap;				/* from pcpu bitmap, we could know VM core number */
	uint64_t guest_flags;				/* VM flags that we want to configure for guest
							 * Now we have two flags:
							 *	SECURE_WORLD_ENABLED
							 *	LAPIC_PASSTHROUGH
							 * We could add more guest flags in future;
							 */
	struct acrn_vm_mem_config memory;		/* memory configuration of VM */
	uint16_t pci_ptdev_num;				/* indicate how many PCI PT devices in VM */
	struct acrn_vm_pci_ptdev_config *pci_ptdevs;	/* point to PCI PT devices BDF list */
	struct acrn_vm_os_config os_config;		/* OS information the VM */

#ifdef CONFIG_PARTITION_MODE
	bool			vm_vuart;
	struct vpci_vdev_array  *vpci_vdev_array;
#endif

} __aligned(8);

/*
 * @pre vcpu_id < CONFIG_MAX_VCPUS_PER_VM
 */
static inline struct acrn_vcpu *vcpu_from_vid(struct acrn_vm *vm, uint16_t vcpu_id)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		if (vcpu->vcpu_id == vcpu_id) {
			break;
		}
	}
	return vcpu;
}

static inline struct acrn_vcpu *vcpu_from_pid(struct acrn_vm *vm, uint16_t pcpu_id)
{
	uint16_t i;
	struct acrn_vcpu *vcpu, *target_vcpu = NULL;

	foreach_vcpu(i, vm, vcpu) {
		if (vcpu->pcpu_id == pcpu_id) {
			target_vcpu = vcpu;
			break;
		}
	}

	return target_vcpu;
}

static inline struct acrn_vcpu *get_primary_vcpu(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu, *target_vcpu = NULL;

	foreach_vcpu(i, vm, vcpu) {
		if (is_vcpu_bsp(vcpu)) {
			target_vcpu = vcpu;
			break;
		}
	}

	return target_vcpu;
}

int32_t shutdown_vm(struct acrn_vm *vm);
void pause_vm(struct acrn_vm *vm);
void resume_vm(struct acrn_vm *vm);
void resume_vm_from_s3(struct acrn_vm *vm, uint32_t wakeup_vec);
void start_vm(struct acrn_vm *vm);
int32_t reset_vm(struct acrn_vm *vm);
int32_t create_vm(uint16_t vm_id, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm);
void prepare_vm(uint16_t vm_id, struct acrn_vm_config *vm_config);
void launch_vms(uint16_t pcpu_id);
int32_t sanitize_vm_config(void);

extern struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM];

bool is_sos_vm(const struct acrn_vm *vm);
uint16_t find_free_vm_id(void);
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id);
struct acrn_vm *get_sos_vm(void);

#ifdef CONFIG_PARTITION_MODE
/*
 * Default e820 mem map:
 *
 * Assumption is every VM launched by ACRN in partition mode uses 2G of RAM.
 * there is reserved memory of 64K for MPtable and PCI hole of 512MB
 */
#define NUM_E820_ENTRIES        5U
extern const struct e820_entry ve820_entry[NUM_E820_ENTRIES];

uint16_t get_vm_pcpu_nums(struct acrn_vm_config *vm_config);
void vrtc_init(struct acrn_vm *vm);
#endif

/*
 * @pre vm_id < CONFIG_MAX_VM_NUM
 */
static inline struct acrn_vm_config *get_vm_config(uint16_t vm_id)
{
	return &vm_configs[vm_id];
}

static inline bool is_lapic_pt(const struct acrn_vm *vm)
{
	return ((vm_configs[vm->vm_id].guest_flags & LAPIC_PASSTHROUGH) != 0U);
}

#endif /* VM_H_ */
