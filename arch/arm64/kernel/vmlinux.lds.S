/*
 * ld script to make ARM Linux kernel
 * taken from the i386 version by Russell King
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#include <asm-generic/vmlinux.lds.h>
#include <asm/thread_info.h>
#include <asm/memory.h>
#include <asm/page.h>

#if (defined(CONFIG_GENERIC_BUG))
#define ARM_EXIT_KEEP(x)        x
#define ARM_EXIT_DISCARD(x)
#else
#define ARM_EXIT_KEEP(x)
#define ARM_EXIT_DISCARD(x)	x
#endif

OUTPUT_ARCH(aarch64)
ENTRY(stext)

jiffies = jiffies_64;

SECTIONS
{
	/*
	 * XXX: The linker does not define how output sections are
	 * assigned to input sections when there are multiple statements
	 * matching the same input section name.  There is no documented
	 * order of matching.
	 */
	/DISCARD/ : {
		ARM_EXIT_DISCARD(EXIT_TEXT)
		ARM_EXIT_DISCARD(EXIT_DATA)
		EXIT_CALL
		*(.discard)
		*(.discard.*)
	}

	. = PAGE_OFFSET + TEXT_OFFSET;

	.head.text : {
		_text = .;
		HEAD_TEXT
	}
	.text : {			/* Real text segment		*/
		_stext = .;		/* Text and read-only data	*/
			__exception_text_start = .;
			*(.exception.text)
			__exception_text_end = .;
			IRQENTRY_TEXT
			TEXT_TEXT
			SCHED_TEXT
			LOCK_TEXT
			*(.fixup)
			*(.gnu.warning)
		. = ALIGN(16);
		*(.got)			/* Global offset table		*/
	}

	RO_DATA(PAGE_SIZE)
	EXCEPTION_TABLE(8)
	NOTES
#ifdef CONFIG_TIMA_RKP
	. = ALIGN(PAGE_SIZE);
	.rkp.bitmap : {
		rkp_pgt_bitmap = .;
		. = rkp_pgt_bitmap + 0x18000;
	}
#ifdef CONFIG_RKP_KDP
	 /* Creating a new section to store task credentials */
	. = ALIGN(PAGE_SIZE);
	.tima.rkp.initcred : {
		__rkp_ro_start = .;
	}
	.tima.rkp.ro : {
		. = __rkp_ro_start + 0x500;
		__rkp_ro_end = .;
	}
	. = __rkp_ro_end + 0x500;
	. = ALIGN(PAGE_SIZE);
	. = ALIGN(1 << 21);
#endif /* CONFIG_RKP_KDP*/
#endif
	_etext = .;			/* End of text and rodata section */

	. = ALIGN(PAGE_SIZE);
	__init_begin = .;
	.init.arch.info : {
		__arch_info_begin = .;
		*(.arch.info.init)
		__arch_info_end = .;
	}
#ifdef CONFIG_TIMA_RKP
	.vmm : { *(.vmm*) }
#endif
	INIT_TEXT_SECTION(8)
	.exit.text : {
		ARM_EXIT_KEEP(EXIT_TEXT)
	}
	. = ALIGN(16);
	.init.data : {
		INIT_DATA
		INIT_SETUP(16)
		INIT_CALLS
		CON_INITCALL
		SECURITY_INITCALL
		INIT_RAM_FS
	}
	.exit.data : {
		ARM_EXIT_KEEP(EXIT_DATA)
	}

	PERCPU_SECTION(64)

	__init_end = .;

	. = ALIGN(PAGE_SIZE);
	_data = .;
	_sdata = .;
	RW_DATA_SECTION(64, PAGE_SIZE, THREAD_SIZE)
	_edata = .;

	BSS_SECTION(0, 0, 0)
	_end = .;

	STABS_DEBUG
	.comment 0 : { *(.comment) }
}
