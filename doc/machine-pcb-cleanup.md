# machine/PCB cleanup status

What still has to be checked by hand in `machine/` now that the naming and
structure work is done. The rules themselves live in
[adding-a-device.md](adding-a-device.md); this page only tracks the work, so
tick a box once the device has been tested against its stock firmware.

Legend for every table: `[x]` checked and correct, `[ ]` still to do,
`-` not applicable.

## Machine targets

One row per `machine/<MACHINE>.c`, i.e. per name accepted by
`make MACHINE=...` (64 targets).

| `<MACHINE>`                           | PCB                                   | PCB is correct | `machine_model` rule | Confirmed working |
| :------------------------------------ | :------------------------------------ | :------------: | :------------------: | :---------------: |
| `AMPCOM_WAM902_SWTG018AS`             | `PCB_SWTG018AS_A_V2_0_1_19649`        | [ ]            | [x]                  | [ ]               |
| `DAVUAZ_DA_K6501W`                    | `PCB_K0501W_V2_0`                     | [ ]            | [ ]                  | [ ]               |
| `DEFAULT_8C_1SFP`                     | `NONAME_KP_9000_9XH_X_EU`             | [ ]            | [ ]                  | [ ]               |
| `DOTX_DX_1009N`                       | `PCB_SWTG118AS_V1_0_11430`            | [ ]            | [x]                  | [ ]               |
| `FOXNEO_FNS_1200P`                    | `PCB_K0402W_U13_V2_0`                 | [ ]            | [ ]                  | [ ]               |
| `GOALAKE_IG108`                       | `PB_2132`                             | [ ]            | [ ]                  | [ ]               |
| `GOODTOP_GT_ST018M`                   | `PCB_SWTG018AS_A_V2_0_1_19649`        | [x]            | [x]                  | [x]               |
| `GOODTOP_GT_ST024M`                   | `PCB_SWTG024AS_A_V2_0_1_19650`        | [ ]            | [x]                  | [ ]               |
| `GUANGNIANWEI_7018_2_5G`              | `F7008_2_5GPOE_V1_2`                  | [ ]            | [ ]                  | [ ]               |
| `HASIVO_S1100P_8GT_1SX_S`             | `S1100P_8GT_1SX_73N_S_V1_01`          | [ ]            | [x]                  | [ ]               |
| `HASIVO_S1100WP_8GT_1SX_SE`           | `S1100W_8GT_1SX_SE_V1_05`             | [ ]            | [x]                  | [ ]               |
| `HASIVO_S600W_4GT_2XGT_SE`            | `NONAME_S600W_4GT_2XGT_SE`            | [ ]            | [x]                  | [ ]               |
| `HISOURCE_HI_K0402WS_PCB_K0402W_V2_0` | `PCB_K0402W_V2_0`                     | [ ]            | [ ]                  | [ ]               |
| `HISOURCE_HI_K0402WS_PCB_K0402W_V3_0` | `PCB_K0402W_V3_0`                     | [ ]            | [ ]                  | [ ]               |
| `HISOURCE_HI_K0801WS`                 | `PCB_K0801W_V2_0`                     | [ ]            | [ ]                  | [ ]               |
| `HONGYAVISION_HYWS_SGT0204S`          | `2G5F4_10G2_V1_01`                    | [ ]            | [ ]                  | [ ]               |
| `HORACO_HC_SWTG124AS`                 | `PCB_SWTG024AS_A_V2_0_1_19650`        | [ ]            | [x]                  | [ ]               |
| `HORACO_HC_SWTG218AS`                 | `PCB_SWTG018AS_A_V2_0_1_19649`        | [ ]            | [x]                  | [ ]               |
| `HORACO_ZX310S_4T2XH`                 | `PCB_SL310S_4T1T1X_V1_0_1_24107`      | [ ]            | [ ]                  | [ ]               |
| `HORACO_ZX310S_4T2XT`                 | `PCB_310S_4T2XT_V1_0_0_22273`         | [ ]            | [ ]                  | [ ]               |
| `HORACO_ZX_SG4T2`                     | `PCB_SWTG024AS_A_V2_0_1_19650`        | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_6XHML_V1_1`         | `2M_PCB43_V1_1`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_6XHML_V1_2`         | `2M_PCB43_V1_2`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_6XHML_V2_1`         | `2M_PCB43_V2_1`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_6XHML_X2`           | `2M_PCB43_V1_1`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_6XH_V1_1`           | `2M_PCB43_V1_1`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_6XH_V1_2`           | `2M_PCB43_V1_2`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_6XH_X`              | `NONAME_KP_9000_6XH_X`                | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_6XH_X2`             | `2M_PCB43_V2_1`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_6XH_X2_V2_1`        | `2M_PCB43_V2_1`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_9XHML_X_V2_2`       | `2M_PCB23_V2_2`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_9XHML_X_V3_1`       | `2M_PCB23_V3_1`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_9XHML_X_V3_2`       | `2M_PCB23_V3_2`                       | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_9XHPML_V1_2`        | `2M_PCB51_ML_V1_2`                    | [ ]            | [x]                  | [ ]               |
| `KEEPLINK_KP_9000_9XH_X_EU`           | `NONAME_KP_9000_9XH_X_EU`             | [ ]            | [x]                  | [ ]               |
| `LIANGUO_HYWS_SGT0108S`               | `2G5F_20G_V1_01`                      | [ ]            | [x]                  | [ ]               |
| `LIANGUO_ZX_SWTGW215AS`               | `PCB_SWTG115AS_V2_0_16030`            | [ ]            | [x]                  | [ ]               |
| `MOKERLINK_2G040210GSM`               | `2M_PCB43_V1_1`                       | [ ]            | [x]                  | [ ]               |
| `MOKERLINK_2G08110GSN`                | `PCB_SWTG018AS_A_V2_0_1_19649`        | [ ]            | [x]                  | [ ]               |
| `MOKERLINK_POE_2G080110GS`            | `2M_PCB51_ML_V1_1`                    | [ ]            | [x]                  | [ ]               |
| `MOKERLINK_ZX_SWTGW218AS`             | `PCB_SWTG118AS_V2_0_16029`            | [ ]            | [ ]                  | [ ]               |
| `ONT_S207CW_62TS_SE`                  | `NONAME_20240111`                     | [ ]            | [ ]                  | [ ]               |
| `PCB_SWTG024AS_A_2_0_1`               | `PCB_SWTG024AS_A_V2_0_1_19650`        | [ ]            | [x]                  | [ ]               |
| `PCB_SWTG024AS_V2_1_0`                | `PCB_SWTG024AS_V2_1_0_19023`          | [ ]            | [ ]                  | [ ]               |
| `REALHD_PSW8_25MG`                    | `PCB_AFGW_SWTG218ANS_V2_0_2_23308`    | [x]            | [x]                  | [x]               |
| `RUIYING_RY_4GT_2SX`                  | `FG_4GT_2SX_V2_0`                     | [ ]            | [ ]                  | [ ]               |
| `RUIYING_RY_8GT_1SX`                  | `FG_8GT_1SX`                          | [ ]            | [ ]                  | [ ]               |
| `SIRIVISION_S25G2206FP_V1_1`          | `S25G2206FP_V1_1`                     | [ ]            | [x]                  | [ ]               |
| `SIRIVISION_SR_S25G2109F`             | `S25G1109FP_V1_3`                     | [ ]            | [x]                  | [ ]               |
| `SODOLA_SL902_SWTGW215AS`             | `PCB_SWTG024AS_V2_1_0_19023__1S1C`    | [ ]            | [x]                  | [ ]               |
| `SODOLA_SL_SWTGW215AS_D`              | `PCB_SWTG115AS_V2_1_17443`            | [ ]            | [x]                  | [ ]               |
| `STEAMEMO_IG204_V1`                   | `PB_2131`                             | [x]            | [x]                  | [ ]               |
| `SWGT024_V2_0_MANAGED`                | `PCB_SWTG024AS_V2_0_17452`            | [ ]            | [x]                  | [ ]               |
| `SWGT024_V2_0_UNMANAGED`              | `PCB_SWTG024AS_V2_0_17452__UNMANAGED` | [ ]            | [x]                  | [ ]               |
| `SWTG018AS_A_V_2_0`                   | `PCB_SWTG018AS_A_V2_0_1_19649`        | [ ]            | [ ]                  | [ ]               |
| `SWTG024AS_A_V2_0_1_5C_1SFP`          | `PCB_SWTG024AS_A_V2_0_1_19650__1S1C`  | [ ]            | [x]                  | [ ]               |
| `SWTG024AS_V2_0`                      | `PCB_SWTG024AS_V2_0_16895`            | [ ]            | [x]                  | [ ]               |
| `TRENDNET_TEG_S562`                   | `270_NB1_MS206B004A1_REV_A1`          | [ ]            | [x]                  | [ ]               |
| `XIKESTOR_SKS3200M_8GPY1XF`           | `PCB_SWTG118AS_V2_1_17462`            | [ ]            | [x]                  | [ ]               |
| `XIKESTOR_SKS3200_8E1X`               | `PCB_SWTG018AS_V2_1_0_19030`          | [ ]            | [x]                  | [ ]               |
| `YUANLEY_YS25_0402M`                  | `S25G2206FP_V1_1`                     | [ ]            | [x]                  | [ ]               |
| `YUANLEY_YS25_0402_PCB_K0402W_V1_0`   | `PCB_K0402W_V1_0`                     | [ ]            | [ ]                  | [ ]               |
| `ZOERAX_HG0402XG`                     | `HG0402XG_V1_1`                       | [ ]            | [ ]                  | [ ]               |
| `ZOERAX_HG0402XG_V1_1`                | `HG0402XG_V1_1`                       | [ ]            | [x]                  | [ ]               |

`machine_model` rule: `MACHINE_MODEL` in `machine/<MACHINE>.h` must
convert to the target name exactly as described in Step 1 (capitals,
punctuation to `_`) and must fit `machine_name`, which is 29 characters.
An unchecked box in this column means the rule is not met today: 19 of 64
targets fail it.

The PCB column is the link text of `machine/<MACHINE>.c`, the `pcb_ver` the GUI
shows. Tick `PCB is correct` when that really is the board inside this device,
and when the link names that board rather than something it resolves to.

## PCB definitions

One row per `machine/pcb/<PCB>.c` (48 definitions, aliases included).

| `<PCB>`                                                                    | Is symlink correct | RJ45 works | LEDs in struct match stock firmware | SFP works | SFP is bicolor | Both SFP colors work | Reset IO correct |
| :------------------------------------------------------------------------- | :----------------: | :--------: | :---------------------------------: | :-------: | :------------: | :------------------: | :--------------: |
| `270_NB1_MS206B004A1_REV_A1`                                               | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `2G5F4_10G2_V1_01`                                                         | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `2G5F_20G_V1_01`                                                           | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `2M_PCB23_V2_2`                                                            | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `2M_PCB23_V3_1`                                                            | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `2M_PCB23_V3_2 (alias of 2M_PCB23_V3_1)`                                   | [ ]                | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `2M_PCB43_V1_1`                                                            | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `2M_PCB43_V1_2 (alias of 2M_PCB43_V1_1)`                                   | [ ]                | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `2M_PCB43_V2_1`                                                            | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `2M_PCB51_ML_V1_1`                                                         | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `2M_PCB51_ML_V1_2`                                                         | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `F7008_2_5GPOE_V1_2`                                                       | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `FG_4GT_2SX_V2_0`                                                          | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `FG_8GT_1SX`                                                               | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `HG0402XG_V1_1`                                                            | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `NONAME_20240111`                                                          | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `NONAME_KP_9000_6XH_X`                                                     | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `NONAME_KP_9000_9XH_X_EU`                                                  | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `NONAME_S600W_4GT_2XGT_SE`                                                 | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PB_2131`                                                                  | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PB_2132`                                                                  | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_310S_4T2XT_V1_0_0_22273`                                              | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_AFGW_SWTG218ANS_V2_0_2_23308`                                         | -                  | [x]        | [x]                                 | [x]       | [x]            | [x]                  | [x]              |
| `PCB_K0402W_U13_V2_0 (alias of PCB_K0402W_V2_0)`                           | [ ]                | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_K0402W_V1_0`                                                          | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_K0402W_V2_0`                                                          | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_K0402W_V3_0`                                                          | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_K0501W_V2_0`                                                          | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_K0801W_V2_0`                                                          | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SL310S_4T1T1X_V1_0_1_24107`                                           | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG018AS_A_V2_0_1_19649 (alias of PCB_AFGW_SWTG218ANS_V2_0_2_23308)` | [x]                | [x]        | [x]                                 | [x]       | [x]            | [x]                  | [x]              |
| `PCB_SWTG018AS_V2_1_0_19030`                                               | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG024AS_A_V2_0_1_19650`                                             | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG024AS_A_V2_0_1_19650__1S1C`                                       | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG024AS_V2_0_16895`                                                 | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG024AS_V2_0_17452`                                                 | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG024AS_V2_0_17452__UNMANAGED`                                      | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG024AS_V2_1_0_19023`                                               | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG024AS_V2_1_0_19023__1S1C`                                         | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG115AS_V2_0_16030`                                                 | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG115AS_V2_1_17443 (alias of PCB_SWTG115AS_V2_0_16030)`             | [ ]                | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG118AS_V1_0_11430`                                                 | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG118AS_V2_0_16029`                                                 | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `PCB_SWTG118AS_V2_1_17462 (alias of PCB_SWTG118AS_V2_0_16029)`             | [ ]                | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `S1100P_8GT_1SX_73N_S_V1_01`                                               | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `S1100W_8GT_1SX_SE_V1_05 (alias of PCB_SWTG118AS_V2_0_16029)`              | [ ]                | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `S25G1109FP_V1_3`                                                          | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |
| `S25G2206FP_V1_1`                                                          | -                  | [ ]        | [ ]                                 | [ ]       | [ ]            | [ ]                  | [ ]              |

