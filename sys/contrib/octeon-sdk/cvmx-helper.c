/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Cavium Networks nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/







/**
 * @file
 *
 * Helper functions for common, but complicated tasks.
 *
 * <hr>$Revision: 52004 $<hr>
 */
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
#include <linux/module.h>
#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-config.h>
#include <asm/octeon/cvmx-bootmem.h>
#include <asm/octeon/cvmx-sriox-defs.h>
#include <asm/octeon/cvmx-npi-defs.h>
#include <asm/octeon/cvmx-pexp-defs.h>
#include <asm/octeon/cvmx-pip-defs.h>
#include <asm/octeon/cvmx-asxx-defs.h>
#include <asm/octeon/cvmx-gmxx-defs.h>
#include <asm/octeon/cvmx-smix-defs.h>
#include <asm/octeon/cvmx-dbg-defs.h>

#include <asm/octeon/cvmx-gmx.h>
#include <asm/octeon/cvmx-fpa.h>
#include <asm/octeon/cvmx-pip.h>
#include <asm/octeon/cvmx-pko.h>
#include <asm/octeon/cvmx-ipd.h>
#include <asm/octeon/cvmx-spi.h>
#include <asm/octeon/cvmx-clock.h>
#include <asm/octeon/cvmx-helper.h>
#include <asm/octeon/cvmx-helper-board.h>
#include <asm/octeon/cvmx-helper-errata.h>
#else
#if !defined(__FreeBSD__) || !defined(_KERNEL)
#include "executive-config.h"
#endif
#include "cvmx.h"
#include "cvmx-sysinfo.h"
#include "cvmx-bootmem.h"
#include "cvmx-version.h"
#include "cvmx-helper-check-defines.h"
#include "cvmx-gmx.h"
#include "cvmx-error.h"
#if !defined(__FreeBSD__) || !defined(_KERNEL)
#include "cvmx-config.h"
#endif

#include "cvmx-fpa.h"
#include "cvmx-pip.h"
#include "cvmx-pko.h"
#include "cvmx-ipd.h"
#include "cvmx-spi.h"
#include "cvmx-helper.h"
#include "cvmx-helper-board.h"
#include "cvmx-helper-errata.h"
#endif



#ifdef CVMX_ENABLE_PKO_FUNCTIONS

/**
 * cvmx_override_pko_queue_priority(int ipd_port, uint64_t
 * priorities[16]) is a function pointer. It is meant to allow
 * customization of the PKO queue priorities based on the port
 * number. Users should set this pointer to a function before
 * calling any cvmx-helper operations.
 */
CVMX_SHARED void (*cvmx_override_pko_queue_priority)(int pko_port, uint64_t priorities[16]) = NULL;
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
EXPORT_SYMBOL(cvmx_override_pko_queue_priority);
#endif

/**
 * cvmx_override_ipd_port_setup(int ipd_port) is a function
 * pointer. It is meant to allow customization of the IPD port
 * setup before packet input/output comes online. It is called
 * after cvmx-helper does the default IPD configuration, but
 * before IPD is enabled. Users should set this pointer to a
 * function before calling any cvmx-helper operations.
 */
CVMX_SHARED void (*cvmx_override_ipd_port_setup)(int ipd_port) = NULL;

/* Port count per interface */
static CVMX_SHARED int interface_port_count[6] = {0,};
/* Port last configured link info index by IPD/PKO port */
static CVMX_SHARED cvmx_helper_link_info_t port_link_info[CVMX_PIP_NUM_INPUT_PORTS];


/**
 * Return the number of interfaces the chip has. Each interface
 * may have multiple ports. Most chips support two interfaces,
 * but the CNX0XX and CNX1XX are exceptions. These only support
 * one interface.
 *
 * @return Number of interfaces on chip
 */
int cvmx_helper_get_number_of_interfaces(void)
{
    switch (cvmx_sysinfo_get()->board_type) {
#if defined(OCTEON_VENDOR_LANNER)
	case CVMX_BOARD_TYPE_CUST_LANNER_MR955:
	    return 2;
	case CVMX_BOARD_TYPE_CUST_LANNER_MR730:
	    return 1;
#endif
	default:
	    break;
    }

    if (OCTEON_IS_MODEL(OCTEON_CN63XX))
	return 6;
    else if (OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN52XX))
        return 4;
    else
        return 3;
}
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
EXPORT_SYMBOL(cvmx_helper_get_number_of_interfaces);
#endif


/**
 * Return the number of ports on an interface. Depending on the
 * chip and configuration, this can be 1-16. A value of 0
 * specifies that the interface doesn't exist or isn't usable.
 *
 * @param interface Interface to get the port count for
 *
 * @return Number of ports on interface. Can be Zero.
 */
int cvmx_helper_ports_on_interface(int interface)
{
    return interface_port_count[interface];
}
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
EXPORT_SYMBOL(cvmx_helper_ports_on_interface);
#endif


/**
 * Get the operating mode of an interface. Depending on the Octeon
 * chip and configuration, this function returns an enumeration
 * of the type of packet I/O supported by an interface.
 *
 * @param interface Interface to probe
 *
 * @return Mode of the interface. Unknown or unsupported interfaces return
 *         DISABLED.
 */
cvmx_helper_interface_mode_t cvmx_helper_interface_get_mode(int interface)
{
    cvmx_gmxx_inf_mode_t mode;
    if (interface == 2)
        return CVMX_HELPER_INTERFACE_MODE_NPI;

    if (interface == 3)
    {
        if (OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN6XXX))
            return CVMX_HELPER_INTERFACE_MODE_LOOP;
        else
            return CVMX_HELPER_INTERFACE_MODE_DISABLED;
    }

    if (OCTEON_IS_MODEL(OCTEON_CN6XXX) && (interface == 4 || interface == 5))
    {
        cvmx_sriox_status_reg_t sriox_status_reg;
        sriox_status_reg.u64 = cvmx_read_csr(CVMX_SRIOX_STATUS_REG(interface-4));
        if (sriox_status_reg.s.srio)
            return CVMX_HELPER_INTERFACE_MODE_SRIO;
        else
            return CVMX_HELPER_INTERFACE_MODE_DISABLED;
    }

    if (interface == 0 && cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_CN3005_EVB_HS5 && cvmx_sysinfo_get()->board_rev_major == 1)
    {
        /* Lie about interface type of CN3005 board.  This board has a switch on port 1 like
        ** the other evaluation boards, but it is connected over RGMII instead of GMII.  Report
        ** GMII mode so that the speed is forced to 1 Gbit full duplex.  Other than some initial configuration
        ** (which does not use the output of this function) there is no difference in setup between GMII and RGMII modes.
        */
        return CVMX_HELPER_INTERFACE_MODE_GMII;
    }

    /* Interface 1 is always disabled on CN31XX and CN30XX */
    if ((interface == 1) && (OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
        return CVMX_HELPER_INTERFACE_MODE_DISABLED;

    mode.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(interface));

    if (OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        switch(mode.cn56xx.mode)
        {
            case 0: return CVMX_HELPER_INTERFACE_MODE_DISABLED;
            case 1: return CVMX_HELPER_INTERFACE_MODE_XAUI;
            case 2: return CVMX_HELPER_INTERFACE_MODE_SGMII;
            case 3: return CVMX_HELPER_INTERFACE_MODE_PICMG;
            default:return CVMX_HELPER_INTERFACE_MODE_DISABLED;
        }
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN63XX))
    {
	switch(mode.cn63xx.mode)
	{
	    case 0: return CVMX_HELPER_INTERFACE_MODE_SGMII;
	    case 1: return CVMX_HELPER_INTERFACE_MODE_XAUI;
	    default: return CVMX_HELPER_INTERFACE_MODE_DISABLED;
	}
    }
    else
    {
        if (!mode.s.en)
            return CVMX_HELPER_INTERFACE_MODE_DISABLED;

        if (mode.s.type)
        {
            if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX))
                return CVMX_HELPER_INTERFACE_MODE_SPI;
            else
                return CVMX_HELPER_INTERFACE_MODE_GMII;
        }
        else
            return CVMX_HELPER_INTERFACE_MODE_RGMII;
    }
}
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
EXPORT_SYMBOL(cvmx_helper_interface_get_mode);
#endif

/**
 * @INTERNAL
 * Configure the IPD/PIP tagging and QoS options for a specific
 * port. This function determines the POW work queue entry
 * contents for a port. The setup performed here is controlled by
 * the defines in executive-config.h.
 *
 * @param ipd_port Port to configure. This follows the IPD numbering, not the
 *                 per interface numbering
 *
 * @return Zero on success, negative on failure
 */
static int __cvmx_helper_port_setup_ipd(int ipd_port)
{
    cvmx_pip_prt_cfgx_t port_config;
    cvmx_pip_prt_tagx_t tag_config;

    port_config.u64 = cvmx_read_csr(CVMX_PIP_PRT_CFGX(ipd_port));
    tag_config.u64 = cvmx_read_csr(CVMX_PIP_PRT_TAGX(ipd_port));

    /* Have each port go to a different POW queue */
    port_config.s.qos = ipd_port & 0x7;

    /* Process the headers and place the IP header in the work queue */
    port_config.s.mode = CVMX_HELPER_INPUT_PORT_SKIP_MODE;

    tag_config.s.ip6_src_flag  = CVMX_HELPER_INPUT_TAG_IPV6_SRC_IP;
    tag_config.s.ip6_dst_flag  = CVMX_HELPER_INPUT_TAG_IPV6_DST_IP;
    tag_config.s.ip6_sprt_flag = CVMX_HELPER_INPUT_TAG_IPV6_SRC_PORT;
    tag_config.s.ip6_dprt_flag = CVMX_HELPER_INPUT_TAG_IPV6_DST_PORT;
    tag_config.s.ip6_nxth_flag = CVMX_HELPER_INPUT_TAG_IPV6_NEXT_HEADER;
    tag_config.s.ip4_src_flag  = CVMX_HELPER_INPUT_TAG_IPV4_SRC_IP;
    tag_config.s.ip4_dst_flag  = CVMX_HELPER_INPUT_TAG_IPV4_DST_IP;
    tag_config.s.ip4_sprt_flag = CVMX_HELPER_INPUT_TAG_IPV4_SRC_PORT;
    tag_config.s.ip4_dprt_flag = CVMX_HELPER_INPUT_TAG_IPV4_DST_PORT;
    tag_config.s.ip4_pctl_flag = CVMX_HELPER_INPUT_TAG_IPV4_PROTOCOL;
    tag_config.s.inc_prt_flag  = CVMX_HELPER_INPUT_TAG_INPUT_PORT;
    tag_config.s.tcp6_tag_type = CVMX_HELPER_INPUT_TAG_TYPE;
    tag_config.s.tcp4_tag_type = CVMX_HELPER_INPUT_TAG_TYPE;
    tag_config.s.ip6_tag_type = CVMX_HELPER_INPUT_TAG_TYPE;
    tag_config.s.ip4_tag_type = CVMX_HELPER_INPUT_TAG_TYPE;
    tag_config.s.non_tag_type = CVMX_HELPER_INPUT_TAG_TYPE;
    /* Put all packets in group 0. Other groups can be used by the app */
    tag_config.s.grp = 0;

    cvmx_pip_config_port(ipd_port, port_config, tag_config);

    /* Give the user a chance to override our setting for each port */
    if (cvmx_override_ipd_port_setup)
        cvmx_override_ipd_port_setup(ipd_port);

    return 0;
}


/**
 * This function probes an interface to determine the actual
 * number of hardware ports connected to it. It doesn't setup the
 * ports or enable them. The main goal here is to set the global
 * interface_port_count[interface] correctly. Hardware setup of the
 * ports will be performed later.
 *
 * @param interface Interface to probe
 *
 * @return Zero on success, negative on failure
 */
int cvmx_helper_interface_probe(int interface)
{
    /* At this stage in the game we don't want packets to be moving yet.
        The following probe calls should perform hardware setup
        needed to determine port counts. Receive must still be disabled */
    switch (cvmx_helper_interface_get_mode(interface))
    {
        /* These types don't support ports to IPD/PKO */
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:
        case CVMX_HELPER_INTERFACE_MODE_PCIE:
            interface_port_count[interface] = 0;
            break;
        /* XAUI is a single high speed port */
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
            interface_port_count[interface] = __cvmx_helper_xaui_probe(interface);
            break;
        /* RGMII/GMII/MII are all treated about the same. Most functions
            refer to these ports as RGMII */
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            interface_port_count[interface] = __cvmx_helper_rgmii_probe(interface);
            break;
        /* SPI4 can have 1-16 ports depending on the device at the other end */
        case CVMX_HELPER_INTERFACE_MODE_SPI:
            interface_port_count[interface] = __cvmx_helper_spi_probe(interface);
            break;
        /* SGMII can have 1-4 ports depending on how many are hooked up */
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            interface_port_count[interface] = __cvmx_helper_sgmii_probe(interface);
            break;
        /* PCI target Network Packet Interface */
        case CVMX_HELPER_INTERFACE_MODE_NPI:
            interface_port_count[interface] = __cvmx_helper_npi_probe(interface);
            break;
        /* Special loopback only ports. These are not the same as other ports
            in loopback mode */
        case CVMX_HELPER_INTERFACE_MODE_LOOP:
            interface_port_count[interface] = __cvmx_helper_loop_probe(interface);
            break;
	/* SRIO has 2^N ports, where N is number of interfaces */
	case CVMX_HELPER_INTERFACE_MODE_SRIO:
	    interface_port_count[interface] = __cvmx_helper_srio_probe(interface);
	    break;
    }

    interface_port_count[interface] = __cvmx_helper_board_interface_probe(interface, interface_port_count[interface]);

    /* Make sure all global variables propagate to other cores */
    CVMX_SYNCWS;

    return 0;
}


/**
 * @INTERNAL
 * Setup the IPD/PIP for the ports on an interface. Packet
 * classification and tagging are set for every port on the
 * interface. The number of ports on the interface must already
 * have been probed.
 *
 * @param interface Interface to setup IPD/PIP for
 *
 * @return Zero on success, negative on failure
 */
static int __cvmx_helper_interface_setup_ipd(int interface)
{
    int ipd_port = cvmx_helper_get_ipd_port(interface, 0);
    int num_ports = interface_port_count[interface];

    while (num_ports--)
    {
        __cvmx_helper_port_setup_ipd(ipd_port);
        ipd_port++;
    }
    return 0;
}


/**
 * @INTERNAL
 * Setup global setting for IPD/PIP not related to a specific
 * interface or port. This must be called before IPD is enabled.
 *
 * @return Zero on success, negative on failure.
 */
static int __cvmx_helper_global_setup_ipd(void)
{
#ifndef CVMX_HELPER_IPD_DRAM_MODE
#define CVMX_HELPER_IPD_DRAM_MODE   CVMX_IPD_OPC_MODE_STT
#endif
    /* Setup the global packet input options */
    cvmx_ipd_config(CVMX_FPA_PACKET_POOL_SIZE/8,
                    CVMX_HELPER_FIRST_MBUFF_SKIP/8,
                    CVMX_HELPER_NOT_FIRST_MBUFF_SKIP/8,
                    (CVMX_HELPER_FIRST_MBUFF_SKIP+8) / 128, /* The +8 is to account for the next ptr */
                    (CVMX_HELPER_NOT_FIRST_MBUFF_SKIP+8) / 128, /* The +8 is to account for the next ptr */
                    CVMX_FPA_WQE_POOL,
                    CVMX_HELPER_IPD_DRAM_MODE,
                    1);
    return 0;
}


/**
 * @INTERNAL
 * Setup the PKO for the ports on an interface. The number of
 * queues per port and the priority of each PKO output queue
 * is set here. PKO must be disabled when this function is called.
 *
 * @param interface Interface to setup PKO for
 *
 * @return Zero on success, negative on failure
 */
static int __cvmx_helper_interface_setup_pko(int interface)
{
    /* Each packet output queue has an associated priority. The higher the
        priority, the more often it can send a packet. A priority of 8 means
        it can send in all 8 rounds of contention. We're going to make each
        queue one less than the last.
        The vector of priorities has been extended to support CN5xxx CPUs,
        where up to 16 queues can be associated to a port.
        To keep backward compatibility we don't change the initial 8
        priorities and replicate them in the second half.
        With per-core PKO queues (PKO lockless operation) all queues have
        the same priority. */
    uint64_t priorities[16] = {8,7,6,5,4,3,2,1,8,7,6,5,4,3,2,1};

    /* Setup the IPD/PIP and PKO for the ports discovered above. Here packet
        classification, tagging and output priorities are set */
    int ipd_port = cvmx_helper_get_ipd_port(interface, 0);
    int num_ports = interface_port_count[interface];
    while (num_ports--)
    {
        /* Give the user a chance to override the per queue priorities */
        if (cvmx_override_pko_queue_priority)
            cvmx_override_pko_queue_priority(ipd_port, priorities);

        cvmx_pko_config_port(ipd_port, cvmx_pko_get_base_queue_per_core(ipd_port, 0),
                             cvmx_pko_get_num_queues(ipd_port), priorities);
        ipd_port++;
    }
    return 0;
}


/**
 * @INTERNAL
 * Setup global setting for PKO not related to a specific
 * interface or port. This must be called before PKO is enabled.
 *
 * @return Zero on success, negative on failure.
 */
static int __cvmx_helper_global_setup_pko(void)
{
    /* Disable tagwait FAU timeout. This needs to be done before anyone might
        start packet output using tags */
    cvmx_iob_fau_timeout_t fau_to;
    fau_to.u64 = 0;
    fau_to.s.tout_val = 0xfff;
    fau_to.s.tout_enb = 0;
    cvmx_write_csr(CVMX_IOB_FAU_TIMEOUT, fau_to.u64);
    return 0;
}


/**
 * @INTERNAL
 * Setup global backpressure setting.
 *
 * @return Zero on success, negative on failure
 */
static int __cvmx_helper_global_setup_backpressure(void)
{
#if CVMX_HELPER_DISABLE_RGMII_BACKPRESSURE
    /* Disable backpressure if configured to do so */
    /* Disable backpressure (pause frame) generation */
    int num_interfaces = cvmx_helper_get_number_of_interfaces();
    int interface;
    for (interface=0; interface<num_interfaces; interface++)
    {
        switch (cvmx_helper_interface_get_mode(interface))
        {
            case CVMX_HELPER_INTERFACE_MODE_DISABLED:
            case CVMX_HELPER_INTERFACE_MODE_PCIE:
            case CVMX_HELPER_INTERFACE_MODE_SRIO:
            case CVMX_HELPER_INTERFACE_MODE_NPI:
            case CVMX_HELPER_INTERFACE_MODE_LOOP:
            case CVMX_HELPER_INTERFACE_MODE_XAUI:
                break;
            case CVMX_HELPER_INTERFACE_MODE_RGMII:
            case CVMX_HELPER_INTERFACE_MODE_GMII:
            case CVMX_HELPER_INTERFACE_MODE_SPI:
            case CVMX_HELPER_INTERFACE_MODE_SGMII:
            case CVMX_HELPER_INTERFACE_MODE_PICMG:
                cvmx_gmx_set_backpressure_override(interface, 0xf);
                break;
        }
    }
    //cvmx_dprintf("Disabling backpressure\n");
#endif

    return 0;
}

/**
 * @INTERNAL
 * Verify the per port IPD backpressure is aligned properly.
 * @return Zero if working, non zero if misaligned
 */
static int __cvmx_helper_backpressure_is_misaligned(void)
{
    uint64_t ipd_int_enb;
    cvmx_ipd_ctl_status_t ipd_reg;
    uint64_t bp_status0;
    uint64_t bp_status1;
    const int port0 = 0;
    const int port1 = 16;
    cvmx_helper_interface_mode_t mode0 = cvmx_helper_interface_get_mode(0);
    cvmx_helper_interface_mode_t mode1 = cvmx_helper_interface_get_mode(1);

    /* Disable error interrupts while we check backpressure */
    ipd_int_enb = cvmx_read_csr(CVMX_IPD_INT_ENB);
    cvmx_write_csr(CVMX_IPD_INT_ENB, 0);

    /* Enable per port backpressure */
    ipd_reg.u64 = cvmx_read_csr(CVMX_IPD_CTL_STATUS);
    ipd_reg.s.pbp_en = 1;
    cvmx_write_csr(CVMX_IPD_CTL_STATUS, ipd_reg.u64);

    if (mode0 != CVMX_HELPER_INTERFACE_MODE_DISABLED)
    {
        /* Enable backpressure for port with a zero threshold */
        cvmx_write_csr(CVMX_IPD_PORTX_BP_PAGE_CNT(port0), 1<<17);
        /* Add 1000 to the page count to simulate packets coming in */
        cvmx_write_csr(CVMX_IPD_SUB_PORT_BP_PAGE_CNT, (port0<<25) | 1000);
    }

    if (mode1 != CVMX_HELPER_INTERFACE_MODE_DISABLED)
    {
        /* Enable backpressure for port with a zero threshold */
        cvmx_write_csr(CVMX_IPD_PORTX_BP_PAGE_CNT(port1), 1<<17);
        /* Add 1000 to the page count to simulate packets coming in */
        cvmx_write_csr(CVMX_IPD_SUB_PORT_BP_PAGE_CNT, (port1<<25) | 1000);
    }

    /* Wait 500 cycles for the BP to update */
    cvmx_wait(500);

    /* Read the BP state from the debug select register */
    switch (mode0)
    {
        case CVMX_HELPER_INTERFACE_MODE_SPI:
            cvmx_write_csr(CVMX_NPI_DBG_SELECT, 0x9004);
            bp_status0 = cvmx_read_csr(CVMX_DBG_DATA);
            bp_status0 = 0xffff & ~bp_status0;
            break;
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            cvmx_write_csr(CVMX_NPI_DBG_SELECT, 0x0e00);
            bp_status0 = 0xffff & cvmx_read_csr(CVMX_DBG_DATA);
            break;
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            cvmx_write_csr(CVMX_PEXP_NPEI_DBG_SELECT, 0x0e00);
            bp_status0 = 0xffff & cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
            break;
        default:
            bp_status0 = 1<<port0;
            break;
    }

    /* Read the BP state from the debug select register */
    switch (mode1)
    {
        case CVMX_HELPER_INTERFACE_MODE_SPI:
            cvmx_write_csr(CVMX_NPI_DBG_SELECT, 0x9804);
            bp_status1 = cvmx_read_csr(CVMX_DBG_DATA);
            bp_status1 = 0xffff & ~bp_status1;
            break;
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            cvmx_write_csr(CVMX_NPI_DBG_SELECT, 0x1600);
            bp_status1 = 0xffff & cvmx_read_csr(CVMX_DBG_DATA);
            break;
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            cvmx_write_csr(CVMX_PEXP_NPEI_DBG_SELECT, 0x1600);
            bp_status1 = 0xffff & cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
            break;
        default:
            bp_status1 = 1<<(port1-16);
            break;
    }

    if (mode0 != CVMX_HELPER_INTERFACE_MODE_DISABLED)
    {
        /* Shutdown BP */
        cvmx_write_csr(CVMX_IPD_SUB_PORT_BP_PAGE_CNT, (port0<<25) | (0x1ffffff & -1000));
        cvmx_write_csr(CVMX_IPD_PORTX_BP_PAGE_CNT(port0), 0);
    }

    if (mode1 != CVMX_HELPER_INTERFACE_MODE_DISABLED)
    {
        /* Shutdown BP */
        cvmx_write_csr(CVMX_IPD_SUB_PORT_BP_PAGE_CNT, (port1<<25) | (0x1ffffff & -1000));
        cvmx_write_csr(CVMX_IPD_PORTX_BP_PAGE_CNT(port1), 0);
    }

    /* Clear any error interrupts that might have been set */
    cvmx_write_csr(CVMX_IPD_INT_SUM, 0x1f);
    cvmx_write_csr(CVMX_IPD_INT_ENB, ipd_int_enb);

    return ((bp_status0 != 1ull<<port0) || (bp_status1 != 1ull<<(port1-16)));
}


/**
 * @INTERNAL
 * Enable packet input/output from the hardware. This function is
 * called after all internal setup is complete and IPD is enabled.
 * After this function completes, packets will be accepted from the
 * hardware ports. PKO should still be disabled to make sure packets
 * aren't sent out partially setup hardware.
 *
 * @param interface Interface to enable
 *
 * @return Zero on success, negative on failure
 */
static int __cvmx_helper_packet_hardware_enable(int interface)
{
    int result = 0;
    switch (cvmx_helper_interface_get_mode(interface))
    {
        /* These types don't support ports to IPD/PKO */
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:
        case CVMX_HELPER_INTERFACE_MODE_PCIE:
            /* Nothing to do */
            break;
        /* XAUI is a single high speed port */
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
            result = __cvmx_helper_xaui_enable(interface);
            break;
        /* RGMII/GMII/MII are all treated about the same. Most functions
            refer to these ports as RGMII */
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            result = __cvmx_helper_rgmii_enable(interface);
            break;
        /* SPI4 can have 1-16 ports depending on the device at the other end */
        case CVMX_HELPER_INTERFACE_MODE_SPI:
            result = __cvmx_helper_spi_enable(interface);
            break;
        /* SGMII can have 1-4 ports depending on how many are hooked up */
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            result = __cvmx_helper_sgmii_enable(interface);
            break;
        /* PCI target Network Packet Interface */
        case CVMX_HELPER_INTERFACE_MODE_NPI:
            result = __cvmx_helper_npi_enable(interface);
            break;
        /* Special loopback only ports. These are not the same as other ports
            in loopback mode */
        case CVMX_HELPER_INTERFACE_MODE_LOOP:
            result = __cvmx_helper_loop_enable(interface);
            break;
	/* SRIO has 2^N ports, where N is number of interfaces */
        case CVMX_HELPER_INTERFACE_MODE_SRIO:
	    result = __cvmx_helper_srio_enable(interface);
	    break;
    }
    result |= __cvmx_helper_board_hardware_enable(interface);
    return result;
}


/**
 * Called after all internal packet IO paths are setup. This
 * function enables IPD/PIP and begins packet input and output.
 *
 * @return Zero on success, negative on failure
 */
int cvmx_helper_ipd_and_packet_input_enable(void)
{
    int num_interfaces;
    int interface;

    /* Enable IPD */
    cvmx_ipd_enable();

    /* Time to enable hardware ports packet input and output. Note that at this
        point IPD/PIP must be fully functional and PKO must be disabled */
    num_interfaces = cvmx_helper_get_number_of_interfaces();
    for (interface=0; interface<num_interfaces; interface++)
    {
        if (cvmx_helper_ports_on_interface(interface) > 0)
        {
            //cvmx_dprintf("Enabling packet I/O on interface %d\n", interface);
            __cvmx_helper_packet_hardware_enable(interface);
        }
    }

    /* Finally enable PKO now that the entire path is up and running */
    cvmx_pko_enable();

    if ((OCTEON_IS_MODEL(OCTEON_CN31XX_PASS1) || OCTEON_IS_MODEL(OCTEON_CN30XX_PASS1)) &&
        (cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_SIM))
        __cvmx_helper_errata_fix_ipd_ptr_alignment();
    return 0;
}
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
EXPORT_SYMBOL(cvmx_helper_ipd_and_packet_input_enable);
#endif

/**
 * Initialize the PIP, IPD, and PKO hardware to support
 * simple priority based queues for the ethernet ports. Each
 * port is configured with a number of priority queues based
 * on CVMX_PKO_QUEUES_PER_PORT_* where each queue is lower
 * priority than the previous.
 *
 * @return Zero on success, non-zero on failure
 */
int cvmx_helper_initialize_packet_io_global(void)
{
    int result = 0;
    int interface;
    cvmx_l2c_cfg_t l2c_cfg;
    cvmx_smix_en_t smix_en;
    const int num_interfaces = cvmx_helper_get_number_of_interfaces();

    /* CN52XX pass 1: Due to a bug in 2nd order CDR, it needs to be disabled */
    if (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_0))
        __cvmx_helper_errata_qlm_disable_2nd_order_cdr(1);

    /* Tell L2 to give the IOB statically higher priority compared to the
        cores. This avoids conditions where IO blocks might be starved under
        very high L2 loads */
    if (OCTEON_IS_MODEL(OCTEON_CN6XXX))
    {
        cvmx_l2c_ctl_t l2c_ctl;
        l2c_ctl.u64 = cvmx_read_csr(CVMX_L2C_CTL);
        l2c_ctl.s.rsp_arb_mode = 1;
        l2c_ctl.s.xmc_arb_mode = 0;
        cvmx_write_csr(CVMX_L2C_CTL, l2c_ctl.u64);
    }
    else
    {
        l2c_cfg.u64 = cvmx_read_csr(CVMX_L2C_CFG);
        l2c_cfg.s.lrf_arb_mode = 0;
        l2c_cfg.s.rfb_arb_mode = 0;
        cvmx_write_csr(CVMX_L2C_CFG, l2c_cfg.u64);
    }

    if (cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_SIM)
    {
        /* Make sure SMI/MDIO is enabled so we can query PHYs */
        smix_en.u64 = cvmx_read_csr(CVMX_SMIX_EN(0));
        if (!smix_en.s.en)
        {
            smix_en.s.en = 1;
            cvmx_write_csr(CVMX_SMIX_EN(0), smix_en.u64);
        }

        /* Newer chips actually have two SMI/MDIO interfaces */
        if (!OCTEON_IS_MODEL(OCTEON_CN3XXX) &&
            !OCTEON_IS_MODEL(OCTEON_CN58XX) &&
            !OCTEON_IS_MODEL(OCTEON_CN50XX))
        {
            smix_en.u64 = cvmx_read_csr(CVMX_SMIX_EN(1));
            if (!smix_en.s.en)
            {
                smix_en.s.en = 1;
                cvmx_write_csr(CVMX_SMIX_EN(1), smix_en.u64);
            }
        }
    }

    for (interface=0; interface<num_interfaces; interface++)
        result |= cvmx_helper_interface_probe(interface);

    cvmx_pko_initialize_global();
    for (interface=0; interface<num_interfaces; interface++)
    {
        if (cvmx_helper_ports_on_interface(interface) > 0)
            cvmx_dprintf("Interface %d has %d ports (%s)\n",
                     interface, cvmx_helper_ports_on_interface(interface),
                     cvmx_helper_interface_mode_to_string(cvmx_helper_interface_get_mode(interface)));
        result |= __cvmx_helper_interface_setup_ipd(interface);
        result |= __cvmx_helper_interface_setup_pko(interface);
    }

    result |= __cvmx_helper_global_setup_ipd();
    result |= __cvmx_helper_global_setup_pko();

    /* Enable any flow control and backpressure */
    result |= __cvmx_helper_global_setup_backpressure();

#if CVMX_HELPER_ENABLE_IPD
    result |= cvmx_helper_ipd_and_packet_input_enable();
#endif
    return result;
}
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
EXPORT_SYMBOL(cvmx_helper_initialize_packet_io_global);
#endif


/**
 * Does core local initialization for packet io
 *
 * @return Zero on success, non-zero on failure
 */
int cvmx_helper_initialize_packet_io_local(void)
{
    return cvmx_pko_initialize_local();
}


/**
 * Undo the initialization performed in
 * cvmx_helper_initialize_packet_io_global(). After calling this routine and the
 * local version on each core, packet IO for Octeon will be disabled and placed
 * in the initial reset state. It will then be safe to call the initialize
 * later on. Note that this routine does not empty the FPA pools. It frees all
 * buffers used by the packet IO hardware to the FPA so a function emptying the
 * FPA after shutdown should find all packet buffers in the FPA.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_helper_shutdown_packet_io_global(void)
{
    const int timeout = 5; /* Wait up to 5 seconds for timeouts */
    int result = 0;
    int num_interfaces;
    int interface;
    int num_ports;
    int index;
    int pool0_count;
    cvmx_wqe_t *work;

    /* Step 1: Disable all backpressure */
    for (interface=0; interface<2; interface++)
        if (cvmx_helper_interface_get_mode(interface) != CVMX_HELPER_INTERFACE_MODE_DISABLED)
            cvmx_gmx_set_backpressure_override(interface, 0xf);

step2:
    /* Step 2: Wait for the PKO queues to drain */
    num_interfaces = cvmx_helper_get_number_of_interfaces();
    for (interface=0; interface<num_interfaces; interface++)
    {
        num_ports = cvmx_helper_ports_on_interface(interface);
        for (index=0; index<num_ports; index++)
        {
            int pko_port = cvmx_helper_get_ipd_port(interface, index);
            int queue = cvmx_pko_get_base_queue(pko_port);
            int max_queue = queue + cvmx_pko_get_num_queues(pko_port);
            while (queue < max_queue)
            {
                int count = cvmx_cmd_queue_length(CVMX_CMD_QUEUE_PKO(queue));
                uint64_t start_cycle = cvmx_get_cycle();
                uint64_t stop_cycle = start_cycle +
                    cvmx_clock_get_rate(CVMX_CLOCK_CORE) * timeout;
                while (count && (cvmx_get_cycle() < stop_cycle))
                {
                    cvmx_wait(10000);
                    count = cvmx_cmd_queue_length(CVMX_CMD_QUEUE_PKO(queue));
                }
                if (count)
                {
                    cvmx_dprintf("PKO port %d, queue %d, timeout waiting for idle\n",
                        pko_port, queue);
                    result = -1;
                }
                queue++;
            }
        }
    }

    /* Step 3: Disable TX and RX on all ports */
    for (interface=0; interface<2; interface++)
    {
        switch (cvmx_helper_interface_get_mode(interface))
        {
            case CVMX_HELPER_INTERFACE_MODE_DISABLED:
            case CVMX_HELPER_INTERFACE_MODE_PCIE:
                /* Not a packet interface */
                break;
            case CVMX_HELPER_INTERFACE_MODE_NPI:
            case CVMX_HELPER_INTERFACE_MODE_SRIO:
                /* We don't handle the NPI/NPEI/SRIO packet engines. The caller
                    must know these are idle */
                break;
            case CVMX_HELPER_INTERFACE_MODE_LOOP:
                /* Nothing needed. Once PKO is idle, the loopback devices
                    must be idle */
                break;
            case CVMX_HELPER_INTERFACE_MODE_SPI:
                /* SPI cannot be disabled from Octeon. It is the responsibility
                    of the caller to make sure SPI is idle before doing
                    shutdown */
                /* Fall through and do the same processing as RGMII/GMII */
            case CVMX_HELPER_INTERFACE_MODE_GMII:
            case CVMX_HELPER_INTERFACE_MODE_RGMII:
                /* Disable outermost RX at the ASX block */
                cvmx_write_csr(CVMX_ASXX_RX_PRT_EN(interface), 0);
                num_ports = cvmx_helper_ports_on_interface(interface);
                if (num_ports > 4)
                    num_ports = 4;
                for (index=0; index<num_ports; index++)
                {
                    cvmx_gmxx_prtx_cfg_t gmx_cfg;
                    gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
                    gmx_cfg.s.en = 0;
                    cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmx_cfg.u64);
                    /* Poll the GMX state machine waiting for it to become idle */
                    cvmx_write_csr(CVMX_NPI_DBG_SELECT, interface*0x800 + index*0x100 + 0x880);
                    if (CVMX_WAIT_FOR_FIELD64(CVMX_DBG_DATA, cvmx_dbg_data_t, data&7, ==, 0, timeout*1000000))
                    {
                        cvmx_dprintf("GMX RX path timeout waiting for idle\n");
                        result = -1;
                    }
                    if (CVMX_WAIT_FOR_FIELD64(CVMX_DBG_DATA, cvmx_dbg_data_t, data&0xf, ==, 0, timeout*1000000))
                    {
                        cvmx_dprintf("GMX TX path timeout waiting for idle\n");
                        result = -1;
                    }
                }
                /* Disable outermost TX at the ASX block */
                cvmx_write_csr(CVMX_ASXX_TX_PRT_EN(interface), 0);
                /* Disable interrupts for interface */
                cvmx_write_csr(CVMX_ASXX_INT_EN(interface), 0);
                cvmx_write_csr(CVMX_GMXX_TX_INT_EN(interface), 0);
                break;
            case CVMX_HELPER_INTERFACE_MODE_XAUI:
            case CVMX_HELPER_INTERFACE_MODE_SGMII:
            case CVMX_HELPER_INTERFACE_MODE_PICMG:
                num_ports = cvmx_helper_ports_on_interface(interface);
                if (num_ports > 4)
                    num_ports = 4;
                for (index=0; index<num_ports; index++)
                {
                    cvmx_gmxx_prtx_cfg_t gmx_cfg;
                    gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
                    gmx_cfg.s.en = 0;
                    cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmx_cfg.u64);
                    if (CVMX_WAIT_FOR_FIELD64(CVMX_GMXX_PRTX_CFG(index, interface), cvmx_gmxx_prtx_cfg_t, rx_idle, ==, 1, timeout*1000000))
                    {
                        cvmx_dprintf("GMX RX path timeout waiting for idle\n");
                        result = -1;
                    }
                    if (CVMX_WAIT_FOR_FIELD64(CVMX_GMXX_PRTX_CFG(index, interface), cvmx_gmxx_prtx_cfg_t, tx_idle, ==, 1, timeout*1000000))
                    {
                        cvmx_dprintf("GMX TX path timeout waiting for idle\n");
                        result = -1;
                    }
                }
                break;
        }
    }

    /* Step 4: Retrieve all packets from the POW and free them */
    while ((work = cvmx_pow_work_request_sync(CVMX_POW_WAIT)))
    {
        cvmx_helper_free_packet_data(work);
        cvmx_fpa_free(work, CVMX_FPA_WQE_POOL, 0);
    }

    /* Step 4b: Special workaround for pass 2 errata */
    if (OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2))
    {
        cvmx_ipd_ptr_count_t ipd_cnt;
        int to_add;
        ipd_cnt.u64 = cvmx_read_csr(CVMX_IPD_PTR_COUNT);
        to_add = (ipd_cnt.s.wqev_cnt + ipd_cnt.s.wqe_pcnt) & 0x7;
        if (to_add)
        {
            int port = -1;
            cvmx_dprintf("Aligning CN38XX pass 2 IPD counters\n");
            if (cvmx_helper_interface_get_mode(0) == CVMX_HELPER_INTERFACE_MODE_RGMII)
                port = 0;
            else if (cvmx_helper_interface_get_mode(1) == CVMX_HELPER_INTERFACE_MODE_RGMII)
                port = 16;

            if (port != -1)
            {
                char *buffer = cvmx_fpa_alloc(CVMX_FPA_PACKET_POOL);
                if (buffer)
                {
                    int queue = cvmx_pko_get_base_queue(port);
                    cvmx_pko_command_word0_t pko_command;
                    cvmx_buf_ptr_t packet;
                    uint64_t start_cycle;
                    uint64_t stop_cycle;

                    /* Populate a minimal packet */
                    memset(buffer, 0xff, 6);
                    memset(buffer+6, 0, 54);
                    pko_command.u64 = 0;
                    pko_command.s.dontfree = 1;
                    pko_command.s.total_bytes = 60;
                    pko_command.s.segs = 1;
                    packet.u64 = 0;
                    packet.s.addr = cvmx_ptr_to_phys(buffer);
                    packet.s.size = CVMX_FPA_PACKET_POOL_SIZE;
                    __cvmx_helper_rgmii_configure_loopback(port, 1, 0);
                    while (to_add--)
                    {
                        cvmx_pko_send_packet_prepare(port, queue, CVMX_PKO_LOCK_CMD_QUEUE);
                        if (cvmx_pko_send_packet_finish(port, queue, pko_command, packet, CVMX_PKO_LOCK_CMD_QUEUE))
                        {
                            cvmx_dprintf("ERROR: Unable to align IPD counters (PKO failed)\n");
                            break;
                        }
                    }
                    cvmx_fpa_free(buffer, CVMX_FPA_PACKET_POOL, 0);

                    /* Wait for the packets to loop back */
                    start_cycle = cvmx_get_cycle();
                    stop_cycle = start_cycle + cvmx_clock_get_rate(CVMX_CLOCK_CORE) * timeout;
                    while (cvmx_cmd_queue_length(CVMX_CMD_QUEUE_PKO(queue)) &&
                           (cvmx_get_cycle() < stop_cycle))
                    {
                        cvmx_wait(1000);
                    }
                    cvmx_wait(1000);
                    __cvmx_helper_rgmii_configure_loopback(port, 0, 0);
                    if (to_add == -1)
                        goto step2;
                }
                else
                    cvmx_dprintf("ERROR: Unable to align IPD counters (Packet pool empty)\n");
            }
            else
                cvmx_dprintf("ERROR: Unable to align IPD counters\n");
        }
    }

    /* Step 5: Disable IPD and PKO. PIP is taken care of in the next step */
    cvmx_ipd_disable();
    cvmx_pko_disable();

    /* Step 6: Drain all prefetched buffers from IPD/PIP. Note that IPD/PIP
        have not been reset yet */
    __cvmx_ipd_free_ptr();

    /* Step 7: Free the PKO command buffers and put PKO in reset */
    cvmx_pko_shutdown();

    /* Step 8: Disable MAC address filtering */
    for (interface=0; interface<2; interface++)
    {
        switch (cvmx_helper_interface_get_mode(interface))
        {
            case CVMX_HELPER_INTERFACE_MODE_DISABLED:
            case CVMX_HELPER_INTERFACE_MODE_PCIE:
            case CVMX_HELPER_INTERFACE_MODE_SRIO:
            case CVMX_HELPER_INTERFACE_MODE_NPI:
            case CVMX_HELPER_INTERFACE_MODE_LOOP:
                break;
            case CVMX_HELPER_INTERFACE_MODE_XAUI:
            case CVMX_HELPER_INTERFACE_MODE_GMII:
            case CVMX_HELPER_INTERFACE_MODE_RGMII:
            case CVMX_HELPER_INTERFACE_MODE_SPI:
            case CVMX_HELPER_INTERFACE_MODE_SGMII:
            case CVMX_HELPER_INTERFACE_MODE_PICMG:
                num_ports = cvmx_helper_ports_on_interface(interface);
                if (num_ports > 4)
                    num_ports = 4;
                for (index=0; index<num_ports; index++)
                {
                    cvmx_write_csr(CVMX_GMXX_RXX_ADR_CTL(index, interface), 1);
                    cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM_EN(index, interface), 0);
                    cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM0(index, interface), 0);
                    cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM1(index, interface), 0);
                    cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM2(index, interface), 0);
                    cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM3(index, interface), 0);
                    cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM4(index, interface), 0);
                    cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM5(index, interface), 0);
                }
                break;
        }
    }

    /* Step 9: Drain all FPA buffers out of pool 0 before we reset IPD/PIP.
        This is needed to keep IPD_QUE0_FREE_PAGE_CNT in sync. We use pool 1
        for temporary storage */
    pool0_count = 0;
    while (1)
    {
        void *buffer = cvmx_fpa_alloc(0);
        if (buffer)
        {
            cvmx_fpa_free(buffer, 1, 0);
            pool0_count++;
        }
        else
            break;
    }


    /* Step 10: Reset IPD and PIP */
    {
        cvmx_ipd_ctl_status_t ipd_ctl_status;
        ipd_ctl_status.u64 = cvmx_read_csr(CVMX_IPD_CTL_STATUS);
        ipd_ctl_status.s.reset = 1;
        cvmx_write_csr(CVMX_IPD_CTL_STATUS, ipd_ctl_status.u64);

        if ((cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_SIM) &&
            (OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN5XXX)))
        {
            /* only try 1000 times.  Normally if this works it will happen in
            ** the first 50 loops. */
            int max_loops = 1000;
            int loop = 0;
            /* Per port backpressure counters can get misaligned after an
               IPD reset. This code realigns them by performing repeated
               resets. See IPD-13473 */
            cvmx_wait(100);
            if (__cvmx_helper_backpressure_is_misaligned())
            {
                cvmx_dprintf("Starting to align per port backpressure counters.\n");
                while (__cvmx_helper_backpressure_is_misaligned() && (loop++ < max_loops))
                {
                    cvmx_write_csr(CVMX_IPD_CTL_STATUS, ipd_ctl_status.u64);
                    cvmx_wait(123);
                }
                if (loop < max_loops)
                    cvmx_dprintf("Completed aligning per port backpressure counters (%d loops).\n", loop);
                else
                {
                    cvmx_dprintf("ERROR: unable to align per port backpressure counters.\n");
                    /* For now, don't hang.... */
                }
            }
        }

        /* PIP_SFT_RST not present in CN38XXp{1,2} */
        if (!OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2))
        {
            cvmx_pip_sft_rst_t pip_sft_rst;
            pip_sft_rst.u64 = cvmx_read_csr(CVMX_PIP_SFT_RST);
            pip_sft_rst.s.rst = 1;
            cvmx_write_csr(CVMX_PIP_SFT_RST, pip_sft_rst.u64);
        }
    }

    /* Step 11: Restore the FPA buffers into pool 0 */
    while (pool0_count--)
        cvmx_fpa_free(cvmx_fpa_alloc(1), 0, 0);

    return result;
}
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
EXPORT_SYMBOL(cvmx_helper_shutdown_packet_io_global);
#endif


/**
 * Does core local shutdown of packet io
 *
 * @return Zero on success, non-zero on failure
 */
int cvmx_helper_shutdown_packet_io_local(void)
{
    /* Currently there is nothing to do per core. This may change in
        the future */
    return 0;
}



/**
 * Auto configure an IPD/PKO port link state and speed. This
 * function basically does the equivalent of:
 * cvmx_helper_link_set(ipd_port, cvmx_helper_link_get(ipd_port));
 *
 * @param ipd_port IPD/PKO port to auto configure
 *
 * @return Link state after configure
 */
cvmx_helper_link_info_t cvmx_helper_link_autoconf(int ipd_port)
{
    cvmx_helper_link_info_t link_info;
    int interface = cvmx_helper_get_interface_num(ipd_port);
    int index = cvmx_helper_get_interface_index_num(ipd_port);

    if (index >= cvmx_helper_ports_on_interface(interface))
    {
        link_info.u64 = 0;
        return link_info;
    }

    link_info = cvmx_helper_link_get(ipd_port);
    if (link_info.u64 ==  port_link_info[ipd_port].u64)
        return link_info;

#if !defined(CVMX_BUILD_FOR_LINUX_KERNEL) && !defined(CVMX_BUILD_FOR_FREEBSD_KERNEL)
    if (!link_info.s.link_up)
        cvmx_error_disable_group(CVMX_ERROR_GROUP_ETHERNET, ipd_port);
#endif

    /* If we fail to set the link speed, port_link_info will not change */
    cvmx_helper_link_set(ipd_port, link_info);

#if !defined(CVMX_BUILD_FOR_LINUX_KERNEL) && !defined(CVMX_BUILD_FOR_FREEBSD_KERNEL)
    if (link_info.s.link_up)
        cvmx_error_enable_group(CVMX_ERROR_GROUP_ETHERNET, ipd_port);
#endif

    /* port_link_info should be the current value, which will be different
        than expect if cvmx_helper_link_set() failed */
    return port_link_info[ipd_port];
}
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
EXPORT_SYMBOL(cvmx_helper_link_autoconf);
#endif

/**
 * Return the link state of an IPD/PKO port as returned by
 * auto negotiation. The result of this function may not match
 * Octeon's link config if auto negotiation has changed since
 * the last call to cvmx_helper_link_set().
 *
 * @param ipd_port IPD/PKO port to query
 *
 * @return Link state
 */
cvmx_helper_link_info_t cvmx_helper_link_get(int ipd_port)
{
    cvmx_helper_link_info_t result;
    int interface = cvmx_helper_get_interface_num(ipd_port);
    int index = cvmx_helper_get_interface_index_num(ipd_port);

    /* The default result will be a down link unless the code below
        changes it */
    result.u64 = 0;

    if (index >= cvmx_helper_ports_on_interface(interface))
        return result;

    switch (cvmx_helper_interface_get_mode(interface))
    {
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:
        case CVMX_HELPER_INTERFACE_MODE_PCIE:
            /* Network links are not supported */
            break;
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
            result = __cvmx_helper_xaui_link_get(ipd_port);
            break;
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            if (index == 0)
                result = __cvmx_helper_rgmii_link_get(ipd_port);
            else
            {
                result.s.full_duplex = 1;
                result.s.link_up = 1;
                result.s.speed = 1000;
            }
            break;
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
            result = __cvmx_helper_rgmii_link_get(ipd_port);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SPI:
            result = __cvmx_helper_spi_link_get(ipd_port);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            result = __cvmx_helper_sgmii_link_get(ipd_port);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SRIO:
            result = __cvmx_helper_srio_link_get(ipd_port);
            break;
        case CVMX_HELPER_INTERFACE_MODE_NPI:
        case CVMX_HELPER_INTERFACE_MODE_LOOP:
            /* Network links are not supported */
            break;
    }
    return result;
}
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
EXPORT_SYMBOL(cvmx_helper_link_get);
#endif


/**
 * Configure an IPD/PKO port for the specified link state. This
 * function does not influence auto negotiation at the PHY level.
 * The passed link state must always match the link state returned
 * by cvmx_helper_link_get(). It is normally best to use
 * cvmx_helper_link_autoconf() instead.
 *
 * @param ipd_port  IPD/PKO port to configure
 * @param link_info The new link state
 *
 * @return Zero on success, negative on failure
 */
int cvmx_helper_link_set(int ipd_port, cvmx_helper_link_info_t link_info)
{
    int result = -1;
    int interface = cvmx_helper_get_interface_num(ipd_port);
    int index = cvmx_helper_get_interface_index_num(ipd_port);

    if (index >= cvmx_helper_ports_on_interface(interface))
        return -1;

    switch (cvmx_helper_interface_get_mode(interface))
    {
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:
        case CVMX_HELPER_INTERFACE_MODE_PCIE:
            break;
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
            result = __cvmx_helper_xaui_link_set(ipd_port, link_info);
            break;
        /* RGMII/GMII/MII are all treated about the same. Most functions
            refer to these ports as RGMII */
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            result = __cvmx_helper_rgmii_link_set(ipd_port, link_info);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SPI:
            result = __cvmx_helper_spi_link_set(ipd_port, link_info);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            result = __cvmx_helper_sgmii_link_set(ipd_port, link_info);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SRIO:
            result = __cvmx_helper_srio_link_set(ipd_port, link_info);
            break;
        case CVMX_HELPER_INTERFACE_MODE_NPI:
        case CVMX_HELPER_INTERFACE_MODE_LOOP:
            break;
    }
    /* Set the port_link_info here so that the link status is updated
       no matter how cvmx_helper_link_set is called. We don't change
       the value if link_set failed */
    if (result == 0)
        port_link_info[ipd_port].u64 = link_info.u64;
    return result;
}
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
EXPORT_SYMBOL(cvmx_helper_link_set);
#endif


/**
 * Configure a port for internal and/or external loopback. Internal loopback
 * causes packets sent by the port to be received by Octeon. External loopback
 * causes packets received from the wire to sent out again.
 *
 * @param ipd_port IPD/PKO port to loopback.
 * @param enable_internal
 *                 Non zero if you want internal loopback
 * @param enable_external
 *                 Non zero if you want external loopback
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_helper_configure_loopback(int ipd_port, int enable_internal, int enable_external)
{
    int result = -1;
    int interface = cvmx_helper_get_interface_num(ipd_port);
    int index = cvmx_helper_get_interface_index_num(ipd_port);

    if (index >= cvmx_helper_ports_on_interface(interface))
        return -1;

    switch (cvmx_helper_interface_get_mode(interface))
    {
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:
        case CVMX_HELPER_INTERFACE_MODE_PCIE:
        case CVMX_HELPER_INTERFACE_MODE_SRIO:
        case CVMX_HELPER_INTERFACE_MODE_SPI:
        case CVMX_HELPER_INTERFACE_MODE_NPI:
        case CVMX_HELPER_INTERFACE_MODE_LOOP:
            break;
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
            result = __cvmx_helper_xaui_configure_loopback(ipd_port, enable_internal, enable_external);
            break;
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            result = __cvmx_helper_rgmii_configure_loopback(ipd_port, enable_internal, enable_external);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            result = __cvmx_helper_sgmii_configure_loopback(ipd_port, enable_internal, enable_external);
            break;
    }
    return result;
}

#endif /* CVMX_ENABLE_PKO_FUNCTIONS */
