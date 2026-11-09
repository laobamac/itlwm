//
//  IO80211AWDLProtocol.h
//  AirportItlwm
//
//  macOS 26 (Tahoe) ABI, generated from the kernel collection symbols.
//  from the IO80211Family vtable in the Tahoe kernel collection.
//

#ifndef IO80211AWDLProtocol_h
#define IO80211AWDLProtocol_h

#include <IOKit/IOService.h>
#include <Airport/apple80211_var.h>
#include <Airport/apple80211_ioctl.h>
#include "IO80211SkywalkInterface.h"

struct apple80211_awdl_af_sec_payload_request;
struct apple80211_awdl_af_tx_mode;
struct apple80211_awdl_bssid;
struct apple80211_awdl_cap;
struct apple80211_awdl_cca;
struct apple80211_awdl_config_request;
struct apple80211_awdl_dev_stats;
struct apple80211_awdl_device_capabilities;
struct apple80211_awdl_dfsp_config;
struct apple80211_awdl_dfsp_ucsa_config;
struct apple80211_awdl_election_id;
struct apple80211_awdl_election_metric;
struct apple80211_awdl_election_params;
struct apple80211_awdl_election_rssi_thresholds;
struct apple80211_awdl_extension_state_machine_parameter;
struct apple80211_awdl_master_channel;
struct apple80211_awdl_min_rate;
struct apple80211_awdl_oob_request;
struct apple80211_awdl_peer_traffic_registration;
struct apple80211_awdl_presence_mode;
struct apple80211_awdl_psf_dwell_params;
struct apple80211_awdl_secondary_master_channel;
struct apple80211_awdl_sync_channel_sequence;
struct apple80211_awdl_sync_enabled;
struct apple80211_awdl_sync_frame_template;
struct apple80211_awdl_sync_params;
struct apple80211_awdl_sync_state;
struct apple80211_awdl_txcal_period;
struct apple80211_bssid_data;
struct apple80211_channel_data;
struct apple80211_companion_skywalk_if_create;
struct apple80211_encrypt;
struct apple80211_key;
struct apple80211_llw_params;
struct apple80211_p2p_coex_protection;
struct apple80211_peer_cache_control;
struct apple80211_peer_cache_maximum_size;
struct apple80211_rtg_peer_stats_cfg;
struct apple80211_ssid_data;
struct apple80211_state_data;
struct apple80211_tx_rate;
struct awdl_peer_advert_add;

class IO80211AWDLProtocol : public IO80211VirtualInterface {
    OSDeclareAbstractStructors(IO80211AWDLProtocol)

public:
    virtual IOReturn getSSID(apple80211_ssid_data*) = 0; // 0x0fc8
    virtual IOReturn getCHANNEL(apple80211_channel_data*) = 0; // 0x0fd0
    virtual IOReturn getBSSID(apple80211_bssid_data*) = 0; // 0x0fd8
    virtual IOReturn getSTATE(apple80211_state_data*) = 0; // 0x0fe0
    virtual IOReturn getAWDL_SYNC_PARAMS(apple80211_awdl_sync_params*) = 0; // 0x0fe8
    virtual IOReturn getAWDL_SYNC_ENABLED(apple80211_awdl_sync_enabled*) = 0; // 0x0ff0
    virtual IOReturn getAWDL_EXTENSION_STATE_MACHINE_PARAMETERS(apple80211_awdl_extension_state_machine_parameter*) = 0; // 0x0ff8
    virtual IOReturn getAWDL_ELECTION_ID(apple80211_awdl_election_id*) = 0; // 0x1000
    virtual IOReturn getAWDL_ELECTION_METRIC(apple80211_awdl_election_metric*) = 0; // 0x1008
    virtual IOReturn getAWDL_SYNCHRONIZATION_CHANNEL_SEQUENCE(apple80211_awdl_sync_channel_sequence*) = 0; // 0x1010
    virtual IOReturn getPEER_CACHE_MAXIMUM_SIZE(apple80211_peer_cache_maximum_size*) = 0; // 0x1018
    virtual IOReturn getAWDL_MASTER_CHANNEL(apple80211_awdl_master_channel*) = 0; // 0x1020
    virtual IOReturn getAWDL_SYNC_STATE(apple80211_awdl_sync_state*) = 0; // 0x1028
    virtual IOReturn getAWDL_ELECTION_RSSI_THRESHOLDS(apple80211_awdl_election_rssi_thresholds*) = 0; // 0x1030
    virtual IOReturn getAWDL_PRESENCE_MODE(apple80211_awdl_presence_mode*) = 0; // 0x1038
    virtual IOReturn getAWDL_ELECTION_PARAMS(apple80211_awdl_election_params*) = 0; // 0x1040
    virtual IOReturn getAWDL_DEVICE_CAPABILITIES(apple80211_awdl_device_capabilities*) = 0; // 0x1048
    virtual IOReturn getAWDL_SECONDARY_MASTER_CHANNEL(apple80211_awdl_secondary_master_channel*) = 0; // 0x1050
    virtual IOReturn getAWDL_AF_TX_MODE(apple80211_awdl_af_tx_mode*) = 0; // 0x1058
    virtual IOReturn getAWDL_MIN_RATE(apple80211_awdl_min_rate*) = 0; // 0x1060
    virtual IOReturn getAWDL_ENCRYPTION_TYPE(apple80211_encrypt*) = 0; // 0x1068
    virtual IOReturn getAWDL_TXCAL_PERIOD(apple80211_awdl_txcal_period*) = 0; // 0x1070
    virtual IOReturn getAWDL_DEV_STATS(apple80211_awdl_dev_stats*) = 0; // 0x1078
    virtual IOReturn getAWDL_DFSP_CONFIG(apple80211_awdl_dfsp_config*) = 0; // 0x1080
    virtual IOReturn getAWDL_CCA(apple80211_awdl_cca*) = 0; // 0x1088
    virtual IOReturn getAWDL_AF_SECONDARY_PAYLOAD_CAPABILITIES(apple80211_awdl_cap*) = 0; // 0x1090
    virtual IOReturn getLLW_PARAMS(apple80211_llw_params*) = 0; // 0x1098
    virtual IOReturn setSSID(apple80211_ssid_data*) = 0; // 0x10a0
    virtual IOReturn setCHANNEL(apple80211_channel_data*) = 0; // 0x10a8
    virtual IOReturn setAWDL_SYNC_PARAMS(apple80211_awdl_sync_params*) = 0; // 0x10b0
    virtual IOReturn setAWDL_SYNC_ENABLED(apple80211_awdl_sync_enabled*) = 0; // 0x10b8
    virtual IOReturn setAWDL_EXTENSION_STATE_MACHINE_PARAMETERS(apple80211_awdl_extension_state_machine_parameter*) = 0; // 0x10c0
    virtual IOReturn setAWDL_ELECTION_ID(apple80211_awdl_election_id*) = 0; // 0x10c8
    virtual IOReturn setAWDL_BSSID(apple80211_awdl_bssid*) = 0; // 0x10d0
    virtual IOReturn setAWDL_ELECTION_METRIC(apple80211_awdl_election_metric*) = 0; // 0x10d8
    virtual IOReturn setAWDL_SYNCHRONIZATION_CHANNEL_SEQUENCE(apple80211_awdl_sync_channel_sequence*) = 0; // 0x10e0
    virtual IOReturn setAWDL_MASTER_CHANNEL(apple80211_awdl_master_channel*) = 0; // 0x10e8
    virtual IOReturn setAWDL_SYNC_STATE(apple80211_awdl_sync_state*) = 0; // 0x10f0
    virtual IOReturn setAWDL_ELECTION_RSSI_THRESHOLDS(apple80211_awdl_election_rssi_thresholds*) = 0; // 0x10f8
    virtual IOReturn setAWDL_PRESENCE_MODE(apple80211_awdl_presence_mode*) = 0; // 0x1100
    virtual IOReturn setPEER_CACHE_CONTROL(apple80211_peer_cache_control*) = 0; // 0x1108
    virtual IOReturn setAWDL_OOB_REQUEST(apple80211_awdl_oob_request*) = 0; // 0x1110
    virtual IOReturn setAWDL_SYNC_FRAME_TEMPLATE(apple80211_awdl_sync_frame_template*) = 0; // 0x1118
    virtual IOReturn setAWDL_ELECTION_PARAMS(apple80211_awdl_election_params*) = 0; // 0x1120
    virtual IOReturn setAWDL_PEER_TRAFFIC_REGISTRATION(apple80211_awdl_peer_traffic_registration*) = 0; // 0x1128
    virtual IOReturn setAWDL_SECONDARY_MASTER_CHANNEL(apple80211_awdl_secondary_master_channel*) = 0; // 0x1130
    virtual IOReturn setAWDL_AF_TX_MODE(apple80211_awdl_af_tx_mode*) = 0; // 0x1138
    virtual IOReturn setAWDL_MIN_RATE(apple80211_awdl_min_rate*) = 0; // 0x1140
    virtual IOReturn setAWDL_ENCRYPTION_KEYS(apple80211_key*) = 0; // 0x1148
    virtual IOReturn setAWDL_ENCRYPTION_TYPE(apple80211_encrypt*) = 0; // 0x1150
    virtual IOReturn setAWDL_OOB_AUTO_REQUEST(apple80211_awdl_oob_request*) = 0; // 0x1158
    virtual IOReturn setAWDL_TXCAL_PERIOD(apple80211_awdl_txcal_period*) = 0; // 0x1160
    virtual IOReturn setAWDL_AWDL_ADVERTISERS(awdl_peer_advert_add*) = 0; // 0x1168
    virtual IOReturn setAWDL_DFSP_CONFIG(apple80211_awdl_dfsp_config*) = 0; // 0x1170
    virtual IOReturn setAWDL_DFSP_UCSA_CONFIG(apple80211_awdl_dfsp_ucsa_config*) = 0; // 0x1178
    virtual IOReturn setCOMPANION_SKYWALK_IF_CREATE(apple80211_companion_skywalk_if_create*) = 0; // 0x1180
    virtual IOReturn setAWDL_OOB_SEC_PAYLOAD_REQUEST(apple80211_awdl_af_sec_payload_request*) = 0; // 0x1188
    virtual IOReturn setLLW_PARAMS(apple80211_llw_params*) = 0; // 0x1190
    virtual IOReturn setTX_RATE(apple80211_tx_rate*) = 0; // 0x1198
    virtual IOReturn setP2P_COEX_PROTECTION(apple80211_p2p_coex_protection*) = 0; // 0x11a0
    virtual IOReturn setPSF_DWELL(apple80211_awdl_psf_dwell_params*) = 0; // 0x11a8
    virtual IOReturn setAWDL_CONFIG(apple80211_awdl_config_request*) = 0; // 0x11b0
    virtual IOReturn setAWDL_RTG_PEER_STATS_CONFIG(apple80211_rtg_peer_stats_cfg*) = 0; // 0x11b8

};
// IO80211AWDLProtocol adds no ivars on Tahoe (OSMetaClass size 0x130).
static_assert(sizeof(IO80211AWDLProtocol) == 0x130, "Tahoe ABI size");

#endif /* IO80211AWDLProtocol_h */
