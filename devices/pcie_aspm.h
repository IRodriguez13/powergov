#ifndef POWERGOV_PCIE_ASPM_H
#define POWERGOV_PCIE_ASPM_H

int pcie_aspm_apply(int aggression);
int pcie_aspm_restore(void);
int pcie_aspm_available(void);

#endif
