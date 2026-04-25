param(
    [Parameter(Mandatory = $true)]
    [string]$IdfPath,

    [switch]$VerifyOnly
)

$ErrorActionPreference = "Stop"

function Get-CheckedFile {
    param(
        [string]$Base,
        [string]$RelativePath
    )

    $path = Join-Path $Base $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Expected ESP-IDF file not found: $path"
    }
    return $path
}

function Set-TextPatch {
    param(
        [string]$Path,
        [string]$Description,
        [string]$AlreadyPattern,
        [string]$SearchPattern,
        [string]$Replacement
    )

    $text = Get-Content -LiteralPath $Path -Raw
    if ($text -match $AlreadyPattern) {
        Write-Host "OK: $Description already applied"
        return
    }

    if ($text -notmatch $SearchPattern) {
        throw "Could not find expected text for patch: $Description in $Path"
    }

    if ($VerifyOnly) {
        Write-Host "PENDING: $Description"
        return
    }

    $updated = [regex]::Replace($text, $SearchPattern, $Replacement, 1)
    Set-Content -LiteralPath $Path -Value $updated -NoNewline
    Write-Host "PATCHED: $Description"
}

$resolvedIdfPath = (Resolve-Path -LiteralPath $IdfPath).Path

$btTargetPath = Get-CheckedFile `
    -Base $resolvedIdfPath `
    -RelativePath "components\bt\host\bluedroid\common\include\common\bt_target.h"

$hiddApiPath = Get-CheckedFile `
    -Base $resolvedIdfPath `
    -RelativePath "components\bt\host\bluedroid\stack\hid\hidd_api.c"

$btaHdActPath = Get-CheckedFile `
    -Base $resolvedIdfPath `
    -RelativePath "components\bt\host\bluedroid\bta\hd\bta_hd_act.c"

$hiddConnPath = Get-CheckedFile `
    -Base $resolvedIdfPath `
    -RelativePath "components\bt\host\bluedroid\stack\hid\hidd_conn.c"

$btcConfigPath = Get-CheckedFile `
    -Base $resolvedIdfPath `
    -RelativePath "components\bt\host\bluedroid\btc\core\btc_config.c"

$btcConfigHeaderPath = Get-CheckedFile `
    -Base $resolvedIdfPath `
    -RelativePath "components\bt\host\bluedroid\btc\include\btc\btc_config.h"

$espBtDevicePath = Get-CheckedFile `
    -Base $resolvedIdfPath `
    -RelativePath "components\bt\host\bluedroid\api\esp_bt_device.c"

$espBtDeviceHeaderPath = Get-CheckedFile `
    -Base $resolvedIdfPath `
    -RelativePath "components\bt\host\bluedroid\api\include\api\esp_bt_device.h"

Set-TextPatch `
    -Path $btcConfigPath `
    -Description "include NVS limits for configurable Bluedroid bond namespace" `
    -AlreadyPattern '#include\s+"nvs\.h"' `
    -SearchPattern '#include\s+"stack/bt_types\.h"\s*' `
    -Replacement @'
#include "stack/bt_types.h"
#include "nvs.h"

'@

Set-TextPatch `
    -Path $btcConfigPath `
    -Description "make Bluedroid bond config namespace mutable" `
    -AlreadyPattern 'static\s+char\s+CONFIG_FILE_PATH\[NVS_KEY_NAME_MAX_SIZE\]\s*=\s*"bt_config\.conf";' `
    -SearchPattern 'static\s+const\s+char\s+\*CONFIG_FILE_PATH\s*=\s*"bt_config\.conf";' `
    -Replacement 'static char CONFIG_FILE_PATH[NVS_KEY_NAME_MAX_SIZE] = "bt_config.conf";'

Set-TextPatch `
    -Path $btcConfigPath `
    -Description "add Bluedroid bond namespace update helper" `
    -AlreadyPattern 'int\s+btc_config_file_path_update\s*\(' `
    -SearchPattern 'static\s+config_t\s+\*config;\s*' `
    -Replacement @'
static config_t *config;

int btc_config_file_path_update(const char *file_path)
{
    if (file_path != NULL && strlen(file_path) < NVS_KEY_NAME_MAX_SIZE) {
        memcpy(CONFIG_FILE_PATH, file_path, strlen(file_path));
        CONFIG_FILE_PATH[strlen(file_path)] = '\0';
        return 0;
    }

    BTC_TRACE_ERROR("Update failed, file_path is NULL or length should be less than %d\n",
                    NVS_KEY_NAME_MAX_SIZE);
    return -1;
}

'@

Set-TextPatch `
    -Path $btcConfigHeaderPath `
    -Description "declare Bluedroid bond namespace update helper" `
    -AlreadyPattern 'int\s+btc_config_file_path_update\s*\(' `
    -SearchPattern 'bool\s+btc_config_clean_up\(void\);\s*' `
    -Replacement @'
bool btc_config_clean_up(void);
int btc_config_file_path_update(const char *file_path);

'@

Set-TextPatch `
    -Path $espBtDevicePath `
    -Description "include Bluedroid config helper in public BT device API" `
    -AlreadyPattern '#include\s+"btc/btc_config\.h"' `
    -SearchPattern '#include\s+"btc/btc_dev\.h"\s*' `
    -Replacement @'
#include "btc/btc_dev.h"
#include "btc/btc_config.h"

'@

Set-TextPatch `
    -Path $espBtDevicePath `
    -Description "backport esp_bt_config_file_path_update public API" `
    -AlreadyPattern 'esp_err_t\s+esp_bt_config_file_path_update\s*\(' `
    -SearchPattern 'return\s+\(btc_transfer_context\(&msg,\s*&arg,\s*sizeof\(btc_dev_args_t\),\s*NULL,\s*NULL\)\s*==\s*BT_STATUS_SUCCESS\s*\?\s*ESP_OK\s*:\s*ESP_FAIL\);\s*\}\s*' `
    -Replacement @'
return (btc_transfer_context(&msg, &arg, sizeof(btc_dev_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_bt_config_file_path_update(const char *file_path)
{
    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_UNINITIALIZED);
    return btc_config_file_path_update(file_path);
}

'@

Set-TextPatch `
    -Path $espBtDeviceHeaderPath `
    -Description "declare esp_bt_config_file_path_update public API" `
    -AlreadyPattern 'esp_bt_config_file_path_update' `
    -SearchPattern '#ifdef __cplusplus\s*extern "C" \{\s*#endif\s*' `
    -Replacement @'
#ifdef __cplusplus
extern "C" {
#endif

#define NANO_SWITCH_BT_CONFIG_FILE_PATH_UPDATE_BACKPORT 1

'@

Set-TextPatch `
    -Path $espBtDeviceHeaderPath `
    -Description "declare esp_bt_config_file_path_update function" `
    -AlreadyPattern 'esp_err_t\s+esp_bt_config_file_path_update\s*\(' `
    -SearchPattern 'esp_err_t\s+esp_bt_dev_set_device_name\(const char \*name\);\s*' `
    -Replacement @'
esp_err_t esp_bt_dev_set_device_name(const char *name);

/**
 * @brief Update the NVS namespace used for Bluetooth bond keys.
 *
 * This function must be called before esp_bluedroid_init().
 *
 * @param[in] file_path NVS namespace name, shorter than NVS_KEY_NAME_MAX_SIZE.
 *
 * @return
 *                  - ESP_OK : Succeed
 *                  - ESP_ERR_INVALID_STATE : Bluedroid is already initialized
 *                  - ESP_FAIL : invalid namespace
 */
esp_err_t esp_bt_config_file_path_update(const char *file_path);

'@

Set-TextPatch `
    -Path $btTargetPath `
    -Description "increase SDP_MAX_PAD_LEN so the 203-byte Joy-Con descriptor fits in the HID SDP record" `
    -AlreadyPattern "#define\s+SDP_MAX_PAD_LEN\s+512\b" `
    -SearchPattern "#define\s+SDP_MAX_PAD_LEN\s+300\b" `
    -Replacement "#define SDP_MAX_PAD_LEN             512"

Set-TextPatch `
    -Path $hiddApiPath `
    -Description "set HIDCountryCode to 0x00 to match the Joy-Con SDP reference" `
    -AlreadyPattern "const\s+uint8_t\s+country_code\s*=\s*0x00\s*;" `
    -SearchPattern "const\s+uint8_t\s+country_code\s*=\s*0x21\s*;" `
    -Replacement "const uint8_t country_code = 0x00;"

Set-TextPatch `
    -Path $btaHdActPath `
    -Description "preserve raw HID/L2CAP open-failure reason in ESP_HIDD_OPEN_EVT status fields" `
    -AlreadyPattern "raw_reason\s*=\s*p_cback->data" `
    -SearchPattern "extern\s+void\s+bta_hd_open_failure\(tBTA_HD_DATA\s+\*p_data\)\s*\{\s*tBTA_HD_CBACK_DATA\s+\*p_cback\s*=\s*\(tBTA_HD_CBACK_DATA\s+\*\)p_data;\s*tBTA_HD\s+cback_data\s*=\s*\{0\};\s*bdcpy\(cback_data\.conn\.bda,\s*p_cback->addr\);\s*cback_data\.conn\.status\s*=\s*BTA_HD_ERROR;\s*cback_data\.conn\.conn_status\s*=\s*BTA_HD_CONN_STATE_DISCONNECTED;\s*bta_hd_cb\.p_cback\(BTA_HD_OPEN_EVT,\s*&cback_data\);\s*\}" `
    -Replacement @'
extern void bta_hd_open_failure(tBTA_HD_DATA *p_data)
{
    tBTA_HD_CBACK_DATA *p_cback = (tBTA_HD_CBACK_DATA *)p_data;
    tBTA_HD cback_data = {0};
    const uint32_t raw_reason = p_cback->data;
    const uint8_t reason_low = (uint8_t)(raw_reason & 0xFFu);
    const uint8_t reason_high = (uint8_t)((raw_reason >> 8) & 0xFFu);

    bdcpy(cback_data.conn.bda, p_cback->addr);
    cback_data.conn.status = reason_low != 0u ? reason_low : reason_high;
    if (cback_data.conn.status == BTA_HD_OK) {
        cback_data.conn.status = BTA_HD_ERROR;
    }
    cback_data.conn.conn_status = (tBTA_HD_CONN_STAT)reason_high;
    bta_hd_cb.p_cback(BTA_HD_OPEN_EVT, &cback_data);
}
'@

Set-TextPatch `
    -Path $hiddConnPath `
    -Description "encode clean peer L2CAP disconnect channel/state when HID open fails before reports" `
    -AlreadyPattern "switch_diag_close_reason" `
    -SearchPattern @'
static\s+void\s+hidd_l2cif_disconnect_ind\(uint16_t\s+cid,\s+bool\s+ack_needed\)\s*\{\s*tHID_CONN\s+\*p_hcon;\s*HIDD_TRACE_EVENT\("%s:\s+cid=%04x\s+ack_needed=%d",\s+__func__,\s+cid,\s+ack_needed\);\s*p_hcon\s*=\s*&hd_cb\.device\.conn;\s*if\s*\(p_hcon->conn_state\s*==\s*HID_CONN_STATE_UNUSED\s*\|\|\s*\(p_hcon->ctrl_cid\s*!=\s*cid\s*&&\s*p_hcon->intr_cid\s*!=\s*cid\)\)\s*\{\s*HIDD_TRACE_WARNING\("%s:\s+unknown\s+cid=%04x",\s+__func__,\s+cid\);\s*return;\s*\}\s*if\s*\(ack_needed\)\s*\{\s*L2CA_DisconnectRsp\(cid\);\s*\}\s*if\s*\(cid\s*==\s*p_hcon->ctrl_cid\)\s*\{\s*p_hcon->ctrl_cid\s*=\s*0;\s*p_hcon->conn_state\s*=\s*HID_CONN_STATE_DISCONNECTING_CTRL;\s*\}\s*else\s*\{\s*p_hcon->intr_cid\s*=\s*0;\s*p_hcon->conn_state\s*=\s*HID_CONN_STATE_DISCONNECTING_INTR;\s*\}\s*if\s*\(\(p_hcon->ctrl_cid\s*==\s*0\)\s*&&\s*\(p_hcon->intr_cid\s*==\s*0\)\)\s*\{\s*HIDD_TRACE_EVENT\("%s:\s+INTR\s+and\s+CTRL\s+disconnected",\s+__func__\);\s*//\s*clean\s+any\s+outstanding\s+data\s+on\s+intr\s*if\s*\(hd_cb\.pending_data\)\s*\{\s*osi_free\(hd_cb\.pending_data\);\s*hd_cb\.pending_data\s*=\s*NULL;\s*\}\s*hd_cb\.device\.state\s*=\s*HIDD_DEV_NO_CONN;\s*p_hcon->conn_state\s*=\s*HID_CONN_STATE_UNUSED;\s*hd_cb\.callback\(hd_cb\.device\.addr,\s*HID_DHOST_EVT_CLOSE,\s*p_hcon->disc_reason,\s*NULL\);\s*\}\s*\}
'@ `
    -Replacement @'
static void hidd_l2cif_disconnect_ind(uint16_t cid, bool ack_needed)
{
    tHID_CONN *p_hcon;
    HIDD_TRACE_EVENT("%s: cid=%04x ack_needed=%d", __func__, cid, ack_needed);
    p_hcon = &hd_cb.device.conn;
    if (p_hcon->conn_state == HID_CONN_STATE_UNUSED || (p_hcon->ctrl_cid != cid && p_hcon->intr_cid != cid)) {
        HIDD_TRACE_WARNING("%s: unknown cid=%04x", __func__, cid);
        return;
    }

    const uint8_t switch_diag_close_state = p_hcon->conn_state;
    const uint8_t switch_diag_close_psm = (cid == p_hcon->ctrl_cid) ? 0x11u : 0x13u;

    if (ack_needed) {
        L2CA_DisconnectRsp(cid);
    }

    if (cid == p_hcon->ctrl_cid) {
        p_hcon->ctrl_cid = 0;
        p_hcon->conn_state = HID_CONN_STATE_DISCONNECTING_CTRL;
    } else {
        p_hcon->intr_cid = 0;
        p_hcon->conn_state = HID_CONN_STATE_DISCONNECTING_INTR;
    }
    if ((p_hcon->ctrl_cid == 0) && (p_hcon->intr_cid == 0)) {
        uint32_t switch_diag_close_reason = p_hcon->disc_reason;
        HIDD_TRACE_EVENT("%s: INTR and CTRL disconnected", __func__);
        // clean any outstanding data on intr
        if (hd_cb.pending_data) {
            osi_free(hd_cb.pending_data);
            hd_cb.pending_data = NULL;
        }
        hd_cb.device.state = HIDD_DEV_NO_CONN;
        p_hcon->conn_state = HID_CONN_STATE_UNUSED;
        if (switch_diag_close_reason == HID_SUCCESS) {
            switch_diag_close_reason = ((uint32_t)switch_diag_close_state << 8u) | switch_diag_close_psm;
        }
    hd_cb.callback(hd_cb.device.addr, HID_DHOST_EVT_CLOSE, switch_diag_close_reason, NULL);
    }
}
'@

Set-TextPatch `
    -Path $hiddConnPath `
    -Description "add Switch helper to initiate HID interrupt after inbound control configuration" `
    -AlreadyPattern "hidd_switch_try_connect_intr" `
    -SearchPattern @'
static\s+void\s+hidd_check_config_done\(void\)\s*\{\s*tHID_CONN\s+\*p_hcon;\s*p_hcon\s*=\s*&hd_cb\.device\.conn;\s*if\s*\(\(\(p_hcon->conn_flags\s*&\s*HID_CONN_FLAGS_ALL_CONFIGURED\)\s*==\s*HID_CONN_FLAGS_ALL_CONFIGURED\)\s*&&\s*\(p_hcon->conn_state\s*==\s*HID_CONN_STATE_CONFIG\)\)\s*\{\s*p_hcon->conn_state\s*=\s*HID_CONN_STATE_CONNECTED;\s*hd_cb\.device\.state\s*=\s*HIDD_DEV_CONNECTED;\s*hd_cb\.callback\(hd_cb\.device\.addr,\s*HID_DHOST_EVT_OPEN,\s*0,\s*NULL\);\s*//\s*send\s+outstanding\s+data\s+on\s+intr\s*if\s*\(hd_cb\.pending_data\)\s*\{\s*L2CA_DataWrite\(p_hcon->intr_cid,\s*hd_cb\.pending_data\);\s*hd_cb\.pending_data\s*=\s*NULL;\s*\}\s*\}\s*\}
'@ `
    -Replacement @'
static void hidd_check_config_done(void)
{
    tHID_CONN *p_hcon;
    p_hcon = &hd_cb.device.conn;
    if (((p_hcon->conn_flags & HID_CONN_FLAGS_ALL_CONFIGURED) == HID_CONN_FLAGS_ALL_CONFIGURED) &&
        (p_hcon->conn_state == HID_CONN_STATE_CONFIG)) {
        p_hcon->conn_state = HID_CONN_STATE_CONNECTED;
        hd_cb.device.state = HIDD_DEV_CONNECTED;
        hd_cb.callback(hd_cb.device.addr, HID_DHOST_EVT_OPEN, 0, NULL);
        // send outstanding data on intr
        if (hd_cb.pending_data) {
            L2CA_DataWrite(p_hcon->intr_cid, hd_cb.pending_data);
            hd_cb.pending_data = NULL;
        }
    }
}

static bool hidd_switch_try_connect_intr(tHID_CONN *p_hcon)
{
    const uint8_t ctrl_config_done =
        HID_CONN_FLAGS_HIS_CTRL_CFG_DONE | HID_CONN_FLAGS_MY_CTRL_CFG_DONE;

    if ((p_hcon->conn_flags & HID_CONN_FLAGS_IS_ORIG) ||
        p_hcon->intr_cid != 0 ||
        p_hcon->conn_state != HID_CONN_STATE_CONNECTING_INTR ||
        ((p_hcon->conn_flags & ctrl_config_done) != ctrl_config_done)) {
        return true;
    }

    p_hcon->disc_reason = HID_L2CAP_CONN_FAIL;
    if ((p_hcon->intr_cid = L2CA_ConnectReq(HID_PSM_INTERRUPT, hd_cb.device.addr)) == 0) {
        p_hcon->conn_state = HID_CONN_STATE_UNUSED;
        hidd_conn_disconnect();
        HIDD_TRACE_WARNING("%s: could not start outbound INTR for inbound CTRL", __func__);
        hd_cb.callback(hd_cb.device.addr, HID_DHOST_EVT_CLOSE, HID_ERR_L2CAP_FAILED, NULL);
        return false;
    }

    HIDD_TRACE_EVENT("%s: started outbound INTR for inbound CTRL", __func__);
    return true;
}
'@

Set-TextPatch `
    -Path $hiddConnPath `
    -Description "allow Switch inbound-control/outbound-interrupt hybrid L2CAP confirmation" `
    -AlreadyPattern "switch_hybrid_intr_cfm" `
    -SearchPattern @'
if\s*\(!\(p_hcon->conn_flags\s*&\s*HID_CONN_FLAGS_IS_ORIG\)\s*\|\|\s*\(\(cid\s*==\s*p_hcon->ctrl_cid\)\s*&&\s*\(p_hcon->conn_state\s*!=\s*HID_CONN_STATE_CONNECTING_CTRL\s*&&\s*\(p_hcon->conn_state\s*!=\s*HID_CONN_STATE_DISCONNECTING_INTR\)\)\)\s*\|\|\s*\(\(cid\s*==\s*p_hcon->intr_cid\)\s*&&\s*\(p_hcon->conn_state\s*!=\s*HID_CONN_STATE_CONNECTING_INTR\)\s*&&\s*\(p_hcon->conn_state\s*!=\s*HID_CONN_STATE_DISCONNECTING_CTRL\)\)\)\s*\{
'@ `
    -Replacement @'
const bool switch_hybrid_intr_cfm =
        !(p_hcon->conn_flags & HID_CONN_FLAGS_IS_ORIG) &&
        cid == p_hcon->intr_cid &&
        p_hcon->conn_state == HID_CONN_STATE_CONNECTING_INTR;
    if (!switch_hybrid_intr_cfm &&
        (!(p_hcon->conn_flags & HID_CONN_FLAGS_IS_ORIG) ||
         ((cid == p_hcon->ctrl_cid) && (p_hcon->conn_state != HID_CONN_STATE_CONNECTING_CTRL && (p_hcon->conn_state != HID_CONN_STATE_DISCONNECTING_INTR))) ||
         ((cid == p_hcon->intr_cid) && (p_hcon->conn_state != HID_CONN_STATE_CONNECTING_INTR) && (p_hcon->conn_state != HID_CONN_STATE_DISCONNECTING_CTRL)))) {
'@

Set-TextPatch `
    -Path $hiddConnPath `
    -Description "repair config-ind and start outbound interrupt after inbound control config-ind" `
    -AlreadyPattern "Switch quirk: outbound INTR can be needed after inbound CTRL config-ind" `
    -SearchPattern @'
(?s)static void hidd_l2cif_config_ind\(uint16_t cid, tL2CAP_CFG_INFO \*p_cfg\)\s*\{.*?\n\}(?=\s*/\*+.*?static void hidd_l2cif_config_cfm)
'@ `
    -Replacement @'
static void hidd_l2cif_config_ind(uint16_t cid, tL2CAP_CFG_INFO *p_cfg)
{
    tHID_CONN *p_hcon;
    HIDD_TRACE_EVENT("%s: cid=%04x", __func__, cid);
    p_hcon = &hd_cb.device.conn;
    if (p_hcon->ctrl_cid != cid && p_hcon->intr_cid != cid) {
        HIDD_TRACE_WARNING("%s: unknown cid=%04x", __func__, cid);
        return;
    }
    if ((!p_cfg->mtu_present) || (p_cfg->mtu > HID_DEV_MTU_SIZE))
        p_hcon->rem_mtu_size = HID_DEV_MTU_SIZE;
    else
        p_hcon->rem_mtu_size = p_cfg->mtu;
    // accept without changes
    p_cfg->flush_to_present = FALSE;
    p_cfg->mtu_present = FALSE;
    p_cfg->result = L2CAP_CFG_OK;
    if (cid == p_hcon->intr_cid && hd_cb.use_in_qos && !p_cfg->qos_present) {
        p_cfg->qos_present = TRUE;
        memcpy(&p_cfg->qos, &hd_cb.in_qos, sizeof(FLOW_SPEC));
    }
    L2CA_ConfigRsp(cid, p_cfg);
    // update flags
    if (cid == p_hcon->ctrl_cid) {
        p_hcon->conn_flags |= HID_CONN_FLAGS_HIS_CTRL_CFG_DONE;
        if ((p_hcon->conn_flags & HID_CONN_FLAGS_IS_ORIG) && (p_hcon->conn_flags & HID_CONN_FLAGS_MY_CTRL_CFG_DONE) &&
            (p_hcon->conn_state != HID_CONN_STATE_CONNECTING_INTR)) {
            p_hcon->disc_reason = HID_L2CAP_CONN_FAIL;
            if ((p_hcon->intr_cid = L2CA_ConnectReq(HID_PSM_INTERRUPT, hd_cb.device.addr)) == 0) {
                p_hcon->conn_state = HID_CONN_STATE_UNUSED;
                hidd_conn_disconnect();
                HIDD_TRACE_WARNING("%s: could not start L2CAP connection for INTR", __func__);
                hd_cb.callback(hd_cb.device.addr, HID_DHOST_EVT_CLOSE, HID_ERR_L2CAP_FAILED, NULL);
                return;
            } else {
                p_hcon->conn_state = HID_CONN_STATE_CONNECTING_INTR;
            }
        }
        // Switch quirk: outbound INTR can be needed after inbound CTRL config-ind.
        if (!hidd_switch_try_connect_intr(p_hcon)) {
            return;
        }
    } else {
        p_hcon->conn_flags |= HID_CONN_FLAGS_HIS_INTR_CFG_DONE;
    }
    hidd_check_config_done();
}
'@

Set-TextPatch `
    -Path $hiddConnPath `
    -Description "repair config-cfm and start outbound interrupt after inbound control config-cfm" `
    -AlreadyPattern "Switch quirk: outbound INTR can be needed after inbound CTRL config-cfm" `
    -SearchPattern @'
(?s)static void hidd_l2cif_config_cfm\(uint16_t cid, tL2CAP_CFG_INFO \*p_cfg\)\s*\{.*?\n\}(?=\s*/\*+.*?static void hidd_l2cif_disconnect_ind)
'@ `
    -Replacement @'
static void hidd_l2cif_config_cfm(uint16_t cid, tL2CAP_CFG_INFO *p_cfg)
{
    tHID_CONN *p_hcon;
    uint32_t reason;
    HIDD_TRACE_EVENT("%s: cid=%04x pcfg->result=%d", __func__, cid, p_cfg->result);
    p_hcon = &hd_cb.device.conn;
    if (p_hcon->ctrl_cid != cid && p_hcon->intr_cid != cid) {
        HIDD_TRACE_WARNING("%s: unknown cid=%04x", __func__, cid);
        return;
    }
    if (p_hcon->intr_cid == cid && p_cfg->result == L2CAP_CFG_UNACCEPTABLE_PARAMS && p_cfg->qos_present) {
        tL2CAP_CFG_INFO new_qos;
        // QoS parameters not accepted for intr, try again with host proposal
        memcpy(&new_qos, &hd_cb.l2cap_intr_cfg, sizeof(new_qos));
        memcpy(&new_qos.qos, &p_cfg->qos, sizeof(FLOW_SPEC));
        new_qos.qos_present = TRUE;
        HIDD_TRACE_WARNING("%s: config failed, retry", __func__);
        L2CA_ConfigReq(cid, &new_qos);
        return;
    } else if (p_hcon->intr_cid == cid && p_cfg->result == L2CAP_CFG_UNKNOWN_OPTIONS) {
        // QoS not understood by remote device, try configuring without QoS
        HIDD_TRACE_WARNING("%s: config failed, retry without QoS", __func__);
        L2CA_ConfigReq(cid, &hd_cb.l2cap_cfg);
        return;
    } else if (p_cfg->result != L2CAP_CFG_OK) {
        HIDD_TRACE_WARNING("%s: config failed, disconnecting", __func__);
        hidd_conn_disconnect();
        reason = HID_L2CAP_CFG_FAIL | (uint32_t)p_cfg->result;
        hd_cb.callback(hd_cb.device.addr, HID_DHOST_EVT_CLOSE, reason, NULL);
        return;
    }
    // update flags
    if (cid == p_hcon->ctrl_cid) {
        p_hcon->conn_flags |= HID_CONN_FLAGS_MY_CTRL_CFG_DONE;
        if ((p_hcon->conn_flags & HID_CONN_FLAGS_IS_ORIG) && (p_hcon->conn_flags & HID_CONN_FLAGS_HIS_CTRL_CFG_DONE) &&
            (p_hcon->conn_state != HID_CONN_STATE_CONNECTING_INTR)) {
            p_hcon->disc_reason = HID_L2CAP_CONN_FAIL;
            if ((p_hcon->intr_cid = L2CA_ConnectReq(HID_PSM_INTERRUPT, hd_cb.device.addr)) == 0) {
                p_hcon->conn_state = HID_CONN_STATE_UNUSED;
                hidd_conn_disconnect();
                HIDD_TRACE_WARNING("%s: could not start L2CAP connection for INTR", __func__);
                hd_cb.callback(hd_cb.device.addr, HID_DHOST_EVT_CLOSE, HID_ERR_L2CAP_FAILED, NULL);
                return;
            } else {
                p_hcon->conn_state = HID_CONN_STATE_CONNECTING_INTR;
            }
        }
        // Switch quirk: outbound INTR can be needed after inbound CTRL config-cfm.
        if (!hidd_switch_try_connect_intr(p_hcon)) {
            return;
        }
    } else {
        p_hcon->conn_flags |= HID_CONN_FLAGS_MY_INTR_CFG_DONE;
    }
    hidd_check_config_done();
}
'@

if ($VerifyOnly) {
    Write-Host "Verify-only complete. Re-run without -VerifyOnly to patch ESP-IDF."
} else {
    Write-Host "ESP-IDF HID SDP compatibility patch complete."
    Write-Host "Next build should use idf.py fullclean build so the bt component is rebuilt."
}
