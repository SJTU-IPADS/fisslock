
#include <core.p4>
#include <tna.p4>

#include "headers.p4"
#include "parser.p4"
#include "ingress.p4"
#include "egress.p4"

Pipeline(
  IngressParser(),
  IngressPipe(),
  IngressDeparser(),
  EgressParser(),
  EgressPipe(),
  EgressDeparser()
) pipe;

Switch(pipe) main;