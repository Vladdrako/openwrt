/*
 * hostapd / ubus support
 * Copyright (c) 2013, Felix Fietkau <nbd@nbd.name>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/wpabuf.h"
#include "common/ieee802_11_defs.h"
#include "common/hw_features_common.h"
#include "hostapd.h"
#include "neighbor_db.h"
#include "wps_hostapd.h"
#include "sta_info.h"
#include "ubus.h"
#include "ap_drv_ops.h"
#include "beacon.h"
#include "rrm.h"
#include "wnm_ap.h"
#include "taxonomy.h"
#include "airtime_policy.h"
#include "hw_features.h"

static struct ubus_context *ctx;
static struct blob_buf b;
static int ctx_ref;

static inline struct hostapd_data *get_hapd_from_object(struct ubus_object *obj)
{
	return container_of(obj, struct hostapd_data, ubus.obj);
}

struct ubus_banned_client {
	struct avl_node avl;
	u8 addr[ETH_ALEN];
};

static void ubus_reconnect_timeout(void *eloop_data, void *user_ctx)
{
	if (ubus_reconnect(ctx, NULL)) {
		eloop_register_timeout(1, 0, ubus_reconnect_timeout, ctx, NULL);
		return;
	}

	ubus_add_uloop(ctx);
}

static void hostapd_ubus_connection_lost(struct ubus_context *ctx)
{
	uloop_fd_delete(&ctx->sock);
	eloop_register_timeout(1, 0, ubus_reconnect_timeout, ctx, NULL);
}

static bool hostapd_ubus_init(void)
{
	if (ctx)
		return true;

	eloop_add_uloop();
	ctx = ubus_connect(NULL);
	if (!ctx)
		return false;

	ctx->connection_lost = hostapd_ubus_connection_lost;
	ubus_add_uloop(ctx);

	return true;
}

static void hostapd_ubus_ref_inc(void)
{
	ctx_ref++;
}

static void hostapd_ubus_ref_dec(void)
{
	ctx_ref--;
	if (!ctx)
		return;

	if (ctx_ref)
		return;

	uloop_fd_delete(&ctx->sock);
	ubus_free(ctx);
	ctx = NULL;
}

void hostapd_ubus_add_iface(struct hostapd_iface *iface)
{
	if (!hostapd_ubus_init())
		return;
}

void hostapd_ubus_free_iface(struct hostapd_iface *iface)
{
	if (!ctx)
		return;
}

static void hostapd_notify_ubus(struct ubus_object *obj, char *bssname, char *event)
{
	char *event_type;

	if (!ctx || !obj)
		return;

	if (asprintf(&event_type, "bss.%s", event) < 0)
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "name", bssname);
	ubus_notify(ctx, obj, event_type, b.head, -1);
	free(event_type);
}

static void
hostapd_bss_del_ban(void *eloop_data, void *user_ctx)
{
	struct ubus_banned_client *ban = eloop_data;
	struct hostapd_data *hapd = user_ctx;

	avl_delete(&hapd->ubus.banned, &ban->avl);
	free(ban);
}

static void
hostapd_bss_ban_client(struct hostapd_data *hapd, u8 *addr, int time)
{
	struct ubus_banned_client *ban;

	if (time < 0)
		time = 0;

	ban = avl_find_element(&hapd->ubus.banned, addr, ban, avl);
	if (!ban) {
		if (!time)
			return;

		ban = os_zalloc(sizeof(*ban));
		memcpy(ban->addr, addr, sizeof(ban->addr));
		ban->avl.key = ban->addr;
		avl_insert(&hapd->ubus.banned, &ban->avl);
	} else {
		eloop_cancel_timeout(hostapd_bss_del_ban, ban, hapd);
		if (!time) {
			hostapd_bss_del_ban(ban, hapd);
			return;
		}
	}

	eloop_register_timeout(0, time * 1000, hostapd_bss_del_ban, ban, hapd);
}

static int
hostapd_bss_reload(struct ubus_context *ctx, struct ubus_object *obj,
		   struct ubus_request_data *req, const char *method,
		   struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);

	return hostapd_reload_config(hapd->iface);
}


static void
hostapd_parse_vht_map_blobmsg(uint16_t map)
{
	char label[4];
	int16_t val;
	int i;

	for (i = 0; i < 8; i++) {
		snprintf(label, 4, "%dss", i + 1);

		val = (map & (BIT(1) | BIT(0))) + 7;
		blobmsg_add_u16(&b, label, val == 10 ? -1 : val);
		map = map >> 2;
	}
}

static void
hostapd_parse_vht_capab_blobmsg(struct ieee80211_vht_capabilities *vhtc)
{
	void *supported_mcs;
	void *map;
	int i;

	static const struct {
		const char *name;
		uint32_t flag;
	} vht_capas[] = {
		{ "su_beamformee", VHT_CAP_SU_BEAMFORMEE_CAPABLE },
		{ "mu_beamformee", VHT_CAP_MU_BEAMFORMEE_CAPABLE },
	};

	for (i = 0; i < ARRAY_SIZE(vht_capas); i++)
		blobmsg_add_u8(&b, vht_capas[i].name,
				!!(vhtc->vht_capabilities_info & vht_capas[i].flag));

	supported_mcs = blobmsg_open_table(&b, "mcs_map");

	/* RX map */
	map = blobmsg_open_table(&b, "rx");
	hostapd_parse_vht_map_blobmsg(le_to_host16(vhtc->vht_supported_mcs_set.rx_map));
	blobmsg_close_table(&b, map);

	/* TX map */
	map = blobmsg_open_table(&b, "tx");
	hostapd_parse_vht_map_blobmsg(le_to_host16(vhtc->vht_supported_mcs_set.tx_map));
	blobmsg_close_table(&b, map);

	blobmsg_close_table(&b, supported_mcs);
}

static void
hostapd_parse_capab_blobmsg(struct sta_info *sta)
{
	void *r, *v;

	v = blobmsg_open_table(&b, "capabilities");

	if (sta->vht_capabilities) {
		r = blobmsg_open_table(&b, "vht");
		hostapd_parse_vht_capab_blobmsg(sta->vht_capabilities);
		blobmsg_close_table(&b, r);
	}

	/* ToDo: Add HT / HE capability parsing */

	blobmsg_close_table(&b, v);
}

static int
hostapd_bss_get_clients(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	struct hostap_sta_driver_data sta_driver_data;
	struct sta_info *sta;
	void *list, *c;
	char mac_buf[20];
	static const struct {
		const char *name;
		uint32_t flag;
	} sta_flags[] = {
		{ "auth", WLAN_STA_AUTH },
		{ "assoc", WLAN_STA_ASSOC },
		{ "authorized", WLAN_STA_AUTHORIZED },
		{ "preauth", WLAN_STA_PREAUTH },
		{ "wds", WLAN_STA_WDS },
		{ "wmm", WLAN_STA_WMM },
		{ "ht", WLAN_STA_HT },
		{ "vht", WLAN_STA_VHT },
		{ "he", WLAN_STA_HE },
		{ "wps", WLAN_STA_WPS },
		{ "mfp", WLAN_STA_MFP },
	};

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "freq", hapd->iface->freq);
	list = blobmsg_open_table(&b, "clients");
	for (sta = hapd->sta_list; sta; sta = sta->next) {
		void *r;
		int i;

		sprintf(mac_buf, MACSTR, MAC2STR(sta->addr));
		c = blobmsg_open_table(&b, mac_buf);
		for (i = 0; i < ARRAY_SIZE(sta_flags); i++)
			blobmsg_add_u8(&b, sta_flags[i].name,
				       !!(sta->flags & sta_flags[i].flag));

#ifdef CONFIG_MBO
		blobmsg_add_u8(&b, "mbo", !!(sta->cell_capa));
#endif

		r = blobmsg_open_array(&b, "rrm");
		for (i = 0; i < ARRAY_SIZE(sta->rrm_enabled_capa); i++)
			blobmsg_add_u32(&b, "", sta->rrm_enabled_capa[i]);
		blobmsg_close_array(&b, r);

		r = blobmsg_open_array(&b, "extended_capabilities");
		/* Check if client advertises extended capabilities */
		if (sta->ext_capability && sta->ext_capability[0] > 0) {
			for (i = 0; i < sta->ext_capability[0]; i++) {
				blobmsg_add_u32(&b, "", sta->ext_capability[1 + i]);
			}
		}
		blobmsg_close_array(&b, r);

		blobmsg_add_u32(&b, "aid", sta->aid);
#ifdef CONFIG_TAXONOMY
		r = blobmsg_alloc_string_buffer(&b, "signature", 1024);
		if (retrieve_sta_taxonomy(hapd, sta, r, 1024) > 0)
			blobmsg_add_string_buffer(&b);
#endif

		/* Driver information */
		if (hostapd_drv_read_sta_data(hapd, &sta_driver_data, sta->addr) >= 0) {
			r = blobmsg_open_table(&b, "bytes");
			blobmsg_add_u64(&b, "rx", sta_driver_data.rx_bytes);
			blobmsg_add_u64(&b, "tx", sta_driver_data.tx_bytes);
			blobmsg_close_table(&b, r);
			r = blobmsg_open_table(&b, "airtime");
			blobmsg_add_u64(&b, "rx", sta_driver_data.rx_airtime);
			blobmsg_add_u64(&b, "tx", sta_driver_data.tx_airtime);
			blobmsg_close_table(&b, r);
			r = blobmsg_open_table(&b, "packets");
			blobmsg_add_u32(&b, "rx", sta_driver_data.rx_packets);
			blobmsg_add_u32(&b, "tx", sta_driver_data.tx_packets);
			blobmsg_close_table(&b, r);
			r = blobmsg_open_table(&b, "rate");
			/* Rate in kbits */
			blobmsg_add_u32(&b, "rx", sta_driver_data.current_rx_rate * 100);
			blobmsg_add_u32(&b, "tx", sta_driver_data.current_tx_rate * 100);
			blobmsg_close_table(&b, r);
			blobmsg_add_u32(&b, "signal", sta_driver_data.signal);
		}

		hostapd_parse_capab_blobmsg(sta);

		blobmsg_close_table(&b, c);
	}
	blobmsg_close_array(&b, list);
	ubus_send_reply(ctx, req, b.head);

	return 0;
}

static int
hostapd_bss_get_features(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);

	blob_buf_init(&b, 0);
	blobmsg_add_u8(&b, "ht_supported", ht_supported(hapd->iface->hw_features));
	blobmsg_add_u8(&b, "vht_supported", vht_supported(hapd->iface->hw_features));
	ubus_send_reply(ctx, req, b.head);

	return 0;
}

static int
hostapd_bss_get_status(struct ubus_context *ctx, struct ubus_object *obj,
		       struct ubus_request_data *req, const char *method,
		       struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	void *airtime_table, *dfs_table, *rrm_table, *wnm_table;
	struct os_reltime now;
	char ssid[SSID_MAX_LEN + 1];
	char phy_name[17];
	size_t ssid_len = SSID_MAX_LEN;
	u8 channel = 0, op_class = 0;

	if (hapd->conf->ssid.ssid_len < SSID_MAX_LEN)
		ssid_len = hapd->conf->ssid.ssid_len;

	ieee80211_freq_to_channel_ext(hapd->iface->freq,
				      hapd->iconf->secondary_channel,
				      hostapd_get_oper_chwidth(hapd->iconf),
				      &op_class, &channel);

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "driver", hapd->driver->name);
	blobmsg_add_string(&b, "status", hostapd_state_text(hapd->iface->state));
	blobmsg_printf(&b, "bssid", MACSTR, MAC2STR(hapd->conf->bssid));

	memset(ssid, 0, SSID_MAX_LEN + 1);
	memcpy(ssid, hapd->conf->ssid.ssid, ssid_len);
	blobmsg_add_string(&b, "ssid", ssid);

	blobmsg_add_u32(&b, "freq", hapd->iface->freq);
	blobmsg_add_u32(&b, "channel", channel);
	blobmsg_add_u32(&b, "op_class", op_class);
	blobmsg_add_u32(&b, "beacon_interval", hapd->iconf->beacon_int);
#ifdef CONFIG_IEEE80211AX
	blobmsg_add_u32(&b, "bss_color", hapd->iface->conf->he_op.he_bss_color_disabled ? -1 :
					 hapd->iface->conf->he_op.he_bss_color);
#else
	blobmsg_add_u32(&b, "bss_color", -1);
#endif

	snprintf(phy_name, 17, "%s", hapd->iface->phy);
	blobmsg_add_string(&b, "phy", phy_name);

	/* RRM */
	rrm_table = blobmsg_open_table(&b, "rrm");
	blobmsg_add_u64(&b, "neighbor_report_tx", hapd->openwrt_stats.rrm.neighbor_report_tx);
	blobmsg_close_table(&b, rrm_table);

	/* WNM */
	wnm_table = blobmsg_open_table(&b, "wnm");
	blobmsg_add_u64(&b, "bss_transition_query_rx", hapd->openwrt_stats.wnm.bss_transition_query_rx);
	blobmsg_add_u64(&b, "bss_transition_request_tx", hapd->openwrt_stats.wnm.bss_transition_request_tx);
	blobmsg_add_u64(&b, "bss_transition_response_rx", hapd->openwrt_stats.wnm.bss_transition_response_rx);
	blobmsg_close_table(&b, wnm_table);

	/* Airtime */
	airtime_table = blobmsg_open_table(&b, "airtime");
	blobmsg_add_u64(&b, "time", hapd->iface->last_channel_time);
	blobmsg_add_u64(&b, "time_busy", hapd->iface->last_channel_time_busy);
	blobmsg_add_u16(&b, "utilization", hapd->iface->channel_utilization);
	blobmsg_close_table(&b, airtime_table);

	/* DFS */
	dfs_table = blobmsg_open_table(&b, "dfs");
	blobmsg_add_u32(&b, "cac_seconds", hapd->iface->dfs_cac_ms / 1000);
	blobmsg_add_u8(&b, "cac_active", !!(hapd->iface->cac_started));
	os_reltime_age(&hapd->iface->dfs_cac_start, &now);
	blobmsg_add_u32(&b, "cac_seconds_left",
			hapd->iface->cac_started ? hapd->iface->dfs_cac_ms / 1000 - now.sec : 0);
	blobmsg_close_table(&b, dfs_table);

	ubus_send_reply(ctx, req, b.head);

	return 0;
}

enum {
	NOTIFY_RESPONSE,
	__NOTIFY_MAX
};

static const struct blobmsg_policy notify_policy[__NOTIFY_MAX] = {
	[NOTIFY_RESPONSE] = { "notify_response", BLOBMSG_TYPE_INT32 },
};

static int
hostapd_notify_response(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	struct blob_attr *tb[__NOTIFY_MAX];
	struct hostapd_data *hapd = get_hapd_from_object(obj);
	struct wpabuf *elems;
	const char *pos;
	size_t len;

	blobmsg_parse(notify_policy, __NOTIFY_MAX, tb,
		      blob_data(msg), blob_len(msg));

	if (!tb[NOTIFY_RESPONSE])
		return UBUS_STATUS_INVALID_ARGUMENT;

	hapd->ubus.notify_response = blobmsg_get_u32(tb[NOTIFY_RESPONSE]);

	return UBUS_STATUS_OK;
}

enum {
	DEL_CLIENT_ADDR,
	DEL_CLIENT_REASON,
	DEL_CLIENT_DEAUTH,
	DEL_CLIENT_BAN_TIME,
	__DEL_CLIENT_MAX
};

static const struct blobmsg_policy del_policy[__DEL_CLIENT_MAX] = {
	[DEL_CLIENT_ADDR] = { "addr", BLOBMSG_TYPE_STRING },
	[DEL_CLIENT_REASON] = { "reason", BLOBMSG_TYPE_INT32 },
	[DEL_CLIENT_DEAUTH] = { "deauth", BLOBMSG_TYPE_INT8 },
	[DEL_CLIENT_BAN_TIME] = { "ban_time", BLOBMSG_TYPE_INT32 },
};

static int
hostapd_bss_del_client(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	struct blob_attr *tb[__DEL_CLIENT_MAX];
	const u8 bcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	struct sta_info *sta;
	bool deauth = false;
	int reason;
	u8 addr[ETH_ALEN];

	blobmsg_parse(del_policy, __DEL_CLIENT_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[DEL_CLIENT_ADDR])
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (hwaddr_aton(blobmsg_data(tb[DEL_CLIENT_ADDR]), addr))
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (tb[DEL_CLIENT_REASON])
		reason = blobmsg_get_u32(tb[DEL_CLIENT_REASON]);

	if (tb[DEL_CLIENT_DEAUTH])
		deauth = blobmsg_get_bool(tb[DEL_CLIENT_DEAUTH]);

	if (deauth)
		hostapd_drv_sta_deauth(hapd, addr, reason);
	else
		hostapd_drv_sta_disassoc(hapd, addr, reason);

	sta = ap_get_sta(hapd, addr);
	if (sta) {
		if (deauth)
			ap_sta_deauthenticate(hapd, sta, reason);
		else
			ap_sta_disassociate(hapd, sta, reason);
	} else if (memcmp(addr, bcast, ETH_ALEN) == 0) {
		hostapd_free_stas(hapd);
	}

	if (tb[DEL_CLIENT_BAN_TIME])
		hostapd_bss_ban_client(hapd, addr, blobmsg_get_u32(tb[DEL_CLIENT_BAN_TIME]));

	return 0;
}

static void
blobmsg_add_macaddr(struct blob_buf *buf, const char *name, const u8 *addr)
{
	char *s;

	s = blobmsg_alloc_string_buffer(buf, name, 20);
	sprintf(s, MACSTR, MAC2STR(addr));
	blobmsg_add_string_buffer(buf);
}

static int
hostapd_bss_list_bans(struct ubus_context *ctx, struct ubus_object *obj,
		      struct ubus_request_data *req, const char *method,
		      struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	struct ubus_banned_client *ban;
	void *c;

	blob_buf_init(&b, 0);
	c = blobmsg_open_array(&b, "clients");
	avl_for_each_element(&hapd->ubus.banned, ban, avl)
		blobmsg_add_macaddr(&b, NULL, ban->addr);
	blobmsg_close_array(&b, c);
	ubus_send_reply(ctx, req, b.head);

	return 0;
}

#ifdef CONFIG_WPS
static int
hostapd_bss_wps_start(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	int rc;
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);

	rc = hostapd_wps_button_pushed(hapd, NULL);

	if (rc != 0)
		return UBUS_STATUS_NOT_SUPPORTED;

	return 0;
}


static const char * pbc_status_enum_str(enum pbc_status status)
{
	switch (status) {
	case WPS_PBC_STATUS_DISABLE:
		return "Disabled";
	case WPS_PBC_STATUS_ACTIVE:
		return "Active";
	case WPS_PBC_STATUS_TIMEOUT:
		return "Timed-out";
	case WPS_PBC_STATUS_OVERLAP:
		return "Overlap";
	default:
		return "Unknown";
	}
}

static int
hostapd_bss_wps_status(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	int rc;
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);

	blob_buf_init(&b, 0);

	blobmsg_add_string(&b, "pbc_status", pbc_status_enum_str(hapd->wps_stats.pbc_status));
	blobmsg_add_string(&b, "last_wps_result",
			   (hapd->wps_stats.status == WPS_STATUS_SUCCESS ?
			    "Success":
			    (hapd->wps_stats.status == WPS_STATUS_FAILURE ?
			     "Failed" : "None")));

	/* If status == Failure - Add possible Reasons */
	if(hapd->wps_stats.status == WPS_STATUS_FAILURE &&
	   hapd->wps_stats.failure_reason > 0)
		blobmsg_add_string(&b, "reason", wps_ei_str(hapd->wps_stats.failure_reason));

	if (hapd->wps_stats.status)
		blobmsg_printf(&b, "peer_address", MACSTR, MAC2STR(hapd->wps_stats.peer_addr));

	ubus_send_reply(ctx, req, b.head);

	return 0;
}

static int
hostapd_bss_wps_cancel(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	int rc;
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);

	rc = hostapd_wps_cancel(hapd);

	if (rc != 0)
		return UBUS_STATUS_NOT_SUPPORTED;

	return 0;
}
#endif /* CONFIG_WPS */

static int
hostapd_bss_update_beacon(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	int rc;
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);

	rc = ieee802_11_set_beacon(hapd);

	if (rc != 0)
		return UBUS_STATUS_NOT_SUPPORTED;

	return 0;
}

enum {
	CONFIG_IFACE,
	CONFIG_FILE,
	__CONFIG_MAX
};

enum {
	CSA_FREQ,
	CSA_BCN_COUNT,
	CSA_CENTER_FREQ1,
	CSA_CENTER_FREQ2,
	CSA_BANDWIDTH,
	CSA_SEC_CHANNEL_OFFSET,
	CSA_HT,
	CSA_VHT,
	CSA_HE,
	CSA_BLOCK_TX,
	CSA_FORCE,
	__CSA_MAX
};

static const struct blobmsg_policy csa_policy[__CSA_MAX] = {
	[CSA_FREQ] = { "freq", BLOBMSG_TYPE_INT32 },
	[CSA_BCN_COUNT] = { "bcn_count", BLOBMSG_TYPE_INT32 },
	[CSA_CENTER_FREQ1] = { "center_freq1", BLOBMSG_TYPE_INT32 },
	[CSA_CENTER_FREQ2] = { "center_freq2", BLOBMSG_TYPE_INT32 },
	[CSA_BANDWIDTH] = { "bandwidth", BLOBMSG_TYPE_INT32 },
	[CSA_SEC_CHANNEL_OFFSET] = { "sec_channel_offset", BLOBMSG_TYPE_INT32 },
	[CSA_HT] = { "ht", BLOBMSG_TYPE_BOOL },
	[CSA_VHT] = { "vht", BLOBMSG_TYPE_BOOL },
	[CSA_HE] = { "he", BLOBMSG_TYPE_BOOL },
	[CSA_BLOCK_TX] = { "block_tx", BLOBMSG_TYPE_BOOL },
	[CSA_FORCE] = { "force", BLOBMSG_TYPE_BOOL },
};


static void switch_chan_fallback_cb(void *eloop_data, void *user_ctx)
{
	struct hostapd_iface *iface = eloop_data;
	struct hostapd_freq_params *freq_params = user_ctx;

	hostapd_switch_channel_fallback(iface, freq_params);
}

#ifdef NEED_AP_MLME
static int
hostapd_switch_chan(struct ubus_context *ctx, struct ubus_object *obj,
		    struct ubus_request_data *req, const char *method,
		    struct blob_attr *msg)
{
	struct blob_attr *tb[__CSA_MAX];
	struct hostapd_data *hapd = get_hapd_from_object(obj);
	struct hostapd_config *iconf = hapd->iface->conf;
	struct hostapd_freq_params *freq_params;
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	struct csa_settings css = {
		.freq_params = {
			.ht_enabled = iconf->ieee80211n,
			.vht_enabled = iconf->ieee80211ac,
			.he_enabled = iconf->ieee80211ax,
			.sec_channel_offset = iconf->secondary_channel,
		}
	};
	u8 chwidth = hostapd_get_oper_chwidth(iconf);
	u8 seg0 = 0, seg1 = 0;
	int ret = UBUS_STATUS_OK;
	int i;

	blobmsg_parse(csa_policy, __CSA_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[CSA_FREQ])
		return UBUS_STATUS_INVALID_ARGUMENT;

	switch (iconf->vht_oper_chwidth) {
	case CHANWIDTH_USE_HT:
		if (iconf->secondary_channel)
			css.freq_params.bandwidth = 40;
		else
			css.freq_params.bandwidth = 20;
		break;
	case CHANWIDTH_160MHZ:
		css.freq_params.bandwidth = 160;
		break;
	default:
		css.freq_params.bandwidth = 80;
		break;
	}

	css.freq_params.freq = blobmsg_get_u32(tb[CSA_FREQ]);

#define SET_CSA_SETTING(name, field, type) \
	do { \
		if (tb[name]) \
			css.field = blobmsg_get_ ## type(tb[name]); \
	} while(0)

	SET_CSA_SETTING(CSA_BCN_COUNT, cs_count, u32);
	SET_CSA_SETTING(CSA_CENTER_FREQ1, freq_params.center_freq1, u32);
	SET_CSA_SETTING(CSA_CENTER_FREQ2, freq_params.center_freq2, u32);
	SET_CSA_SETTING(CSA_BANDWIDTH, freq_params.bandwidth, u32);
	SET_CSA_SETTING(CSA_SEC_CHANNEL_OFFSET, freq_params.sec_channel_offset, u32);
	SET_CSA_SETTING(CSA_HT, freq_params.ht_enabled, bool);
	SET_CSA_SETTING(CSA_VHT, freq_params.vht_enabled, bool);
	SET_CSA_SETTING(CSA_HE, freq_params.he_enabled, bool);
	SET_CSA_SETTING(CSA_BLOCK_TX, block_tx, bool);

	css.freq_params.channel = hostapd_hw_get_channel(hapd, css.freq_params.freq);
	if (!css.freq_params.channel)
		return UBUS_STATUS_NOT_SUPPORTED;

	switch (css.freq_params.bandwidth) {
	case 160:
		chwidth = CHANWIDTH_160MHZ;
		break;
	case 80:
		chwidth = css.freq_params.center_freq2 ? CHANWIDTH_80P80MHZ : CHANWIDTH_80MHZ;
		break;
	default:
		chwidth = CHANWIDTH_USE_HT;
		break;
	}

	hostapd_set_freq_params(&css.freq_params, iconf->hw_mode,
				css.freq_params.freq,
				css.freq_params.channel, iconf->enable_edmg,
				iconf->edmg_channel,
				css.freq_params.ht_enabled,
				css.freq_params.vht_enabled,
				css.freq_params.he_enabled,
				css.freq_params.eht_enabled,
				css.freq_params.sec_channel_offset,
				chwidth, seg0, seg1,
				iconf->vht_capab,
				mode ? &mode->he_capab[IEEE80211_MODE_AP] :
				NULL,
				mode ? &mode->eht_capab[IEEE80211_MODE_AP] :
				NULL,
				hostapd_get_punct_bitmap(hapd));

	for (i = 0; i < hapd->iface->num_bss; i++) {
		struct hostapd_data *bss = hapd->iface->bss[i];

		if (hostapd_switch_channel(bss, &css) != 0)
			ret = UBUS_STATUS_NOT_SUPPORTED;
	}

	if (!ret || !tb[CSA_FORCE] || !blobmsg_get_bool(tb[CSA_FORCE]))
		return ret;

	freq_params = malloc(sizeof(*freq_params));
	memcpy(freq_params, &css.freq_params, sizeof(*freq_params));
	eloop_register_timeout(0, 1, switch_chan_fallback_cb,
			       hapd->iface, freq_params);

	return 0;
#undef SET_CSA_SETTING
}
#endif

enum {
	VENDOR_ELEMENTS,
	__VENDOR_ELEMENTS_MAX
};

static const struct blobmsg_policy ve_policy[__VENDOR_ELEMENTS_MAX] = {
	/* vendor elements are provided as hex-string */
	[VENDOR_ELEMENTS] = { "vendor_elements", BLOBMSG_TYPE_STRING },
};

static int
hostapd_vendor_elements(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	struct blob_attr *tb[__VENDOR_ELEMENTS_MAX];
	struct hostapd_data *hapd = get_hapd_from_object(obj);
	struct hostapd_bss_config *bss = hapd->conf;
	struct wpabuf *elems;
	const char *pos;
	size_t len;

	blobmsg_parse(ve_policy, __VENDOR_ELEMENTS_MAX, tb,
		      blob_data(msg), blob_len(msg));

	if (!tb[VENDOR_ELEMENTS])
		return UBUS_STATUS_INVALID_ARGUMENT;

	pos = blobmsg_data(tb[VENDOR_ELEMENTS]);
	len = os_strlen(pos);
	if (len & 0x01)
			return UBUS_STATUS_INVALID_ARGUMENT;

	len /= 2;
	if (len == 0) {
		wpabuf_free(bss->vendor_elements);
		bss->vendor_elements = NULL;
		return 0;
	}

	elems = wpabuf_alloc(len);
	if (elems == NULL)
		return 1;

	if (hexstr2bin(pos, wpabuf_put(elems, len), len)) {
		wpabuf_free(elems);
		return UBUS_STATUS_INVALID_ARGUMENT;
	}

	wpabuf_free(bss->vendor_elements);
	bss->vendor_elements = elems;

	/* update beacons if vendor elements were set successfully */
	if (ieee802_11_update_beacons(hapd->iface) != 0)
		return UBUS_STATUS_NOT_SUPPORTED;
	return UBUS_STATUS_OK;
}

static void
hostapd_rrm_print_nr(struct hostapd_neighbor_entry *nr)
{
	const u8 *data;
	char *str;
	int len;

	blobmsg_printf(&b, "", MACSTR, MAC2STR(nr->bssid));

	str = blobmsg_alloc_string_buffer(&b, "", nr->ssid.ssid_len + 1);
	memcpy(str, nr->ssid.ssid, nr->ssid.ssid_len);
	str[nr->ssid.ssid_len] = 0;
	blobmsg_add_string_buffer(&b);

	len = wpabuf_len(nr->nr);
	str = blobmsg_alloc_string_buffer(&b, "", 2 * len + 1);
	wpa_snprintf_hex(str, 2 * len + 1, wpabuf_head_u8(nr->nr), len);
	blobmsg_add_string_buffer(&b);
}

enum {
	BSS_MGMT_EN_NEIGHBOR,
	BSS_MGMT_EN_BEACON,
	BSS_MGMT_EN_LINK_MEASUREMENT,
#ifdef CONFIG_WNM_AP
	BSS_MGMT_EN_BSS_TRANSITION,
#endif
	__BSS_MGMT_EN_MAX
};

static bool
__hostapd_bss_mgmt_enable_f(struct hostapd_data *hapd, int flag)
{
	struct hostapd_bss_config *bss = hapd->conf;
	uint32_t flags;

	switch (flag) {
	case BSS_MGMT_EN_NEIGHBOR:
		if (bss->radio_measurements[0] &
		    WLAN_RRM_CAPS_NEIGHBOR_REPORT)
			return false;

		bss->radio_measurements[0] |=
			WLAN_RRM_CAPS_NEIGHBOR_REPORT;
		hostapd_neighbor_set_own_report(hapd);
		return true;
	case BSS_MGMT_EN_BEACON:
		flags = WLAN_RRM_CAPS_BEACON_REPORT_PASSIVE |
			WLAN_RRM_CAPS_BEACON_REPORT_ACTIVE |
			WLAN_RRM_CAPS_BEACON_REPORT_TABLE;

		if (bss->radio_measurements[0] & flags == flags)
			return false;

		bss->radio_measurements[0] |= (u8) flags;
		return true;
	case BSS_MGMT_EN_LINK_MEASUREMENT:
		flags = WLAN_RRM_CAPS_LINK_MEASUREMENT;

		if (bss->radio_measurements[0] & flags == flags)
			return false;

		bss->radio_measurements[0] |= (u8) flags;
		return true;
#ifdef CONFIG_WNM_AP
	case BSS_MGMT_EN_BSS_TRANSITION:
		if (bss->bss_transition)
			return false;

		bss->bss_transition = 1;
		return true;
#endif
	}
}

static void
__hostapd_bss_mgmt_enable(struct hostapd_data *hapd, uint32_t flags)
{
	bool update = false;
	int i;

	for (i = 0; i < __BSS_MGMT_EN_MAX; i++) {
		if (!(flags & (1 << i)))
			continue;

		update |= __hostapd_bss_mgmt_enable_f(hapd, i);
	}

	if (update)
		ieee802_11_update_beacons(hapd->iface);
}


static const struct blobmsg_policy bss_mgmt_enable_policy[__BSS_MGMT_EN_MAX] = {
	[BSS_MGMT_EN_NEIGHBOR] = { "neighbor_report", BLOBMSG_TYPE_BOOL },
	[BSS_MGMT_EN_BEACON] = { "beacon_report", BLOBMSG_TYPE_BOOL },
	[BSS_MGMT_EN_LINK_MEASUREMENT] = { "link_measurement", BLOBMSG_TYPE_BOOL },
#ifdef CONFIG_WNM_AP
	[BSS_MGMT_EN_BSS_TRANSITION] = { "bss_transition", BLOBMSG_TYPE_BOOL },
#endif
};

static int
hostapd_bss_mgmt_enable(struct ubus_context *ctx, struct ubus_object *obj,
		   struct ubus_request_data *req, const char *method,
		   struct blob_attr *msg)

{
	struct hostapd_data *hapd = get_hapd_from_object(obj);
	struct blob_attr *tb[__BSS_MGMT_EN_MAX];
	struct blob_attr *cur;
	uint32_t flags = 0;
	int i;
	bool neigh = false, beacon = false;

	blobmsg_parse(bss_mgmt_enable_policy, __BSS_MGMT_EN_MAX, tb, blob_data(msg), blob_len(msg));

	for (i = 0; i < ARRAY_SIZE(tb); i++) {
		if (!tb[i] || !blobmsg_get_bool(tb[i]))
			continue;

		flags |= (1 << i);
	}

	__hostapd_bss_mgmt_enable(hapd, flags);

	return 0;
}


static void
hostapd_rrm_nr_enable(struct hostapd_data *hapd)
{
	__hostapd_bss_mgmt_enable(hapd, 1 << BSS_MGMT_EN_NEIGHBOR);
}

static int
hostapd_rrm_nr_get_own(struct ubus_context *ctx, struct ubus_object *obj,
		       struct ubus_request_data *req, const char *method,
		       struct blob_attr *msg)
{
	struct hostapd_data *hapd = get_hapd_from_object(obj);
	struct hostapd_neighbor_entry *nr;
	void *c;

	hostapd_rrm_nr_enable(hapd);

	nr = hostapd_neighbor_get(hapd, hapd->own_addr, NULL);
	if (!nr)
		return UBUS_STATUS_NOT_FOUND;

	blob_buf_init(&b, 0);

	c = blobmsg_open_array(&b, "value");
	hostapd_rrm_print_nr(nr);
	blobmsg_close_array(&b, c);

	ubus_send_reply(ctx, req, b.head);

	return 0;
}

static int
hostapd_rrm_nr_list(struct ubus_context *ctx, struct ubus_object *obj,
		    struct ubus_request_data *req, const char *method,
		    struct blob_attr *msg)
{
	struct hostapd_data *hapd = get_hapd_from_object(obj);
	struct hostapd_neighbor_entry *nr;
	void *c;

	hostapd_rrm_nr_enable(hapd);
	blob_buf_init(&b, 0);

	c = blobmsg_open_array(&b, "list");
	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry, list) {
		void *cur;

		if (!memcmp(nr->bssid, hapd->own_addr, ETH_ALEN))
			continue;

		cur = blobmsg_open_array(&b, NULL);
		hostapd_rrm_print_nr(nr);
		blobmsg_close_array(&b, cur);
	}
	blobmsg_close_array(&b, c);

	ubus_send_reply(ctx, req, b.head);

	return 0;
}

enum {
	NR_SET_LIST,
	__NR_SET_LIST_MAX
};

static const struct blobmsg_policy nr_set_policy[__NR_SET_LIST_MAX] = {
	[NR_SET_LIST] = { "list", BLOBMSG_TYPE_ARRAY },
};


static void
hostapd_rrm_nr_clear(struct hostapd_data *hapd)
{
	struct hostapd_neighbor_entry *nr;

restart:
	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry, list) {
		if (!memcmp(nr->bssid, hapd->own_addr, ETH_ALEN))
			continue;

		hostapd_neighbor_remove(hapd, nr->bssid, &nr->ssid);
		goto restart;
	}
}

static int
hostapd_rrm_nr_set(struct ubus_context *ctx, struct ubus_object *obj,
		   struct ubus_request_data *req, const char *method,
		   struct blob_attr *msg)
{
	static const struct blobmsg_policy nr_e_policy[] = {
		{ .type = BLOBMSG_TYPE_STRING },
		{ .type = BLOBMSG_TYPE_STRING },
		{ .type = BLOBMSG_TYPE_STRING },
	};
	struct hostapd_data *hapd = get_hapd_from_object(obj);
	struct blob_attr *tb_l[__NR_SET_LIST_MAX];
	struct blob_attr *tb[ARRAY_SIZE(nr_e_policy)];
	struct blob_attr *cur;
	int rem;

	hostapd_rrm_nr_enable(hapd);

	blobmsg_parse(nr_set_policy, __NR_SET_LIST_MAX, tb_l, blob_data(msg), blob_len(msg));
	if (!tb_l[NR_SET_LIST])
		return UBUS_STATUS_INVALID_ARGUMENT;

	hostapd_rrm_nr_clear(hapd);
	blobmsg_for_each_attr(cur, tb_l[NR_SET_LIST], rem) {
		struct wpa_ssid_value ssid;
		struct wpabuf *data;
		u8 bssid[ETH_ALEN];
		char *s, *nr_s;

		blobmsg_parse_array(nr_e_policy, ARRAY_SIZE(nr_e_policy), tb, blobmsg_data(cur), blobmsg_data_len(cur));
		if (!tb[0] || !tb[1] || !tb[2])
			goto invalid;

		/* Neighbor Report binary */
		nr_s = blobmsg_get_string(tb[2]);
		data = wpabuf_parse_bin(nr_s);
		if (!data)
			goto invalid;

		/* BSSID */
		s = blobmsg_get_string(tb[0]);
		if (strlen(s) == 0) {
			/* Copy BSSID from neighbor report */
			if (hwaddr_compact_aton(nr_s, bssid))
				goto invalid;
		} else if (hwaddr_aton(s, bssid)) {
			goto invalid;
		}

		/* SSID */
		s = blobmsg_get_string(tb[1]);
		if (strlen(s) == 0) {
			/* Copy SSID from hostapd BSS conf */
			memcpy(&ssid, &hapd->conf->ssid, sizeof(ssid));
		} else {
			ssid.ssid_len = strlen(s);
			if (ssid.ssid_len > sizeof(ssid.ssid))
				goto invalid;

			memcpy(&ssid, s, ssid.ssid_len);
		}

		hostapd_neighbor_set(hapd, bssid, &ssid, data, NULL, NULL, 0, 0);
		wpabuf_free(data);
		continue;

invalid:
		return UBUS_STATUS_INVALID_ARGUMENT;
	}

	return 0;
}

enum {
	BEACON_REQ_ADDR,
	BEACON_REQ_MODE,
	BEACON_REQ_OP_CLASS,
	BEACON_REQ_CHANNEL,
	BEACON_REQ_DURATION,
	BEACON_REQ_BSSID,
	BEACON_REQ_SSID,
	__BEACON_REQ_MAX,
};

static const struct blobmsg_policy beacon_req_policy[__BEACON_REQ_MAX] = {
	[BEACON_REQ_ADDR] = { "addr", BLOBMSG_TYPE_STRING },
	[BEACON_REQ_OP_CLASS] { "op_class", BLOBMSG_TYPE_INT32 },
	[BEACON_REQ_CHANNEL] { "channel", BLOBMSG_TYPE_INT32 },
	[BEACON_REQ_DURATION] { "duration", BLOBMSG_TYPE_INT32 },
	[BEACON_REQ_MODE] { "mode", BLOBMSG_TYPE_INT32 },
	[BEACON_REQ_BSSID] { "bssid", BLOBMSG_TYPE_STRING },
	[BEACON_REQ_SSID] { "ssid", BLOBMSG_TYPE_STRING },
};

static int
hostapd_rrm_beacon_req(struct ubus_context *ctx, struct ubus_object *obj,
		       struct ubus_request_data *ureq, const char *method,
		       struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	struct blob_attr *tb[__BEACON_REQ_MAX];
	struct blob_attr *cur;
	struct wpabuf *req;
	u8 bssid[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	u8 addr[ETH_ALEN];
	int mode, rem, ret;
	int buf_len = 13;

	blobmsg_parse(beacon_req_policy, __BEACON_REQ_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[BEACON_REQ_ADDR] || !tb[BEACON_REQ_MODE] || !tb[BEACON_REQ_DURATION] ||
	    !tb[BEACON_REQ_OP_CLASS] || !tb[BEACON_REQ_CHANNEL])
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (tb[BEACON_REQ_SSID])
		buf_len += blobmsg_data_len(tb[BEACON_REQ_SSID]) + 2 - 1;

	mode = blobmsg_get_u32(tb[BEACON_REQ_MODE]);
	if (hwaddr_aton(blobmsg_data(tb[BEACON_REQ_ADDR]), addr))
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (tb[BEACON_REQ_BSSID] &&
	    hwaddr_aton(blobmsg_data(tb[BEACON_REQ_BSSID]), bssid))
		return UBUS_STATUS_INVALID_ARGUMENT;

	req = wpabuf_alloc(buf_len);
	if (!req)
		return UBUS_STATUS_UNKNOWN_ERROR;

	/* 1: regulatory class */
	wpabuf_put_u8(req, blobmsg_get_u32(tb[BEACON_REQ_OP_CLASS]));

	/* 2: channel number */
	wpabuf_put_u8(req, blobmsg_get_u32(tb[BEACON_REQ_CHANNEL]));

	/* 3-4: randomization interval */
	wpabuf_put_le16(req, 0);

	/* 5-6: duration */
	wpabuf_put_le16(req, blobmsg_get_u32(tb[BEACON_REQ_DURATION]));

	/* 7: mode */
	wpabuf_put_u8(req, blobmsg_get_u32(tb[BEACON_REQ_MODE]));

	/* 8-13: BSSID */
	wpabuf_put_data(req, bssid, ETH_ALEN);

	if ((cur = tb[BEACON_REQ_SSID]) != NULL) {
		wpabuf_put_u8(req, WLAN_EID_SSID);
		wpabuf_put_u8(req, blobmsg_data_len(cur) - 1);
		wpabuf_put_data(req, blobmsg_data(cur), blobmsg_data_len(cur) - 1);
	}

	ret = hostapd_send_beacon_req(hapd, addr, 0, req);
	if (ret < 0)
		return -ret;

	return 0;
}

enum {
	LM_REQ_ADDR,
	LM_REQ_TX_POWER_USED,
	LM_REQ_TX_POWER_MAX,
	__LM_REQ_MAX,
};

static const struct blobmsg_policy lm_req_policy[__LM_REQ_MAX] = {
	[LM_REQ_ADDR] = { "addr", BLOBMSG_TYPE_STRING },
	[LM_REQ_TX_POWER_USED] = { "tx-power-used", BLOBMSG_TYPE_INT32 },
	[LM_REQ_TX_POWER_MAX] = { "tx-power-max", BLOBMSG_TYPE_INT32 },
};

static int
hostapd_rrm_lm_req(struct ubus_context *ctx, struct ubus_object *obj,
		   struct ubus_request_data *ureq, const char *method,
		   struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	struct blob_attr *tb[__LM_REQ_MAX];
	struct wpabuf *buf;
	u8 addr[ETH_ALEN];
	int ret;
	int8_t txp_used, txp_max;

	txp_used = 0;
	txp_max = 0;

	blobmsg_parse(lm_req_policy, __LM_REQ_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[LM_REQ_ADDR])
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (tb[LM_REQ_TX_POWER_USED])
		txp_used = (int8_t) blobmsg_get_u32(tb[LM_REQ_TX_POWER_USED]);

	if (tb[LM_REQ_TX_POWER_MAX])
		txp_max = (int8_t) blobmsg_get_u32(tb[LM_REQ_TX_POWER_MAX]);

	if (hwaddr_aton(blobmsg_data(tb[LM_REQ_ADDR]), addr))
		return UBUS_STATUS_INVALID_ARGUMENT;

	buf = wpabuf_alloc(5);
	if (!buf)
		return UBUS_STATUS_UNKNOWN_ERROR;

	wpabuf_put_u8(buf, WLAN_ACTION_RADIO_MEASUREMENT);
	wpabuf_put_u8(buf, WLAN_RRM_LINK_MEASUREMENT_REQUEST);
	wpabuf_put_u8(buf, 1);
	/* TX-Power used */
	wpabuf_put_u8(buf, txp_used);
	/* Max TX Power */
	wpabuf_put_u8(buf, txp_max);

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      wpabuf_head(buf), wpabuf_len(buf));

	wpabuf_free(buf);
	if (ret < 0)
		return -ret;

	return 0;
}


void hostapd_ubus_handle_link_measurement(struct hostapd_data *hapd, const u8 *data, size_t len)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) data;
	const u8 *pos, *end;
	u8 token;

	end = data + len;
	token = mgmt->u.action.u.rrm.dialog_token;
	pos = mgmt->u.action.u.rrm.variable;

	if (end - pos < 8)
		return;

	if (!hapd->ubus.obj.has_subscribers)
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_macaddr(&b, "address", mgmt->sa);
	blobmsg_add_u16(&b, "dialog-token", token);
	blobmsg_add_u16(&b, "rx-antenna-id", pos[4]);
	blobmsg_add_u16(&b, "tx-antenna-id", pos[5]);
	blobmsg_add_u16(&b, "rcpi", pos[6]);
	blobmsg_add_u16(&b, "rsni", pos[7]);

	ubus_notify(ctx, &hapd->ubus.obj, "link-measurement-report", b.head, -1);
}


#ifdef CONFIG_WNM_AP

static int
hostapd_bss_tr_send(struct hostapd_data *hapd, u8 *addr, bool disassoc_imminent, bool abridged,
		    u16 disassoc_timer, u8 validity_period, u8 dialog_token,
		    struct blob_attr *neighbors, u8 mbo_reason, u8 cell_pref, u8 reassoc_delay)
{
	struct blob_attr *cur;
	struct sta_info *sta;
	int nr_len = 0;
	int rem;
	u8 *nr = NULL;
	u8 req_mode = 0;
	u8 mbo[10];
	size_t mbo_len = 0;

	sta = ap_get_sta(hapd, addr);
	if (!sta)
		return UBUS_STATUS_NOT_FOUND;

	if (neighbors) {
		u8 *nr_cur;

		if (blobmsg_check_array(neighbors,
					BLOBMSG_TYPE_STRING) < 0)
			return UBUS_STATUS_INVALID_ARGUMENT;

		blobmsg_for_each_attr(cur, neighbors, rem) {
			int len = strlen(blobmsg_get_string(cur));

			if (len % 2)
				return UBUS_STATUS_INVALID_ARGUMENT;

			nr_len += (len / 2) + 2;
		}

		if (nr_len) {
			nr = os_zalloc(nr_len);
			if (!nr)
				return UBUS_STATUS_UNKNOWN_ERROR;
		}

		nr_cur = nr;
		blobmsg_for_each_attr(cur, neighbors, rem) {
			int len = strlen(blobmsg_get_string(cur)) / 2;

			*nr_cur++ = WLAN_EID_NEIGHBOR_REPORT;
			*nr_cur++ = (u8) len;
			if (hexstr2bin(blobmsg_data(cur), nr_cur, len)) {
				free(nr);
				return UBUS_STATUS_INVALID_ARGUMENT;
			}

			nr_cur += len;
		}
	}

	if (nr)
		req_mode |= WNM_BSS_TM_REQ_PREF_CAND_LIST_INCLUDED;

	if (abridged)
		req_mode |= WNM_BSS_TM_REQ_ABRIDGED;

	if (disassoc_imminent)
		req_mode |= WNM_BSS_TM_REQ_DISASSOC_IMMINENT;

#ifdef CONFIG_MBO
	u8 *mbo_pos = mbo;

	if (mbo_reason > MBO_TRANSITION_REASON_PREMIUM_AP)
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (cell_pref != 0 && cell_pref != 1 && cell_pref != 255)
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (reassoc_delay > 65535 || (reassoc_delay && !disassoc_imminent))
		return UBUS_STATUS_INVALID_ARGUMENT;

	*mbo_pos++ = MBO_ATTR_ID_TRANSITION_REASON;
	*mbo_pos++ = 1;
	*mbo_pos++ = mbo_reason;
	*mbo_pos++ = MBO_ATTR_ID_CELL_DATA_PREF;
	*mbo_pos++ = 1;
	*mbo_pos++ = cell_pref;

	if (reassoc_delay) {
		*mbo_pos++ = MBO_ATTR_ID_ASSOC_RETRY_DELAY;
		*mbo_pos++ = 2;
		WPA_PUT_LE16(mbo_pos, reassoc_delay);
		mbo_pos += 2;
	}

	mbo_len = mbo_pos - mbo;
#endif

	if (wnm_send_bss_tm_req(hapd, sta, req_mode, disassoc_timer, validity_period, NULL,
				dialog_token, NULL, nr, nr_len, mbo_len ? mbo : NULL, mbo_len))
		return UBUS_STATUS_UNKNOWN_ERROR;

	return 0;
}

enum {
	BSS_TR_ADDR,
	BSS_TR_DA_IMMINENT,
	BSS_TR_DA_TIMER,
	BSS_TR_VALID_PERIOD,
	BSS_TR_NEIGHBORS,
	BSS_TR_ABRIDGED,
	BSS_TR_DIALOG_TOKEN,
#ifdef CONFIG_MBO
	BSS_TR_MBO_REASON,
	BSS_TR_CELL_PREF,
	BSS_TR_REASSOC_DELAY,
#endif
	__BSS_TR_DISASSOC_MAX
};

static const struct blobmsg_policy bss_tr_policy[__BSS_TR_DISASSOC_MAX] = {
	[BSS_TR_ADDR] = { "addr", BLOBMSG_TYPE_STRING },
	[BSS_TR_DA_IMMINENT] = { "disassociation_imminent", BLOBMSG_TYPE_BOOL },
	[BSS_TR_DA_TIMER] = { "disassociation_timer", BLOBMSG_TYPE_INT32 },
	[BSS_TR_VALID_PERIOD] = { "validity_period", BLOBMSG_TYPE_INT32 },
	[BSS_TR_NEIGHBORS] = { "neighbors", BLOBMSG_TYPE_ARRAY },
	[BSS_TR_ABRIDGED] = { "abridged", BLOBMSG_TYPE_BOOL },
	[BSS_TR_DIALOG_TOKEN] = { "dialog_token", BLOBMSG_TYPE_INT32 },
#ifdef CONFIG_MBO
	[BSS_TR_MBO_REASON] = { "mbo_reason", BLOBMSG_TYPE_INT32 },
	[BSS_TR_CELL_PREF] = { "cell_pref", BLOBMSG_TYPE_INT32 },
	[BSS_TR_REASSOC_DELAY] = { "reassoc_delay", BLOBMSG_TYPE_INT32 },
#endif
};

static int
hostapd_bss_transition_request(struct ubus_context *ctx, struct ubus_object *obj,
			       struct ubus_request_data *ureq, const char *method,
			       struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	struct blob_attr *tb[__BSS_TR_DISASSOC_MAX];
	struct sta_info *sta;
	u32 da_timer = 0;
	u32 valid_period = 0;
	u8 addr[ETH_ALEN];
	u32 dialog_token = 1;
	bool abridged;
	bool da_imminent;
	u8 mbo_reason;
	u8 cell_pref;
	u8 reassoc_delay;

	blobmsg_parse(bss_tr_policy, __BSS_TR_DISASSOC_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[BSS_TR_ADDR])
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (hwaddr_aton(blobmsg_data(tb[BSS_TR_ADDR]), addr))
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (tb[BSS_TR_DA_TIMER])
		da_timer = blobmsg_get_u32(tb[BSS_TR_DA_TIMER]);

	if (tb[BSS_TR_VALID_PERIOD])
		valid_period = blobmsg_get_u32(tb[BSS_TR_VALID_PERIOD]);

	if (tb[BSS_TR_DIALOG_TOKEN])
		dialog_token = blobmsg_get_u32(tb[BSS_TR_DIALOG_TOKEN]);

	da_imminent = !!(tb[BSS_TR_DA_IMMINENT] && blobmsg_get_bool(tb[BSS_TR_DA_IMMINENT]));
	abridged = !!(tb[BSS_TR_ABRIDGED] && blobmsg_get_bool(tb[BSS_TR_ABRIDGED]));

#ifdef CONFIG_MBO
	if (tb[BSS_TR_MBO_REASON])
		mbo_reason = blobmsg_get_u32(tb[BSS_TR_MBO_REASON]);

	if (tb[BSS_TR_CELL_PREF])
		cell_pref = blobmsg_get_u32(tb[BSS_TR_CELL_PREF]);

	if (tb[BSS_TR_REASSOC_DELAY])
		reassoc_delay = blobmsg_get_u32(tb[BSS_TR_REASSOC_DELAY]);
#endif

	return hostapd_bss_tr_send(hapd, addr, da_imminent, abridged, da_timer, valid_period,
				   dialog_token, tb[BSS_TR_NEIGHBORS], mbo_reason, cell_pref, reassoc_delay);
}
#endif

#ifdef CONFIG_AIRTIME_POLICY
enum {
	UPDATE_AIRTIME_STA,
	UPDATE_AIRTIME_WEIGHT,
	__UPDATE_AIRTIME_MAX,
};


static const struct blobmsg_policy airtime_policy[__UPDATE_AIRTIME_MAX] = {
	[UPDATE_AIRTIME_STA] = { "sta", BLOBMSG_TYPE_STRING },
	[UPDATE_AIRTIME_WEIGHT] = { "weight", BLOBMSG_TYPE_INT32 },
};

static int
hostapd_bss_update_airtime(struct ubus_context *ctx, struct ubus_object *obj,
			   struct ubus_request_data *ureq, const char *method,
			   struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	struct blob_attr *tb[__UPDATE_AIRTIME_MAX];
	struct sta_info *sta = NULL;
	u8 addr[ETH_ALEN];
	int weight;

	blobmsg_parse(airtime_policy, __UPDATE_AIRTIME_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[UPDATE_AIRTIME_WEIGHT])
		return UBUS_STATUS_INVALID_ARGUMENT;

	weight = blobmsg_get_u32(tb[UPDATE_AIRTIME_WEIGHT]);

	if (!tb[UPDATE_AIRTIME_STA]) {
		if (!weight)
			return UBUS_STATUS_INVALID_ARGUMENT;

		hapd->conf->airtime_weight = weight;
		return 0;
	}

	if (hwaddr_aton(blobmsg_data(tb[UPDATE_AIRTIME_STA]), addr))
		return UBUS_STATUS_INVALID_ARGUMENT;

	sta = ap_get_sta(hapd, addr);
	if (!sta)
		return UBUS_STATUS_NOT_FOUND;

	sta->dyn_airtime_weight = weight;
	airtime_policy_new_sta(hapd, sta);

	return 0;
}
#endif

#ifdef CONFIG_TAXONOMY
static const struct blobmsg_policy addr_policy[] = {
	{ "address", BLOBMSG_TYPE_STRING }
};

static bool
hostapd_add_b64_data(const char *name, const struct wpabuf *buf)
{
	char *str;

	if (!buf)
		return false;

	str = blobmsg_alloc_string_buffer(&b, name, B64_ENCODE_LEN(wpabuf_len(buf)));
	b64_encode(wpabuf_head(buf), wpabuf_len(buf), str, B64_ENCODE_LEN(wpabuf_len(buf)));
	blobmsg_add_string_buffer(&b);

	return true;
}

static int
hostapd_bss_get_sta_ies(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	struct blob_attr *tb;
	struct sta_info *sta;
	u8 addr[ETH_ALEN];

	blobmsg_parse(addr_policy, 1, &tb, blobmsg_data(msg), blobmsg_len(msg));

	if (!tb || hwaddr_aton(blobmsg_data(tb), addr))
		return UBUS_STATUS_INVALID_ARGUMENT;

	sta = ap_get_sta(hapd, addr);
	if (!sta || (!sta->probe_ie_taxonomy && !sta->assoc_ie_taxonomy))
		return UBUS_STATUS_NOT_FOUND;

	blob_buf_init(&b, 0);
	hostapd_add_b64_data("probe_ie", sta->probe_ie_taxonomy);
	hostapd_add_b64_data("assoc_ie", sta->assoc_ie_taxonomy);
	ubus_send_reply(ctx, req, b.head);

	return 0;
}
#endif


static const struct ubus_method bss_methods[] = {
	UBUS_METHOD_NOARG("reload", hostapd_bss_reload),
	UBUS_METHOD_NOARG("get_clients", hostapd_bss_get_clients),
#ifdef CONFIG_TAXONOMY
	UBUS_METHOD("get_sta_ies", hostapd_bss_get_sta_ies, addr_policy),
#endif
	UBUS_METHOD_NOARG("get_status", hostapd_bss_get_status),
	UBUS_METHOD("del_client", hostapd_bss_del_client, del_policy),
#ifdef CONFIG_AIRTIME_POLICY
	UBUS_METHOD("update_airtime", hostapd_bss_update_airtime, airtime_policy),
#endif
	UBUS_METHOD_NOARG("list_bans", hostapd_bss_list_bans),
#ifdef CONFIG_WPS
	UBUS_METHOD_NOARG("wps_start", hostapd_bss_wps_start),
	UBUS_METHOD_NOARG("wps_status", hostapd_bss_wps_status),
	UBUS_METHOD_NOARG("wps_cancel", hostapd_bss_wps_cancel),
#endif
	UBUS_METHOD_NOARG("update_beacon", hostapd_bss_update_beacon),
	UBUS_METHOD_NOARG("get_features", hostapd_bss_get_features),
#ifdef NEED_AP_MLME
	UBUS_METHOD("switch_chan", hostapd_switch_chan, csa_policy),
#endif
	UBUS_METHOD("set_vendor_elements", hostapd_vendor_elements, ve_policy),
	UBUS_METHOD("notify_response", hostapd_notify_response, notify_policy),
	UBUS_METHOD("bss_mgmt_enable", hostapd_bss_mgmt_enable, bss_mgmt_enable_policy),
	UBUS_METHOD_NOARG("rrm_nr_get_own", hostapd_rrm_nr_get_own),
	UBUS_METHOD_NOARG("rrm_nr_list", hostapd_rrm_nr_list),
	UBUS_METHOD("rrm_nr_set", hostapd_rrm_nr_set, nr_set_policy),
	UBUS_METHOD("rrm_beacon_req", hostapd_rrm_beacon_req, beacon_req_policy),
	UBUS_METHOD("link_measurement_req", hostapd_rrm_lm_req, lm_req_policy),
#ifdef CONFIG_WNM_AP
	UBUS_METHOD("bss_transition_request", hostapd_bss_transition_request, bss_tr_policy),
#endif
};

static struct ubus_object_type bss_object_type =
	UBUS_OBJECT_TYPE("hostapd_bss", bss_methods);

static int avl_compare_macaddr(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, ETH_ALEN);
}

static int
hostapd_wired_get_clients(struct ubus_context *ctx, struct ubus_object *obj,
			  struct ubus_request_data *req, const char *method,
			  struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	struct hostap_sta_driver_data sta_driver_data;
	struct sta_info *sta;
	void *list, *c;
	char mac_buf[20];
	static const struct {
		const char *name;
		uint32_t flag;
	} sta_flags[] = {
		{ "authorized", WLAN_STA_AUTHORIZED },
	};

	blob_buf_init(&b, 0);
	list = blobmsg_open_table(&b, "clients");
	for (sta = hapd->sta_list; sta; sta = sta->next) {
		void *r;
		int i;

		sprintf(mac_buf, MACSTR, MAC2STR(sta->addr));
		c = blobmsg_open_table(&b, mac_buf);
		for (i = 0; i < ARRAY_SIZE(sta_flags); i++)
			blobmsg_add_u8(&b, sta_flags[i].name,
				       !!(sta->flags & sta_flags[i].flag));

		blobmsg_close_table(&b, c);
	}
	blobmsg_close_array(&b, list);
	ubus_send_reply(ctx, req, b.head);

	return 0;
}

static int
hostapd_wired_get_status(struct ubus_context *ctx, struct ubus_object *obj,
			 struct ubus_request_data *req, const char *method,
			 struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);
	char iface_name[17];

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "driver", hapd->driver->name);
	blobmsg_add_string(&b, "status", hostapd_state_text(hapd->iface->state));

	snprintf(iface_name, 17, "%s", hapd->iface->phy);
	blobmsg_add_string(&b, "iface", iface_name);

	ubus_send_reply(ctx, req, b.head);

	return 0;
}

static int
hostapd_wired_del_clients(struct ubus_context *ctx, struct ubus_object *obj,
			  struct ubus_request_data *req, const char *method,
			  struct blob_attr *msg)
{
	struct hostapd_data *hapd = container_of(obj, struct hostapd_data, ubus.obj);

	hostapd_free_stas(hapd);

	return 0;
}

static const struct ubus_method wired_methods[] = {
	UBUS_METHOD_NOARG("reload", hostapd_bss_reload),
	UBUS_METHOD_NOARG("get_clients", hostapd_wired_get_clients),
	UBUS_METHOD_NOARG("del_clients", hostapd_wired_del_clients),
	UBUS_METHOD_NOARG("get_status", hostapd_wired_get_status),
};

static struct ubus_object_type wired_object_type =
	UBUS_OBJECT_TYPE("hostapd_wired", wired_methods);

void hostapd_ubus_add_bss(struct hostapd_data *hapd)
{
	struct ubus_object *obj = &hapd->ubus.obj;
	char *name;
	int ret;

#ifdef CONFIG_MESH
	if (hapd->conf->mesh & MESH_ENABLED)
		return;
#endif

	if (!hostapd_ubus_init())
		return;

	if (asprintf(&name, "hostapd.%s", hapd->conf->iface) < 0)
		return;

	avl_init(&hapd->ubus.banned, avl_compare_macaddr, false, NULL);
	obj->name = name;
	if (!strcmp(hapd->driver->name, "wired")) {
		obj->type = &wired_object_type;
		obj->methods = wired_object_type.methods;
		obj->n_methods = wired_object_type.n_methods;
	} else {
		obj->type = &bss_object_type;
		obj->methods = bss_object_type.methods;
		obj->n_methods = bss_object_type.n_methods;
	}
	ret = ubus_add_object(ctx, obj);
	hostapd_ubus_ref_inc();
}

void hostapd_ubus_free_bss(struct hostapd_data *hapd)
{
	struct ubus_object *obj = &hapd->ubus.obj;
	char *name = (char *) obj->name;

#ifdef CONFIG_MESH
	if (hapd->conf->mesh & MESH_ENABLED)
		return;
#endif

	if (!ctx)
		return;

	if (obj->id) {
		ubus_remove_object(ctx, obj);
		hostapd_ubus_ref_dec();
	}

	free(name);
	obj->name = NULL;
}

static void
hostapd_ubus_vlan_action(struct hostapd_data *hapd, struct hostapd_vlan *vlan,
			 const char *action)
{
	struct vlan_description *desc = &vlan->vlan_desc;
	void *c;
	int i;

	if (!hapd->ubus.obj.has_subscribers)
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "ifname", vlan->ifname);
	blobmsg_add_string(&b, "bridge", vlan->bridge);
	blobmsg_add_u32(&b, "vlan_id", vlan->vlan_id);

	if (desc->notempty) {
		blobmsg_add_u32(&b, "untagged", desc->untagged);
		c = blobmsg_open_array(&b, "tagged");
		for (i = 0; i < ARRAY_SIZE(desc->tagged) && desc->tagged[i]; i++)
			blobmsg_add_u32(&b, "", desc->tagged[i]);
		blobmsg_close_array(&b, c);
	}

	ubus_notify(ctx, &hapd->ubus.obj, action, b.head, -1);
}

void hostapd_ubus_add_vlan(struct hostapd_data *hapd, struct hostapd_vlan *vlan)
{
	hostapd_ubus_vlan_action(hapd, vlan, "vlan_add");
}

void hostapd_ubus_remove_vlan(struct hostapd_data *hapd, struct hostapd_vlan *vlan)
{
	hostapd_ubus_vlan_action(hapd, vlan, "vlan_remove");
}

struct ubus_event_req {
	struct ubus_notify_request nreq;
	int resp;
};

static void
ubus_event_cb(struct ubus_notify_request *req, int idx, int ret)
{
	struct ubus_event_req *ureq = container_of(req, struct ubus_event_req, nreq);

	ureq->resp = ret;
}

int hostapd_ubus_handle_event(struct hostapd_data *hapd, struct hostapd_ubus_request *req)
{
	struct ubus_banned_client *ban;
	const u8 bcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	const char *types[HOSTAPD_UBUS_TYPE_MAX] = {
		[HOSTAPD_UBUS_PROBE_REQ] = "probe",
		[HOSTAPD_UBUS_AUTH_REQ] = "auth",
		[HOSTAPD_UBUS_ASSOC_REQ] = "assoc",
	};
	const char *type = "mgmt";
	struct ubus_event_req ureq = {};
	const u8 *addr;

	if (req->mgmt_frame)
		addr = req->mgmt_frame->sa;
	else
		addr = req->addr;

	ban = avl_find_element(&hapd->ubus.banned, addr, ban, avl);
	if (ban)
		return WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;

	ban = avl_find_element(&hapd->ubus.banned, bcast, ban, avl);
	if (ban)
		return WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;

	if (!hapd->ubus.obj.has_subscribers)
		return WLAN_STATUS_SUCCESS;

	if (req->type < ARRAY_SIZE(types))
		type = types[req->type];

	blob_buf_init(&b, 0);
	blobmsg_add_macaddr(&b, "address", addr);
	blobmsg_add_string(&b, "ifname", hapd->conf->iface);
	if (req->mgmt_frame)
		blobmsg_add_macaddr(&b, "target", req->mgmt_frame->da);
	if (req->ssi_signal)
		blobmsg_add_u32(&b, "signal", req->ssi_signal);
	blobmsg_add_u32(&b, "freq", hapd->iface->freq);

	if (req->elems) {
		if(req->elems->ht_capabilities)
		{
			struct ieee80211_ht_capabilities *ht_capabilities;
			void *ht_cap, *ht_cap_mcs_set, *mcs_set;


			ht_capabilities = (struct ieee80211_ht_capabilities*) req->elems->ht_capabilities;
			ht_cap = blobmsg_open_table(&b, "ht_capabilities");
			blobmsg_add_u16(&b, "ht_capabilities_info", ht_capabilities->ht_capabilities_info);
			ht_cap_mcs_set = blobmsg_open_table(&b, "supported_mcs_set");
			blobmsg_add_u16(&b, "a_mpdu_params", ht_capabilities->a_mpdu_params);
			blobmsg_add_u16(&b, "ht_extended_capabilities", ht_capabilities->ht_extended_capabilities);
			blobmsg_add_u32(&b, "tx_bf_capability_info", ht_capabilities->tx_bf_capability_info);
			blobmsg_add_u16(&b, "asel_capabilities", ht_capabilities->asel_capabilities);
			mcs_set = blobmsg_open_array(&b, "supported_mcs_set");
			for (int i = 0; i < 16; i++) {
				blobmsg_add_u16(&b, NULL, (u16) ht_capabilities->supported_mcs_set[i]);
			}
			blobmsg_close_array(&b, mcs_set);
			blobmsg_close_table(&b, ht_cap_mcs_set);
			blobmsg_close_table(&b, ht_cap);
		}
		if(req->elems->vht_capabilities)
		{
			struct ieee80211_vht_capabilities *vht_capabilities;
			void *vht_cap, *vht_cap_mcs_set;

			vht_capabilities = (struct ieee80211_vht_capabilities*) req->elems->vht_capabilities;
			vht_cap = blobmsg_open_table(&b, "vht_capabilities");
			blobmsg_add_u32(&b, "vht_capabilities_info", vht_capabilities->vht_capabilities_info);
			vht_cap_mcs_set = blobmsg_open_table(&b, "vht_supported_mcs_set");
			blobmsg_add_u16(&b, "rx_map", vht_capabilities->vht_supported_mcs_set.rx_map);
			blobmsg_add_u16(&b, "rx_highest", vht_capabilities->vht_supported_mcs_set.rx_highest);
			blobmsg_add_u16(&b, "tx_map", vht_capabilities->vht_supported_mcs_set.tx_map);
			blobmsg_add_u16(&b, "tx_highest", vht_capabilities->vht_supported_mcs_set.tx_highest);
			blobmsg_close_table(&b, vht_cap_mcs_set);
			blobmsg_close_table(&b, vht_cap);
		}
	}

	if (!hapd->ubus.notify_response) {
		ubus_notify(ctx, &hapd->ubus.obj, type, b.head, -1);
		return WLAN_STATUS_SUCCESS;
	}

	if (ubus_notify_async(ctx, &hapd->ubus.obj, type, b.head, &ureq.nreq))
		return WLAN_STATUS_SUCCESS;

	ureq.nreq.status_cb = ubus_event_cb;
	ubus_complete_request(ctx, &ureq.nreq.req, 100);

	if (ureq.resp)
		return ureq.resp;

	return WLAN_STATUS_SUCCESS;
}

void hostapd_ubus_notify(struct hostapd_data *hapd, const char *type, const u8 *addr)
{
	if (!hapd->ubus.obj.has_subscribers)
		return;

	if (!addr)
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_macaddr(&b, "address", addr);
	blobmsg_add_string(&b, "ifname", hapd->conf->iface);

	ubus_notify(ctx, &hapd->ubus.obj, type, b.head, -1);
}

void hostapd_ubus_notify_authorized(struct hostapd_data *hapd, struct sta_info *sta,
				    const char *auth_alg)
{
	if (!hapd->ubus.obj.has_subscribers)
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_macaddr(&b, "address", sta->addr);
	if (sta->vlan_id)
		blobmsg_add_u32(&b, "vlan", sta->vlan_id);
	blobmsg_add_string(&b, "ifname", hapd->conf->iface);
	if (auth_alg)
		blobmsg_add_string(&b, "auth-alg", auth_alg);
	if (sta->bandwidth[0] || sta->bandwidth[1]) {
		void *r = blobmsg_open_array(&b, "rate-limit");

		blobmsg_add_u32(&b, "", sta->bandwidth[0]);
		blobmsg_add_u32(&b, "", sta->bandwidth[1]);
		blobmsg_close_array(&b, r);
	}

	ubus_notify(ctx, &hapd->ubus.obj, "sta-authorized", b.head, -1);
}

void hostapd_ubus_notify_beacon_report(
	struct hostapd_data *hapd, const u8 *addr, u8 token, u8 rep_mode,
	struct rrm_measurement_beacon_report *rep, size_t len)
{
	if (!hapd->ubus.obj.has_subscribers)
		return;

	if (!addr || !rep)
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_macaddr(&b, "address", addr);
	blobmsg_add_u16(&b, "op-class", rep->op_class);
	blobmsg_add_u16(&b, "channel", rep->channel);
	blobmsg_add_u64(&b, "start-time", rep->start_time);
	blobmsg_add_u16(&b, "duration", rep->duration);
	blobmsg_add_u16(&b, "report-info", rep->report_info);
	blobmsg_add_u16(&b, "rcpi", rep->rcpi);
	blobmsg_add_u16(&b, "rsni", rep->rsni);
	blobmsg_add_macaddr(&b, "bssid", rep->bssid);
	blobmsg_add_u16(&b, "antenna-id", rep->antenna_id);
	blobmsg_add_u16(&b, "parent-tsf", rep->parent_tsf);
	blobmsg_add_u16(&b, "rep-mode", rep_mode);

	ubus_notify(ctx, &hapd->ubus.obj, "beacon-report", b.head, -1);
}

void hostapd_ubus_notify_radar_detected(struct hostapd_iface *iface, int frequency,
					int chan_width, int cf1, int cf2)
{
	struct hostapd_data *hapd;
	int i;

	if (!ctx)
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_u16(&b, "frequency", frequency);
	blobmsg_add_u16(&b, "width", chan_width);
	blobmsg_add_u16(&b, "center1", cf1);
	blobmsg_add_u16(&b, "center2", cf2);

	for (i = 0; i < iface->num_bss; i++) {
		hapd = iface->bss[i];
		ubus_notify(ctx, &hapd->ubus.obj, "radar-detected", b.head, -1);
	}
}

#ifdef CONFIG_WNM_AP
static void hostapd_ubus_notify_bss_transition_add_candidate_list(
	const u8 *candidate_list, u16 candidate_list_len)
{
	char *cl_str;
	int i;

	if (candidate_list_len == 0)
		return;

	cl_str = blobmsg_alloc_string_buffer(&b, "candidate-list", candidate_list_len * 2 + 1);
	for (i = 0; i < candidate_list_len; i++)
		snprintf(&cl_str[i*2], 3, "%02X", candidate_list[i]);
	blobmsg_add_string_buffer(&b);

}
#endif

void hostapd_ubus_notify_bss_transition_response(
	struct hostapd_data *hapd, const u8 *addr, u8 dialog_token, u8 status_code,
	u8 bss_termination_delay, const u8 *target_bssid,
	const u8 *candidate_list, u16 candidate_list_len)
{
#ifdef CONFIG_WNM_AP
	u16 i;

	if (!hapd->ubus.obj.has_subscribers)
		return;

	if (!addr)
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_macaddr(&b, "address", addr);
	blobmsg_add_u8(&b, "dialog-token", dialog_token);
	blobmsg_add_u8(&b, "status-code", status_code);
	blobmsg_add_u8(&b, "bss-termination-delay", bss_termination_delay);
	if (target_bssid)
		blobmsg_add_macaddr(&b, "target-bssid", target_bssid);

	hostapd_ubus_notify_bss_transition_add_candidate_list(candidate_list, candidate_list_len);

	ubus_notify(ctx, &hapd->ubus.obj, "bss-transition-response", b.head, -1);
#endif
}

int hostapd_ubus_notify_bss_transition_query(
	struct hostapd_data *hapd, const u8 *addr, u8 dialog_token, u8 reason,
	const u8 *candidate_list, u16 candidate_list_len)
{
#ifdef CONFIG_WNM_AP
	struct ubus_event_req ureq = {};
	char *cl_str;
	u16 i;

	if (!hapd->ubus.obj.has_subscribers)
		return 0;

	if (!addr)
		return 0;

	blob_buf_init(&b, 0);
	blobmsg_add_macaddr(&b, "address", addr);
	blobmsg_add_u8(&b, "dialog-token", dialog_token);
	blobmsg_add_u8(&b, "reason", reason);
	hostapd_ubus_notify_bss_transition_add_candidate_list(candidate_list, candidate_list_len);

	if (!hapd->ubus.notify_response) {
		ubus_notify(ctx, &hapd->ubus.obj, "bss-transition-query", b.head, -1);
		return 0;
	}

	if (ubus_notify_async(ctx, &hapd->ubus.obj, "bss-transition-query", b.head, &ureq.nreq))
		return 0;

	ureq.nreq.status_cb = ubus_event_cb;
	ubus_complete_request(ctx, &ureq.nreq.req, 100);

	return ureq.resp;
#endif
}

#ifdef CONFIG_APUP
void hostapd_ubus_notify_apup_newpeer(
	struct hostapd_data *hapd, const u8 *addr, const char *ifname)
{
	if (!hapd->ubus.obj.has_subscribers)
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_macaddr(&b, "address", addr);
	blobmsg_add_string(&b, "ifname", ifname);

	ubus_notify(ctx, &hapd->ubus.obj, "apup-newpeer", b.head, -1);
}
#endif // def CONFIG_APUP

void hostapd_ubus_notify_csa(struct hostapd_data *hapd, int freq)
{
	if (!hapd->ubus.obj.has_subscribers)
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "ifname", hapd->conf->iface);
	blobmsg_add_u32(&b, "freq", freq);
	blobmsg_printf(&b, "bssid", MACSTR, MAC2STR(hapd->conf->bssid));

	ubus_notify(ctx, &hapd->ubus.obj, "channel-switch", b.head, -1);
}
