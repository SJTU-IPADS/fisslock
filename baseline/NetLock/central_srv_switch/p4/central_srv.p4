#include <core.p4>
#include <tna.p4>

#include "constants.p4"
#include "headers.p4"
#include "util.p4"
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