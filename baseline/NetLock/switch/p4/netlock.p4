#include <core.p4>
#include <tna.p4>

#include "includes/constants.p4"
#include "includes/headers.p4"
#include "includes/util.p4"
#include "includes/registers.p4"
#include "egress.p4"
#include "ingress.p4"
#include "parser.p4"

Pipeline(
  SwitchIngressParser(),
  SwitchIngress(),
  SwitchIngressDeparser(),
  SwitchEgressParser(),
  SwitchEgress(),
  SwitchEgressDeparser()
) pipe;

Switch(pipe) main;