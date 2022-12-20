
#  Copyright 1984 UniSoft Systems, Version 1.1
#  @(#)renem.mk	1.1 4/14/87

SHELL	=/bin/sh

#
#	Machine configuration information
#

dorenem:
		echo 1000 > .renem
		sh -c "renem lib/at_atp_op_rp.c"
		echo 1050 > .renem
		sh -c "renem lib/at_atp_op_rq.c"
		echo 1100 > .renem
		sh -c "renem lib/at_atp_rcv.c"
		echo 1150 > .renem
		sh -c "renem lib/at_atp_rp.c"
		echo 1200 > .renem
		sh -c "renem lib/at_atp_rp_mb.c"
		echo 1250 > .renem
		sh -c "renem lib/at_atp_rp_sb.c"
		echo 1300 > .renem
		sh -c "renem lib/at_atp_sn.c"
		echo 1350 > .renem
		sh -c "renem lib/at_atp_sn_mb.c"
		echo 1400 > .renem
		sh -c "renem lib/at_atp_sn_sb.c"
		echo 1450 > .renem
		sh -c "renem lib/at_chooser.c"
		echo 1500 > .renem
		sh -c "renem lib/at_cl_d_skt.c"
		echo 1550 > .renem
		sh -c "renem lib/at_cl_skt.c"
		echo 1600 > .renem
		sh -c "renem lib/at_con_nve.c"
		echo 1650 > .renem
		sh -c "renem lib/at_ddp_rd.c"
		echo 1700 > .renem
		sh -c "renem lib/at_ddp_wr.c"
		echo 1750 > .renem
		sh -c "renem lib/at_dec_en.c"
		echo 1800 > .renem
		sh -c "renem lib/at_declare.c"
		echo 1850 > .renem
		sh -c "renem lib/at_drg_nve.c"
		echo 1900 > .renem
		sh -c "renem lib/at_drgn_en.c"
		echo 1950 > .renem
		sh -c "renem lib/at_drgn_nve.c"
		echo 2000 > .renem
		sh -c "renem lib/at_error.c"
		echo 2050 > .renem
		sh -c "renem lib/at_error_set.c"
		echo 2100 > .renem
		sh -c "renem lib/at_execute.c"
		echo 2150 > .renem
		sh -c "renem lib/at_fnd_netw.c"
		echo 2200 > .renem
		sh -c "renem lib/at_gddplen.c"
		echo 2250 > .renem
		sh -c "renem lib/at_getnet.c"
		echo 2300 > .renem
		sh -c "renem lib/at_lk_cnf.c"
		echo 2350 > .renem
		sh -c "renem lib/at_lk_en.c"
		echo 2400 > .renem
		sh -c "renem lib/at_lk_nve.c"
		echo 2450 > .renem
		sh -c "renem lib/at_nbp_down.c"
		echo 2500 > .renem
		sh -c "renem lib/at_op_d_skt.c"
		echo 2550 > .renem
		sh -c "renem lib/at_op_skt.c"
		echo 2600 > .renem
		sh -c "renem lib/at_pap_cl.c"
		echo 2650 > .renem
		sh -c "renem lib/at_pap_dereg.c"
		echo 2700 > .renem
		sh -c "renem lib/at_pap_get.c"
		echo 2750 > .renem
		sh -c "renem lib/at_pap_hsts.c"
		echo 2800 > .renem
		sh -c "renem lib/at_pap_init.c"
		echo 2850 > .renem
		sh -c "renem lib/at_pap_open.c"
		echo 2900 > .renem
		sh -c "renem lib/at_pap_read.c"
		echo 2950 > .renem
		sh -c "renem lib/at_pap_reg.c"
		echo 3000 > .renem
		sh -c "renem lib/at_pap_slcl.c"
		echo 3050 > .renem
		sh -c "renem lib/at_pap_sts.c"
		echo 3100 > .renem
		sh -c "renem lib/at_pap_wr.c"
		echo 3150 > .renem
		sh -c "renem lib/at_pap_wr_e.c"
		echo 3200 > .renem
		sh -c "renem lib/at_pap_wrf.c"
		echo 3250 > .renem
		sh -c "renem lib/at_ptm.c"
		echo 3300 > .renem
		sh -c "renem lib/at_reg_en.c"
		echo 3350 > .renem
		sh -c "renem lib/at_reg_nve.c"
		echo 3400 > .renem
		sh -c "renem lib/at_sddplen.c"
		echo 3450 > .renem
		sh -c "renem lib/at_snd_dev.c"
		echo 3500 > .renem
		sh -c "renem lib/atp_get.c"
		echo 3550 > .renem
		sh -c "renem lib/atp_get_note.c"
		echo 3600 > .renem
		sh -c "renem lib/atp_get_nw.c"
		echo 3650 > .renem
		sh -c "renem lib/atp_get_poll.c"
		echo 3700 > .renem
		sh -c "renem lib/atp_is.c"
		echo 3750 > .renem
		sh -c "renem lib/atp_is_nt.c"
		echo 3800 > .renem
		sh -c "renem lib/atp_is_nw.c"
		echo 3850 > .renem
		sh -c "renem lib/atp_is_x.c"
		echo 3900 > .renem
		sh -c "renem lib/atp_is_x_nt.c"
		echo 3950 > .renem
		sh -c "renem lib/atp_is_x_nw.c"
		echo 4000 > .renem
		sh -c "renem lib/atp_isd.c"
		echo 4050 > .renem
		sh -c "renem lib/atp_isd_nt.c"
		echo 4100 > .renem
		sh -c "renem lib/atp_isd_nw.c"
		echo 4150 > .renem
		sh -c "renem lib/atp_isd_x.c"
		echo 4200 > .renem
		sh -c "renem lib/atp_isd_x_nt.c"
		echo 4250 > .renem
		sh -c "renem lib/atp_isd_x_nw.c"
		echo 4300 > .renem
		sh -c "renem lib/atp_open.c"
		echo 4350 > .renem
		sh -c "renem lib/atp_poll_req.c"
		echo 4400 > .renem
		sh -c "renem lib/atp_release.c"
		echo 4450 > .renem
		sh -c "renem lib/atp_send.c"
		echo 4500 > .renem
		sh -c "renem lib/atp_send_eof.c"
		echo 4550 > .renem
		sh -c "renem lib/atp_set_def.c"
		echo 4600 > .renem
		sh -c "renem lib/atp_set_hdr.c"
		echo 5000 > .renem
		sh -c "renem util/at_nbpd.c"
		echo 5500 > .renem
		sh -c "renem util/at_nvedel.c"
		echo 6000 > .renem
		sh -c "renem util/at_nvereg.c"
		echo 6500 > .renem
		sh -c "renem util/at_nvelkup.c"
		echo 7000 > .renem
		sh -c "renem util/at_printer.c"
		echo 7500 > .renem
		sh -c "renem util/at_server.c"
		echo 8000 > .renem
		sh -c "renem util/at_status.c"
		echo 8500 > .renem
		sh -c "renem util/at_nveshut.c"
		echo 9000 > .renem
		sh -c "renem appletalk.c"
