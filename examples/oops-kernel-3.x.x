------------[ cut here ]------------
WARNING: at lib/dma-debug.c:800 check_unmap+0x9c/0x67f()
firewire_ohci 0000:05:00.0: DMA-API: device driver tries to free an invalid DMA memory address
Modules linked in: firewire_ohci(+) firewire_core crc_itu_t radeon ttm drm_kms_helper drm i2c_algo_bit i2c_core
Pid: 202, comm: modprobe Not tainted 3.0.0-3.fc16.i686.PAE #1
Call Trace:
 [<c04463d4>] warn_slowpath_common+0x7c/0x91
 [<c05fcc7a>] ? check_unmap+0x9c/0x67f
 [<c05fcc7a>] ? check_unmap+0x9c/0x67f
 [<c0446474>] warn_slowpath_fmt+0x33/0x35
 [<c05fcc7a>] check_unmap+0x9c/0x67f
 [<c05eeba4>] ? trace_hardirqs_on_thunk+0xc/0x10
 [<c05eebb4>] ? trace_hardirqs_off_thunk+0xc/0x10
 [<c084f3bd>] ? restore_all+0xf/0xf
 [<c04400d8>] ? rt_mutex_setprio+0xb2/0xf6
 [<c04464a1>] ? arch_local_irq_restore+0x5/0xb
 [<c05fd2b9>] debug_dma_free_coherent+0x5c/0x64
 [<f7a755ee>] dma_free_coherent+0x6a/0x8f [firewire_ohci]
 [<f7a77c9e>] ohci_enable+0x3c2/0x439 [firewire_ohci]
 [<f7a4d7c4>] fw_card_add+0x45/0x71 [firewire_core]
 [<f7a78e8d>] pci_probe+0x377/0x4a1 [firewire_ohci]
 [<c084f0f5>] ? _raw_spin_unlock_irqrestore+0x44/0x48
 [<c0605db8>] pci_device_probe+0x62/0xab
 [<c06a838e>] driver_probe_device+0x129/0x208
 [<c084dc54>] ? mutex_lock_nested+0x43/0x49
 [<c06a84bc>] __driver_attach+0x4f/0x6b
 [<c06a757f>] bus_for_each_dev+0x42/0x6b
 [<c06a7fc1>] driver_attach+0x1f/0x23
 [<c06a846d>] ? driver_probe_device+0x208/0x208
 [<c06a7c47>] bus_add_driver+0xcd/0x214
 [<c06a8902>] driver_register+0x84/0xe3
 [<c05f2e2b>] ? __raw_spin_lock_init+0x2d/0x4e
 [<c0606530>] __pci_register_driver+0x4f/0xab
 [<f7a39000>] ? 0xf7a38fff
 [<f7a39000>] ? 0xf7a38fff
 [<f7a39017>] fw_ohci_init+0x17/0x1000 [firewire_ohci]
 [<c04030a2>] do_one_initcall+0x8c/0x146
 [<c04296cf>] ? set_memory_nx+0x38/0x3a
 [<f7a39000>] ? 0xf7a38fff
 [<f7a39000>] ? 0xf7a38fff
 [<c047cea1>] sys_init_module+0x14b9/0x16dd
 [<c085525f>] sysenter_do_call+0x12/0x38
---[ end trace 50b7d6497bc6b1f4 ]---
