"use strict";
var LANG={
en:{
nav_dash:"Dashboard",nav_ports:"Ports",nav_stp:"Spanning tree",nav_stats:"Statistics",
nav_vlan:"VLANs",nav_l2:"MAC table",nav_mirror:"Mirroring",nav_lag:"LAG",nav_eee:"EEE",
nav_bw:"Bandwidth",nav_system:"System",nav_fw:"Firmware",
hdr_dirty:"unsaved changes",hdr_dirty_t:"Running config differs from startup config",
hdr_save:"Save to flash",hdr_save_t:"Persist running configuration to flash",
th_auto:"Theme: System",th_auto_sel:"Theme: System (Selenized)",th_light:"Light",th_dark:"Dark",
th_sel_light:"Selenized Light",th_sel_dark:"Selenized Dark",
c_port:"Port",c_name:"Name",c_link:"Link",c_apply:"Apply",c_cancel:"Cancel",c_confirm:"Confirm",
c_close:"Close",c_refresh:"Refresh",c_off:"off",c_offc:"Off",c_down:"down",c_on:"On",c_auto:"Auto",c_delete:"Delete",
c_type:"Type",c_state:"State",c_enabled:"Enabled",c_load:"Load",c_disable:"Disable",c_sfp:"SFP",
c_full:"full",c_half:"half",c_devices:"devices",c_yes:"yes",c_no:"no",
d_ports:"Ports",d_ports_h:"click a port for details",d_system:"System",d_traffic:"Traffic",
d_traffic_h:"packets/s, live",d_txpps:"TX pps",d_rxpps:"RX pps",d_txbad:"TX bad",d_rxbad:"RX bad",
i_host:"Hostname",i_ip:"IP address",i_mask:"Netmask",i_gw:"Gateway",i_mac:"MAC",i_fw:"Firmware",
i_built:"Built",i_hw:"Hardware",i_flash:"Flash",i_syslog:"Syslog",
p_state:"State",p_disabled:"disabled",p_up:"up",p_txgb:"TX good / bad",p_rxgb:"RX good / bad",
p_pkts:"pkts",p_module:"Module",p_temp:"Temperature",p_vcc:"Vcc",p_txbias:"TX bias",p_txpower:"TX power",
p_rxpower:"RX power",p_txfault:"TX fault",p_txdis:"TX disabled",p_rxlos:"RX LOS",p_adv:"Advertising",
pt_title:"Port configuration",pt_conn:"Connected",pt_speed:"Speed",pt_mtu:"MTU",
pt_sfp_note:"SFP speed is set per slot and is not saved in the startup config.",
pt_name_err:"Port name: 1-15 characters, no spaces",pt_mtu_err:"MTU must be 64-16383",
stp_title:"Spanning tree",stp_h:"RSTP, 802.1w",stp_version:"Version",stp_v_stp:"STP compatible",
stp_prio:"Bridge priority",stp_hello:"Hello [s]",stp_maxage:"Max age [s]",stp_fwd:"Forward delay [s]",
stp_txhold:"TX hold",stp_bridge_apply:"Apply bridge settings",
stp_off_msg:"Spanning tree is disabled: all ports forward.",stp_bridge:"Bridge",stp_root:"Root",
stp_root_self:"this bridge is the root",stp_via:"via port",stp_cost:"path cost",stp_tc:"topology changes",
stp_prio_note:"Lower priority wins the root election (default 8 = 32768). Settings apply immediately; use Save to flash to keep them.",
stp_portcfg:"Port configuration",stp_edge:"Edge",stp_pcost:"Path cost",stp_pprio:"Priority",
stp_guard:"Guard",stp_filter:"BPDU filter",stp_p2p:"Point-to-point",
stp_port_note:"Edge ports forward immediately and cause no topology change; Auto treats a port as edge after 3 s without BPDUs, and any received BPDU revokes it. Path cost 0 = automatic.",
stp_status:"Port status",stp_role:"Role",stp_db:"Designated bridge",stp_dp:"Designated port",
stp_dc:"Designated cost",stp_oedge:"Oper. edge",stp_op2p:"Oper. P2P",
stp_s0:"disabled",stp_s1:"blocking",stp_s2:"learning",stp_s3:"forwarding",
stp_r1:"Root",stp_r2:"Designated",stp_r3:"Alternate",stp_trip:"guard tripped",
stp_g_none:"None",stp_g_bpdu:"BPDU",stp_g_root:"Root",
stp_en_q:"Enable spanning tree?",
stp_en_d:"Ports start blocked and take up to twice the forward delay to reach forwarding; edge ports recover immediately.",
stp_dis_q:"Disable spanning tree?",stp_dis_d:"All ports go straight to forwarding; loop protection is lost.",
stp_cost_err:"Path cost must be 0-200000000",
st_title:"Port statistics",st_h:"totals since boot",st_txg:"TX good",st_txb:"TX bad",st_rxg:"RX good",
st_rxb:"RX bad",st_details:"Details",st_counters:"MIB counters",st_nonzero:"non-zero only",
st_autoref:"auto-refresh",st_counter:"Counter",st_value:"Value",st_fail:"failed to load counters",
v_title:"VLANs",v_vid:"VID",v_members:"Members",v_tagged:"Tagged",v_untagged:"Untagged",v_pvid_on:"PVID on",
v_empty:"No VLANs configured.",v_mgmt:"Management VLAN",v_mgmt_none:"untagged",v_editor:"VLAN editor",
v_optional:"optional",v_setmgmt:"Set as mgmt VLAN",v_setmgmt_t:"Make this the management VLAN",
v_member:"Member",v_pvid:"PVID",
v_legend:"U = untagged member, T = tagged member, - = not a member. PVID assigns this VLAN to untagged ingress traffic.",
v_ingress:"Ingress filtering",v_ingress_h:"write-only: the state is not readable from the switch",
v_accept:"Accept",v_ing_all:"All",v_ing_apply:"Apply ingress modes",v_del_t:"Delete VLAN",
v_del_q:"Delete VLAN {n}?",v_del_d:"Ports keep their PVID until reassigned.",
v_vid_err:"Enter a VLAN ID (1-4094)",v_name_err:"Name must start with a letter (letters, digits, _)",
v_loaded:"Loaded VLAN {n}",v_notfound:"VLAN {n} not found (a new one can still be created)",
v_nomember:"Select at least one member port (or delete the VLAN instead)",
v_ing_none:"Pick an ingress mode for at least one port",v_vid_first:"Enter a VLAN ID first",
v_mgmt_q:"Set VLAN {n} as management VLAN?",
v_mgmt_d:"The switch will start tagging its own traffic with that VLAN. If the port you are connected through does not carry it, this page becomes unreachable and the setting can only be undone over the console.",
l2_title:"MAC address table",l2_filter:"filter...",l2_flush:"Flush learned entries",l2_static:"static",
l2_learned:"learned",l2_loading:"loading...",l2_failed:"load failed",l2_entries:"entries",
l2_del_t:"Delete entry",l2_flush_q:"Flush all learned MAC entries?",
m_title:"Port mirroring",m_active:"active",m_monitor:"Monitor port",m_mirror:"Mirror",m_both:"Both",
m_note:"Both = mirror RX and TX of the port to the monitor port.",m_none:"Select at least one mirrored port",
lag_hash:"Hash:",lag_note:"A LAG needs at least one member to be saved in the startup config; applying an empty group clears it.",
lag_clear_q:"Clear LAG {n}?",lag_clear_d:"All member ports return to normal operation.",
e_title:"Energy Efficient Ethernet",e_adv:"Advertised",e_lp:"Link partner",e_active:"Active",e_enable:"Enable",
e_note:"Advertised and link-partner flags per speed: 100M, 1G, 2.5G. SFP ports do not support EEE.",
e_idle:"idle",e_na:"n/a",
bw_title:"Bandwidth limits",bw_h:"Mbit/s, 0.016-10000",bw_in:"Ingress limit",bw_out:"Egress limit",
bw_exceed:"When exceeded",bw_fc:"Flow control",bw_drop:"Drop",
bw_in_err:"Ingress limit must be 0.016-10000 Mbit/s",bw_out_err:"Egress limit must be 0.016-10000 Mbit/s",
sy_network:"Network",sy_dhcp:"Use DHCP",sy_dhcp_t:"Request address via DHCP",sy_services:"Services",
sy_igmp:"IGMP snooping",sy_sysip:"server IP",sy_server:"Server",sy_port:"Port",
sy_services_note:"Service state reflects the startup config; runtime state is not readable.",
sy_password:"Admin password",sy_newpw:"New password",sy_repeat:"Repeat",sy_pwapply:"Change password",
sy_pw_note:"Takes effect immediately; save to flash to keep it after reboot.",sy_console:"Console",
sy_cmd:"CLI command...",sy_send:"Send",sy_startup:"Startup configuration",sy_replayed:"replayed on every boot",
sy_reload:"Reload from flash",sy_write:"Write to flash",sy_unknown_note:"Unknown lines are highlighted before writing.",
sy_maint:"Maintenance",sy_reboot:"Reboot switch",sy_lang:"Language",
sy_ip_err:"Invalid IP / netmask / gateway",sy_host_err:"Hostname: 1-23 printable characters, no spaces or quotes",
sy_net_q:"Apply network settings?",sy_net_d:"The management IP changes to {ip}: this page will need to be reopened there.",
sy_ip_changed:"IP changed, reconnect at http://{ip}/",sy_dhcp_q:"Switch to DHCP?",
sy_dhcp_d:"The switch requests an address via DHCP. You must find its new IP to reconnect.",
sy_sysip_err:"Invalid syslog server IP",sy_sysport_err:"Syslog port must be 1-65535",
sy_pw_len:"Password: 1-20 characters",sy_pw_space:"Password cannot contain spaces",sy_pw_match:"Passwords do not match",
sy_pw_q:"Change admin password?",sy_pw_d:"Takes effect immediately for new logins. Save to flash to persist.",
sy_reboot_q:"Reboot the switch?",sy_reboot_d:"There are UNSAVED changes and they will be lost. Save to flash first if you want to keep them.",
sy_rebooting:"Rebooting, reconnect in about 20 s",sy_bytes:"bytes",
cw_title:"Write this startup configuration?",cw_save_title:"Save running configuration to flash?",
cw_info:"{n} / 2048 bytes, replayed line by line on every boot",
cw_toolarge:"Too large: the config sector accepts at most 2048 bytes. Remove lines first.",
cw_unknown:"Not in the known config grammar (will still be written): ",cw_empty:"(empty)",
cw_writing:"Writing configuration...",cw_failed:"config write failed: HTTP {n}",
cw_verify_fail:"Verification failed: flash content differs from what was sent. Command log NOT cleared.",
cw_saved:"Startup configuration saved and verified",cw_collect:"Collecting running changes...",
t_applied:"Applied: {c}",t_cmds:"{n} command(s) applied",t_rejected:"command rejected: {c}",t_failed:"failed: {c}",
fw_title:"Firmware update",
fw_intro:"Upload an RTLPlayground image (512 KiB .bin). The image is staged to flash and CRC-checked; on success the switch reboots itself, re-verifies the image and applies it. The startup configuration is preserved.",
fw_upload:"Upload",fw_checking:"{f}: {n} bytes. Checking...",fw_size_err:"size is {n}, expected 524288 (512 KiB)",
fw_magic_err:"missing bank header / LJMP magic: not a firmware image",fw_crc_err:"CRC16 check failed: corrupt image",
fw_valid:"valid RTLPlayground image (size, magic and CRC16 OK)",fw_q:"Upload firmware?",
fw_d:"On a good checksum the switch resets itself and applies the image during boot (the startup config is preserved). Do not power it off until it comes back.",
fw_finishing:"finishing flash write... {s} s",fw_uploading:"uploading... {p}% / {s} s",
fw_verified:"checksum verified, the switch is rebooting...",
fw_rejected:"the switch rejected the image (bad checksum), nothing was applied",
fw_lost:"upload failed: connection lost mid-transfer",
fw_noreboot:"no reboot detected: the image was most likely rejected. If updating from old firmware, verify the version in the sidebar after logging in again.",
fw_applied:"update applied",fw_done_t:"Firmware updated",
fw_done:"The switch verified the image and rebooted into it. The session was reset, so you will be asked to log in again.",
fw_login:"Go to login",fw_rebooting:"switch is rebooting...",
fw_timeout:"switch has not come back after 150 s: check power / serial console"
},
ja:{
nav_dash:"ダッシュボード",nav_ports:"ポート",nav_stp:"スパニングツリー",nav_stats:"統計",
nav_vlan:"VLAN",nav_l2:"MAC テーブル",nav_mirror:"ミラーリング",nav_lag:"LAG",nav_eee:"EEE",
nav_bw:"帯域制限",nav_system:"システム",nav_fw:"ファームウェア",
hdr_dirty:"未保存の変更",hdr_dirty_t:"実行中の設定が起動設定と異なります",
hdr_save:"フラッシュに保存",hdr_save_t:"実行中の設定をフラッシュに書き込む",
th_auto:"テーマ: システム",th_auto_sel:"テーマ: システム (Selenized)",th_light:"ライト",th_dark:"ダーク",
th_sel_light:"Selenized ライト",th_sel_dark:"Selenized ダーク",
c_port:"ポート",c_name:"名前",c_link:"リンク",c_apply:"適用",c_cancel:"キャンセル",c_confirm:"確認",
c_close:"閉じる",c_refresh:"更新",c_off:"オフ",c_offc:"オフ",c_down:"リンクダウン",c_on:"オン",c_auto:"自動",c_delete:"削除",
c_type:"タイプ",c_state:"状態",c_enabled:"有効",c_load:"読み込み",c_disable:"無効化",c_sfp:"SFP",
c_full:"全二重",c_half:"半二重",c_devices:"デバイス",c_yes:"はい",c_no:"いいえ",
d_ports:"ポート",d_ports_h:"ポートをクリックすると詳細を表示",d_system:"システム",d_traffic:"トラフィック",
d_traffic_h:"パケット/秒、リアルタイム",d_txpps:"TX pps",d_rxpps:"RX pps",d_txbad:"TX 異常",d_rxbad:"RX 異常",
i_host:"ホスト名",i_ip:"IP アドレス",i_mask:"ネットマスク",i_gw:"ゲートウェイ",i_mac:"MAC",i_fw:"ファームウェア",
i_built:"ビルド日",i_hw:"ハードウェア",i_flash:"フラッシュ",i_syslog:"Syslog",
p_state:"状態",p_disabled:"無効",p_up:"アップ",p_txgb:"TX 正常 / 異常",p_rxgb:"RX 正常 / 異常",
p_pkts:"pkts",p_module:"モジュール",p_temp:"温度",p_vcc:"電圧",p_txbias:"TX バイアス",p_txpower:"TX 電力",
p_rxpower:"RX 電力",p_txfault:"TX 障害",p_txdis:"TX 無効",p_rxlos:"RX 信号ロス",p_adv:"アドバタイズ",
pt_title:"ポート設定",pt_conn:"接続デバイス",pt_speed:"速度",pt_mtu:"MTU",
pt_sfp_note:"SFP の速度はスロット単位で設定され、起動設定には保存されません。",
pt_name_err:"ポート名: 1〜15 文字、空白不可",pt_mtu_err:"MTU は 64〜16383 の範囲で指定してください",
stp_title:"スパニングツリー",stp_h:"RSTP, 802.1w",stp_version:"バージョン",stp_v_stp:"STP 互換",
stp_prio:"ブリッジ優先度",stp_hello:"Hello [秒]",stp_maxage:"Max age [秒]",stp_fwd:"Forward delay [秒]",
stp_txhold:"TX hold",stp_bridge_apply:"ブリッジ設定を適用",
stp_off_msg:"スパニングツリーは無効です: すべてのポートが転送状態です。",stp_bridge:"ブリッジ",stp_root:"ルート",
stp_root_self:"このブリッジがルートです",stp_via:"経由ポート",stp_cost:"パスコスト",stp_tc:"トポロジ変更回数",
stp_prio_note:"優先度の値が小さいブリッジがルートに選出されます (既定 8 = 32768)。設定は即時に反映されます。保持するにはフラッシュに保存してください。",
stp_portcfg:"ポート設定",stp_edge:"エッジ",stp_pcost:"パスコスト",stp_pprio:"優先度",
stp_guard:"ガード",stp_filter:"BPDU フィルタ",stp_p2p:"ポイントツーポイント",
stp_port_note:"エッジポートは即時に転送を開始し、トポロジ変更を発生させません。自動では BPDU を 3 秒間受信しないとエッジと見なし、BPDU を受信すると解除されます。パスコスト 0 = 自動。",
stp_status:"ポート状態",stp_role:"ロール",stp_db:"指定ブリッジ",stp_dp:"指定ポート",
stp_dc:"指定コスト",stp_oedge:"動作エッジ",stp_op2p:"動作 P2P",
stp_s0:"無効",stp_s1:"ブロッキング",stp_s2:"学習中",stp_s3:"転送中",
stp_r1:"ルート",stp_r2:"指定",stp_r3:"代替",stp_trip:"ガード作動",
stp_g_none:"なし",stp_g_bpdu:"BPDU",stp_g_root:"ルート",
stp_en_q:"スパニングツリーを有効にしますか?",
stp_en_d:"ポートはブロック状態から始まり、転送状態になるまで最大で forward delay の 2 倍の時間がかかります。エッジポートは即時に復帰します。",
stp_dis_q:"スパニングツリーを無効にしますか?",stp_dis_d:"すべてのポートが直ちに転送状態になり、ループ保護が失われます。",
stp_cost_err:"パスコストは 0〜200000000 の範囲で指定してください",
st_title:"ポート統計",st_h:"起動からの累計",st_txg:"TX 正常",st_txb:"TX 異常",st_rxg:"RX 正常",
st_rxb:"RX 異常",st_details:"詳細",st_counters:"MIB カウンタ",st_nonzero:"0 以外のみ",
st_autoref:"自動更新",st_counter:"カウンタ",st_value:"値",st_fail:"カウンタの取得に失敗しました",
v_title:"VLAN",v_vid:"VID",v_members:"メンバー",v_tagged:"タグ付き",v_untagged:"タグ無し",v_pvid_on:"PVID",
v_empty:"VLAN は設定されていません。",v_mgmt:"管理 VLAN",v_mgmt_none:"タグ無し",v_editor:"VLAN エディタ",
v_optional:"任意",v_setmgmt:"管理 VLAN に設定",v_setmgmt_t:"この VLAN を管理 VLAN にする",
v_member:"メンバー",v_pvid:"PVID",
v_legend:"U = タグ無しメンバー、T = タグ付きメンバー、- = 非メンバー。PVID はタグ無し受信トラフィックをこの VLAN に割り当てます。",
v_ingress:"受信フィルタ",v_ingress_h:"書き込み専用: 現在の状態はスイッチから読み取れません",
v_accept:"受け入れ",v_ing_all:"すべて",v_ing_apply:"受信モードを適用",v_del_t:"VLAN を削除",
v_del_q:"VLAN {n} を削除しますか?",v_del_d:"ポートの PVID は再割り当てされるまで維持されます。",
v_vid_err:"VLAN ID (1〜4094) を入力してください",v_name_err:"名前は英字で始めてください (英字、数字、_)",
v_loaded:"VLAN {n} を読み込みました",v_notfound:"VLAN {n} は存在しません (新規作成は可能です)",
v_nomember:"メンバーポートを 1 つ以上選択してください (または VLAN を削除してください)",
v_ing_none:"1 つ以上のポートの受信モードを選択してください",v_vid_first:"先に VLAN ID を入力してください",
v_mgmt_q:"VLAN {n} を管理 VLAN に設定しますか?",
v_mgmt_d:"スイッチは自身のトラフィックにこの VLAN のタグを付けるようになります。接続中のポートがこの VLAN を通さない場合、このページにアクセスできなくなり、コンソールからしか元に戻せません。",
l2_title:"MAC アドレステーブル",l2_filter:"フィルタ...",l2_flush:"学習エントリを消去",l2_static:"静的",
l2_learned:"学習",l2_loading:"読み込み中...",l2_failed:"読み込み失敗",l2_entries:"エントリ",
l2_del_t:"エントリを削除",l2_flush_q:"学習した MAC エントリをすべて消去しますか?",
m_title:"ポートミラーリング",m_active:"有効",m_monitor:"モニタポート",m_mirror:"ミラー",m_both:"両方",
m_note:"両方 = ポートの RX と TX をモニタポートにミラーします。",m_none:"ミラー元ポートを 1 つ以上選択してください",
lag_hash:"ハッシュ:",lag_note:"起動設定に保存するには LAG に 1 つ以上のメンバーが必要です。メンバーなしで適用するとグループを解除します。",
lag_clear_q:"LAG {n} を解除しますか?",lag_clear_d:"すべてのメンバーポートが通常動作に戻ります。",
e_title:"Energy Efficient Ethernet",e_adv:"アドバタイズ",e_lp:"リンクパートナー",e_active:"有効",e_enable:"有効化",
e_note:"速度ごとのアドバタイズ/リンクパートナーのフラグ: 100M、1G、2.5G。SFP ポートは EEE に対応していません。",
e_idle:"アイドル",e_na:"n/a",
bw_title:"帯域制限",bw_h:"Mbit/s、0.016〜10000",bw_in:"入力制限",bw_out:"出力制限",
bw_exceed:"超過時の動作",bw_fc:"フロー制御",bw_drop:"破棄",
bw_in_err:"入力制限は 0.016〜10000 Mbit/s の範囲で指定してください",bw_out_err:"出力制限は 0.016〜10000 Mbit/s の範囲で指定してください",
sy_network:"ネットワーク",sy_dhcp:"DHCP を使用",sy_dhcp_t:"DHCP でアドレスを取得",sy_services:"サービス",
sy_igmp:"IGMP スヌーピング",sy_sysip:"サーバー IP",sy_server:"サーバー",sy_port:"ポート",
sy_services_note:"サービスの状態は起動設定を反映しています。実行時の状態は読み取れません。",
sy_password:"管理者パスワード",sy_newpw:"新しいパスワード",sy_repeat:"再入力",sy_pwapply:"パスワードを変更",
sy_pw_note:"即時に反映されます。再起動後も保持するにはフラッシュに保存してください。",sy_console:"コンソール",
sy_cmd:"CLI コマンド...",sy_send:"送信",sy_startup:"起動設定",sy_replayed:"起動のたびに再実行されます",
sy_reload:"フラッシュから再読み込み",sy_write:"フラッシュに書き込み",sy_unknown_note:"不明な行は書き込み前に強調表示されます。",
sy_maint:"メンテナンス",sy_reboot:"スイッチを再起動",sy_lang:"言語",
sy_ip_err:"IP / ネットマスク / ゲートウェイが無効です",sy_host_err:"ホスト名: 1〜23 文字の印字可能文字、空白と引用符は不可",
sy_net_q:"ネットワーク設定を適用しますか?",sy_net_d:"管理 IP が {ip} に変わります。このページは新しいアドレスで開き直す必要があります。",
sy_ip_changed:"IP を変更しました。http://{ip}/ に再接続してください",sy_dhcp_q:"DHCP に切り替えますか?",
sy_dhcp_d:"スイッチは DHCP でアドレスを要求します。再接続するには新しい IP を確認する必要があります。",
sy_sysip_err:"syslog サーバー IP が無効です",sy_sysport_err:"syslog ポートは 1〜65535 の範囲で指定してください",
sy_pw_len:"パスワード: 1〜20 文字",sy_pw_space:"パスワードに空白は使えません",sy_pw_match:"パスワードが一致しません",
sy_pw_q:"管理者パスワードを変更しますか?",sy_pw_d:"新しいログインから即時に有効になります。保持するにはフラッシュに保存してください。",
sy_reboot_q:"スイッチを再起動しますか?",sy_reboot_d:"未保存の変更があり、失われます。保持したい場合は先にフラッシュに保存してください。",
sy_rebooting:"再起動中です。約 20 秒後に再接続してください",sy_bytes:"バイト",
cw_title:"この起動設定を書き込みますか?",cw_save_title:"実行中の設定をフラッシュに保存しますか?",
cw_info:"{n} / 2048 バイト、起動のたびに 1 行ずつ実行されます",
cw_toolarge:"サイズ超過: 設定セクタは最大 2048 バイトです。先に行を削除してください。",
cw_unknown:"既知の設定文法に含まれない行 (そのまま書き込まれます): ",cw_empty:"(空)",
cw_writing:"設定を書き込み中...",cw_failed:"設定の書き込みに失敗しました: HTTP {n}",
cw_verify_fail:"検証失敗: フラッシュの内容が送信内容と異なります。コマンドログは消去されていません。",
cw_saved:"起動設定を保存し検証しました",cw_collect:"実行中の変更を収集中...",
t_applied:"適用: {c}",t_cmds:"{n} 件のコマンドを適用しました",t_rejected:"コマンドが拒否されました: {c}",t_failed:"失敗: {c}",
fw_title:"ファームウェア更新",
fw_intro:"RTLPlayground イメージ (512 KiB の .bin) をアップロードします。イメージはフラッシュに書き込まれ CRC を検証され、成功するとスイッチが自動的に再起動して再検証のうえ適用します。起動設定は保持されます。",
fw_upload:"アップロード",fw_checking:"{f}: {n} バイト。確認中...",fw_size_err:"サイズが {n} です。524288 (512 KiB) が必要です",
fw_magic_err:"バンクヘッダ / LJMP マジックがありません: ファームウェアイメージではありません",fw_crc_err:"CRC16 検証に失敗しました: イメージが破損しています",
fw_valid:"有効な RTLPlayground イメージです (サイズ、マジック、CRC16 OK)",fw_q:"ファームウェアをアップロードしますか?",
fw_d:"チェックサムが正しければスイッチは自動的にリセットし、起動時にイメージを適用します (起動設定は保持されます)。復帰するまで電源を切らないでください。",
fw_finishing:"フラッシュ書き込みを完了中... {s} 秒",fw_uploading:"アップロード中... {p}% / {s} 秒",
fw_verified:"チェックサム検証済み。スイッチを再起動しています...",
fw_rejected:"スイッチがイメージを拒否しました (チェックサム不正)。何も適用されていません",
fw_lost:"アップロード失敗: 転送中に接続が切れました",
fw_noreboot:"再起動が検出されませんでした。イメージは拒否された可能性が高いです。旧ファームウェアからの更新の場合は、再ログイン後にサイドバーのバージョンを確認してください。",
fw_applied:"更新を適用しました",fw_done_t:"ファームウェアを更新しました",
fw_done:"スイッチはイメージを検証し、新しいイメージで再起動しました。セッションがリセットされたため、再度ログインが必要です。",
fw_login:"ログインへ",fw_rebooting:"スイッチを再起動中...",
fw_timeout:"150 秒経ってもスイッチが復帰しません。電源 / シリアルコンソールを確認してください"
},
zh:{
nav_dash:"仪表盘",nav_ports:"端口",nav_stp:"生成树",nav_stats:"统计",
nav_vlan:"VLAN",nav_l2:"MAC 表",nav_mirror:"端口镜像",nav_lag:"LAG",nav_eee:"EEE",
nav_bw:"带宽限制",nav_system:"系统",nav_fw:"固件",
hdr_dirty:"未保存的更改",hdr_dirty_t:"运行配置与启动配置不同",
hdr_save:"保存到 Flash",hdr_save_t:"将运行配置写入 Flash",
th_auto:"主题: 跟随系统",th_auto_sel:"主题: 跟随系统 (Selenized)",th_light:"浅色",th_dark:"深色",
th_sel_light:"Selenized 浅色",th_sel_dark:"Selenized 深色",
c_port:"端口",c_name:"名称",c_link:"链路",c_apply:"应用",c_cancel:"取消",c_confirm:"确认",
c_close:"关闭",c_refresh:"刷新",c_off:"关",c_offc:"关",c_down:"未连接",c_on:"开",c_auto:"自动",c_delete:"删除",
c_type:"类型",c_state:"状态",c_enabled:"启用",c_load:"读取",c_disable:"禁用",c_sfp:"SFP",
c_full:"全双工",c_half:"半双工",c_devices:"台设备",c_yes:"是",c_no:"否",
d_ports:"端口",d_ports_h:"点击端口查看详情",d_system:"系统",d_traffic:"流量",
d_traffic_h:"包/秒，实时",d_txpps:"TX pps",d_rxpps:"RX pps",d_txbad:"TX 错误",d_rxbad:"RX 错误",
i_host:"主机名",i_ip:"IP 地址",i_mask:"子网掩码",i_gw:"网关",i_mac:"MAC",i_fw:"固件",
i_built:"构建日期",i_hw:"硬件",i_flash:"Flash",i_syslog:"Syslog",
p_state:"状态",p_disabled:"已禁用",p_up:"已连接",p_txgb:"TX 正常 / 错误",p_rxgb:"RX 正常 / 错误",
p_pkts:"个包",p_module:"模块",p_temp:"温度",p_vcc:"供电电压",p_txbias:"TX 偏置电流",p_txpower:"TX 光功率",
p_rxpower:"RX 光功率",p_txfault:"TX 故障",p_txdis:"TX 禁用",p_rxlos:"RX 信号丢失",p_adv:"通告能力",
pt_title:"端口配置",pt_conn:"已连接设备",pt_speed:"速率",pt_mtu:"MTU",
pt_sfp_note:"SFP 速率按插槽设置，不会保存到启动配置。",
pt_name_err:"端口名称: 1-15 个字符，不能包含空格",pt_mtu_err:"MTU 范围为 64-16383",
stp_title:"生成树",stp_h:"RSTP, 802.1w",stp_version:"版本",stp_v_stp:"STP 兼容",
stp_prio:"网桥优先级",stp_hello:"Hello [秒]",stp_maxage:"Max age [秒]",stp_fwd:"Forward delay [秒]",
stp_txhold:"TX hold",stp_bridge_apply:"应用网桥设置",
stp_off_msg:"生成树已禁用: 所有端口均为转发状态。",stp_bridge:"网桥",stp_root:"根桥",
stp_root_self:"本网桥为根桥",stp_via:"经由端口",stp_cost:"路径开销",stp_tc:"拓扑变更次数",
stp_prio_note:"优先级数值越小越容易被选为根桥 (默认 8 = 32768)。设置立即生效，如需保留请保存到 Flash。",
stp_portcfg:"端口配置",stp_edge:"边缘端口",stp_pcost:"路径开销",stp_pprio:"优先级",
stp_guard:"保护",stp_filter:"BPDU 过滤",stp_p2p:"点对点",
stp_port_note:"边缘端口立即转发且不触发拓扑变更；自动模式下 3 秒未收到 BPDU 即视为边缘端口，收到 BPDU 后立即撤销。路径开销 0 = 自动。",
stp_status:"端口状态",stp_role:"角色",stp_db:"指定网桥",stp_dp:"指定端口",
stp_dc:"指定开销",stp_oedge:"实际边缘",stp_op2p:"实际 P2P",
stp_s0:"禁用",stp_s1:"阻塞",stp_s2:"学习",stp_s3:"转发",
stp_r1:"根端口",stp_r2:"指定端口",stp_r3:"替代端口",stp_trip:"保护触发",
stp_g_none:"无",stp_g_bpdu:"BPDU",stp_g_root:"根桥",
stp_en_q:"启用生成树?",
stp_en_d:"端口将从阻塞状态开始，最多需要两倍 forward delay 的时间才能进入转发状态；边缘端口立即恢复。",
stp_dis_q:"禁用生成树?",stp_dis_d:"所有端口将直接进入转发状态，失去环路保护。",
stp_cost_err:"路径开销范围为 0-200000000",
st_title:"端口统计",st_h:"自启动以来的累计值",st_txg:"TX 正常",st_txb:"TX 错误",st_rxg:"RX 正常",
st_rxb:"RX 错误",st_details:"详情",st_counters:"MIB 计数器",st_nonzero:"仅显示非零",
st_autoref:"自动刷新",st_counter:"计数器",st_value:"值",st_fail:"读取计数器失败",
v_title:"VLAN",v_vid:"VID",v_members:"成员",v_tagged:"Tagged",v_untagged:"Untagged",v_pvid_on:"PVID",
v_empty:"尚未配置 VLAN。",v_mgmt:"管理 VLAN",v_mgmt_none:"untagged",v_editor:"VLAN 编辑器",
v_optional:"可选",v_setmgmt:"设为管理 VLAN",v_setmgmt_t:"将此 VLAN 设为管理 VLAN",
v_member:"成员",v_pvid:"PVID",
v_legend:"U = untagged 成员，T = tagged 成员，- = 非成员。PVID 将入方向的未标记流量归入此 VLAN。",
v_ingress:"入方向过滤",v_ingress_h:"只写: 当前状态无法从交换机读取",
v_accept:"接受",v_ing_all:"全部",v_ing_apply:"应用入方向模式",v_del_t:"删除 VLAN",
v_del_q:"删除 VLAN {n}?",v_del_d:"端口的 PVID 在重新分配前保持不变。",
v_vid_err:"请输入 VLAN ID (1-4094)",v_name_err:"名称必须以字母开头 (字母、数字、_)",
v_loaded:"已读取 VLAN {n}",v_notfound:"VLAN {n} 不存在 (仍可新建)",
v_nomember:"请至少选择一个成员端口 (或改为删除该 VLAN)",
v_ing_none:"请至少为一个端口选择入方向模式",v_vid_first:"请先输入 VLAN ID",
v_mgmt_q:"将 VLAN {n} 设为管理 VLAN?",
v_mgmt_d:"交换机将开始为自身流量打上该 VLAN 标签。如果您所连接的端口不属于该 VLAN，本页面将无法访问，且只能通过控制台撤销此设置。",
l2_title:"MAC 地址表",l2_filter:"筛选...",l2_flush:"清除学习条目",l2_static:"静态",
l2_learned:"动态学习",l2_loading:"加载中...",l2_failed:"加载失败",l2_entries:"条",
l2_del_t:"删除条目",l2_flush_q:"清除所有动态学习的 MAC 条目?",
m_title:"端口镜像",m_active:"已启用",m_monitor:"镜像目的端口",m_mirror:"镜像",m_both:"双向",
m_note:"双向 = 将该端口的 RX 和 TX 都镜像到目的端口。",m_none:"请至少选择一个被镜像端口",
lag_hash:"哈希:",lag_note:"LAG 至少需要一个成员才能保存到启动配置；应用空组将清除该组。",
lag_clear_q:"清除 LAG {n}?",lag_clear_d:"所有成员端口恢复正常工作。",
e_title:"节能以太网 (EEE)",e_adv:"本端通告",e_lp:"链路伙伴",e_active:"已生效",e_enable:"启用",
e_note:"按速率显示本端/链路伙伴的通告标志: 100M、1G、2.5G。SFP 端口不支持 EEE。",
e_idle:"空闲",e_na:"n/a",
bw_title:"带宽限制",bw_h:"Mbit/s，0.016-10000",bw_in:"入方向限速",bw_out:"出方向限速",
bw_exceed:"超限动作",bw_fc:"流量控制",bw_drop:"丢弃",
bw_in_err:"入方向限速范围为 0.016-10000 Mbit/s",bw_out_err:"出方向限速范围为 0.016-10000 Mbit/s",
sy_network:"网络",sy_dhcp:"使用 DHCP",sy_dhcp_t:"通过 DHCP 获取地址",sy_services:"服务",
sy_igmp:"IGMP 侦听",sy_sysip:"服务器 IP",sy_server:"服务器",sy_port:"端口",
sy_services_note:"服务状态反映启动配置；运行时状态无法读取。",
sy_password:"管理员密码",sy_newpw:"新密码",sy_repeat:"重复输入",sy_pwapply:"修改密码",
sy_pw_note:"立即生效；如需重启后保留请保存到 Flash。",sy_console:"控制台",
sy_cmd:"CLI 命令...",sy_send:"发送",sy_startup:"启动配置",sy_replayed:"每次启动时重新执行",
sy_reload:"从 Flash 重新读取",sy_write:"写入 Flash",sy_unknown_note:"未知行会在写入前高亮显示。",
sy_maint:"维护",sy_reboot:"重启交换机",sy_lang:"语言",
sy_ip_err:"IP / 子网掩码 / 网关无效",sy_host_err:"主机名: 1-23 个可打印字符，不能包含空格或引号",
sy_net_q:"应用网络设置?",sy_net_d:"管理 IP 将变为 {ip}，需要使用新地址重新打开本页面。",
sy_ip_changed:"IP 已更改，请访问 http://{ip}/ 重新连接",sy_dhcp_q:"切换到 DHCP?",
sy_dhcp_d:"交换机将通过 DHCP 请求地址。您需要查到新的 IP 才能重新连接。",
sy_sysip_err:"syslog 服务器 IP 无效",sy_sysport_err:"syslog 端口范围为 1-65535",
sy_pw_len:"密码: 1-20 个字符",sy_pw_space:"密码不能包含空格",sy_pw_match:"两次输入的密码不一致",
sy_pw_q:"修改管理员密码?",sy_pw_d:"对新的登录立即生效。如需保留请保存到 Flash。",
sy_reboot_q:"重启交换机?",sy_reboot_d:"存在未保存的更改，重启后将丢失。如需保留请先保存到 Flash。",
sy_rebooting:"正在重启，约 20 秒后重新连接",sy_bytes:"字节",
cw_title:"写入此启动配置?",cw_save_title:"将运行配置保存到 Flash?",
cw_info:"{n} / 2048 字节，每次启动时逐行执行",
cw_toolarge:"过大: 配置扇区最多容纳 2048 字节。请先删除部分行。",
cw_unknown:"不属于已知配置语法的行 (仍会写入): ",cw_empty:"(空)",
cw_writing:"正在写入配置...",cw_failed:"配置写入失败: HTTP {n}",
cw_verify_fail:"校验失败: Flash 内容与发送内容不一致。命令日志未清除。",
cw_saved:"启动配置已保存并校验",cw_collect:"正在收集运行中的更改...",
t_applied:"已应用: {c}",t_cmds:"已应用 {n} 条命令",t_rejected:"命令被拒绝: {c}",t_failed:"失败: {c}",
fw_title:"固件升级",
fw_intro:"上传 RTLPlayground 镜像 (512 KiB 的 .bin)。镜像会写入 Flash 并进行 CRC 校验；成功后交换机自动重启，再次校验并应用镜像。启动配置将被保留。",
fw_upload:"上传",fw_checking:"{f}: {n} 字节。正在检查...",fw_size_err:"大小为 {n}，应为 524288 (512 KiB)",
fw_magic_err:"缺少 bank 头 / LJMP 魔数: 不是固件镜像",fw_crc_err:"CRC16 校验失败: 镜像已损坏",
fw_valid:"有效的 RTLPlayground 镜像 (大小、魔数和 CRC16 正确)",fw_q:"上传固件?",
fw_d:"校验和正确时交换机会自动复位并在启动时应用镜像 (启动配置保留)。在其恢复前请勿断电。",
fw_finishing:"正在完成 Flash 写入... {s} 秒",fw_uploading:"上传中... {p}% / {s} 秒",
fw_verified:"校验和通过，交换机正在重启...",
fw_rejected:"交换机拒绝了该镜像 (校验和错误)，未做任何更改",
fw_lost:"上传失败: 传输中连接断开",
fw_noreboot:"未检测到重启: 镜像很可能被拒绝。若从旧固件升级，请重新登录后在侧边栏确认版本。",
fw_applied:"升级已应用",fw_done_t:"固件已升级",
fw_done:"交换机已校验镜像并以新镜像重启。会话已重置，需要重新登录。",
fw_login:"前往登录",fw_rebooting:"交换机正在重启...",
fw_timeout:"150 秒后交换机仍未恢复: 请检查电源 / 串口控制台"
}
};
var rtlLang=(function(){
  var s=null;
  try{s=localStorage.getItem("rtl_lang");}catch(e){}
  if(s&&LANG[s])return s;
  var b=(navigator.language||"en").slice(0,2);
  return LANG[b]?b:"en";
})();
function t(k,v){
  var s=LANG[rtlLang][k]||LANG.en[k]||k;
  if(v)for(var x in v)s=s.split("{"+x+"}").join(v[x]);
  return s;
}
function i18nApply(){
  document.querySelectorAll("[data-i18n]").forEach(function(el){el.textContent=t(el.getAttribute("data-i18n"))});
  document.querySelectorAll("[data-i18n-t]").forEach(function(el){el.title=t(el.getAttribute("data-i18n-t"))});
  document.querySelectorAll("[data-i18n-p]").forEach(function(el){el.placeholder=t(el.getAttribute("data-i18n-p"))});
}

var S={
  ports:[],n:0,physToLog:[],logToPhys:[],sfpSlot:[],info:{},
  dirty:false,prev:null,prevT:0,rates:[],mtu:[],
};
var LINKS=["Down","10M","100M","1000M","500M","10G","2.5G","5G"];
var LINKC=[null,"--s10","--s100","--s1000","--s5g","--s10g","--s2g5","--s5g"];
var $=function(id){return document.getElementById(id)};
function h(tag,attrs,kids){
  var e=document.createElement(tag);
  if(attrs)for(var k in attrs){
    if(k==="text")e.textContent=attrs[k];
    else if(k==="html")e.innerHTML=attrs[k];
    else if(k.slice(0,2)==="on")e.addEventListener(k.slice(2),attrs[k]);
    else e.setAttribute(k,attrs[k]);
  }
  if(kids)kids.forEach(function(c){e.appendChild(c)});
  return e;
}
function esc(s){return String(s).replace(/[&<>"]/g,function(c){return{"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]})}
function badge(txt,cls){return'<span class="badge'+(cls?" "+cls:"")+'">'+esc(txt)+"</span>"}
function linkBadge(p){
  return!p.enabled?badge(t("c_off")):(p.link>0?badge(LINKS[p.link],"ok"):badge(t("c_down")));
}

function applyTheme(){
  var pref;
  try{pref=localStorage.getItem("theme")||"auto";}catch(e){pref="auto";}
  var dark=window.matchMedia("(prefers-color-scheme: dark)").matches;
  var th=pref;
  if(pref==="auto")th=dark?"dark":"light";
  if(pref==="auto-sel")th=dark?"sel-dark":"sel-light";
  document.documentElement.dataset.theme=th;
  $("themeSel").value=pref;
}
$("themeSel").addEventListener("change",function(){
  try{localStorage.setItem("theme",this.value);}catch(e){}
  applyTheme();
});
window.matchMedia("(prefers-color-scheme: dark)").addEventListener("change",applyTheme);
applyTheme();
$("langSel").value=rtlLang;
$("langSel").addEventListener("change",function(){
  try{localStorage.setItem("rtl_lang",this.value);}catch(e){}
  location.reload();
});
i18nApply();

/* The firmware shares one output buffer across connections and delimits
 * responses by closing, so the next request must wait for the previous
 * BODY, not just its headers. */
var _q=Promise.resolve();
function api(path,opts){
  return new Promise(function(resolve,reject){
    _q=_q.then(function(){
      opts=opts||{};
      var ctl=("AbortController"in window)?new AbortController():null;
      if(ctl)opts.signal=ctl.signal;
      var to=setTimeout(function(){if(ctl)ctl.abort()},10000);
      return fetch(path,opts).then(function(r){
        if(r.status===401){clearTimeout(to);location.href="/login.html";throw new Error("auth");}
        return r.text().then(function(body){
          clearTimeout(to);
          resolve({ok:r.ok,status:r.status,body:body});
        });
      }).catch(function(e){clearTimeout(to);reject(e)});
    });
  });
}
function getJSON(p){return api(p).then(function(r){if(!r.ok)throw new Error(p+" "+r.status);return JSON.parse(r.body)})}
function getText(p){return api(p).then(function(r){if(!r.ok)throw new Error(p+" "+r.status);return r.body})}

var CONF_CMDS=[
  /^ip\s+(\d{1,3}\.){3}\d{1,3}$/,/^ip\s+dhcp$/,
  /^gw\s+(\d{1,3}\.){3}\d{1,3}$/,/^netmask\s+(\d{1,3}\.){3}\d{1,3}$/,
  /^syslog\s+(on|off)$/,/^syslog\s+ip\s+(\d{1,3}\.){3}\d{1,3}$/,/^syslog\s+port\s+\d{1,5}$/,
  /^passwd\s+\S+$/,/^hostname\s+\S{1,23}$/,
  /^vlan\s+\d{1,4}\s+d$/,/^vlan\s+\d{1,4}\s+mgmt$/,
  /^vlan\s+\d{1,4}(\s+[a-zA-Z]\w*)?(\s+\d{1,2}t?)+$/,
  /^pvid\s+\d{1,2}\s+\d{1,4}$/,
  /^ingress(\s+\d{1,2}[tua])+$/,/^ingress\s+[tua]$/,
  /^port\s+\d{1,2}\s+(10m|100m|1g|2g5|5g|10g|auto|on|off)(\s+(half|full))?$/,
  /^port\s+\d{1,2}\s+name\s+\S+$/,
  /^eee(\s+\d{1,2})?\s+(on|off)$/,
  /^mirror(\s+\d{1,2})(\s+\d{1,2}[tr]?)+$/,/^mirror\s+off$/,
  /^lag\s+[1-4](\s+\d{1,2})+$/,/^lag\s+[1-4]\s+d$/,/^laghash\s+[1-4](\s+\w+)+$/,
  /^isolate\s+\d{1,2}(\s+(off|\d{1,2}))+$/,
  /^stp\s+(on|off)$/,/^stp\s+(prio|hello|maxage|fwd|txhold)\s+\d{1,2}$/,
  /^stp\s+version\s+(rstp|stp)$/,
  /^stp\s+port\s+\d{1,2}\s+(on|off)$/,/^stp\s+port\s+\d{1,2}\s+edge\s+(on|off|auto)$/,
  /^stp\s+port\s+\d{1,2}\s+cost\s+\d{1,9}$/,/^stp\s+port\s+\d{1,2}\s+prio\s+\d{1,3}$/,
  /^stp\s+port\s+\d{1,2}\s+guard\s+(none|bpdu|root)$/,/^stp\s+port\s+\d{1,2}\s+filter\s+(on|off)$/,
  /^stp\s+port\s+\d{1,2}\s+p2p\s+(auto|on|off)$/,
  /^igmp\s+(on|off)$/,/^mtu\s+\d{1,2}\s+\d+$/,
  /^bw\s+(in|out)\s+\d{1,2}\s+\S+$/,
];
function isConfCmd(line){
  for(var i=0;i<CONF_CMDS.length;i++)if(CONF_CMDS[i].test(line))return true;
  return false;
}
function setDirty(d){
  S.dirty=d;
  $("dirty").classList.toggle("show",d);
}
function postCmd(cmd,quiet){
  return api("/cmd",{method:"POST",body:cmd}).then(function(r){
    if(!r.ok)throw new Error(((r.body||"").split("\n")[0])||t("t_rejected",{c:cmd}));
    if(isConfCmd(cmd.trim()))setDirty(true);
    if(!quiet)toast(t("t_applied",{c:cmd}),"ok");
    return r;
  },function(e){toast(e.message||t("t_failed",{c:cmd}),"err");throw e;});
}
function postCmds(list){
  var p=Promise.resolve();
  list.forEach(function(c){p=p.then(function(){return postCmd(c,true)})});
  return p.then(function(){toast(t("t_cmds",{n:list.length}),"ok")});
}

function toast(msg,cls){
  var el=h("div",{class:"toast "+(cls||""),text:msg});
  $("toasts").appendChild(el);
  setTimeout(function(){el.style.opacity="0";el.style.transition="opacity .3s";},3400);
  setTimeout(function(){el.remove()},3800);
}
function modal(title,bodyEl,buttons){
  $("mtitle").textContent=title;
  var b=$("mbody");b.innerHTML="";b.appendChild(bodyEl);
  var f=$("mfoot");f.innerHTML="";
  (buttons||[]).forEach(function(bt){f.appendChild(bt)});
  $("mback").classList.add("show");
}
function closeModal(){$("mback").classList.remove("show")}
$("mx").addEventListener("click",closeModal);
$("mback").addEventListener("click",function(e){if(e.target===this)closeModal()});
function confirmModal(title,detail,onok){
  var b=h("div");
  if(detail)b.appendChild(h("p",{class:"small",text:detail}));
  modal(title,b,[
    h("button",{class:"ctl",text:t("c_cancel"),onclick:closeModal}),
    h("button",{class:"ctl pri",text:t("c_confirm"),onclick:function(){closeModal();onok()}}),
  ]);
}

var TABS=[
  {id:"dash",  icon:"M3 13h4v8H3zM10 8h4v13h-4zM17 3h4v18h-4z"},
  {id:"ports", icon:"M2 7h20v10H2zM6 11v2M10 11v2M14 11v2M18 11v2"},
  {id:"stp",   icon:"M12 3v5M12 8L5 13v8M12 8l7 5v8M2 21h20"},
  {id:"stats", icon:"M4 20V10M10 20V4M16 20v-7M22 20H2"},
  {id:"vlan",  icon:"M12 3v6M12 9l-7 5M12 9l7 5M5 14v5M19 14v5M3 21h4M17 21h4"},
  {id:"l2",    icon:"M4 5h16M4 12h16M4 19h10"},
  {id:"mirror",icon:"M12 3v18M7 8l-4 4 4 4M17 8l4 4-4 4"},
  {id:"lag",   icon:"M7 8a4 4 0 100 8h3M17 8a4 4 0 110 8h-3M9 12h6"},
  {id:"eee",   icon:"M13 2L4 14h6l-1 8 9-12h-6z"},
  {id:"bw",    icon:"M4 18a8 8 0 0116 0M12 18l4-6"},
  {id:"system",icon:"M12 8a4 4 0 100 8 4 4 0 000-8zM4 12h2M18 12h2M12 4v2M12 18v2M6 6l1.5 1.5M16.5 16.5L18 18M18 6l-1.5 1.5M7.5 16.5L6 18"},
  {id:"fw",    icon:"M12 3v12M8 11l4 4 4-4M4 19h16"},
];
var curTab="dash";
var tabHooks={};
function showTab(id){
  var old=tabHooks[curTab];
  if(old&&old.leave)old.leave();
  curTab=id;
  TABS.forEach(function(tb){
    $("tab-"+tb.id).classList.toggle("act",tb.id===id);
    $("nv-"+tb.id).classList.toggle("act",tb.id===id);
  });
  $("ttitle").textContent=t("nav_"+id);
  $("nav").classList.remove("open");
  if(location.hash!=="#"+id)history.replaceState(null,"","#"+id);
  var hk=tabHooks[id];
  if(hk&&hk.enter)hk.enter();
}
TABS.forEach(function(tb){
  $("navlist").appendChild(h("li",{id:"nv-"+tb.id,onclick:function(){showTab(tb.id)}},[
    h("span",{html:'<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke-width="2"><path d="'+tb.icon+'"/></svg>'}),
    h("span",{text:t("nav_"+tb.id)}),
  ]));
});
$("burger").addEventListener("click",function(){$("nav").classList.toggle("open")});

function Poller(fn,ms){this.fn=fn;this.ms=ms;this.on=false;this.t=null}
Poller.prototype.start=function(){if(this.on)return;this.on=true;this.tick()};
Poller.prototype.stop=function(){this.on=false;clearTimeout(this.t)};
Poller.prototype.tick=function(){
  var self=this;
  if(!self.on)return;
  var run=document.hidden?Promise.resolve():Promise.resolve().then(self.fn).catch(function(){});
  run.then(function(){ if(self.on)self.t=setTimeout(function(){self.tick()},self.ms); });
};

function pollStatus(){
  return getJSON("/status.json").then(function(s){
    var now=Date.now();
    if(!S.n){
      S.n=s.length;
      var slot=0;
      s.forEach(function(p){
        S.physToLog[p.portNum-1]=p.logPort;
        S.logToPhys[p.logPort]=p.portNum;
        if(p.isSFP){slot++;S.sfpSlot[p.portNum-1]=slot;}
      });
      buildStrip();
    }
    if(S.prev){
      var dt=(now-S.prevT)/1000;
      if(dt>0.2)s.forEach(function(p,i){
        var q=S.prev[i];
        if(q)S.rates[p.portNum-1]={
          tx:Number(BigInt(p.txG)-BigInt(q.txG))/dt,
          rx:Number(BigInt(p.rxG)-BigInt(q.rxG))/dt,
        };
      });
    }
    S.prev=s;S.prevT=now;S.ports=s;
    updateStrip();
    var hk=tabHooks[curTab];
    if(hk&&hk.status)hk.status();
  });
}
var statusPoller=new Poller(pollStatus,2500);
function needPorts(fn){
  if(S.n)return fn();
  pollStatus().then(fn).catch(function(){});
}
function fmtPps(v){
  if(v==null)return"-";
  if(v>=1e6)return(v/1e6).toFixed(1)+" M";
  if(v>=1e3)return(v/1e3).toFixed(1)+" k";
  return Math.round(v);
}
function portLabel(p){return p.name?p.portNum+" "+p.name:String(p.portNum)}

function buildStrip(){
  var st=$("strip");st.innerHTML="";
  for(var i=0;i<S.n;i++)(function(i){
    var p=S.ports[i]||{};
    st.appendChild(h("div",{class:"port"+(p.isSFP?" sfp":""),id:"pp"+i,onclick:function(){portDetail(i)}},[
      h("span",{class:"pn",text:String(i+1)}),
      h("i",{class:"jack"}),
      h("span",{class:"ps",id:"ppl"+i,text:"..."}),
    ]));
  })(i);
}
function updateStrip(){
  S.ports.forEach(function(p){
    var i=p.portNum-1,el=$("pp"+i),lb=$("ppl"+i);
    if(!el)return;
    el.classList.toggle("dis",!p.enabled);
    var up=p.enabled&&p.link>0;
    el.classList.toggle("up",!!up);
    if(up)el.style.setProperty("--pc","var("+(LINKC[p.link]||"--s1000")+")");
    lb.textContent=!p.enabled?t("c_off"):(p.link>0?LINKS[p.link]:t("c_down"));
    el.title=(p.name?p.name+" ":"")+(p.isSFP?"SFP":"RJ45");
  });
}

function pU16(v){return parseInt(v,16)&0xffff}
function pI16(v){var x=parseInt(v,16),n=x&0x7fff;return(x&0x8000)?n-0x8000:n}
function calSO(val,cal){
  if(typeof cal!=="string")return val;
  if(cal.slice(0,2)==="0x")cal=cal.slice(2);
  if(cal.length!==8)return val;
  return(pU16(cal.slice(0,4))/256)*val+pI16(cal.slice(4,8));
}
function calRx(val,cal){
  if(typeof cal!=="string")return val;
  if(cal.slice(0,2)==="0x")cal=cal.slice(2);
  if(cal.length!==40)return val;
  var b=cal.match(/.{2}/g).map(function(x){return parseInt(x,16)});
  var v=new DataView(new Uint8Array(b).buffer);
  return v.getFloat32(0)*Math.pow(val,4)+v.getFloat32(4)*Math.pow(val,3)
    +v.getFloat32(8)*Math.pow(val,2)+v.getFloat32(12)*val+v.getFloat32(16);
}
function dBm(mw){return 10*Math.log10(mw)}
function portDetail(i){
  var p=S.ports[i];
  if(!p)return;
  var rows=[[t("c_port"),String(p.portNum)],[t("c_type"),p.isSFP?"SFP":"RJ45"]];
  if(p.name)rows.push([t("c_name"),p.name]);
  rows.push([t("p_state"),!p.enabled?t("p_disabled"):(p.link>0?t("p_up")+" "+LINKS[p.link]:t("c_down"))]);
  rows.push([t("p_txgb"),BigInt(p.txG)+" / "+BigInt(p.txB)+" "+t("p_pkts")]);
  rows.push([t("p_rxgb"),BigInt(p.rxG)+" / "+BigInt(p.rxB)+" "+t("p_pkts")]);
  if(p.isSFP){
    if(p.sfp_vendor)rows.push([t("p_module"),[p.sfp_vendor,p.sfp_model,p.sfp_serial].filter(Boolean).join(" / ")]);
    var ext=p.sfp_options&0x40;
    if(ext){
      var tx=calSO(pU16(p.sfp_txpower),p.sfp_txpower_cal)/10000;
      var rx=calRx(pU16(p.sfp_rxpower),p.sfp_rxpower_cal)/10000;
      rows.push([t("p_temp"),(calSO(pI16(p.sfp_temp),p.sfp_temp_cal)/256).toFixed(1)+" \u00b0C"]);
      rows.push([t("p_vcc"),(calSO(pU16(p.sfp_vcc),p.sfp_vcc_cal)/10000).toFixed(2)+" V"]);
      rows.push([t("p_txbias"),(calSO(pU16(p.sfp_txbias),p.sfp_txbias_cal)/500).toFixed(1)+" mA"]);
      rows.push([t("p_txpower"),tx.toFixed(3)+" mW / "+dBm(tx).toFixed(2)+" dBm"]);
      rows.push([t("p_rxpower"),rx.toFixed(3)+" mW / "+dBm(rx).toFixed(2)+" dBm"]);
      rows.push([t("p_txfault"),t((Number(p.sfp_state)&0x4)?"c_yes":"c_no")]);
      rows.push([t("p_txdis"),t((Number(p.sfp_state)&0x80)?"c_yes":"c_no")]);
    }
    var losPin=(p.sfp_los!=null)?!!Number(p.sfp_los):null;
    var losMod=ext?!!(Number(p.sfp_state)&0x2):null;
    if(losPin!=null||losMod!=null){
      var v=(losMod!=null&&losPin!=null&&losMod!==losPin)
        ?("pin="+losPin+" mod="+losMod+" !"):t((losMod!=null?losMod:losPin)?"c_yes":"c_no");
      rows.push([t("p_rxlos"),v]);
    }
  }else if(p.adv){
    var bits=parseInt(p.adv,2),names=["10M "+t("c_half"),"10M "+t("c_full"),"100M "+t("c_half"),"100M "+t("c_full"),"1G","2.5G"];
    var on=names.filter(function(_,b){return bits&(1<<b)});
    rows.push([t("p_adv"),on.join(", ")||"-"]);
  }
  var tb=h("table",{class:"t"});
  rows.forEach(function(r){
    tb.appendChild(h("tr",null,[h("td",{class:"mut",text:r[0]}),h("td",{text:r[1]})]));
  });
  modal(t("c_port")+" "+(i+1),tb);
}

function renderInfo(){
  var m=[["i_host","hostname"],["i_ip","ip_address"],["i_mask","ip_netmask"],
    ["i_gw","ip_gateway"],["i_mac","mac_address"],["i_fw","sw_ver"],["i_built","build_date"],
    ["i_hw","hw_ver"],["i_flash","flash_size"],["i_syslog","syslog_server"]];
  var tb=$("sysinfo");tb.innerHTML="";
  m.forEach(function(r){
    var v=S.info[r[1]];
    if(v==null||v==="")return;
    tb.appendChild(h("tr",null,[h("td",{class:"mut",text:t(r[0])}),h("td",{class:"mono",text:String(v)})]));
  });
  if(S.info.hostname)$("brandname").textContent=S.info.hostname;
  if(S.info.sw_ver)$("fver").textContent="RTLPlayground "+S.info.sw_ver;
}
function pollInfo(){
  return getJSON("/information.json").then(function(j){S.info=j;renderInfo()});
}
function dashStatus(){
  var tb=$("traffic").tBodies[0];
  if(tb.rows.length!==S.n){
    tb.innerHTML="";
    for(var i=0;i<S.n;i++){
      var tr=tb.insertRow();
      for(var c=0;c<6;c++)tr.insertCell().className=c>=2?"num":"";
    }
  }
  S.ports.forEach(function(p){
    var r=tb.rows[p.portNum-1];
    if(!r)return;
    var rt=S.rates[p.portNum-1];
    r.cells[0].textContent=portLabel(p);
    r.cells[1].innerHTML=linkBadge(p);
    r.cells[2].textContent=rt?fmtPps(rt.tx):"-";
    r.cells[3].textContent=rt?fmtPps(rt.rx):"-";
    r.cells[4].textContent=BigInt(p.txB).toString();
    r.cells[5].textContent=BigInt(p.rxB).toString();
  });
}
tabHooks.dash={
  enter:function(){statusPoller.start();pollInfo().catch(function(){})},
  leave:function(){statusPoller.stop()},
  status:dashStatus,
};

var SPEEDS=[["auto","c_auto"],["2g5","2.5G"],["1g","1G"],["100m full","100M full"],
  ["100m half","100M half"],["10m full","10M full"],["10m half","10M half"]];
var SFPRATES=[["auto","c_auto"],["10g","10G"],["2g5","2.5G"],["1g","1G"],["100m","100M"]];
function speedLabel(s){
  if(s==="c_auto")return t("c_auto");
  return s.replace(" full"," "+t("c_full")).replace(" half"," "+t("c_half"));
}
function buildPorts(){
  var tb=$("ptable").tBodies[0];
  if(tb.rows.length||!S.n)return;
  S.ports.forEach(function(p){
    var i=p.portNum-1;
    var spd=h("select",{class:"in",id:"pspd"+i});
    (p.isSFP?SFPRATES:SPEEDS).forEach(function(o){
      spd.appendChild(h("option",{value:o[0],text:speedLabel(o[1])}));
    });
    var tr=tb.insertRow();
    tr.insertCell().textContent=p.portNum+(p.isSFP?" (SFP)":"");
    tr.insertCell().appendChild(h("input",{class:"in",id:"pname"+i,size:"9",maxlength:"15",value:p.name||"",placeholder:"-"}));
    tr.insertCell().id="plink"+i;
    var mc=tr.insertCell();mc.id="pmac"+i;mc.className="mono";mc.textContent="-";
    tr.insertCell().appendChild(spd);
    var sw=h("label",{class:"switch"},[h("input",{type:"checkbox",id:"pen"+i}),h("i")]);
    sw.firstChild.checked=!!p.enabled;
    tr.insertCell().appendChild(sw);
    tr.insertCell().appendChild(h("input",{class:"in sm",id:"pmtu"+i,type:"number",min:"64",max:"16383"}));
    tr.insertCell().appendChild(h("button",{class:"ctl",text:t("c_apply"),onclick:function(){applyPort(i)}}));
  });
  loadMtu();
}
function loadMtu(){
  return getJSON("/mtu.json").then(function(s){
    s.forEach(function(m){
      S.mtu[m.portNum-1]=parseInt(m.mtu,16);
      var el=$("pmtu"+(m.portNum-1));
      if(el&&document.activeElement!==el)el.value=parseInt(m.mtu,16);
    });
  });
}
/* One learned MAC is the attached device; several means a switch or AP
 * sits behind the port, so only the count is shown. */
function portsMacs(){
  l2Load().then(function(all){
    var by={};
    all.forEach(function(e){
      if(e.pport==="CPU")return;
      var l=by[e.pport]=by[e.pport]||[];
      if(l.indexOf(e.mac)<0)l.push(e.mac);
    });
    S.ports.forEach(function(p){
      var el=$("pmac"+(p.portNum-1));
      if(!el)return;
      var m=by[p.portNum]||[];
      el.textContent=!m.length?"-":(m.length===1?m[0]:m.length+" "+t("c_devices"));
      el.title=m.join("\n");
    });
  }).catch(function(){});
}
var _macTick=0;
function portsStatus(){
  buildPorts();
  if(++_macTick%10===0)portsMacs();
  S.ports.forEach(function(p){
    var i=p.portNum-1,el=$("plink"+i);
    if(el)el.innerHTML=linkBadge(p);
  });
}
function applyPort(i){
  var p=S.ports[i],cmds=[];
  var name=$("pname"+i).value.trim();
  var en=$("pen"+i).checked;
  var spd=$("pspd"+i).value;
  var mtu=parseInt($("pmtu"+i).value,10);
  if(name&&name!==(p.name||"")){
    if(!/^\S{1,15}$/.test(name)){toast(t("pt_name_err"),"err");return;}
    cmds.push("port "+p.portNum+" name "+name);
  }
  if(!en)cmds.push("port "+p.portNum+" off");
  else if(p.isSFP){
    if(!p.enabled)cmds.push("port "+p.portNum+" on");
    cmds.push("sfp "+S.sfpSlot[i]+" "+spd);
  }else cmds.push("port "+p.portNum+" "+spd);
  if(mtu&&mtu!==S.mtu[i]){
    if(mtu<64||mtu>16383){toast(t("pt_mtu_err"),"err");return;}
    cmds.push("mtu "+p.portNum+" "+mtu);
  }
  postCmds(cmds).then(loadMtu).catch(function(){});
}
tabHooks.ports={
  enter:function(){statusPoller.start();needPorts(function(){buildPorts();portsMacs()})},
  leave:function(){statusPoller.stop()},
  status:portsStatus,
};

var STP_PF={EN:1,ADMEDGE:2,AUTOEDGE:4,BPDUG:8,ROOTG:16,FILTER:32,OPEREDGE:64,TRIP:128};
var stpRows=0,stpCur=null;
function stpSel(id,opts){
  var s=h("select",{class:"in",id:id,onchange:stpTouch});
  opts.forEach(function(o){s.appendChild(h("option",{value:o[0],text:o[1]}))});
  return s;
}
function stpTouch(){this.closest("tr,.card").dataset.dirty="1"}
function fmtBridge(hex){
  if(!hex||hex.length<16)return"-";
  return parseInt(hex.slice(0,4),16)+" / "+hex.slice(4).replace(/(..)(?=.)/g,"$1:");
}
function stpBuild(ports){
  var tb=$("stpcfg").tBodies[0],st=$("stpstat").tBodies[0];
  ports.forEach(function(pt){
    var p=pt.p;
    var tr=tb.insertRow();
    tr.insertCell().textContent=p;
    var sw=h("label",{class:"switch"},[h("input",{type:"checkbox",id:"sten"+p,onchange:stpTouch}),h("i")]);
    tr.insertCell().appendChild(sw);
    tr.insertCell().appendChild(stpSel("sted"+p,[["auto",t("c_auto")],["on",t("c_on")],["off",t("c_offc")]]));
    tr.insertCell().appendChild(h("input",{class:"in",id:"stco"+p,type:"number",min:"0",max:"200000000",style:"width:9em",onchange:stpTouch}));
    var pr=[];
    for(var v=0;v<=240;v+=16)pr.push([String(v),String(v)]);
    tr.insertCell().appendChild(stpSel("stpr"+p,pr));
    tr.insertCell().appendChild(stpSel("stgu"+p,[["none",t("stp_g_none")],["bpdu",t("stp_g_bpdu")],["root",t("stp_g_root")]]));
    tr.insertCell().appendChild(stpSel("stfi"+p,[["off",t("c_offc")],["on",t("c_on")]]));
    tr.insertCell().appendChild(stpSel("stpp"+p,[["auto",t("c_auto")],["on",t("c_on")],["off",t("c_offc")]]));
    tr.insertCell().appendChild(h("button",{class:"ctl",text:t("c_apply"),onclick:function(){stpApplyPort(p)}}));
    var sr=st.insertRow();
    sr.insertCell().textContent=p;
    ["ro","st","db","dp","dc","oe","op"].forEach(function(k){sr.insertCell().id="st"+k+p});
  });
  stpRows=ports.length;
}
function stpPortVals(pt){
  var f=pt.f;
  return{
    en:!!(f&STP_PF.EN),
    edge:(f&STP_PF.ADMEDGE)?"on":((f&STP_PF.AUTOEDGE)?"auto":"off"),
    cost:parseInt(pt.pc,16),prio:String(pt.prio),
    guard:(f&STP_PF.BPDUG)?"bpdu":((f&STP_PF.ROOTG)?"root":"none"),
    filter:(f&STP_PF.FILTER)?"on":"off",
    p2p:["auto","on","off"][pt.p2]||"auto",
  };
}
function stpLoad(){
  return getJSON("/stp.json").then(function(s){
    if(!stpRows)stpBuild(s.ports);
    stpCur=s;
    var en=$("stpen");
    if(document.activeElement!==en)en.checked=!!s.on;
    var msg;
    if(!s.on)msg=t("stp_off_msg");
    else{
      var me=(s.prio*4096).toString(16).padStart(4,"0")+s.myMac;
      msg=t("stp_bridge")+" "+fmtBridge(me)+(s.weRoot
        ?" ("+t("stp_root_self")+")"
        :"; "+t("stp_root")+" "+fmtBridge(s.rootPrio+s.rootMac)+" "+t("stp_via")+" "+s.rootPort+", "+t("stp_cost")+" "+parseInt(s.cost,16))
        +"; "+t("stp_tc")+": "+parseInt(s.tc,16);
    }
    $("stpids").textContent=msg;
    var bc=$("stpbridge");
    if(!bc.dataset.dirty){
      $("stpver").value=s.rstp?"rstp":"stp";
      $("stpprio").value=String(s.prio);
      $("stphello").value=s.hello;$("stpmaxage").value=s.maxage;
      $("stpfwd").value=s.fwd;$("stptxhold").value=s.txhold;
    }
    s.ports.forEach(function(pt){
      var p=pt.p;
      var trip=(pt.f&STP_PF.TRIP)?" "+badge(t("stp_trip"),"bad"):"";
      $("stro"+p).textContent=s.on&&pt.role?t("stp_r"+pt.role):"-";
      $("stst"+p).innerHTML=s.on?badge(t("stp_s"+pt.st),pt.st===3?"ok":"")+trip:"-";
      $("stdb"+p).textContent=s.on?fmtBridge(pt.db):"-";
      $("stdp"+p).textContent=s.on?parseInt(pt.dp.slice(0,2),16)+"."+parseInt(pt.dp.slice(2),16):"-";
      $("stdc"+p).textContent=s.on?parseInt(pt.dc,16):"-";
      $("stoe"+p).textContent=s.on?t((pt.f&STP_PF.OPEREDGE)?"c_yes":"c_no"):"-";
      $("stop"+p).textContent=s.on?t(pt.p2===2?"c_no":"c_yes"):"-";
      var row=$("sten"+p).closest("tr");
      if(row.dataset.dirty)return;
      var v=stpPortVals(pt);
      $("sten"+p).checked=v.en;$("sted"+p).value=v.edge;$("stco"+p).value=v.cost;
      $("stpr"+p).value=v.prio;$("stgu"+p).value=v.guard;$("stfi"+p).value=v.filter;$("stpp"+p).value=v.p2p;
    });
  }).catch(function(){});
}
function stpApplyPort(p){
  var cur=null;
  (stpCur?stpCur.ports:[]).forEach(function(pt){if(pt.p===p)cur=stpPortVals(pt)});
  var cost=parseInt($("stco"+p).value,10);
  if(isNaN(cost)||cost<0||cost>200000000){toast(t("stp_cost_err"),"err");return;}
  var w={en:$("sten"+p).checked,edge:$("sted"+p).value,cost:cost,prio:$("stpr"+p).value,
    guard:$("stgu"+p).value,filter:$("stfi"+p).value,p2p:$("stpp"+p).value};
  var cmds=[],pre="stp port "+p+" ";
  if(!cur||w.edge!==cur.edge)cmds.push(pre+"edge "+w.edge);
  if(!cur||w.cost!==cur.cost)cmds.push(pre+"cost "+w.cost);
  if(!cur||w.prio!==cur.prio)cmds.push(pre+"prio "+w.prio);
  if(!cur||w.guard!==cur.guard)cmds.push(pre+"guard "+w.guard);
  if(!cur||w.filter!==cur.filter)cmds.push(pre+"filter "+w.filter);
  if(!cur||w.p2p!==cur.p2p)cmds.push(pre+"p2p "+w.p2p);
  if(!cur||w.en!==cur.en)cmds.push(pre+(w.en?"on":"off"));
  delete $("sten"+p).closest("tr").dataset.dirty;
  if(!cmds.length)return;
  postCmds(cmds).then(stpLoad).catch(function(){});
}
$("stpbridge").addEventListener("change",function(){this.dataset.dirty="1"});
$("stpbapply").addEventListener("click",function(){
  var s=stpCur||{},cmds=[];
  var ver=$("stpver").value,prio=$("stpprio").value;
  if(ver!==(s.rstp?"rstp":"stp"))cmds.push("stp version "+ver);
  if(prio!==String(s.prio))cmds.push("stp prio "+prio);
  [["stphello","hello"],["stpmaxage","maxage"],["stpfwd","fwd"],["stptxhold","txhold"]].forEach(function(f){
    var v=$(f[0]).value;
    if(String(v)!==String(s[f[1]]))cmds.push("stp "+f[1]+" "+v);
  });
  delete $("stpbridge").dataset.dirty;
  if(!cmds.length)return;
  postCmds(cmds).then(stpLoad).catch(function(){});
});
$("stpen").addEventListener("change",function(){
  var el=this,want=el.checked;
  el.checked=!want;
  confirmModal(t(want?"stp_en_q":"stp_dis_q"),t(want?"stp_en_d":"stp_dis_d"),
    function(){postCmd("stp "+(want?"on":"off")).then(stpLoad).catch(function(){})});
});
(function(){
  var sel=$("stpprio");
  for(var i=0;i<16;i++)sel.appendChild(h("option",{value:String(i),text:i+" ("+(i*4096)+")"}));
})();
var stpPoller=new Poller(stpLoad,3000);
tabHooks.stp={enter:function(){stpPoller.start()},leave:function(){stpPoller.stop()}};

var MIB=[
  "Interface in Octets",8,"",0,"Interface out Octets",8,"",0,
  "Interface in Unicast Pkts",8,"",0,"Interface in Multicast Pkts",8,"",0,
  "Interface in Broadcast Pkts",8,"",0,"Interface out Unicast Pkts",8,"",0,
  "Interface out Multicast Pkts",8,"",0,"Interface out Broadcast Pkts",8,"",0,
  "Interface out discards",4,"802.1d Tp Port in discards",4,
  "802.3 Single collision frames",4,"802.3 Multi collision frames",4,
  "802.3 Deferred transmissions",4,"802.3 Late collisions",4,
  "802.3 Excessive collisions",4,"802.3 Symbol errors",4,
  "802.3 Control in unknown opcodes",4,"802.3 In Pause frames",4,
  "802.3 Out Pause frames",4,"Ether drop events",4,
  "TX Ether Broadcast Pkts",4,"TX Ether Multicast Pkts",4,
  "TX Ether CRC Align errors",4,"RX Ether CRC Align errors",4,
  "TX Ether Undersized Pkts",4,"RX Ether Undersized Pkts",4,
  "TX Ether Oversized Pkts",4,"RX Ether Oversized Pkts",4,
  "TX Ether Fragments",4,"RX Ether fragments",4,
  "TX Ether Jabbers",4,"RX Ether Jabbers",4,
  "TX Ether Collisions",4,"TX Ether Pkts 64 Octets",4,"RX Ether Pkts 64 Octets",4,
  "TX Ether 65-127 Octets",4,"RX Ether 65-127 Octets",4,
  "TX Ether Pkts 128-255 Octets",4,"RX Ether Pkts 128-255 Octets",4,
  "TX Ether Pkts 256-511 Octets",4,"RX Ether Pkts 256-511 Octets",4,
  "TX Ether Pkts 512-1023 Octets",4,"RX Ether Pkts 512-1023 Octets",4,
  "TX Ether Pkts 1024-1518 Octets",4,"RX Ether Pkts 1024-1518 Octets",4,
  "",4,"RX Ether Undersized Drop Pkts",4,
  "TX Ether Pkts >1518 Octets",4,"RX Ether Pkts >1518 Octets",4,
  "TX Ether Pkts too large",4,"RX Ether Pkts too large",4,
  "TX Ether Flexible Octets Set 1",4,"RX Ether Flexible Octets Set 1",4,
  "TX Ether Flexible Octets CRC Set 1",4,"RX Ether Flexible Octets CRC Set 1",4,
  "TX Ether Flexible Octets Set 0",4,"RX Ether Flexible Octets Set 0",4,
  "TX Ether Flexible Octets CRC Set 0",4,"RX Ether Flexible Octets CRC Set 0",4,
  "Length Field Errors",4,"False Carriers",4,"Undersized Octets",4,"Framing Errors",4,
  "",4,"RX MAC Discards",4,"RX MAC IPG Short Drop",4,"",4,
  "802.1d TP Learned Entry Discards",4,
  "Egress Queue 7 Dropped Pkts",4,"Egress Queue 6 Dropped Pkts",4,
  "Egress Queue 5 Dropped Pkts",4,"Egress Queue 4 Dropped Pkts",4,
  "Egress Queue 3 Dropped Pkts",4,"Egress Queue 2 Dropped Pkts",4,
  "Egress Queue 1 Dropped Pkts",4,"Egress Queue 0 Dropped Pkts",4,
  "Egress Queue 7 Out Pkts",4,"Egress Queue 6 Out Pkts",4,
  "Egress Queue 5 Out Pkts",4,"Egress Queue 4 Out Pkts",4,
  "Egress Queue 3 Out Pkts",4,"Egress Queue 2 Out Pkts",4,
  "Egress Queue 1 Out Pkts",4,"Egress Queue 0 Out Pkts",4,
  "TX Good Counter",8,"",0,"RX Good Counter",8,"",0,
  "RX Error Counter",4,"TX Error Counter",4,
  "TX Good Counter PHY",8,"",0,"RX Good Counter PHY",8,"",0,
  "RX Error Counter PHY",4,"TX Error Counter PHY",4,
];
/* counters.json returns 64-bit hex words; two 32-bit counters share one word */
function decodeCounters(s){
  var out=[];
  for(var i=0;i<MIB.length;i+=4){
    if(MIB[i]===""&&MIB[i+1]===8)continue;
    var w=BigInt(s[i/4]||"0x0");
    if(MIB[i+1]===8)out.push([MIB[i],w]);
    else{
      if(MIB[i]!=="")out.push([MIB[i],w>>32n]);
      if(MIB[i+2]!=="")out.push([MIB[i+2],w&4294967295n]);
    }
  }
  return out;
}
var ctrPoll=null;
function showCounters(i){
  var body=h("div");
  var bar=h("div",{style:"display:flex;gap:12px;align-items:center;margin-bottom:10px"});
  var nz=h("input",{type:"checkbox",checked:""});
  var auto=h("input",{type:"checkbox"});
  bar.appendChild(h("label",null,[nz,document.createTextNode(" "+t("st_nonzero"))]));
  bar.appendChild(h("label",null,[auto,document.createTextNode(" "+t("st_autoref"))]));
  var wrap=h("div",{class:"scrollx"});
  body.appendChild(bar);body.appendChild(wrap);
  function load(){
    return getJSON("/counters.json?port="+(i+1)).then(function(s){
      var rows=decodeCounters(s);
      var tb=h("table",{class:"t"});
      tb.appendChild(h("tr",null,[h("th",{text:t("st_counter")}),h("th",{class:"num",text:t("st_value")})]));
      rows.forEach(function(r){
        if(nz.checked&&r[1]===0n)return;
        tb.appendChild(h("tr",null,[h("td",{text:r[0]}),h("td",{class:"num mono",text:r[1].toString()})]));
      });
      wrap.innerHTML="";wrap.appendChild(tb);
    });
  }
  nz.addEventListener("change",load);
  if(ctrPoll)ctrPoll.stop();
  ctrPoll=new Poller(function(){return auto.checked?load():Promise.resolve()},2500);
  ctrPoll.start();
  load().catch(function(){wrap.textContent=t("st_fail")});
  modal(t("c_port")+" "+(i+1)+": "+t("st_counters"),body,
    [h("button",{class:"ctl",text:t("c_refresh"),onclick:function(){load()}}),
     h("button",{class:"ctl pri",text:t("c_close"),onclick:function(){ctrPoll.stop();closeModal()}})]);
}
function statsStatus(){
  var tb=$("stable").tBodies[0];
  if(tb.rows.length!==S.n){
    tb.innerHTML="";
    for(var i=0;i<S.n;i++)(function(i){
      var tr=tb.insertRow();
      for(var c=0;c<7;c++)tr.insertCell().className=c>=3?"num":"";
      tr.insertCell().appendChild(h("button",{class:"ctl",text:t("st_details"),onclick:function(){showCounters(i)}}));
    })(i);
  }
  S.ports.forEach(function(p){
    var r=tb.rows[p.portNum-1];
    if(!r)return;
    r.cells[0].textContent=p.portNum;
    r.cells[1].textContent=p.name||"";
    r.cells[2].innerHTML=linkBadge(p);
    r.cells[3].textContent=BigInt(p.txG).toString();
    r.cells[4].textContent=BigInt(p.txB).toString();
    r.cells[5].textContent=BigInt(p.rxG).toString();
    r.cells[6].textContent=BigInt(p.rxB).toString();
  });
}
tabHooks.stats={
  enter:function(){statusPoller.start()},
  leave:function(){statusPoller.stop();if(ctrPoll)ctrPoll.stop()},
  status:statsStatus,
};

function maskToPorts(mask){
  var out=[];
  for(var p=1;p<=S.n;p++)if((mask>>S.physToLog[p-1])&1)out.push(p);
  return out;
}
function rangeStr(list){
  if(!list.length)return"-";
  var parts=[],s=list[0],e=list[0];
  for(var i=1;i<=list.length;i++){
    if(list[i]===e+1){e=list[i];continue}
    parts.push(s===e?String(s):s+"-"+e);
    s=e=list[i];
  }
  return parts.join(", ");
}
function vlanRefresh(){
  var tb=$("vtable").tBodies[0];
  return getJSON("/vlanlist").then(function(d){
    var vl=d.vlan||[];
    $("vmgmtcur").textContent=d.mgmt?String(d.mgmt):t("v_mgmt_none");
    $("vempty").style.display=vl.length?"none":"";
    tb.innerHTML="";
    var p=Promise.resolve();
    vl.forEach(function(v){
      p=p.then(function(){return getJSON("/vlan.json?vid="+v.id)}).then(function(d){
        var m=parseInt(d.members,16),mem=m&0x3ff,unt=((m>>10)&0x3ff)&mem;
        var pv=parseInt(d.pvid,16)&0x3ff;
        var tr=tb.insertRow();
        tr.insertCell().appendChild(h("a",{href:"#vlan",text:String(v.id),onclick:function(e){
          e.preventDefault();$("vvid").value=v.id;vlanLoad();
        }}));
        tr.insertCell().textContent=v.name||"";
        tr.insertCell().textContent=rangeStr(maskToPorts(mem));
        tr.insertCell().textContent=rangeStr(maskToPorts(mem&~unt));
        tr.insertCell().textContent=rangeStr(maskToPorts(unt));
        tr.insertCell().textContent=rangeStr(maskToPorts(pv));
        var del=tr.insertCell();
        if(v.id!==1)del.appendChild(h("button",{class:"ctl",text:"\u2715",title:t("v_del_t"),onclick:function(){
          confirmModal(t("v_del_q",{n:v.id}),t("v_del_d"),function(){
            postCmd("vlan "+v.id+" d").then(vlanRefresh).catch(function(){});
          });
        }}));
      }).catch(function(){});
    });
    return p;
  });
}
function buildVlanEdit(){
  var tb=$("vedit").tBodies[0];
  if(tb.rows.length||!S.n)return;
  var hd=tb.insertRow();hd.insertCell().className="mut";
  var rM=tb.insertRow();rM.insertCell().textContent=t("v_member");
  var rP=tb.insertRow();rP.insertCell().textContent=t("v_pvid");
  for(var p=1;p<=S.n;p++)(function(p){
    hd.insertCell().innerHTML='<b>'+p+'</b>';
    var seg=h("span",{class:"seg",id:"vm"+p});
    ["-","U","T"].forEach(function(s,ix){
      seg.appendChild(h("button",{text:s,"data-v":ix,onclick:function(){
        seg.querySelectorAll("button").forEach(function(b){b.classList.remove("on")});
        this.classList.add("on");
      }}));
    });
    seg.children[0].classList.add("on");
    rM.insertCell().appendChild(seg);
    rP.insertCell().appendChild(h("input",{type:"checkbox",id:"vp"+p}));
  })(p);
  var it=$("ingress").tBodies[0];
  var ih=it.insertRow();ih.insertCell().className="mut";
  var ir=it.insertRow();ir.insertCell().textContent=t("v_accept");
  for(var q=1;q<=S.n;q++)(function(q){
    ih.insertCell().innerHTML='<b>'+q+'</b>';
    var sel=h("select",{class:"in",id:"ing"+q});
    [["","-"],["a",t("v_ing_all")],["u",t("v_untagged")],["t",t("v_tagged")]].forEach(function(o){
      sel.appendChild(h("option",{value:o[0],text:o[1]}));
    });
    ir.insertCell().appendChild(sel);
  })(q);
}
function segVal(p){
  return Number($("vm"+p).querySelector("button.on").getAttribute("data-v"));
}
function segSet(p,v){
  $("vm"+p).querySelectorAll("button").forEach(function(b,i){b.classList.toggle("on",i===v)});
}
function vlanLoad(){
  var vid=parseInt($("vvid").value,10);
  if(!vid||vid<1||vid>4094){toast(t("v_vid_err"),"err");return;}
  getJSON("/vlan.json?vid="+vid).then(function(d){
    $("vname").value=d.name||"";
    var m=parseInt(d.members,16),mem=m&0x3ff,unt=((m>>10)&0x3ff)&mem;
    var pv=parseInt(d.pvid,16)&0x3ff;
    for(var p=1;p<=S.n;p++){
      var bit=S.physToLog[p-1];
      segSet(p,(mem>>bit)&1?(((unt>>bit)&1)?1:2):0);
      $("vp"+p).checked=!!((pv>>bit)&1);
    }
    toast(t("v_loaded",{n:vid}),"ok");
  }).catch(function(){toast(t("v_notfound",{n:vid}),"err")});
}
function vlanApply(){
  var vid=parseInt($("vvid").value,10);
  if(!vid||vid<1||vid>4094){toast(t("v_vid_err"),"err");return;}
  var name=$("vname").value.trim();
  if(name&&!/^[a-zA-Z]\w*$/.test(name)){toast(t("v_name_err"),"err");return;}
  var cmd="vlan "+vid+(name?" "+name:""),members=0;
  for(var p=1;p<=S.n;p++){
    var v=segVal(p);
    if(v===1){cmd+=" "+p;members++;}
    else if(v===2){cmd+=" "+p+"t";members++;}
  }
  if(!members){toast(t("v_nomember"),"err");return;}
  var cmds=[cmd];
  for(var q=1;q<=S.n;q++)if($("vp"+q).checked)cmds.push("pvid "+q+" "+vid);
  postCmds(cmds).then(vlanRefresh).catch(function(){});
}
function ingressApply(){
  var cmd="ingress",any=false;
  for(var p=1;p<=S.n;p++){
    var v=$("ing"+p).value;
    if(v){cmd+=" "+p+v;any=true;}
  }
  if(!any){toast(t("v_ing_none"),"err");return;}
  postCmd(cmd).catch(function(){});
}
$("vload").addEventListener("click",vlanLoad);
$("vapply").addEventListener("click",vlanApply);
$("ingapply").addEventListener("click",ingressApply);
$("vmgmt").addEventListener("click",function(){
  var vid=parseInt($("vvid").value,10);
  if(!vid){toast(t("v_vid_first"),"err");return;}
  confirmModal(t("v_mgmt_q",{n:vid}),t("v_mgmt_d"),
    function(){postCmd("vlan "+vid+" mgmt").then(vlanRefresh).catch(function(){})});
});
tabHooks.vlan={
  enter:function(){needPorts(function(){buildVlanEdit();vlanRefresh().catch(function(){})})},
};

var l2Rows=[],l2SortCol="pport",l2SortDir=1;
function l2Load(){
  var seen={},all=[],idx=0,guard=0;
  function step(){
    return getJSON("/l2.json?idx="+idx).then(function(s){
      if(!s.length)return all;
      var wrapped=false;
      s.forEach(function(e){
        e.idx=parseInt(e.idx,16);
        if(seen[e.idx]){wrapped=true;return;}
        seen[e.idx]=1;
        e.vlan=parseInt(e.vlan,16);
        e.stat=e.type==="s";
        e.pport=e.port===9?"CPU":S.logToPhys[e.port];
        e.where=e.lag?"LAG"+e.lag:String(e.pport);
        all.push(e);
      });
      if(wrapped||++guard>140)return all;
      idx=s[s.length-1].idx+1;
      return step();
    });
  }
  return step();
}
function l2Key(e){
  if(l2SortCol==="pport"){
    if(e.pport==="CPU")return 1e6;
    return e.lag?1000+e.lag:Number(e.pport);
  }
  if(l2SortCol==="vlan")return e.vlan;
  if(l2SortCol==="stat")return e.stat?1:0;
  return e.mac;
}
function l2Fetch(){
  $("l2count").textContent=t("l2_loading");
  return l2Load().then(function(all){
    l2Rows=all;
    l2Render();
  }).catch(function(){$("l2count").textContent=t("l2_failed")});
}
function l2Render(){
  var f=$("l2filter").value.toLowerCase();
  var tb=$("l2table").tBodies[0];tb.innerHTML="";
  var rows=l2Rows.slice().sort(function(a,b){
    var x=l2Key(a),y=l2Key(b);
    return((x>y)-(x<y))*l2SortDir||((a.mac>b.mac)-(a.mac<b.mac));
  });
  $("l2table").querySelectorAll("th[data-sort]").forEach(function(th){
    th.querySelector(".arrow").textContent=th.dataset.sort===l2SortCol?(l2SortDir>0?" \u25b2":" \u25bc"):"";
  });
  var shown=0;
  rows.forEach(function(e){
    var ty=t(e.stat?"l2_static":"l2_learned");
    var hay=(e.mac+" "+e.vlan+" "+e.where+" "+ty).toLowerCase();
    if(f&&hay.indexOf(f)<0)return;
    shown++;
    var tr=tb.insertRow();
    tr.insertCell().textContent=e.where;
    tr.insertCell().className="mono";tr.cells[1].textContent=e.mac;
    tr.insertCell().textContent=e.vlan;
    tr.insertCell().textContent=ty;
    var dc=tr.insertCell();
    if(e.pport!=="CPU")dc.appendChild(h("button",{class:"ctl",text:"\u2715",title:t("l2_del_t"),onclick:function(){
      getJSON("/l2_del.json?idx="+e.idx).then(function(){
        l2Rows=l2Rows.filter(function(x){return x!==e});
        l2Render();
      }).catch(function(){});
    }}));
  });
  $("l2count").textContent=shown+" / "+l2Rows.length+" "+t("l2_entries");
}
$("l2table").querySelectorAll("th[data-sort]").forEach(function(th){
  th.addEventListener("click",function(){
    var c=th.dataset.sort;
    l2SortDir=(c===l2SortCol)?-l2SortDir:1;
    l2SortCol=c;
    l2Render();
  });
});
$("l2filter").addEventListener("input",l2Render);
$("l2refresh").addEventListener("click",function(){l2Fetch()});
$("l2flush").addEventListener("click",function(){
  confirmModal(t("l2_flush_q"),"",function(){
    postCmd("l2 forget").then(function(){setTimeout(l2Fetch,500)}).catch(function(){});
  });
});
tabHooks.l2={enter:function(){needPorts(function(){l2Fetch()})}};

function buildMirror(){
  var sel=$("mport");
  if(sel.options.length)return;
  for(var p=1;p<=S.n;p++)sel.appendChild(h("option",{value:p,text:t("c_port")+" "+p}));
  var tb=$("mtable").tBodies[0];
  var hd=tb.insertRow();hd.insertCell().className="mut";
  var r=tb.insertRow();r.insertCell().textContent=t("m_mirror");
  for(var q=1;q<=S.n;q++)(function(q){
    hd.insertCell().innerHTML="<b>"+q+"</b>";
    var seg=h("span",{class:"seg",id:"mm"+q});
    ["-","RX","TX",t("m_both")].forEach(function(s,ix){
      seg.appendChild(h("button",{text:s,"data-v":ix,onclick:function(){
        seg.querySelectorAll("button").forEach(function(b){b.classList.remove("on")});
        this.classList.add("on");
      }}));
    });
    seg.children[0].classList.add("on");
    r.insertCell().appendChild(seg);
  })(q);
}
function mirrorLoad(){
  return getJSON("/mirror.json").then(function(m){
    $("mstate").textContent=t(m.enabled?"m_active":"c_off");
    $("mstate").className="badge "+(m.enabled?"ok":"");
    if(m.enabled)$("mport").value=m.mPort;
    var tx=parseInt(m.mirror_tx,2),rx=parseInt(m.mirror_rx,2);
    for(var p=1;p<=S.n;p++){
      var bit=S.physToLog[p-1];
      var v=(((rx>>bit)&1)?1:0)+(((tx>>bit)&1)?2:0);
      $("mm"+p).querySelectorAll("button").forEach(function(b,i){b.classList.toggle("on",i===v)});
    }
  });
}
$("mapply").addEventListener("click",function(){
  var mp=$("mport").value,cmd="mirror "+mp,any=false;
  for(var p=1;p<=S.n;p++){
    if(String(p)===mp)continue;
    var v=Number($("mm"+p).querySelector("button.on").getAttribute("data-v"));
    if(v===1){cmd+=" "+p+"r";any=true;}
    else if(v===2){cmd+=" "+p+"t";any=true;}
    else if(v===3){cmd+=" "+p;any=true;}
  }
  if(!any){toast(t("m_none"),"err");return;}
  postCmd(cmd).then(mirrorLoad).catch(function(){});
});
$("moff").addEventListener("click",function(){
  postCmd("mirror off").then(mirrorLoad).catch(function(){});
});
tabHooks.mirror={enter:function(){needPorts(function(){buildMirror();mirrorLoad().catch(function(){})})}};

var HASHF=["spa","smac","dmac","sip","dip","sport","dport"];
function buildLag(){
  var w=$("lagwrap");
  if(w.children.length)return;
  for(var g=1;g<=4;g++)(function(g){
    var card=h("div",{class:"card"});
    card.appendChild(h("h2",{text:"LAG "+g}));
    var pr=h("div",{style:"display:flex;gap:10px;flex-wrap:wrap;margin-bottom:10px"});
    for(var p=1;p<=S.n;p++)pr.appendChild(h("label",null,[
      h("input",{type:"checkbox",id:"lg"+g+"p"+p}),document.createTextNode(" "+p+" "),
    ]));
    card.appendChild(pr);
    var hr=h("div",{style:"display:flex;gap:10px;flex-wrap:wrap;margin-bottom:10px",class:"small"});
    hr.appendChild(h("span",{class:"mut",text:t("lag_hash")}));
    HASHF.forEach(function(f){
      hr.appendChild(h("label",null,[h("input",{type:"checkbox",id:"lg"+g+"h"+f}),document.createTextNode(" "+f+" ")]));
    });
    card.appendChild(hr);
    card.appendChild(h("button",{class:"ctl pri",text:t("c_apply"),onclick:function(){lagApply(g)}}));
    w.appendChild(card);
  })(g);
}
function lagLoad(){
  return getJSON("/lag.json").then(function(s){
    s.forEach(function(l){
      var g=l.lagNum+1;
      var members=parseInt(l.members,2);
      for(var p=1;p<=S.n;p++)
        $("lg"+g+"p"+p).checked=!!((members>>S.physToLog[p-1])&1);
      var hash=parseInt(l.hash,16);
      HASHF.forEach(function(f,i){$("lg"+g+"h"+f).checked=!!((hash>>i)&1)});
    });
  });
}
function lagApply(g){
  var cmd="lag "+g,n=0;
  for(var p=1;p<=S.n;p++)if($("lg"+g+"p"+p).checked){cmd+=" "+p;n++;}
  if(!n){
    confirmModal(t("lag_clear_q",{n:g}),t("lag_clear_d"),function(){
      postCmd("lag "+g+" d").then(lagLoad).catch(function(){});
    });
    return;
  }
  var hcmd="laghash "+g,nh=0;
  HASHF.forEach(function(f){if($("lg"+g+"h"+f).checked){hcmd+=" "+f;nh++;}});
  var cmds=[cmd];
  if(nh)cmds.push(hcmd);
  postCmds(cmds).then(lagLoad).catch(function(){});
}
tabHooks.lag={enter:function(){needPorts(function(){buildLag();lagLoad().catch(function(){})})}};

function eeeFlags(bits){
  var b=parseInt(bits,2);
  return["100M","1G","2.5G"].map(function(s,i){
    return(b&(4>>i))?s:null;
  }).filter(Boolean).join(", ")||"-";
}
function eeeLoad(){
  return getJSON("/eee.json").then(function(s){
    var tb=$("etable").tBodies[0];tb.innerHTML="";
    s.forEach(function(p){
      var tr=tb.insertRow();
      tr.insertCell().textContent=p.portNum+(p.isSFP?" (SFP)":"");
      if(p.isSFP){
        for(var c=0;c<3;c++)tr.insertCell().textContent="-";
        tr.insertCell().textContent=t("e_na");
        return;
      }
      tr.insertCell().textContent=eeeFlags(p.eee);
      tr.insertCell().textContent=eeeFlags(p.eee_lp);
      tr.insertCell().innerHTML=p.active?badge(t("e_active"),"ok"):badge(t("e_idle"));
      var on=parseInt(p.eee,2)!==0;
      var sw=h("label",{class:"switch"},[
        h("input",{type:"checkbox",onchange:function(){
          postCmd("eee "+p.portNum+" "+(this.checked?"on":"off"))
            .then(function(){setTimeout(eeeLoad,300)}).catch(function(){});
        }}),h("i")]);
      sw.firstChild.checked=on;
      tr.insertCell().appendChild(sw);
    });
  });
}
var eeePoller=new Poller(function(){return eeeLoad()},4000);
tabHooks.eee={enter:function(){eeePoller.start()},leave:function(){eeePoller.stop()}};

function bwLoad(){
  return getJSON("/bandwidth.json").then(function(s){
    var tb=$("btable").tBodies[0];tb.innerHTML="";
    s.forEach(function(p){
      var n=p.portNum;
      var iOn=!!Number(p.iLimited),eOn=!!Number(p.eLimited);
      var iM=(parseInt(p.iBW,16)*16/1000),eM=(parseInt(p.eBW,16)*16/1000);
      var tr=tb.insertRow();
      tr.insertCell().textContent=n;
      var icb=h("input",{type:"checkbox",id:"bwi"+n});icb.checked=iOn;
      tr.insertCell().appendChild(icb);
      var iin=h("input",{class:"in sm",id:"bwiv"+n,type:"number",min:"0.016",max:"10000",step:"any"});
      if(iOn)iin.value=+iM.toFixed(3);
      tr.insertCell().appendChild(iin);
      var msel=h("select",{class:"in",id:"bwm"+n},[
        h("option",{value:"fc",text:t("bw_fc")}),
        h("option",{value:"drop",text:t("bw_drop")}),
      ]);
      msel.value=Number(p.iFC)===1?"fc":"drop";
      tr.insertCell().appendChild(msel);
      var ecb=h("input",{type:"checkbox",id:"bwe"+n});ecb.checked=eOn;
      tr.insertCell().appendChild(ecb);
      var ein=h("input",{class:"in sm",id:"bwev"+n,type:"number",min:"0.016",max:"10000",step:"any"});
      if(eOn)ein.value=+eM.toFixed(3);
      tr.insertCell().appendChild(ein);
      tr.insertCell().appendChild(h("button",{class:"ctl",text:t("c_apply"),onclick:function(){bwApply(n)}}));
    });
  });
}
/* The bw command takes kbit/s and converts to 16-kbit register steps itself. */
function bwHex(mbit){
  var kbit=Math.round(mbit*1000);
  if(kbit<16||kbit>10000000)return null;
  var x=kbit.toString(16);
  return x.length%2?"0"+x:x;
}
function bwApply(n){
  var cmds=[];
  if($("bwi"+n).checked){
    var ih=bwHex(parseFloat($("bwiv"+n).value));
    if(!ih){toast(t("bw_in_err"),"err");return;}
    cmds.push("bw in "+n+" "+ih);
    cmds.push("bw in "+n+" "+$("bwm"+n).value);
  }else cmds.push("bw in "+n+" off");
  if($("bwe"+n).checked){
    var eh=bwHex(parseFloat($("bwev"+n).value));
    if(!eh){toast(t("bw_out_err"),"err");return;}
    cmds.push("bw out "+n+" "+eh);
  }else cmds.push("bw out "+n+" off");
  postCmds(cmds).then(bwLoad).catch(function(){});
}
tabHooks.bw={enter:function(){needPorts(function(){bwLoad().catch(function(){})})}};

var IPRE=/^(\d{1,3}\.){3}\d{1,3}$/;
function okIp(s){
  if(!IPRE.test(s))return false;
  return s.split(".").every(function(o){return+o<=255});
}
function sysLoad(){
  pollInfo().then(function(){
    $("sy-ip").value=S.info.ip_address||"";
    $("sy-mask").value=S.info.ip_netmask||"";
    $("sy-gw").value=S.info.ip_gateway||"";
    $("sy-host").value=S.info.hostname||"";
    var sl=(S.info.syslog_server||"").split(":");
    if(sl[0]&&sl[0]!=="0.0.0.0")$("sy-sysip").value=sl[0];
    if(sl[1])$("sy-sysport").value=sl[1];
  }).catch(function(){});
  cfgReload();
}
function cfgParseKnown(txt){
  var igmp=false,syslog=false;
  txt.split(/\r?\n/).forEach(function(l){
    l=l.trim();
    if(/^igmp on$/.test(l))igmp=true;
    if(/^igmp off$/.test(l))igmp=false;
    if(/^syslog on$/.test(l))syslog=true;
    if(/^syslog off$/.test(l))syslog=false;
  });
  $("sy-igmp").checked=igmp;
  $("sy-syslog").checked=syslog;
}
$("sy-apply").addEventListener("click",function(){
  var ip=$("sy-ip").value.trim(),mask=$("sy-mask").value.trim(),gw=$("sy-gw").value.trim();
  if(!okIp(ip)||!okIp(mask)||!okIp(gw)){toast(t("sy_ip_err"),"err");return;}
  var cmds=[];
  var hn=$("sy-host").value.trim();
  if(hn&&hn!==S.info.hostname){
    if(!/^[\x21-\x7e]{1,23}$/.test(hn)||/["\\]/.test(hn)){toast(t("sy_host_err"),"err");return;}
    cmds.push("hostname "+hn);
  }
  cmds.push("ip "+ip,"netmask "+mask,"gw "+gw);
  var changingIp=ip!==S.info.ip_address;
  confirmModal(t("sy_net_q"),changingIp?t("sy_net_d",{ip:ip}):"",function(){
    postCmds(cmds).then(function(){
      if(changingIp)toast(t("sy_ip_changed",{ip:ip}),"ok");
      else sysLoad();
    }).catch(function(){});
  });
});
$("sy-dhcp").addEventListener("click",function(){
  confirmModal(t("sy_dhcp_q"),t("sy_dhcp_d"),function(){postCmd("ip dhcp").catch(function(){})});
});
$("sy-igmp").addEventListener("change",function(){
  var el=this;
  postCmd("igmp "+(el.checked?"on":"off")).catch(function(){el.checked=!el.checked});
});
$("sy-syslog").addEventListener("change",function(){
  var cmds=[],el=this;
  if(el.checked){
    var sip=$("sy-sysip").value.trim(),sp=$("sy-sysport").value.trim();
    if(sip&&!okIp(sip)){toast(t("sy_sysip_err"),"err");el.checked=false;return;}
    if(sp&&!(+sp>=1&&+sp<=65535)){toast(t("sy_sysport_err"),"err");el.checked=false;return;}
    if(sip)cmds.push("syslog ip "+sip);
    if(sp)cmds.push("syslog port "+sp);
    cmds.push("syslog on");
  }else cmds.push("syslog off");
  postCmds(cmds).catch(function(){});
});
$("sy-pwapply").addEventListener("click",function(){
  var a=$("sy-pw1").value,b=$("sy-pw2").value;
  if(a.length<1||a.length>20){toast(t("sy_pw_len"),"err");return;}
  if(/\s/.test(a)){toast(t("sy_pw_space"),"err");return;}
  if(a!==b){toast(t("sy_pw_match"),"err");return;}
  confirmModal(t("sy_pw_q"),t("sy_pw_d"),function(){
    postCmd("passwd "+a).then(function(){
      $("sy-pw1").value=$("sy-pw2").value="";
    }).catch(function(){});
  });
});
$("sy-send").addEventListener("click",sysConsole);
$("sy-cmd").addEventListener("keydown",function(e){if(e.key==="Enter")sysConsole()});
function sysConsole(){
  var c=$("sy-cmd").value.trim();
  if(!c)return;
  var out=$("sy-cout");
  api("/cmd",{method:"POST",body:c}).then(function(r){
    var body=r.body.replace(/\s+$/,"");
    out.textContent+="> "+c+"\n";
    if(body)out.textContent+=body+"\n";
    else out.textContent+=(r.ok?"OK":"ERROR "+r.status)+"\n";
    if(out.textContent.length>20000)out.textContent=out.textContent.slice(-16000);
    out.scrollTop=out.scrollHeight;
    if(r.ok&&isConfCmd(c))setDirty(true);
  }).catch(function(e){out.textContent+="> "+c+"\n"+e+"\n"});
  $("sy-cmd").value="";
}
$("sy-reboot").addEventListener("click",function(){
  confirmModal(t("sy_reboot_q"),S.dirty?t("sy_reboot_d"):"",function(){
    api("/reset").catch(function(){});
    toast(t("sy_rebooting"),"ok");
  });
});
function cfgReload(){
  return getText("/config").then(function(x){
    x=x.replace(/\0[\s\S]*$/,"");
    $("cfgedit").value=x;
    cfgBytes();
    cfgParseKnown(x);
  }).catch(function(){});
}
function cfgBytes(){
  var n=new Blob([$("cfgedit").value]).size;
  var el=$("cfgbytes");
  el.textContent=n+" / 2048 "+t("sy_bytes");
  el.style.color=n>2048?"var(--bad)":"";
  return n;
}
$("cfgedit").addEventListener("input",cfgBytes);
$("cfgreload").addEventListener("click",cfgReload);
$("cfgwrite").addEventListener("click",function(){
  writeConfig($("cfgedit").value,t("cw_title"));
});
tabHooks.system={enter:sysLoad};

var CONF_OVERWRITE=[
  /^ip\b/,/^gw\b/,/^netmask\b/,/^hostname\b/,
  /^syslog\s+ip\b/,/^syslog\s+port\b/,/^passwd\b/,
  /^vlan\s+\d{1,4}\s+mgmt$/,/^vlan\s+\d{1,4}(?!\s+mgmt\b)/,
  /^pvid\s+\d{1,2}\b/,/^ingress\b/,
  /^port\s+\d{1,2}(?!\s+name\b)/,/^port\s+\d{1,2}\s+name\b/,
  /^eee\s+\d{1,2}\b/,/^eee\b/,/^mirror\b/,
  /^lag\s+\d\b/,/^laghash\s+\d\b/,/^isolate\s+\d{1,2}\b/,
  /^stp\s+(prio|hello|maxage|fwd|txhold|version)\b/,
  /^stp\s+port\s+\d{1,2}\s+(edge|cost|prio|guard|filter|p2p)\b/,
  /^igmp\b/,/^mtu\s+\d{1,2}\b/,
];
var CONF_TOGGLE=[/^(syslog)\s+(on|off)$/,/^(stp)\s+(on|off)$/,/^(stp\s+port\s+\d{1,2})\s+(on|off)$/];
function mergeConf(base,texts){
  var conf=base.slice();
  function drop(rx){conf=conf.filter(function(c){return!rx.test(c)})}
  texts.forEach(function(txt){
    txt.split(/\r?\n/).forEach(function(line){
      line=line.trim().replace(/\s+/g," ");
      if(!line)return;
      var m;
      if((m=line.match(/^vlan (\d{1,4}) d$/))){drop(new RegExp("^vlan "+m[1]+"( |$)"));return;}
      if((m=line.match(/^lag (\d) d$/))){drop(new RegExp("^lag(hash)? "+m[1]+"( |$)"));return;}
      if(line==="mirror off"){drop(/^mirror /);return;}
      if(!isConfCmd(line))return;
      if((m=line.match(/^bw (in|out) (\d{1,2}) (\S+)$/))){
        var pre="^bw "+m[1]+" "+m[2]+" ";
        if(m[1]==="out"||m[3]==="off")drop(new RegExp(pre));
        else if(m[3]==="drop"||m[3]==="fc")drop(new RegExp(pre+"(drop|fc)$"));
        else drop(new RegExp(pre+"(off|[0-9a-f]+)$"));
        conf.push(line);return;
      }
      for(var i=0;i<CONF_TOGGLE.length;i++){
        if((m=line.match(CONF_TOGGLE[i]))){drop(new RegExp("^"+m[1]+" (on|off)$"));break;}
      }
      for(var j=0;j<CONF_OVERWRITE.length;j++){
        var rx=CONF_OVERWRITE[j];
        if(rx.test(line)){
          var key=line.match(rx)[0];
          conf=conf.filter(function(item){
            return!(item===key||(item.indexOf(key+" ")===0
              &&!/\smgmt$/.test(item)&&item.indexOf(key+" name ")!==0));
          });
          break;
        }
      }
      if(/^vlan \d{1,4} mgmt$/.test(line))drop(/^vlan \d{1,4} mgmt$/);
      conf.push(line);
    });
  });
  return conf;
}
function writeConfig(txt,title){
  txt=txt.replace(/\r\n/g,"\n");
  if(txt&&txt.slice(-1)!=="\n")txt+="\n";
  var bytes=new Blob([txt]).size;
  var lines=txt.split("\n").filter(function(l){return l.trim()});
  var unknown=lines.filter(function(l){return!isConfCmd(l.trim().replace(/\s+/g," "))});
  var body=h("div");
  body.appendChild(h("p",{class:"small mut",text:t("cw_info",{n:bytes})}));
  if(bytes>2048){
    body.appendChild(h("p",{class:"small",style:"color:var(--bad)",text:t("cw_toolarge")}));
    modal(title,body,[h("button",{class:"ctl",text:t("c_close"),onclick:closeModal})]);
    return;
  }
  if(unknown.length)
    body.appendChild(h("p",{class:"small",style:"color:var(--warn)",text:t("cw_unknown")+unknown.join(" | ")}));
  body.appendChild(h("pre",{class:"cfg",text:txt||t("cw_empty")}));
  modal(title,body,[
    h("button",{class:"ctl",text:t("c_cancel"),onclick:closeModal}),
    h("button",{class:"ctl pri",text:t("sy_write"),onclick:function(){
      closeModal();
      doWriteConfig(txt);
    }}),
  ]);
}
function doWriteConfig(txt){
  var form=new FormData();
  form.append("configuration",new Blob([txt],{type:"application/octet-stream"}),"config.txt");
  toast(t("cw_writing"));
  api("/config",{method:"POST",body:form}).then(function(r){
    if(!r.ok)throw new Error(t("cw_failed",{n:r.status}));
    return getText("/config");
  }).then(function(back){
    back=back.replace(/\0[\s\S]*$/,"").replace(/\r\n/g,"\n").trim();
    if(back!==txt.trim())throw new Error(t("cw_verify_fail"));
    return api("/cmd_log_clear").catch(function(){});
  }).then(function(){
    setDirty(false);
    $("cfgedit").value=txt;cfgBytes();cfgParseKnown(txt);
    toast(t("cw_saved"),"ok");
  }).catch(function(e){toast(e.message||String(e),"err")});
}
$("saveBtn").addEventListener("click",function(){
  toast(t("cw_collect"));
  Promise.all([
    getText("/config").catch(function(){return""}),
    getText("/cmd_log").catch(function(){return""}),
  ]).then(function(r){
    var cur=r[0].replace(/\0[\s\S]*$/,"");
    var merged=mergeConf([],[cur,r[1].replace(/\0[\s\S]*$/,"")]);
    writeConfig(merged.join("\n"),t("cw_save_title"));
  });
});

var fwBuf=null;
$("fwfile").addEventListener("change",function(){
  var f=this.files[0];
  fwBuf=null;
  $("fwup").disabled=true;
  $("fwinfo").textContent="";
  if(!f)return;
  var info=$("fwinfo");
  info.textContent=t("fw_checking",{f:f.name,n:f.size});
  f.arrayBuffer().then(function(buf){
    var u=new Uint8Array(buf),msg=null;
    if(u.length!==524288)msg=t("fw_size_err",{n:u.length});
    else if(u[0]!==0x00||u[1]!==0x40||u[2]!==0x02)msg=t("fw_magic_err");
    else{
      var crc=0;
      for(var i=0;i<u.length;i++){
        crc^=u[i];
        for(var b=0;b<8;b++)crc=(crc&1)?((crc>>>1)^0xA001):(crc>>>1);
      }
      if(crc!==0xB001)msg=t("fw_crc_err");
    }
    if(msg){
      info.innerHTML='<span style="color:var(--bad)">\u2715 '+esc(msg)+"</span>";
      return;
    }
    fwBuf=f;
    info.innerHTML='<span style="color:var(--ok)">\u2713 '+esc(t("fw_valid"))+"</span>";
    $("fwup").disabled=false;
  });
});
$("fwup").addEventListener("click",function(){
  if(!fwBuf)return;
  confirmModal(t("fw_q"),t("fw_d"),function(){
    var form=new FormData();
    form.append("uploadedfile",fwBuf,fwBuf.name);
    var xhr=new XMLHttpRequest();
    var prog=$("fwprog"),st=$("fwstat");
    var sent=false,settled=false,t0=Date.now(),pct=0;
    prog.style.display="";prog.value=0;
    $("fwup").disabled=true;
    function settle(fn){
      if(settled)return;
      settled=true;
      clearInterval(tick);
      prog.style.display="none";
      fn();
    }
    var tick=setInterval(function(){
      var s=Math.round((Date.now()-t0)/1000);
      st.textContent=sent?t("fw_finishing",{s:s}):t("fw_uploading",{p:pct,s:s});
    },500);
    st.textContent=t("fw_uploading",{p:0,s:0});
    xhr.upload.onprogress=function(e){
      if(e.lengthComputable){pct=Math.round(100*e.loaded/e.total);prog.value=pct;}
    };
    xhr.upload.onload=function(){
      sent=true;
      prog.removeAttribute("value");
    };
    xhr.onload=function(){settle(function(){
      if(xhr.status===200){
        st.textContent=t("fw_verified");
        fwSettle(st,true);
      }else{
        var why=(xhr.responseText||"").trim().split("\n")[0];
        st.textContent="\u2715 "+t("fw_rejected")+" (HTTP "+xhr.status+(why?": "+why:"")+")";
        $("fwup").disabled=false;
      }
    })};
    xhr.onerror=function(){settle(function(){
      if(!sent){st.textContent=t("fw_lost");$("fwup").disabled=false;return;}
      fwSettle(st,false);
    })};
    xhr.open("POST","/upload");
    xhr.send(form);
  });
});
/* knownGood: the firmware answered 200, so an early reply only means the
 * reset is still pending. Without a verdict an early reply means no reboot
 * happened, i.e. the image was rejected. Raw fetch: a 401 from the fresh
 * boot still counts as "the switch is back". */
function fwSettle(st,knownGood){
  var waited=3,down=false;
  function probe(){
    var ctl=("AbortController"in window)?new AbortController():null;
    var to=setTimeout(function(){if(ctl)ctl.abort()},2500);
    fetch("/information.json",{signal:ctl?ctl.signal:undefined,cache:"no-store"}).then(function(){
      clearTimeout(to);
      if(!down&&waited<=9){
        if(!knownGood){
          st.textContent="\u2715 "+t("fw_noreboot");
          $("fwup").disabled=false;
          return;
        }
        waited+=3;setTimeout(probe,3000);
        return;
      }
      st.textContent=t("fw_applied")+" \u2713";
      modal(t("fw_done_t"),h("p",{text:t("fw_done")}),
        [h("button",{class:"ctl pri",text:t("fw_login"),onclick:function(){location.href="/login.html"}})]);
    },function(){
      clearTimeout(to);
      down=true;
      waited+=3;
      st.textContent=t("fw_rebooting");
      if(waited>150){
        st.textContent=t("fw_timeout");
        $("fwup").disabled=false;
        return;
      }
      setTimeout(probe,3000);
    });
  }
  setTimeout(probe,3000);
}
tabHooks.fw={};

window.addEventListener("hashchange",function(){
  var id=location.hash.slice(1);
  if(TABS.some(function(tb){return tb.id===id})&&id!==curTab)showTab(id);
});
(function(){
  var id=location.hash.slice(1);
  if(!TABS.some(function(tb){return tb.id===id}))id="dash";
  pollInfo().catch(function(){});
  getText("/cmd_log").then(function(x){
    x=x.replace(/\0[\s\S]*$/,"").trim();
    if(x&&x.split(/\r?\n/).some(function(l){return isConfCmd(l.trim())}))setDirty(true);
  }).catch(function(){});
  showTab(id);
})();
