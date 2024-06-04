
control EgressPipe(
	inout header_t hdr,
  inout metadata_t meta,
  in egress_intrinsic_metadata_t eg_intr_md,
  in egress_intrinsic_metadata_from_parser_t eg_intr_md_from_prsr,
  inout egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md,
  inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {

  apply {
    if (hdr.lock.multicasted == 1 && eg_intr_md.egress_rid == 2) {
      hdr.lock.type = GRANT_WO_AGENT;
      hdr.udp.dst_port = UDP_PORT_CLIENT;
      hdr.lock.multicasted = 0;
    } else if (hdr.lock.multicasted == 1 && eg_intr_md.egress_rid == 1) {
      hdr.lock.type = ACQUIRE;
      hdr.udp.dst_port = UDP_PORT_SERVER;
      hdr.lock.multicasted = 0;
      hdr.lock.granted = 1;
    }
  }
}