/* Merged WebUI script bundle (i18n, shared helpers, navigation,

 * and the per-page scripts in one file so the browser fetches it

 * once and gzip can compress across all of them). */

var LANG = {
  en: {
    nav_overview: 'Overview',
    nav_port_config: 'Port Configuration',
    nav_port_stat: 'Port Statistics',
    nav_l2: 'L2 Configuration',
    nav_mirror: 'Mirroring',
    nav_lag: 'Link Aggregation',
    nav_eee: 'EEE',
    nav_bandwidth: 'Bandwidth Limits',
    nav_system: 'System Settings',
    nav_fw_update: 'Firmware Update',

    port_name: 'Name',
    port_status: 'Status',
    port_not_enabled: 'Not enabled.',
    port_link_speed: 'Link speed',
    port_vendor: 'Vendor',
    port_model: 'Model',
    port_serial: 'Serial',
    port_temp: 'Temp',
    port_vcc: 'Vcc',
    port_tx_fault: 'TX-Fault',
    port_tx_disabled: 'TX-Disabled',
    port_tx_bias: 'TX-Bias',
    port_tx_power: 'TX-Power',
    port_rx_power: 'RX-Power',
    port_rx_los: 'RX-LOS',

    speed_disabled: 'Disabled',
    speed_down: 'Down',

    port_title: 'FreeSwitchOS Port Configuration',
    port_heading: 'Port Configuration',
    port_col_port: 'Port',
    port_col_name: 'Name',
    port_col_speed: 'Current Link Speed',
    port_col_set_speed: 'Set Speed',
    port_col_disabled: 'Disabled',
    port_col_apply: 'Apply',
    port_mtu_heading: 'Configure Maximum Frame Size (MTU) forwarded at Port',
    port_auto: 'Auto',
    port_2500m: '2500MBit/Full',
    port_1000m: '1000MBit/Full',
    port_100m_f: '100MBit/Full',
    port_100m_h: '100MBit/Half',
    port_10m_f: '10MBit/Full',
    port_10m_h: '10MBit/Half',
    port_apply: 'Apply',

    stat_title: 'FreeSwitchOS Port Statistics',
    stat_heading: 'Port Statistics',
    stat_detailed: 'Detailed Port Statistics',
    stat_close: 'Close',
    stat_col_port: 'Port',
    stat_col_name: 'Name',
    stat_col_link: 'link',
    stat_col_tx_good: 'TX Good',
    stat_col_tx_bad: 'TX Bad',
    stat_col_rx_good: 'RX Good',
    stat_col_rx_bad: 'RX Bad',
    stat_col_all: 'All Counters',
    stat_counter: 'Counter',
    stat_value: 'Value',
    stat_show: 'Show',

    vlan_title: 'FreeSwitchOS VLAN Configuration',
    vlan_heading: 'VLAN Configuration',
    vlan_select: 'VLAN Select:',
    vlan_choose: '— VLAN Choose —',
    vlan_id: 'VLAN ID:',
    vlan_get_config: 'Get Configuration',
    vlan_name: 'VLAN Name:',
    vlan_tagged: 'Tagged Ports',
    vlan_untagged: 'Untagged Ports',
    vlan_select_all: 'Select all',
    vlan_pvid: 'Use as default VLAN for incoming traffic (PVID)',
    vlan_update: 'Update / Create',
    vlan_configured: 'Configured VLANs',
    vlan_col_name: 'Name',
    vlan_col_member: 'Member Ports',
    vlan_col_tagged: 'Tagged Ports',
    vlan_col_untagged: 'Untagged Ports',
    vlan_col_pvid: 'PVID Ports',
    vlan_col_delete: 'Delete',
    vlan_set_id_first: 'Set VLAN ID first',
    vlan_delete_confirm: 'Delete VLAN ',

    lag_title: 'Link Aggregation Configuration',
    lag_heading: 'Link Aggregation Groups Configuration',
    lag_update: 'Update / Create',

    mirror_title: 'Mirror Configuration',
    mirror_heading: 'Mirror Configuration',
    mirror_enabled: 'Enabled:',
    mirror_port: 'Mirroring Port:',
    mirror_tx: 'Mirrored Ports (TX)',
    mirror_rx: 'Mirrored Ports (RX)',
    mirror_update: 'Update / Create',
    mirror_disable: 'Disable Mirroring',
    mirror_set_port_first: 'Set Mirroring Port first',
    mirror_select_ports: 'Select Mirrored Ports',

    eee_title: 'EEE Configuration',
    eee_heading: 'EEE Status',
    eee_advertising: 'Advertising',
    eee_partner: 'Link-Partner advertises',
    eee_port: 'Port',
    eee_active: 'Active?',
    eee_enable: 'Enable EEE',
    eee_disable: 'Disable EEE',
    eee_on: 'ON',
    eee_off: 'OFF',

    l2_title: 'FreeSwitchOS L2 Configuration',
    l2_heading: 'L2 Configuration',
    l2_col_port: 'Port',
    l2_col_type: 'Type',
    l2_col_remove: 'Remove Entry',
    l2_shown: 'Shown:',
    l2_delete: 'Delete',
    l2_static: 'static',
    l2_learned: 'learned',

    bw_title: 'Ingress and Egress Bandwidth',
    bw_heading: 'Ingress and Egress Bandwidth',
    bw_ingress: 'Ingress',
    bw_egress: 'Egress',
    bw_col_port: 'Port',
    bw_col_limit: 'Limit',
    bw_col_bandwidth: 'Bandwidth [kBit/s]',
    bw_col_flow: 'Flow Control',
    bw_col_apply: 'Apply',
    bw_unlimited: 'UNLIMITED',

    sys_title: 'System Settings',
    sys_tab_system: 'System',
    sys_tab_advanced: 'Advanced',
    sys_tab_console: 'Console',
    sys_heading: 'System Settings',
    sys_ip: 'IP address:',
    sys_model: 'Model:',
    sys_hostname: 'Hostname:',
    sys_apply: 'Apply',
    sys_netmask: 'Netmask:',
    sys_gateway: 'Gateway:',
    sys_language: 'Language:',
    sys_mgmt_vlan: 'Management VLAN:',
    sys_mgmt_untagged: 'untagged',
    sys_mgmt_confirm: 'Move switch management to VLAN ',
    sys_mgmt_warn: 'The switch will start tagging its own traffic with that VLAN. If the port you are connected through does not carry it, this page becomes unreachable and the setting can only be undone over the console. Continue?',
    sys_ip_note: 'When updating the above settings, remember to point your browser to the new IP afterwards:',
    sys_update: 'Update Settings',
    sys_save_label: 'Save all current settings to Flash:',
    sys_save: 'Save Settings to Flash',
    sys_advanced: 'Advanced Settings',
    sys_startup_config: 'Startup configuration:',
    sys_startup_warn: 'Be careful when saving the directly edited startup configuration, you can lock yourself out:',
    sys_clear_config: 'Clear Startup Config',
    sys_save_startup: 'Save Startup Settings to Flash',
    sys_reset: 'Reset Switch',
    sys_console: 'Console Command',
    sys_enter_cmd: 'Enter command:',
    sys_send_cmd: 'Send Command',
    sys_console_warn: 'Be careful when entering console commands, you can lock yourself out!',
    sys_invalid_ip: 'Invalid ip:',
    sys_reset_confirm: 'Are you sure you want to reset the switch?',
    sys_resetting: 'Switch is resetting. Please wait and refresh the page.',

    login_title: 'RTL Switch Login',
    login_heading: 'RTL Switch Login',
    login_wrong: 'Wrong password!',
    login_password: 'Password',
    login_login: 'Login',

    index_title: 'FreeSwitchOS Main Page',
    index_heading: 'Switch Configuration',
    index_settings: 'Settings',

    update_title: 'Firmware update',
    update_heading: 'Firmware Update',
    update_instruction: 'Choose a firmware update file to upload:',
    update_upload: 'Upload File',

    common_port: 'Port ',
    common_pkts: ' pkts',
  },

  ja: {
    nav_overview: '概要',
    nav_port_config: 'ポート設定',
    nav_port_stat: 'ポート統計',
    nav_l2: 'L2 設定',
    nav_mirror: 'ミラーリング',
    nav_lag: 'リンクアグリゲーション',
    nav_eee: 'EEE',
    nav_bandwidth: '帯域制限',
    nav_system: 'システム設定',
    nav_fw_update: 'ファームウェア更新',

    port_name: '名前',
    port_status: '状態',
    port_not_enabled: '無効',
    port_link_speed: 'リンク速度',
    port_vendor: 'ベンダー',
    port_model: 'モデル',
    port_serial: 'シリアル',
    port_temp: '温度',
    port_vcc: '電圧',
    port_tx_fault: 'TX 障害',
    port_tx_disabled: 'TX 無効',
    port_tx_bias: 'TX バイアス',
    port_tx_power: 'TX 電力',
    port_rx_power: 'RX 電力',
    port_rx_los: 'RX 信号ロス',

    speed_disabled: '無効',
    speed_down: 'リンクダウン',

    port_title: 'FreeSwitchOS ポート設定',
    port_heading: 'ポート設定',
    port_col_port: 'ポート',
    port_col_name: '名前',
    port_col_speed: '現在のリンク速度',
    port_col_set_speed: '速度設定',
    port_col_disabled: '無効',
    port_col_apply: '適用',
    port_mtu_heading: 'ポートの最大フレームサイズ (MTU) 設定',
    port_auto: '自動',
    port_2500m: '2500Mbps/全二重',
    port_1000m: '1000Mbps/全二重',
    port_100m_f: '100Mbps/全二重',
    port_100m_h: '100Mbps/半二重',
    port_10m_f: '10Mbps/全二重',
    port_10m_h: '10Mbps/半二重',
    port_apply: '適用',

    stat_title: 'FreeSwitchOS ポート統計',
    stat_heading: 'ポート統計',
    stat_detailed: '詳細ポート統計',
    stat_close: '閉じる',
    stat_col_port: 'ポート',
    stat_col_name: '名前',
    stat_col_link: 'リンク',
    stat_col_tx_good: 'TX 正常',
    stat_col_tx_bad: 'TX 異常',
    stat_col_rx_good: 'RX 正常',
    stat_col_rx_bad: 'RX 異常',
    stat_col_all: '全カウンタ',
    stat_counter: 'カウンタ',
    stat_value: '値',
    stat_show: '表示',

    vlan_title: 'FreeSwitchOS VLAN 設定',
    vlan_heading: 'VLAN 設定',
    vlan_select: 'VLAN 選択:',
    vlan_choose: '— VLAN 選択 —',
    vlan_id: 'VLAN ID:',
    vlan_get_config: '設定取得',
    vlan_name: 'VLAN 名:',
    vlan_tagged: 'タグ付きポート',
    vlan_untagged: 'タグ無しポート',
    vlan_select_all: 'すべて選択',
    vlan_pvid: '受信トラフィックのデフォルト VLAN (PVID)',
    vlan_update: '更新 / 作成',
    vlan_configured: '設定済み VLAN',
    vlan_col_name: '名前',
    vlan_col_member: 'メンバーポート',
    vlan_col_tagged: 'タグ付きポート',
    vlan_col_untagged: 'タグ無しポート',
    vlan_col_pvid: 'PVID ポート',
    vlan_col_delete: '削除',
    vlan_set_id_first: 'VLAN ID を先に設定してください',
    vlan_delete_confirm: 'VLAN 削除 ',

    lag_title: 'リンクアグリゲーション設定',
    lag_heading: 'リンクアグリゲーショングループ設定',
    lag_update: '更新 / 作成',

    mirror_title: 'ミラーリング設定',
    mirror_heading: 'ミラーリング設定',
    mirror_enabled: '有効:',
    mirror_port: 'ミラーポート:',
    mirror_tx: 'ミラー元ポート (TX)',
    mirror_rx: 'ミラー元ポート (RX)',
    mirror_update: '更新 / 作成',
    mirror_disable: 'ミラーリング無効化',
    mirror_set_port_first: 'ミラーポートを先に設定してください',
    mirror_select_ports: 'ミラー元ポートを選択してください',

    eee_title: 'EEE 設定',
    eee_heading: 'EEE 状態',
    eee_advertising: 'EEE アドバタイジング',
    eee_partner: 'リンクパートナー広告',
    eee_port: 'ポート',
    eee_active: '有効?',
    eee_enable: 'EEE 有効化',
    eee_disable: 'EEE 無効化',
    eee_on: 'オン',
    eee_off: 'オフ',

    l2_title: 'FreeSwitchOS L2 設定',
    l2_heading: 'L2 設定',
    l2_col_port: 'ポート',
    l2_col_type: 'タイプ',
    l2_col_remove: 'エントリ削除',
    l2_shown: 'Shown:',
    l2_delete: '削除',
    l2_static: '静的',
    l2_learned: '学習',

    bw_title: '入力/出力帯域制限',
    bw_heading: '入力/出力帯域制限',
    bw_ingress: '入力',
    bw_egress: '出力',
    bw_col_port: 'ポート',
    bw_col_limit: '制限',
    bw_col_bandwidth: '帯域 [kbps]',
    bw_col_flow: 'フロー制御',
    bw_col_apply: '適用',
    bw_unlimited: '制限無し',

    sys_title: 'システム設定',
    sys_tab_system: 'システム',
    sys_tab_advanced: '詳細設定',
    sys_tab_console: 'コンソール',
    sys_heading: 'システム設定',
    sys_ip: 'IP アドレス:',
    sys_model: 'モデル:',
    sys_hostname: 'ホスト名:',
    sys_apply: '適用',
    sys_netmask: 'ネットマスク:',
    sys_gateway: 'ゲートウェイ:',
    sys_language: '言語:',
    sys_mgmt_vlan: 'Management VLAN:',
    sys_mgmt_untagged: 'untagged',
    sys_mgmt_confirm: 'Move switch management to VLAN ',
    sys_mgmt_warn: 'The switch will start tagging its own traffic with that VLAN. If the port you are connected through does not carry it, this page becomes unreachable and the setting can only be undone over the console. Continue?',
    sys_ip_note: '上記設定を変更した場合は、ブラウザで新しい IP にアクセスしてください:',
    sys_update: '設定更新',
    sys_save_label: '現在の設定をフラッシュに保存:',
    sys_save: '設定をフラッシュに保存',
    sys_advanced: '詳細設定',
    sys_startup_config: '起動設定:',
    sys_startup_warn: '起動設定を直接編集する際は注意してください。ロックアウトされる可能性があります:',
    sys_clear_config: '起動設定クリア',
    sys_save_startup: '起動設定をフラッシュに保存',
    sys_reset: 'スイッチ再起動',
    sys_console: 'コンソールコマンド',
    sys_enter_cmd: 'コマンド入力:',
    sys_send_cmd: 'コマンド送信',
    sys_console_warn: 'コンソールコマンドは注意して入力してください。ロックアウトされる可能性があります!',
    sys_invalid_ip: '無効な IP: ',
    sys_reset_confirm: 'スイッチを再起動してもよろしいですか？',
    sys_resetting: 'スイッチを再起動中です。しばらく待ってからページをリロードしてください。',

    login_title: 'RTL スイッチ ログイン',
    login_heading: 'RTL スイッチ ログイン',
    login_wrong: 'パスワードが違います!',
    login_password: 'パスワード',
    login_login: 'ログイン',

    index_title: 'FreeSwitchOS メインページ',
    index_heading: 'スイッチ設定',
    index_settings: '設定',

    update_title: 'ファームウェア更新',
    update_heading: 'ファームウェア更新',
    update_instruction: 'アップロードするファームウェアファイルを選択:',
    update_upload: 'ファイルをアップロード',

    common_port: 'ポート ',
    common_pkts: ' pkts',
  },

  zh: {
    nav_overview: '概览',
    nav_port_config: '端口配置',
    nav_port_stat: '端口统计',
    nav_l2: 'L2 配置',
    nav_mirror: '端口镜像',
    nav_lag: '链路聚合',
    nav_eee: 'EEE',
    nav_bandwidth: '带宽限制',
    nav_system: '系统设置',
    nav_fw_update: '固件升级',

    port_name: '名称',
    port_status: '状态',
    port_not_enabled: '未启用。',
    port_link_speed: '链路速率',
    port_vendor: '厂商',
    port_model: '型号',
    port_serial: '序列号',
    port_temp: '温度',
    port_vcc: '供电电压',
    port_tx_fault: 'TX 故障',
    port_tx_disabled: 'TX 禁用',
    port_tx_bias: 'TX 偏置电流',
    port_tx_power: 'TX 光功率',
    port_rx_power: 'RX 光功率',
    port_rx_los: 'RX 信号丢失',

    speed_disabled: '禁用',
    speed_down: '未连接',

    port_title: 'FreeSwitchOS 端口配置',
    port_heading: '端口配置',
    port_col_port: '端口',
    port_col_name: '名称',
    port_col_speed: '当前链路速率',
    port_col_set_speed: '设置速率',
    port_col_disabled: '禁用',
    port_col_apply: '应用',
    port_mtu_heading: '配置端口转发的最大帧大小 (MTU)',
    port_auto: '自动',
    port_2500m: '2500Mbps/全双工',
    port_1000m: '1000Mbps/全双工',
    port_100m_f: '100Mbps/全双工',
    port_100m_h: '100Mbps/半双工',
    port_10m_f: '10Mbps/全双工',
    port_10m_h: '10Mbps/半双工',
    port_apply: '应用',

    stat_title: 'FreeSwitchOS 端口统计',
    stat_heading: '端口统计',
    stat_detailed: '详细端口统计',
    stat_close: '关闭',
    stat_col_port: '端口',
    stat_col_name: '名称',
    stat_col_link: '链路',
    stat_col_tx_good: 'TX 正常包',
    stat_col_tx_bad: 'TX 错误包',
    stat_col_rx_good: 'RX 正常包',
    stat_col_rx_bad: 'RX 错误包',
    stat_col_all: '所有计数器',
    stat_counter: '计数器',
    stat_value: '值',
    stat_show: '查看',

    vlan_title: 'FreeSwitchOS VLAN 配置',
    vlan_heading: 'VLAN 配置',
    vlan_select: 'VLAN 选择:',
    vlan_choose: '-- 请选择 VLAN --',
    vlan_id: 'VLAN ID:',
    vlan_get_config: '获取配置',
    vlan_name: 'VLAN 名称:',
    vlan_tagged: 'Tagged 端口',
    vlan_untagged: 'Untagged 端口',
    vlan_select_all: '全选',
    vlan_pvid: '作为入方向流量的默认 VLAN (PVID)',
    vlan_update: '更新 / 创建',
    vlan_configured: '已配置 VLAN',
    vlan_col_name: '名称',
    vlan_col_member: '成员端口',
    vlan_col_tagged: 'Tagged 端口',
    vlan_col_untagged: 'Untagged 端口',
    vlan_col_pvid: 'PVID 端口',
    vlan_col_delete: '删除',
    vlan_set_id_first: '请先设置 VLAN ID',
    vlan_delete_confirm: '删除 VLAN ',

    lag_title: '链路聚合配置',
    lag_heading: '链路聚合组配置',
    lag_update: '更新 / 创建',

    mirror_title: '端口镜像配置',
    mirror_heading: '端口镜像配置',
    mirror_enabled: '启用:',
    mirror_port: '镜像目的端口:',
    mirror_tx: '被镜像端口 (TX)',
    mirror_rx: '被镜像端口 (RX)',
    mirror_update: '更新 / 创建',
    mirror_disable: '禁用端口镜像',
    mirror_set_port_first: '请先设置镜像目的端口',
    mirror_select_ports: '请选择被镜像端口',

    eee_title: 'EEE 配置',
    eee_heading: 'EEE 状态',
    eee_advertising: '本端通告',
    eee_partner: '链路伙伴通告',
    eee_port: '端口',
    eee_active: '已生效?',
    eee_enable: '启用 EEE',
    eee_disable: '禁用 EEE',
    eee_on: '开',
    eee_off: '关',

    l2_title: 'FreeSwitchOS L2 配置',
    l2_heading: 'L2 配置',
    l2_col_port: '端口',
    l2_col_type: '类型',
    l2_col_remove: '删除条目',
    l2_shown: 'Shown:',
    l2_delete: '删除',
    l2_static: '静态',
    l2_learned: '动态学习',

    bw_title: '入方向/出方向带宽限制',
    bw_heading: '入方向/出方向带宽限制',
    bw_ingress: '入方向',
    bw_egress: '出方向',
    bw_col_port: '端口',
    bw_col_limit: '限速',
    bw_col_bandwidth: '带宽 [kbit/s]',
    bw_col_flow: '流量控制',
    bw_col_apply: '应用',
    bw_unlimited: '不限速',

    sys_title: '系统设置',
    sys_tab_system: '系统',
    sys_tab_advanced: '高级',
    sys_tab_console: '控制台',
    sys_heading: '系统设置',
    sys_ip: 'IP 地址:',
    sys_model: '型号:',
    sys_hostname: '主机名:',
    sys_apply: '应用',
    sys_netmask: '子网掩码:',
    sys_gateway: '网关:',
    sys_language: '语言:',
    sys_mgmt_vlan: 'Management VLAN:',
    sys_mgmt_untagged: 'untagged',
    sys_mgmt_confirm: 'Move switch management to VLAN ',
    sys_mgmt_warn: 'The switch will start tagging its own traffic with that VLAN. If the port you are connected through does not carry it, this page becomes unreachable and the setting can only be undone over the console. Continue?',
    sys_ip_note: '更新上述设置后，请使用新的 IP 地址重新访问管理界面:',
    sys_update: '更新设置',
    sys_save_label: '将当前全部设置保存到 Flash:',
    sys_save: '保存设置到 Flash',
    sys_advanced: '高级设置',
    sys_startup_config: '启动配置:',
    sys_startup_warn: '直接编辑启动配置时请谨慎，错误配置可能导致无法访问设备:',
    sys_clear_config: '清除启动配置',
    sys_save_startup: '保存启动配置到 Flash',
    sys_reset: '重启交换机',
    sys_console: '控制台命令',
    sys_enter_cmd: '输入命令:',
    sys_send_cmd: '发送命令',
    sys_console_warn: '输入控制台命令时请谨慎，错误命令可能导致无法访问设备!',
    sys_invalid_ip: '无效 IP: ',
    sys_reset_confirm: '确定要重启交换机吗?',
    sys_resetting: '交换机正在重启。请稍候并刷新页面。',

    login_title: 'RTL 交换机登录',
    login_heading: 'RTL 交换机登录',
    login_wrong: '密码错误!',
    login_password: '密码',
    login_login: '登录',

    index_title: 'FreeSwitchOS 主页',
    index_heading: '交换机配置',
    index_settings: '设置',

    update_title: '固件升级',
    update_heading: '固件升级',
    update_instruction: '请选择要上传的固件文件:',
    update_upload: '上传文件',

    common_port: '端口 ',
    common_pkts: ' 个包',
  }
};

var rtlLang = (function() {
  var saved = localStorage.getItem('rtl_lang');
  if (saved && LANG[saved]) return saved;
  var browser = (navigator.language || navigator.userLanguage || 'en').substring(0, 2);
  return LANG[browser] ? browser : 'en';
})();

function t(key) {
  return LANG[rtlLang][key] || LANG['en'][key] || key;
}

function setLang(lang) {
  if (LANG[lang]) {
    localStorage.setItem('rtl_lang', lang);
    rtlLang = lang;
    document.querySelectorAll('[data-i18n]').forEach(function(el) {
      applyTranslation(el);
    });
  }
}

function applyTranslation(el) {
  var key = el.getAttribute('data-i18n');
  if (!key) return;
  if (el.tagName === 'INPUT' && (el.type === 'submit' || el.type === 'button')) {
    el.value = t(key);
  } else if (el.tagName === 'OPTION') {
    el.textContent = t(key);
  } else if (el.tagName === 'TITLE') {
    el.textContent = t(key);
  } else {
    el.innerHTML = t(key);
  }
}

document.addEventListener('DOMContentLoaded', function() {
  document.querySelectorAll('[data-i18n]').forEach(function(el) {
    applyTranslation(el);
  });
});

var txG = new BigInt64Array(10);
var txB = new BigInt64Array(10);
var rxG = new BigInt64Array(10);
var rxB = new BigInt64Array(10);
const linkS = [function(){return t('speed_disabled')}, function(){return t('speed_down')}, "10M", "100M", "1000M", "500M", "10G", "2.5G", "5G"];
var pState = new Int8Array(10);
var pIsSFP = new Int8Array(10);
var pAdvertised = new Int8Array(10);
var numPorts = 0;
function linkText(idx) { var v = linkS[idx]; return typeof v === 'function' ? v() : v; }
var logToPhysPort = new Int8Array(10);
var physToLogPort = new Int8Array(10);
var portNames = new Array(10);
var currentRequests = [];
var currentCallback;
function drawPorts() {
  var f = document.getElementById('ports');
  console.log("DRAWING PORTS: ", numPorts);
  for (let i = 0; i < numPorts; i++) {
    console.log("DRAWING isSFP: ", pIsSFP[i]);
    const d = document.createElement("div");
    d.classList.add('tooltip');
    const s = document.createElement("span");
    s.classList.add("tooltiptext");
    s.innerHTML = t('common_port');
    s.id="tt_" + (i+1);
    const l = document.createElement("object");
    d.appendChild(l);
    d.appendChild(s);
    l.type = "image/svg+xml";
    if (!pIsSFP[i]) {
      l.data = "port.svg";
      l.width ="40";
      l.height ="40";
    } else {
      l.data = "sfp.svg";
      l.width = "60";
      l.height = "60";
    }
    l.id="port" + (i+1);
    f.appendChild(d);
  }
}

function parseUint16(val) {
  return parseInt(val, 16) & 0xffff;
}

function parseInt16(val) {
  let valInt = parseInt(val, 16);
  let num = valInt & 0x7fff;
  if (valInt & 0x8000) {
    return num - 0x8000;
  }
  return num;
}

function applyCalibrationSlopeOffset(val, cal) {
  if (typeof cal !== 'string') {
    return val;
  }
  if (cal.startsWith("0x")) {
    cal = cal.substring(2);
  }
  if (cal.length != 8) {
    return val;
  }
  let slope = parseUint16(cal.substring(0, 4)) / 256;
  let offset = parseInt16(cal.substring(4, 8));
  return slope * val + offset;
}

function applyRxPowerCalibration(val, cal) {
  if (typeof cal !== 'string') {
    return val;
  }
  if (cal.startsWith("0x")) {
    cal = cal.substring(2);
  }
  if (cal.length != 40) {
    return val;
  }
  let bytes = cal.match(/.{1,2}/g).map(function (x) { return parseInt(x, 16); });
  let view = new DataView(new Uint8Array(bytes).buffer);
  return view.getFloat32(0) * Math.pow(val, 4)
    + view.getFloat32(4) * Math.pow(val, 3)
    + view.getFloat32(8) * Math.pow(val, 2)
    + view.getFloat32(12) * val
    + view.getFloat32(16);
}

function decodeSfpTemp(val, cal) {
  let temp = parseInt16(val);
  return applyCalibrationSlopeOffset(temp, cal) / 256;
}

function decodeSfpVcc(val, cal) {
  let vcc = parseUint16(val);
  return applyCalibrationSlopeOffset(vcc, cal) / 10000;
}

function decodeSfpTxBias(val, cal) {
  let bias = parseUint16(val);
  return applyCalibrationSlopeOffset(bias, cal) / 500;
}

function decodeSfpTxPower(val, cal) {
  let txPower = parseUint16(val);
  return applyCalibrationSlopeOffset(txPower, cal) / 10000;
}

function decodeSfpRxPower(val, cal) {
  let rxPower = parseUint16(val);
  return applyRxPowerCalibration(rxPower, cal) / 10000;
}

function convertPowerTodBm(val) {
  return 10 * Math.log10(val);
}

function update(callback) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    console.log("IN UPDATE ");
    if (this.readyState == 4 && this.status == 401)
	    document.location = "/login.html"
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      if (!numPorts) {
	numPorts = s.length;
	for (let i = 0; i < s.length; i++)
	  pIsSFP[s[i].portNum-1] = s[i].isSFP;
	drawPorts();
      }
      console.log("RES:", JSON.stringify(s));
      for (let i = 0; i < s.length; i++) {
	p = s[i];
	let n = p.portNum;
	logToPhysPort[p.logPort] = n;
	physToLogPort[n-1] = p.logPort;
	portNames[p.logPort] = p.name;
	let pid = "port" + n;
	let ttid = "tt_" + n;
	n--;
	txG[n] = BigInt(p.txG); txB[n] = BigInt(p.txB); rxG[n] = BigInt(p.rxG); rxB[n] = BigInt(p.rxB);
	var psvg = document.getElementById(pid);
	var tt = document.getElementById(ttid);
	if (psvg == null || !psvg.contentDocument)
	  continue;
	var bgs = psvg.contentDocument.getElementsByClassName("bg");
	var leds = psvg.contentDocument.getElementsByClassName("led");
        if (leds[0] == null || leds[0].style == null)
          continue;
	const portName = p.name || portNames[p.logPort] || '';
	var iHTML = "<table border=\"0\" class=\"tt_table\">";
	if (portName) iHTML += "<tr><td align=\"left\">" + t('port_name') + "</td><td>:</td><td>" + portName + "</td></tr>";
	if (p.enabled == 0) {
	  pState[n] = -1;
	  bgs[0].style.fill = "red";
	  leds[0].style.fill = "black"; leds[1].style.fill = "black";
	  psvg.style.opacity = 0.4;
	  iHTML += "<tr><td align=\"left\">" + t('port_status') + "</td><td>:</td><td>" + t('port_not_enabled') + "</td></tr>";
	  iHTML += "</table>";
	  tt.innerHTML = iHTML;
	} else {
	  psvg.style.opacity = 1.0;
	  pState[n] = p.link;
	  if (p.link == 5 || p.link == 7) {
	    leds[0].style.fill = "green"; leds[1].style.fill = "blue";
	  } else if (p.link == 4 || p.link == 6) {
	    leds[0].style.fill = "green"; leds[1].style.fill = "orange";
	  } else if (p.link == 1 || p.link == 2 || p.link == 3) {
	    leds[0].style.fill = "green"; leds[1].style.fill = "green";
	  } else {
	    leds[0].style.fill = "black"; leds[1].style.fill = "black";
	    psvg.style.opacity = 0.4
	  }
	  iHTML += "<tr><td align=\"left\">" + t('port_link_speed') + "</td><td>:</td><td>" + linkText(p.link + 1) + "</td></tr>";
	  if (p.isSFP) {
	    pAdvertised[n] = 0;
	    const hasExtendedStatus = p.sfp_options & 0x40;
	    iHTML += "<tr><td>" + t('port_vendor') + "</td><td>:</td><td>" + p.sfp_vendor + "</td></tr>";
	    iHTML += "<tr><td>" + t('port_model') + "</td><td>:</td><td>" + p.sfp_model + "</td></tr>";
	    iHTML += "<tr><td>" + t('port_serial') + "</td><td>:</td><td>" + p.sfp_serial + "</td></tr>";
	    if (hasExtendedStatus) {
	      let txPower = decodeSfpTxPower(p.sfp_txpower, p.sfp_txpower_cal);
	      let txPowerdBm = convertPowerTodBm(txPower);
	      let rxPower = decodeSfpRxPower(p.sfp_rxpower, p.sfp_rxpower_cal);
	      let rxPowerdBm = convertPowerTodBm(rxPower);
	      iHTML += "<tr><td>" + t('port_temp') + "</td><td>:</td><td>" + decodeSfpTemp(p.sfp_temp, p.sfp_temp_cal).toFixed(2) + "&#8239;&#8451;</td></tr>";
	      iHTML += "<tr><td>" + t('port_vcc') + "</td><td>:</td><td>" + decodeSfpVcc(p.sfp_vcc, p.sfp_vcc_cal).toFixed(2) + "&#8239;V</td></tr>";
	      iHTML += "<tr><td>" + t('port_tx_fault') + "</td><td>:</td><td>" + (Boolean(Number(p.sfp_state) & 0x4)) + "</td></tr>";
	      iHTML += "<tr><td>" + t('port_tx_disabled') + "</td><td>:</td><td>" + (Boolean(Number(p.sfp_state) & 0x80)) + "</td></tr>";
	      iHTML += "<tr><td>" + t('port_tx_bias') + "</td><td>:</td><td>" + decodeSfpTxBias(p.sfp_txbias, p.sfp_txbias_cal).toFixed(1) + "&#8239;mA</td></tr>";
	      iHTML += "<tr><td>" + t('port_tx_power') + "</td><td>:</td><td>" + txPower.toFixed(3) + "&#8239;mW / " + txPowerdBm.toFixed(2) + "&#8239;dBm</td></tr>";
	      iHTML += "<tr><td>" + t('port_rx_power') + "</td><td>:</td><td>" + rxPower.toFixed(3) + "&#8239;mW / " + rxPowerdBm.toFixed(2) + "&#8239;dBm</td></tr>";
	    }
	    // Not all devices & modules have LOS pin...
	    const rx_los_pin = p.sfp_los !== null ? Boolean(Number(p.sfp_los)) : null;
	    const rx_los_module = hasExtendedStatus ? Boolean(Number(p.sfp_state) & 0x2) : null;
	    if (rx_los_module !== null || rx_los_pin !== null) {
	      iHTML += `<tr><td>` + t('port_rx_los') + `</td><td>:</td><td>${rxLosHTML(rx_los_pin, rx_los_module)}</td></tr>`;
	    }
	  } else {
	    pAdvertised[n] = parseInt(p.adv, 2);
	  };
	  iHTML += "</table>";
	  tt.innerHTML = iHTML;
	}}
	if (callback)
	  callback();
	}};
	xhttp.open("GET", "/status.json", true);
	xhttp.timeout = 5000;
	sendXHTTP(xhttp);
}

function rxLosHTML(pinStatus, moduleStatus) {
  if (moduleStatus !== null && pinStatus !== null && moduleStatus !== pinStatus) {
    return `pin=${pinStatus}<br/>mod=${moduleStatus}<br/>❗❗❗❗`;
  }

	// Returns first non null value
  return moduleStatus ?? pinStatus;
}

function callbackXHTTP()
{
  x = currentRequests.shift();
  x.onreadystatechange = currentCallback;
  x.onreadystatechange();
  if (currentRequests.length === 0)
    return;
  x = currentRequests[0];
  currentCallback = x.onreadystatechange;
  x.onreadystatechange = callbackXHTTP;
  var retries = 10;
  while (retries) {
    try {
      setTimeout(() => {
              x.send();
              console.log("B1");
      }, 20);
    } catch (error) {
      retries--;
      setTimeout(() => {
        console.log(`Retry ${retries}/${maxRetries} failed: ${error.message}`);
      }, 200);
      if (retries < 1) {
        throw error;
      }
    }
    console.log("B2");
    return;
  }
}

function sendXHTTP(x)
{
  console.log("sendXHTTP ", x);
  if (currentRequests.length === 0) {
    currentRequests.push(x);
    currentCallback = x.onreadystatechange;
    x.onreadystatechange = callbackXHTTP;
    var retries = 10;
    while (retries) {
      try {
        x.send();
        console.log("A1");
      } catch (error) {
        retries--;
        setTimeout(() => {
          console.log(`Retry ${retries}/${maxRetries} failed: ${error.message}`);
        }, 200);
        if (retries < 1) {
          throw error;
        }
      }
      console.log("A2");
      return;
    }
    console.log("A3");
    return;
  }
  currentRequests.push(x);
}

document.addEventListener('DOMContentLoaded', function() {
  var sidebarEl = document.getElementById('sidebar');
  if (sidebarEl)
    sidebarEl.innerHTML =
   "<ul><li><a href='#/overview' data-i18n='nav_overview'>Overview</a></li>"
   + "<li><a href='#/ports' data-i18n='nav_port_config'>Port Configuration</a></li>"
   + "<li><a href='#/stat' data-i18n='nav_port_stat'>Port Statistics</a></li>"
   + "<li><a href='#/vlan' >VLAN</a></li>"
   + "<li><a href='#/l2' data-i18n='nav_l2'>L2 Configuration</a></li>"
   + "<li><a href='#/mirror' data-i18n='nav_mirror'>Mirroring</a></li>"
   + "<li><a href='#/lag' data-i18n='nav_lag'>Link Aggregation</a></li>"
   + "<li><a href='#/eee' data-i18n='nav_eee'>EEE</a></li>"
   + "<li><a href='#/bandwidth' data-i18n='nav_bandwidth'>Bandwidth Limits</a></li>"
   + "<li><a href='#/system' data-i18n='nav_system'>System Settings</a></li>"
   + "<li><a href='#/update' data-i18n='nav_fw_update'>Firmware Update</a></li></ul>";
});

document.addEventListener('DOMContentLoaded', function() {
  var links = document.querySelectorAll('#sidebar a[data-i18n]');
  links.forEach(function(el) {
    var key = el.getAttribute('data-i18n');
    if (key) el.textContent = t(key);
  });
});

/* Single-page navigation: the sidebar links set the URL hash
 * ("#/ports" and friends) and the matching section in index.html is
 * shown.  Each section initialises on first display, and its polling
 * intervals run only while the section is visible. */
var sectionIntervals = [];
var sectionInits = {};

function setSectionInterval(fn, ms) {
  sectionIntervals.push(setInterval(fn, ms));
}

function clearSectionIntervals() {
  sectionIntervals.forEach(function(t) { clearInterval(t); });
  sectionIntervals = [];
  if (l2Timer) {
    clearTimeout(l2Timer);
    l2Timer = null;
  }
}

function showSection(name) {
  document.querySelectorAll('.page').forEach(function(el) { el.style.display = 'none'; });
  var sec = document.getElementById('page-' + name);
  if (sec) sec.style.display = 'block';
  clearSectionIntervals();
  if (sectionInits[name])
    sectionInits[name]();
}

window.addEventListener('hashchange', function() {
  showSection((location.hash || '#/overview').replace(/^#\//, ''));
});

document.addEventListener('DOMContentLoaded', function() {
  showSection((location.hash || '#/overview').replace(/^#\//, ''));
});

sectionInits.overview = function() {
  update( () => {
    setSectionInterval(update, 2000);
  });
};

/* System page tabs (system.html section of index.html) */
function openTab(evt, tabId) {
  document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.getElementById(tabId).classList.add('active');
  evt.currentTarget.classList.add('active');
}


var configInterval = Number();
var configuration = [];
const conf_cmds = [
  /^ip\s+(\d{1,3}\.){3}\d{1,3}$/,
  /^ip\s+dhcp$/,
  /^gw\s+(\d{1,3}\.){3}\d{1,3}$/,
  /^netmask\s+(\d{1,3}\.){3}\d{1,3}$/,
  /^syslog\s+(on|off)$/,
  /^syslog\s+ip\s+(\d{1,3}\.){3}\d{1,3}$/,
  /^passwd\s+\S+$/,
  /^vlan\s+\d{1,4}\s+d$/,
  /^vlan\s+\d{1,4}\s+mgmt$/,
  /^vlan\s+\d{1,4}(\s+[a-zA-Z]\w*)?(\s+\d{1,2}[tu]?)+$/,
  /^pvid\s+\d{1,2}\s+\d{1,4}$/,
  /^ingress(\s+\d{1,2}[tua])+$/,
  /^ingress\s+[tua]$/,
  /^port\s+\d{1,2}\s+(10m|100m|1g|2g5|5g|10g|auto|on|off)(\s+(half|full))?$/,
  /^port\s+\d{1,2}\s+name\s+\S+$/,
  /^eee(\s+\d{1,2})?\s+(on|off)$/,
  /^mirror(\s+\d{1,2})(\s+\d{1,2}[tr]?)+$/,
  /^lag\s+\d(\s+\d{1,2})+$/,
  /^laghash\s+\d(\s+\w+)+$/,
  /^isolate\s+\d{1,2}(\s+(off|\d{1,2}))+$/,
  /^stp\s+(on|off)$/,
  /^igmp\s+(on|off)$/,
  /^mtu\s+\d{1,2}\s+\d+$/,
  /^bw\s+(in|out)\s+\d{1,2}\s+\S+$/,
  /^hostname\s+.{1,23}$/,
];
const conf_overwrite = [
  /^ip\b/,
  /^gw\b/,
  /^netmask\b/,
  /^syslog\s+ip\b/,
  /^syslog\b/,
  /^passwd\b/,
  /^vlan\s+\d{1,4}\s+mgmt$/,
  /^vlan\s+\d{1,4}(?!\s+mgmt\b)/,
  /^pvid\s+\d{1,2}\b/,
  /^ingress\b/,
  /^port\s+\d{1,2}(?!\s+name\b)/,
  /^port\s+\d{1,2}\s+name\b/,
  /^eee\s+\d{1,2}\b/,
  /^eee\b/,
  /^mirror\b/,
  /^lag\s+\d+\b/,
  /^laghash\b/,
  /^isolate\s+\d{1,2}\b/,
  /^stp\b/,
  /^igmp\b/,
  /^mtu\s+\d{1,2}\b/,
  /^bw\s+(in|out)\s+\d{1,2}\b/,
  /^hostname\b/,
];

function parseConf(s){
  var a = s.split(/\r\n|\n/);
  for (var l = 0; l < a.length; l++) {
    var line = a[l].trim().replace(/\s+/g, ' ');
    if (!line.length) continue;
    const deleteMatch = line.match(/^vlan\s+(\d{1,4})\s+d$/);
    if (deleteMatch) {
      const prefix = "vlan " + deleteMatch[1] + " ";
      configuration = configuration.filter(c => !c.startsWith(prefix));
      continue;
    }
    console.log(l + ' --> ' + line);
    var ignore = true;
    for (const x of conf_cmds)
      if (x.test(line)) { ignore = false; break; }
    if (ignore) continue;
    for (const x of conf_overwrite) {
      if (x.test(line)) {
        let m = line.match(x);
        let matchStr = m[0];
        configuration = configuration.filter(item =>
          !(item === matchStr || (item.startsWith(matchStr + " ") && !item.endsWith(" mgmt") && !item.startsWith(matchStr + " name "))));
        break;
      }
    }
    // Only one management VLAN can be active, so drop any previous mgmt entry
    if (/^vlan\s+\d{1,4}\s+mgmt$/.test(line))
      configuration = configuration.filter(item => !/^vlan\s+\d{1,4}\s+mgmt$/.test(item));
    configuration.push(line);
  }
  console.log("Configuration now:");
  for (const x of configuration) { console.log(x); }
}

async function fetchConfig() {
  try {
    const response = await fetch('/config');
    console.log("CONFIG: ", response);
    const t = await response.text();
    return t;
  } catch(err) {
    console.error("Error: ", err);
  }
}

async function fetchCmdLog() {
  try {
    const response = await fetch('/cmd_log');
    console.log("CMD-Log: ", response);
    const t = await response.text();
    return t;
  } catch(err) {
    console.error("Error: ", err);
    return "";
  }
}

var systemInterval = Number();
var isSaving = false;
const ips = ["ip", "netmask", "gw"];

function changeLang() {
  var lang = document.getElementById('lang-select').value;
  setLang(lang);
}

function checkIp(ip) {
  const ipv4 = /^(\d{1,3}\.){3}\d{1,3}$/;
  if (!ipv4.test(ip)) {alert(t('sys_invalid_ip') + ip); return false };
  return true;
}

async function ipSub() {
  for (let i=0;i<3;i++) {
    if (!checkIp(document.getElementById(ips[i]).value))
      return;
  }
  var cmd = '';
  for (let i=0; i<3;i++){
    cmd += ips[i]+' '+document.getElementById(ips[i]).value+'\n';
  }
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
    fetchIP();
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

async function cmdSub() {
  var cmd = document.getElementById('console_cmd').value;
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
  } catch(err) {
      console.error(`Error: ${err}`);
  }
}


async function hostSub() {
  const h = document.getElementById("hostname").value;
  try { await fetch('/cmd', { method: 'POST', body: "hostname " + h }); }
  catch(err) { console.error(`Error: ${err}`); }
  fetchIP();
}


async function sendConfig(c) {
  if (isSaving) return;
  isSaving = true;
  clearInterval(systemInterval);
  const form = new FormData();
  form.append("MAX_FILE_SIZE", "4096");
  form.append("configuration", new Blob([c], {type: "application/octet-stream"}), "config.txt");
  try {
    const response = await fetch('/config', {
      method: 'POST',
      body: form
    });
    console.log('Completed!', response);
    try {
      await fetch('/cmd_log_clear', { method: 'GET' });
    } catch(e) {}
  } catch(err) {
    console.error(`Error: ${err}`);
  } finally {
    isSaving = false;
    systemInterval = setInterval(fetchIP, 1000);
  }
}


async function flashSave() {
  configuration = [];
  const savedConfig = await fetchConfig();
  const cmdLog = await fetchCmdLog();
  if (savedConfig) parseConf(savedConfig);
  if (cmdLog) parseConf(cmdLog);
  const body = configuration.join('\n') + '\n';
  console.log("CONFIGURATION to save: ", body);
  await sendConfig(body);
}

async function flashStartupSave() {
  var configContent = document.getElementById("config_display").value;
  console.log("CONFIGURATION to save: ", configContent);
  sendConfig(configContent);
  // Clear the command log 1 second after initiating the config save
  setTimeout(() => {
    fetch('/cmd_log_clear', { method: 'GET' })
      .then(response => console.log('Command log cleared', response))
      .catch(err => console.error('Error clearing command log:', err));
  }, 1000);
}

function clearConfig() {
  document.getElementById("config_display").value = "";
  
  // Validate and populate with current IP settings
  for (let i=0; i<3; i++) {
    if (!checkIp(document.getElementById(ips[i]).value))
      return;
  }
  
  var configLines = "";
  for (let i=0; i<3; i++){
    var cmd = ips[i]+' '+document.getElementById(ips[i]).value;
    configLines += cmd + "\n";
  }
  
  document.getElementById("config_display").value = configLines;
}

function fetchIP() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("IP: ", s);
      document.getElementById("ip").value=s.ip_address;
      document.getElementById("netmask").value=s.ip_netmask;
      document.getElementById("gw").value=s.ip_gateway;
      document.getElementById("hostname").value=s.hostname;
      document.getElementById("model").textContent=s.hw_ver;
      loadMgmtVlan();
      clearInterval(systemInterval);
      // Fetch and populate the config textbox
      fetchConfig().then((configText) => {
        let fullConfig = configText;
        // Fetch and append cmd_log
        //return fetchCmdLog().then((cmdLogText) => {
        //  if (cmdLogText) {
        //    fullConfig = fullConfig + cmdLogText;
        //  }
        document.getElementById("config_display").value = fullConfig;
        });
      };
    }
  xhttp.open("GET", `/information.json`, true);
  xhttp.send();
}

function resetSwitch() {
  if (!confirm(t('sys_reset_confirm'))) {
    return;
  }
  fetch('/reset', { method: 'GET' }).catch(() => {});
  setTimeout(() => {
    alert(t('sys_resetting'));
  }, 3000);
}

sectionInits.system = function() {
  var langSel = document.getElementById('lang-select');
  if (langSel) langSel.value = rtlLang;
  systemInterval = setSectionInterval(fetchIP, 1000);
};


var mgmtVlanCurrent = 0;

function loadMgmtVlan() {
  var sel = document.getElementById('mgmtvlan');
  if (!sel) return;
  fetch('/vlanlist').then(function(r) { return r.json(); }).then(function(d) {
    var cur = d.mgmt || 0;
    var list = d.vlan || [];
    mgmtVlanCurrent = cur;
    sel.innerHTML = '';
    if (!cur) {
      var none = document.createElement('option');
      none.value = 0; none.disabled = true;
      none.textContent = t('sys_mgmt_untagged');
      sel.appendChild(none);
    }
    for (var i = 0; i < list.length; i++) {
      var o = document.createElement('option');
      o.value = list[i].id;
      o.textContent = list[i].name ? (list[i].id + ' (' + list[i].name + ')') : list[i].id;
      sel.appendChild(o);
    }
    sel.value = cur;
  }).catch(function(err) { console.error('VLAN list failed:', err); });
}

function mgmtVlanChanged() {
  var sel = document.getElementById('mgmtvlan');
  var id = parseInt(sel.value, 10);
  if (!id || id === mgmtVlanCurrent) return;
  if (!confirm(t('sys_mgmt_confirm') + id + '.\n\n' + t('sys_mgmt_warn'))) {
    sel.value = mgmtVlanCurrent;
    return;
  }
  fetch('/cmd', { method: 'POST', body: 'vlan ' + id + ' mgmt' })
    .then(function() { mgmtVlanCurrent = id; })
    .catch(function(err) { console.error('Set management VLAN failed:', err); sel.value = mgmtVlanCurrent; });
}

var mtus = new Int16Array(10);
var clicked = new Int8Array(10);
function createPortTable() {
  var tbl = document.getElementById('speedtable');
   if (tbl.rows.length <= 2 && numPorts) {
      const sSelect = '<select name="speed_sel" id="speed_sel">'
       + '<option value="auto">' + t('port_auto') + '</option>'
       + '<option value="2g5">' + t('port_2500m') + '</option>'
       + '<option value="1g">' + t('port_1000m') + '</option>'
       + '<option value="100m full">' + t('port_100m_f') + '</option>'
       + '<option value="100m half">' + t('port_100m_h') + '</option>'
       + '<option value="10m full">' + t('port_10m_f') + '</option>'
       + '<option value="10m half">' + t('port_10m_h') + '</option>'
       + '</select>';
      const dSwitch = '<input type="checkbox" id="disable_port" onchange="portOnOff();">'
     for (let i = 1; i <= numPorts; i++) {
      if (pIsSFP[i-1])
        continue;
      console.log("Table row: " + i + "pState: " + pState[i-2]);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(t('common_port') + i));
      let portName = portNames[physToLogPort[i-1]] || '';
      td = tr.insertCell(); td.appendChild(document.createTextNode(portName));
      td = tr.insertCell(); td.innerHTML = linkText(pState[i] + 1);
      td = tr.insertCell(); td.innerHTML = sSelect.replaceAll("speed_sel", "speed_sel_" + i);
      td = tr.insertCell(); td.innerHTML = dSwitch.replaceAll("disable_port", "disable_port_" + i)
						  .replace("portOnOff()", "portOnOff(" + i + ")");
      var button = '<button type="button" style="margin: 0 0 0 24px" onclick="applySpeed(' + i + ');">' + t('port_apply') + '</button>';
      td = tr.insertCell();
      td.innerHTML = button;
    }
  }
  tbl = document.getElementById('mtutable');
  if (tbl.rows.length <= 2 && numPorts) {
     const mSelect = '<select name="mtu_sel" id="mtu_sel">'
      + '<option value="16383">16383</option>'
      + '<option value="1522">1522</option>'
      + '<option value="1536">1536</option>'
      + '<option value="1552">1552</option>'
      + '<option value="9216">9216</option>'
      + '</select>';
      var tr = tbl.insertRow();
      for (let i = 1; i <= numPorts; i++) {
        let td = tr.insertCell();
        if (pIsSFP[i-1])
          td.innerHTML = '<object type="image/svg+xml" data="sfp.svg" width="60"></object>'
        else
          td.innerHTML = '<object type="image/svg+xml" data="port.svg" width="40"></object>'
      }
      tr = tbl.insertRow();
      for (let i = 1; i <= numPorts; i++) {
        let td = tr.insertCell();
        td.innerHTML = mSelect.replaceAll("mtu_sel", "mtu_sel_" + i);
      }
      tr = tbl.insertRow();
      for (let i = 1; i <= numPorts; i++) {
        let td = tr.insertCell();
        td.innerHTML = '<button type="button" style="margin: 0 0 0 24px" onclick="applyMTU(' + i + ');">' + t('port_apply') + '</button>';
      }
  }
}

function updatePortTable() {
  console.log("updatePortTable called");
  var tbl = document.getElementById('speedtable');
  if (tbl.rows.length <= 2 || !numPorts)
    return;
  for (let i = 1; i <= numPorts ; i++) {
    if (pIsSFP[i-1])
      continue;
    tbl.rows[i].cells[2].innerHTML = linkText(pState[i-1]+1);
    if (!clicked[i] && pState[i - 1] < 0) {
      document.getElementById('speed_sel_' + i).disabled = true;
      document.getElementById('disable_port_' + i).checked = true;
    }
  }
}

async function applySpeed(port) {
  var speed = document.getElementById('speed_sel_' + port).value;
  var disabled = document.getElementById('disable_port_' + port).checked;
  var cmd = "port " + port + " ";
  if (!disabled)
    cmd = cmd + speed;
  else
    cmd = cmd + "off";
  console.log("CMD: " + cmd);
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

async function portOnOff(p) {
  var disabled = document.getElementById('disable_port_' + p).checked;
  document.getElementById('speed_sel_' + p).disabled = disabled;
  clicked[p] = 1;
}

async function applyMTU(port) {
  var mtu = document.getElementById('mtu_sel_' + port).value;
  var cmd = "mtu " + port + " " + mtu;
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('MTU Completed!', response);
    getMTUs();
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

function getMTUs() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("MTUS: ", JSON.stringify(s));
      for (let i = 0; i < s.length; i++) {
        p = s[i];
	let n = p.portNum;
        mtus[n] = parseInt(p.mtu, 16);
        var mtu = document.getElementById('mtu_sel_' + n);
        if (!mtu)
          continue;
        mtu.value = mtus[n];
      }
    }
  };
  xhttp.open("GET", "/mtu.json", true);
  xhttp.timeout = 1500; sendXHTTP(xhttp);
}

sectionInits.ports = function() {
  update( () => {
    createPortTable();
    updatePortTable();
    getMTUs()
    setSectionInterval(update, 2000);
    setSectionInterval(updatePortTable, 1000);
  });
};

const mib_counters = [
  "Interface in Octets", 8,
  "", 0,
  "Interface out Octets", 8,
  "", 0,
  "Interface in Unicast Pkts", 8,
  "", 0,
  "Interface in Multicast Pkts", 8,
  "", 0,
  "Interface in Broadcast Pkts", 8,
  "", 0,
  "Interface out Unicast Pkts", 8, // 10
  "", 0,
  "Interface out Multicast Pkts", 8,
  "", 0,
  "Interface out Broadcast Pkts", 8,
  "", 0,
  "Interface out discards", 4,
  "802.1d Tp Port in discards", 4,
  "802.3 Single collision frames", 4,
  "802.3 Multi collision frames", 4,
  "802.3 Deferred transmissions", 4, // 20
  "802.3 Late collisions", 4,
  "802.3 Excessive collisions", 4,
  "802.3 Symbol errors", 4,
  "802.3 Control in unknown opcodes", 4,
  "802.3 In Pause frames", 4,
  "802.3 Out Pause frames", 4,
  "Ether drop events", 4,
  "TX Ether Broadcast Pkts", 4,
  "TX Ether Multicast Pkts", 4,
  "TX Ether CRC Align errors", 4, // 30
  "RX Ether CRC Align errors", 4,
  "TX Ether Undersized Pkts", 4,
  "RX Ether Undersized Pkts", 4,
  "TX Ether Oversized Pkts", 4,
  "RX Ether Oversized Pkts", 4,
  "TX Ether Fragments", 4,
  "RX Ether fragments", 4,
  "TX Ether Jabbers", 4,
  "RX Ether Jabbers", 4,
  "TX Ether Collisions", 4, // 40
  "TX Ether Pkts 640 Octets", 4,
  "RX Ether Pkts 640 Octets", 4,
  "TX Ether 65-127 Octets", 4,
  "RX Ether 65-127 Octets", 4,
  "TX Ether Pkts 128-255 Octets", 4,
  "RX Ether Pkts 128-255 Octets", 4,
  "TX Ether Pkts 256-511 Octets", 4,
  "RX Ether Pkts 256-511 Octets", 4,
  "TX Ether Pkts 512-1023 Octets", 4,
  "RX Ether Pkts 512-1023 Octets", 4, // 50
  "TX Ether Pkts 1024-1518 Octets", 4,
  "RX Ether Pkts 1024-1518 Octets", 4,
  "", 4,
  "RX Ether Undersized Drop Pkts", 4, // 54
  "TX Ether Pkts >1518 Octets", 4,
  "RX Ether Pkts >1518 Octets", 4,
  "TX Ether Pkts too large", 4,
  "RX Ether Pkts too large", 4,
  "TX Ether Flexible Octets Set 1", 4,
  "RX Ether Flexible Octets Set 1", 4,// 60
  "TX Ether Flexible Octets CRC Set 1", 4,
  "RX Ether Flexible Octets CRC Set 1", 4,
  "TX Ether Flexible Octets Set 0", 4,
  "RX Ether Flexible Octets Set 0", 4,
  "TX Ether Flexible Octets CRC Set 0", 4,
  "RX Ether Flexible Octets CRC Set 0", 4,
  "Lenth Field Errors", 4,
  "False Carriers", 4,
  "Undersized Octets", 4,
  "Framing Errors", 4, // 70
  "", 4,
  "RX MAC Discards", 4, // 72
  "RX MAC IPG Short Drop", 4,
  "", 4,
  "802.1d TP Learned Entry Discards", 4, // 75
  "Egress Queue 7 Dropped Pkts", 4,
  "Egress Queue 6 Dropped Pkts", 4,
  "Egress Queue 5 Dropped Pkts", 4,
  "Egress Queue 4 Dropped Pkts", 4,
  "Egress Queue 3 Dropped Pkts", 4, // 80
  "Egress Queue 2 Dropped Pkts", 4,
  "Egress Queue 1 Dropped Pkts", 4,
  "Egress Queue 0 Dropped Pkts", 4,
  "Egress Queue 7 Out Pkts", 4,
  "Egress Queue 6 Out Pkts", 4,
  "Egress Queue 5 Out Pkts", 4,
  "Egress Queue 4 Out Pkts", 4,
  "Egress Queue 3 Out Pkts", 4,
  "Egress Queue 2 Out Pkts", 4,
  "Egress Queue 1 Out Pkts", 4, // 90
  "Egress Queue 0 Out Pkts", 4,
  "TX Good Counter", 8,
  "", 0,
  "RX Good Counter", 8,
  "", 0,
  "RX Error Counter", 4,
  "TX Error Counter", 4,
  "TX Good Counter PHY", 8,
  "", 0,
  "RX Good Counter PHY", 8, // 100
  "", 0,
  "RX Error Counter PHY", 4,
  "TX Error Counter PHY", 4
];


function getCounters(port) {
  var xhttp = new XMLHttpRequest();
  const popup = document.getElementById('popup');
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("Counters: ", JSON.stringify(s));
      const ptext = document.getElementById('popup_text');
      var tableHtml = "<table style='width:100%'> <tr> <th>" + t('stat_counter') + "</th> <th>" + t('stat_value') + "</th> <th>" + t('stat_counter') + "</th> <th>" + t('stat_value') + "</th></tr> <tr>";
      console.log("Counter 0: ", BigInt(s[0]).toString(), " length: ", s.length);
      var c = 0;
      for (i = 0; i < mib_counters.length; i += 4) {
        console.log(i, " ", mib_counters[i], ": ", mib_counters[i+1]);
        if (mib_counters[i] == "" && mib_counters[i + 1] == 8) {
          console.log("c " + i + ": continue");
          continue;
        }
        var count = BigInt(s[i/4]);
        if (mib_counters[i+1] == 8) {
          tableHtml += "<td>" + mib_counters[i] + "</td><td>" + count.toString() + "</td>";
          c += 1;
        } else if (mib_counters[i+1] == 4) {
          if (mib_counters[i] != "") {
            tableHtml += "<td>" + mib_counters[i] + "</td><td>" + (count >> 32n).toString() + "</td>";
            c += 1;
          }
          if (c == 2) {
            tableHtml += "</tr> <tr>";
            c = 0;
          }
          if (mib_counters[i+2] != "") {
            tableHtml += "<td>" + mib_counters[i+2] + "</td><td>" + (count & 4294967295n).toString() + "</td>";
            c += 1;
          }
        }
        if (c == 2) {
          tableHtml += "</tr> <tr>";
          c = 0;
        }
      }
      ptext.innerHTML = tableHtml + "</tr></table>";
      popup.style.display = 'flex';
    }
  };
  xhttp.open("GET", "/counters.json?port=" + port, true);
  xhttp.timeout = 1500; sendXHTTP(xhttp);
}


function fillStats() {
  var tbl = document.getElementById('statstable');
  if (!numPorts)
    return;
  if (tbl.rows.length > 1) {
    for (let i = 0; i < numPorts; i++) {
      console.log("Table Update row: " + i + " state " + pState[i] + " is " + linkS[pState[i] +1]);
      tbl.rows[i+1].cells[2].innerHTML = linkText(pState[i]+1);
      tbl.rows[i+1].cells[3].innerHTML = `${txG[i]}` + t('common_pkts');
      tbl.rows[i+1].cells[4].innerHTML = `${txB[i]}` + t('common_pkts');
      tbl.rows[i+1].cells[5].innerHTML = `${rxG[i]}` + t('common_pkts');
      tbl.rows[i+1].cells[6].innerHTML = `${rxB[i]}` + t('common_pkts');
    }
  } else {
    for (let i = 0; i < numPorts; i++) {
      console.log("Table row: " + i);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(t('common_port') + (i+1)));
      let portName = portNames[physToLogPort[i]] || '';
      td = tr.insertCell(); td.appendChild(document.createTextNode(portName));
      td = tr.insertCell(); td.appendChild(document.createTextNode(linkText(pState[i]+1)));
      td = tr.insertCell(); td.appendChild(document.createTextNode(`${txG[i]}` + t('common_pkts')));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${txB[i]}` + t('common_pkts')));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxG[i]}` + t('common_pkts')));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxB[i]}` + t('common_pkts')));
      var button = '<button type="button" style="margin: 0 0 0 24px" onclick="getCounters(' + i + ');">' + t('stat_show') + '</button>';
      td = tr.insertCell(); td.innerHTML = button;
    }
  }
}

document.addEventListener('DOMContentLoaded', function() {
  const popup = document.getElementById('popup');
  const closePopup = document.getElementById('closePopup');
  if (closePopup)
    closePopup.addEventListener('click', () => {
      popup.style.display = 'none';
    });
  window.addEventListener('click', (event) => {
    if (event.target === popup) {
      popup.style.display = 'none';
    }
  });
});

sectionInits.stat = function() {
  update( () => {
    update();
    fillStats();
    setSectionInterval(fillStats, 1000);
    setSectionInterval(update, 2000);
  });
};

var l2Timer;

function l2CMP(a, b)
{
  if (a.port < b.port)
    return -1;
  if (a.port > b.port)
    return 1;
  if (a.mac < b.mac)
    return -1;
  if (a.mac > b.mac)
    return 1;
  if (a.vlan < b.vlan)
    return -1;
  if (a.vlan > b.vlan)
    return 1;
  return 0;
}

function uniq(a) {
    return a.filter(function(item, pos, ary) {
        return !pos || item.idx != ary[pos - 1].idx;
    });
}

function delL2(idx) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var s = JSON.parse(xhttp.responseText);
      console.log("Entry deletion result: ", s.result);
    }
  };
  xhttp.open("GET", "/l2_del.json?idx=" + idx, true);
  xhttp.timeout = 1500; xhttp.send();
}

var l2All = [];
const l2Cols = ['port', 'mac', 'vlan', 'type'];
var l2SortCol = 'port';
var l2SortDir = 1;

function l2Key(e, col) {
  if (col === 'port') return e.port === 'CPU' ? Number.MAX_SAFE_INTEGER : Number(e.port);
  if (col === 'vlan') return Number(e.vlan);
  return String(e[col]).toLowerCase();
}

function l2SortBy(col) {
  l2SortDir = (col === l2SortCol) ? -l2SortDir : 1;
  l2SortCol = col;
  renderL2();
}

function l2FilterChanged() { renderL2(); }

function renderL2() {
  var tbl = document.getElementById('l2table');
  if (!tbl) return;
  var f = {};
  l2Cols.forEach(function(c) {
    var el = document.getElementById('l2f_' + c);
    f[c] = el ? el.value.trim().toLowerCase() : '';
  });
  var rows = l2All.filter(function(e) {
    return l2Cols.every(function(c) {
      return !f[c] || String(e[c]).toLowerCase().indexOf(f[c]) !== -1;
    });
  });
  rows.sort(function(a, b) {
    var x = l2Key(a, l2SortCol), y = l2Key(b, l2SortCol);
    return (x < y ? -1 : x > y ? 1 : 0) * l2SortDir;
  });
  l2Cols.forEach(function(c) {
    var a = document.getElementById('l2a_' + c);
    if (a) a.textContent = (c === l2SortCol) ? (l2SortDir > 0 ? ' \u25b2' : ' \u25bc') : ' \u21c5';
  });
  paintL2(tbl, rows);
  var cnt = document.getElementById('l2count');
  if (cnt) cnt.textContent = rows.length + ' / ' + l2All.length;
}

function fillL2(s)
{
  var tbl = document.getElementById('l2table');
  if (!s.length)
    return;
  s.sort(l2CMP);
  s = uniq(s);
  l2All = s;
  renderL2();
}

function paintL2(tbl, s)
{
  console.log("L2: ", JSON.stringify(s));
  for (let i = 0; i < s.length; i++) {
    var e = s[i];
    console.log(i, e);
    if (tbl.rows[i+1]) {
      tbl.rows[i+1].cells[0].innerHTML = `${e.port}`;
      tbl.rows[i+1].cells[1].innerHTML = `${e.mac}`;
      tbl.rows[i+1].cells[2].innerHTML = `${e.vlan}`;
      tbl.rows[i+1].cells[3].innerHTML = `${e.type}`;
      tbl.rows[i+1].cells[4].innerHTML = '<button type="button" onclick="delL2(' + e.idx + ');">' + t('l2_delete') + '</button>';
    } else {
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.innerHTML = `${e.port}`;
      td = tr.insertCell(); td.innerHTML = `${e.mac}`;
      td = tr.insertCell(); td.innerHTML = `${e.vlan}`;
      td = tr.insertCell(); td.innerHTML = `${e.type}`;
      td = tr.insertCell(); td.innerHTML = '<button type="button" onclick="delL2(' + e.idx + ');">' + t('l2_delete') + '</button>';
    }
  }
  for (let i = tbl.rows.length - 1; i > s.length; i--)
    tbl.deleteRow(i);
}

function getL2() {
  walkL2(function(entries, ok) {
    if (ok) {
      for (var i = 0; i < entries.length; i++)
        entries[i].type = entries[i].type == "s" ? t('l2_static') : t('l2_learned');
      fillL2(entries);
    }
    var sec = document.getElementById('page-l2');
    if (sec && sec.style.display === 'block')
      l2Timer = setTimeout(getL2, 1000);
  });
}

sectionInits.l2 = function() {
  update( () => {
    getL2();
    setSectionInterval(update, 2000);
  });
};

var vlanInterval = Number();

function vlanForm() {
  if (!numPorts)
    return;
  document.querySelectorAll('#tPorts .cbgroup, #uPorts .cbgroup, #pPorts .cbgroup')
    .forEach(function(el) { el.remove(); });
  var t = document.getElementById('tPorts');
  var u = document.getElementById('uPorts');
  var p = document.getElementById('pPorts');
  for (let i = 1; i <= numPorts; i++) {
    const d = document.createElement("div");
    d.classList.add("cbgroup");
    const l = document.createElement("label");
    l.innerHTML = "" + i;
    l.classList.add("cbgroup");
    const inp = document.createElement("input");
    inp.type = "checkbox"; inp.setAttribute("class","psel");
    inp.id = "tport" + i;
    inp.setAttribute('onclick', `setC("u", ${i}, false);`);
    const o = document.createElement("img");
    if (pIsSFP[i - 1]) {
      o.src = "sfp.svg"; o.width ="60"; o.height ="60";
    } else {
      o.src = "port.svg"; o.width = "40"; o.height = "40";
    }
    l.appendChild(inp); l.appendChild(o);
    d.appendChild(l)
    t.appendChild(d);
    var d2=d.cloneNode(true);
    d2.children[0].children[0].id = "uport" + i;
    d2.children[0].children[0].setAttribute('onclick', `setC("t", ${i}, false);`);
    u.appendChild(d2);
    var d3=d.cloneNode(true);
    d3.children[0].children[0].id = "pport" + i;
    d3.children[0].children[0].removeAttribute('onclick');
    p.appendChild(d3);
  }
}

function setC(t, p, c){
  document.getElementById(t+'port'+p).checked=c;
}

function utClicked(t){
  for (let i = 1; i <= numPorts; i++) {
    setC('t', i, t); setC('u', i, !t);
  }
}

function pvClicked(p){
  for (let i = 1; i <= numPorts; i++) {
    setC('p', i, p);
  }
}

function fetchVLAN() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("VLAN: ", JSON.stringify(s));
      m = parseInt(s.members, 16);
      document.getElementById('vname').value = s.name;
      var members = m & 0x3FF;
      var untag = (m >> 10) & 0x3FF;
      var pvid = parseInt(s.pvid, 16);
      console.log("PVID: ", pvid);
      for (let p = 1; p <= numPorts; p++) {
        var bit = physToLogPort[p-1];
        var isMember = (members >> bit) & 1;
        var isUntag = (untag >> bit) & 1;
        setC('t', p, isMember && !isUntag);
        setC('u', p, isMember && isUntag);
        setC('p', p, (pvid >> bit) & 1);
      }
    }
  };
  var v=document.getElementById('vid').value
  if (!v) {
    alert(t('vlan_set_id_first'));
    return;
  }
  xhttp.open("GET", `/vlan.json?vid=${v}`, true);
  sendXHTTP(xhttp);
}

function portsToRange(mask, nPorts) {
  var parts = [];
  var start = -1, prev = -1;
  for (var p = 1; p <= nPorts; p++) {
    var bit = physToLogPort[p - 1];
    if ((mask >> bit) & 1) {
      if (start < 0) start = p;
      prev = p;
    } else {
      if (start >= 0) {
        parts.push(start === prev ? String(start) : start + '-' + prev);
        start = -1; prev = -1;
      }
    }
  }
  if (start >= 0)
    parts.push(start === prev ? String(start) : start + '-' + prev);
  return parts.length ? parts.join(',') : '-';
}

async function loadVlanTable() {
  var tbody = document.getElementById('vlanTableBody');
  if (!tbody) return;
  tbody.innerHTML = '';
  var resp;
  try { resp = await fetch('/vlanlist'); } catch(e) { return; }
  if (!resp.ok) return;
  var vlans = (await resp.json()).vlan || [];
  for (var i = 0; i < vlans.length; i++) {
    var v = vlans[i];
    var vresp;
    try { vresp = await fetch('/vlan.json?vid=' + v.id); } catch(e) { continue; }
    if (!vresp.ok) continue;
    var s = await vresp.json();
    var m = parseInt(s.members, 16);
    var members = m & 0x3FF;
    var untag   = ((m >> 10) & 0x3FF) & members;
    var tagged  = members & ~untag;
    var pvid    = parseInt(s.pvid, 16) & 0x3FF;
    var tr = document.createElement('tr');
    var td, a, btn;
    td = document.createElement('td');
    a = document.createElement('a');
    a.href = '#';
    a.textContent = v.id;
    (function(vid) {
      a.onclick = function(e) {
        e.preventDefault();
        document.getElementById('vid').value = vid;
        fetchVLAN();
      };
    })(v.id);
    td.appendChild(a); tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = v.name || ''; tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = portsToRange(members, numPorts); tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = portsToRange(tagged, numPorts); tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = portsToRange(untag, numPorts); tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = portsToRange(pvid, numPorts); tr.appendChild(td);
    td = document.createElement('td');
    if (v.id !== 1) {
      btn = document.createElement('button');
      btn.textContent = '✕';
      (function(vid) {
        btn.onclick = function() { deleteVlan(vid); };
      })(v.id);
      td.appendChild(btn);
    }
    tr.appendChild(td);
    tbody.appendChild(tr);
  }
}

function deleteVlan(id) {
  if (!confirm(t('vlan_delete_confirm') + id + '?')) return;
  fetch('/cmd', { method: 'POST', body: 'vlan ' + id + ' d' })
    .then(function() { refreshVlanViews(); })
    .catch(function(err) { console.error('Delete failed:', err); });
}

function refreshVlanViews() {
  loadVlanList();
  loadVlanTable();
}

function loadVlanList() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState !== 4) return;
    var sel = document.getElementById('vlanSelect');
    if (this.status !== 200) {
      sel.style.display = 'none';
      return;
    }
    var vlans = JSON.parse(this.responseText).vlan || [];
    if (!vlans.length) {
      sel.style.display = 'none';
      return;
    }
    sel.options.length = 1;
    for (var i = 0; i < vlans.length; i++) {
      var opt = document.createElement('option');
      opt.value = vlans[i].id;
      opt.text = vlans[i].name ? vlans[i].id + ' — ' + vlans[i].name : String(vlans[i].id);
      sel.appendChild(opt);
    }
  };
  xhttp.open('GET', '/vlanlist', true);
  sendXHTTP(xhttp);
}

sectionInits.vlan = function() {
  update( () => {
    vlanForm();
    refreshVlanViews();
    document.getElementById('vlanSelect').onchange = function() {
      document.getElementById('vid').value = this.value;
      fetchVLAN();
    };
    setSectionInterval(update, 2000);
  });
};

async function vlanSub() {
  var commands = [];
  var cmd = "vlan ";
  var v=document.getElementById('vid').value
  if (!v) {
    alert(t('vlan_set_id_first'));
    return;
  }
  cmd = cmd + v;
  if (document.getElementById('vname').value)
    cmd = cmd + ' ' + document.getElementById('vname').value;
  for (let i = 1; i <= numPorts; i++) {
    if (document.getElementById('tport' + i).checked)
      cmd = cmd + ` ${i}t`;
    else if (document.getElementById('uport' + i).checked)
      cmd = cmd + ` ${i}`;
  }
  commands.push(cmd);
  for (let i = 1; i <= numPorts; i++) {
    if (document.getElementById('pport' + i).checked)
      commands.push(`pvid ${i} ${v}`);
  }
  try {
    for (let c of commands) {
      const response = await fetch('/cmd', {
        method: 'POST',
        body: c
      });
      console.log('Completed!', response);
    }
    refreshVlanViews();
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

var lagInterval = Number();

function lagForm() {
  if (!numPorts)
    return;
  for (let j = 0; j < 4; j++)
    document.getElementById("mLAG" + j).innerHTML = '';
  for (let j=0; j < 4; j++) {
    var lag = "mLAG" + j
    console.log("Adding LAG " + lag)
    var m = document.getElementById(lag);
    for (let i = 1; i <= numPorts; i++) {
      const d = document.createElement("div");
      d.classList.add("cbgroup");
      const l = document.createElement("label");
      l.innerHTML = "" + i;
      l.classList.add("cbgroup");
      const inp = document.createElement("input");
      inp.type = "checkbox"; inp.setAttribute("class","psel");
      inp.id = "p_" + lag + "_" + i;
      const o = document.createElement("img");
      if (pIsSFP[i - 1]) {
        o.src = "sfp.svg"; o.width ="60"; o.height ="60";
      } else {
        o.src = "port.svg"; o.width = "40"; o.height = "40";
      }
      l.appendChild(inp); l.appendChild(o);
      d.appendChild(l)
      m.appendChild(d);
    }
  }
  fetchLag();
}

function setL(p, c){
  console.log("LAG setting: ", p, " to ", c);
  document.getElementById(p).checked=c;
}

function fetchLag() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("LAG: ", JSON.stringify(s));
      for (let l = 0; l < 4; l++) {
        let members = parseInt(s[l].members, 2);
        let hash = parseInt(s[l].hash, 16);
        for (let i = 1; i <= numPorts; i++) {
          let p = i - 1;
          if (numPorts < 9)
            p = physToLogPort[p];            
          setL("p_mLAG"+l+"_"+i, members & (1<<p));
        }
      }
    }
  };
  xhttp.open("GET", `/lag.json`, true);
  sendXHTTP(xhttp);
}
async function lagSub(l) {
  var cmd = "lag " + l;
  for (let i = 1; i <= numPorts; i++) {
    if (document.getElementById("p_mLAG"+l+"_"+i).checked)
      cmd = cmd + ` ${i}`;
  }
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

sectionInits.lag = function() {
  update( () => {
    lagForm();
    setSectionInterval(update, 2000);
  });
};

var mirrorInterval = Number();
const mirrors = ["mPortsTX", "mPortsRX"];

function mirrorForm() {
  if (!numPorts)
    return;
  mirrors.forEach(function(m) { document.getElementById(m).innerHTML = ''; });
  for (let j=0; j < mirrors.length; j++) {
    console.log("Adding Mirror " + j)
    var m = document.getElementById(mirrors[j]);
      for (let i = 1; i <= numPorts; i++) {
      const d = document.createElement("div");
      d.classList.add("cbgroup");
      const l = document.createElement("label");
      l.innerHTML = "" + i;
      l.classList.add("cbgroup");
      const inp = document.createElement("input");
      inp.type = "checkbox"; inp.setAttribute("class","psel");
      inp.id = mirrors[j] + i;
      const o = document.createElement("img");
      if (pIsSFP[i - 1]) {
        o.src = "sfp.svg"; o.width ="60"; o.height ="60";
      } else {
        o.src = "port.svg"; o.width = "40"; o.height = "40";
      }
      l.appendChild(inp); l.appendChild(o);
      d.appendChild(l)
      m.appendChild(d);
    }
  }
  fetchMirror();
}

function setM(p, c){
  document.getElementById(p).checked=c;
}

function fetchMirror() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("MIRROR: ", JSON.stringify(s));
      document.getElementById('me').checked = s.enabled;
      document.getElementById('mp').value = s.mPort;
      let m_tx = parseInt(s.mirror_tx, 2);
      let m_rx = parseInt(s.mirror_rx, 2);
      for (let i = 1; i <= numPorts; i++) {
        let p = i - 1;
        if (numPorts < 9)
          p = physToLogPort[p];
        setM("mPortsTX"+i, m_tx&(1<<p)); setM("mPortsRX"+i, m_rx&(1<<p));
      }
    }
  };
  xhttp.open("GET", `/mirror.json`, true);
  sendXHTTP(xhttp);
}

sectionInits.mirror = function() {
  update( () => {
    mirrorForm();
    setSectionInterval(update, 2000);
  });
};

async function mirrorSub() {
  var cmd = "mirror ";
  var mp=document.getElementById('mp').value
  if (!mp) {
    alert(t('mirror_set_port_first'));
    return;
  }
  document.getElementById(mirrors[0]+mp).checked=false;document.getElementById(mirrors[1]+mp).checked=false;
  cmd = cmd + mp;
  for (let i = 1; i <= numPorts; i++) {
    if (document.getElementById(mirrors[0] + i).checked && document.getElementById(mirrors[1] + i).checked)
      cmd = cmd + ` ${i}`;
    else if (document.getElementById(mirrors[0] + i).checked)
      cmd = cmd + ` ${i}t`;
    else if (document.getElementById(mirrors[1] + i).checked)
      cmd = cmd + ` ${i}r`;
  }
  if (cmd.length < 10) {
    alert(t('mirror_select_ports'));
    return;
  }
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}
async function mirrorDel() {
  var cmd = "mirror off";
try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    location.reload();
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

function createEEE() {
  var tbl = document.getElementById('eeetable');
   if (tbl.rows.length <= 2  && numPorts) {
     console.log("CREATING TABLE ", tbl.rows.length);
     for (let i = 2; i < 2 + numPorts; i++) {
      console.log("Table row: " + i + "pState: " + pState[i-2]);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(t('common_port') + (i-1)));
      for (let j = 0; j < 7; j++) {
        td = tr.insertCell(); td.appendChild(document.createTextNode(" "));
      }
    }
  }
}

function getEEE() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("EEE: ", JSON.stringify(s));
      var tbl = document.getElementById('eeetable');
      if (tbl.rows.length > 2 && numPorts) {
        for (let i = 2; i < 2 + numPorts; i++) {
          p = s[i-2];
          let n = p.portNum;
          console.log("Table Update row: " + i + " portNum is " + n + ", pState is " + pState[i-2]);
          let tr = tbl.rows[n+1];
          if (!p.isSFP) {
            let eee = parseInt(p.eee,2); let lp = parseInt(p.eee_lp,2);
            tr.cells[1].innerHTML = `${eee&4?t('eee_on'):t('eee_off')}`; tr.cells[2].innerHTML = `${eee&2?t('eee_on'):t('eee_off')}`; tr.cells[3].innerHTML = `${eee&1?t('eee_on'):t('eee_off')}`;
            tr.cells[4].innerHTML = `${lp&4?t('eee_on'):t('eee_off')}`; tr.cells[5].innerHTML = `${lp&2?t('eee_on'):t('eee_off')}`; tr.cells[6].innerHTML = `${lp&1?t('eee_on'):t('eee_off')}`;
            tr.cells[7].innerHTML = `${p.active}`;
            tr.classList.toggle('disabled', pState[i-2] < 0); tr.classList.toggle('isNOK', !p.active); tr.classList.toggle('isOK', p.active);
          }
          tr.classList.toggle('isSFP', p.isSFP);
        }
      }
    }
  };
  xhttp.open("GET", "/eee.json", true);
  xhttp.timeout = 1500; sendXHTTP(xhttp);
}

sectionInits.eee = function() {
  update( () => {
    createEEE();
    getEEE();
    setSectionInterval(update, 2000);
    setSectionInterval(getEEE, 2000);
  });
};

async function eeeSub(port, enable) {
  var cmd = "eee ";
  if (enable)
    cmd = cmd + "on";
  else
    cmd = cmd + "off";
  console.log("eeeSub port " + port, ", value " + enable);
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

const iLayout = '" type="number" maxlength="10" size="10"  onfocus="inputFocus(';
function createBW() {
  var tbl = document.getElementById('bwtable');
  const limit = '<input type="checkbox" id="limit_port" onchange="exec();">'
  if (tbl.rows.length <= 2  && numPorts) {
     console.log("CREATING TABLE ", tbl.rows.length);
     for (let i = 2; i < 2 + numPorts; i++) {
       const tr = tbl.insertRow();
        let td = tr.insertCell(); td.appendChild(document.createTextNode(t('common_port') + (i-1)));
       td = tr.insertCell();
       td.innerHTML = limit.replaceAll("limit_port", "ilimit_port_" + i).replace("exec()", "iClicked(" + i + ")");
       td = tr.insertCell();
        td.innerHTML = t('bw_unlimited');
        td = tr.insertCell();
        td.innerHTML = limit.replaceAll("limit_port", "fc_port_" + i).replace("exec()", "document.getElementById('bwapply_" + i + "').disabled=false;");
        td = tr.insertCell();
        td.innerHTML = limit.replaceAll("limit_port", "elimit_port_" + i).replace("exec()", "eClicked(" + i + ")");
        td = tr.insertCell();
        td.innerHTML = t('bw_unlimited');
        var button = '<button type="button" id="bwapply_' + i + '" style="margin: 0 0 0 24px" onclick="applyBandwidth(' + i + ');">' + t('bw_col_apply') + '</button>';
       td = tr.insertCell();
       td.innerHTML = button;
       document.getElementById("bwapply_" + i).disabled = true;
    }
  }
}

function iClicked(i)
{
  document.getElementById("bwapply_" + i).disabled=false;
  var tbl = document.getElementById('bwtable');
  var tr = tbl.rows[i];
  if (!document.getElementById("ilimit_port_" + i).checked) {
            tr.cells[2].innerHTML = t('bw_unlimited');
    document.getElementById("fc_port_" + i).disabled = true;
    document.getElementById("fc_port_" + i).checked = true;
  } else {
    tr.cells[2].innerHTML = '<input id="ibw_' + i + iLayout + i + ')" value="0"/>';
    document.getElementById("fc_port_" + i).disabled = false;
    document.getElementById("fc_port_" + i).checked = true;
  }
}

function eClicked(i)
{
  document.getElementById("bwapply_" + i).disabled=false;
  var tbl = document.getElementById('bwtable');
  var tr = tbl.rows[i];
  if (!document.getElementById("elimit_port_" + i).checked) {
            tr.cells[5].innerHTML = t('bw_unlimited');
  } else {
    tr.cells[5].innerHTML = '<input id="ebw_' + i + iLayout + i + ')" value="0"/>';
  }
}

function inputFocus(i)
{
    document.getElementById("bwapply_" + i).disabled=false;
}

async function doCMD(cmd)
{
  console.log("Sending >" + cmd + "<");
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

async function applyBandwidth(i) {
  var tbl = document.getElementById('bwtable');
  var tr = tbl.rows[i];
  var cmd = "bw in " + (i-1) + " off";
  if (document.getElementById("ilimit_port_" + i).checked)
    cmd = 'bw in ' + (i-1) + ' ' + parseInt(document.getElementById("ibw_" + i).value).toString(16).padStart(4, "0");;
  doCMD(cmd);
  if (document.getElementById("ilimit_port_" + i).checked) {
    if (!document.getElementById("fc_port_" + i).checked)
      cmd = "bw in " + (i-1) + " drop";
    doCMD(cmd);
  }
  var cmd = "bw out " + (i-1) + " off";
  if (document.getElementById("elimit_port_" + i).checked)
    cmd = 'bw out ' + (i-1) + ' ' + parseInt(document.getElementById("ebw_" + i).value).toString(16).padStart(4, "0");;
  doCMD(cmd);
}

function getBW() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    console.log("IN getBW ");
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("BW: ", JSON.stringify(s));
      var tbl = document.getElementById('bwtable');
      if (tbl.rows.length > 2 && numPorts) {
        for (let i = 2; i < 2 + numPorts; i++) {
          p = s[i-2];
          let n = p.portNum;
          let tr = tbl.rows[n+1];
          if (!document.getElementById("bwapply_" + (n+1)).disabled)
            continue;
          console.log("Table Update row: " + i + " portNum is " + n + ", pState is " + pState[i-2] + ", row number is " + (n+1));
          let iBW = parseInt(p.iBW,16) * 16; let eBW = parseInt(p.eBW,16) * 16;
          document.getElementById("ilimit_port_" + (n+1)).checked = p.iLimited;
          document.getElementById("elimit_port_" + (n+1)).checked = p.eLimited;
          if (!p.iLimited) {
    tr.cells[2].innerHTML = t('bw_unlimited');
          } else {
            tr.cells[2].innerHTML = '<input id="ibw_' + (n+1) + iLayout + (n+1) + ')" value="' + iBW +'"/>';
          }
          if (!p.eLimited) {
    tr.cells[5].innerHTML = t('bw_unlimited');
          } else {
            tr.cells[5].innerHTML = '<input id="ebw_' + (n+1) + iLayout + (n+1) + ')" value="' + eBW +'"/>';
          }
          document.getElementById("fc_port_" + (n+1)).checked = p.iFC==1?true:false;
          document.getElementById("fc_port_" + (n+1)).disabled = p.iLimited==1?false:true;
        }
      }
    }
  };
  xhttp.open("GET", "/bandwidth.json", true);
  xhttp.timeout = 1500; sendXHTTP(xhttp);
}

sectionInits.bandwidth = function() {
  update( () => {
    createBW();
    getBW();
    setSectionInterval(update, 2000);
  });
};

document.addEventListener("DOMContentLoaded", function () {
    if (!document.getElementById('infoTable')) return;
    fetch('/information.json')
        .then(response => response.json())
        .then(data => {
            const tableBody = document.getElementById('infoTable').querySelector('tbody');

            // Create table rows
            for (const [key, value] of Object.entries(data)) {
                const row = document.createElement('tr');
                const cellKey = document.createElement('td');
                const cellValue = document.createElement('td');

                cellKey.textContent = key;
                cellValue.textContent = value;

                row.appendChild(cellKey);
                row.appendChild(cellValue);
                tableBody.appendChild(row);
            }
        })
        .catch(error => console.error('Error fetching the data:', error));
});

function walkL2(onDone)
{
  var entries = [];
  var idx = 0;
  var tries = 0;

  function retry() {
    if (++tries < 3) {
      setTimeout(page, 1000);
      return;
    }
    onDone(entries, false);
  }

  function page() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState != 4)
        return;
      if (this.status != 200) {
        retry();
        return;
      }
      var s;
      try {
        s = JSON.parse(xhttp.responseText);
      } catch (err) {
        retry();
        return;
      }
      tries = 0;
      s = s.map(function(e) {
        e.vlan = parseInt(e.vlan, 16);
        e.idx = parseInt(e.idx, 16);
        e.port = e.port == 9 ? 'CPU' : logToPhysPort[e.port];
        return e;
      });
      if (!s.length) {
        onDone(entries, true);
        return;
      }
      entries.push(...s);
      for (var i = entries.length - 1; i > 0; i--) {
        if (entries[0].idx == entries[i].idx) {
          onDone(entries, true);
          return;
        }
      }
      if (entries.length >= 4096) {
        onDone(entries, true);
        return;
      }
      idx = s[s.length - 1].idx + 1;
      setTimeout(page, 1000);
    };
    xhttp.open("GET", "/l2.json?idx=" + idx, true);
    xhttp.timeout = 1500;
    sendXHTTP(xhttp);
  }

  page();
}
