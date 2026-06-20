document.addEventListener("DOMContentLoaded", function () {
    const infoLabels = {
        ip_address: "IP 地址",
        ip_gateway: "网关",
        ip_netmask: "子网掩码",
        syslog_server_ip: "Syslog 服务器 IP",
        mac_address: "MAC 地址",
        sw_ver: "软件版本",
        build_date: "构建日期",
        hw_ver: "硬件型号",
        flash_size: "Flash 容量",
        sfp_slot_0: "SFP 插槽 0",
        sfp_slot_1: "SFP 插槽 1"
    };

    fetch('/information.json')
        .then(response => response.json())
        .then(data => {
            const tableBody = document.getElementById('infoTable').querySelector('tbody');

            // Create table rows
            for (const [key, value] of Object.entries(data)) {
                const row = document.createElement('tr');
                const cellKey = document.createElement('td');
                const cellValue = document.createElement('td');

                cellKey.textContent = infoLabels[key] || key;
                cellValue.textContent = value;

                row.appendChild(cellKey);
                row.appendChild(cellValue);
                tableBody.appendChild(row);
            }
        })
        .catch(error => console.error('Error fetching the data:', error));
});
