// Keep the same host/control harness in the explicit GPU-only executable.
#define CUEXIS_PD5_DEVICE 1
#include "pd5_host_probe.cpp"
#define CUEXIS_PD5_MATRIX_DEVICE 1
#include "pd5_matrix_probe.cpp"
