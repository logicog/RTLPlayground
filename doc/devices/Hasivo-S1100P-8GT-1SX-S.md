# Hasivo S1100P-8GT-1SX-S

Local hardware profile reconstructed from the preserved owner source.
Eight copper ports and one SFP map to MAC0–8; CPU is MAC9. SFP uses SDS1, detect GPIO30, LOS GPIO37, TX-disable unspecified/NA, SDA39/SCL40. Reset GPIO54. Copper LED0 is orange at 1G/100M/10M; LED2 is green at 2.5G. Custom LED mux is preserved verbatim.

Select explicitly with `make MACHINE=HASIVO_S1100P_8GT_1SX_S CI=1`. No global machine.h selection and no PoE implementation or validation claim. Software build tests do not validate wiring.
