local m, s, o

m = Map("udpspeeder", "%s - %s" %{translate("UDPspeeder"), translate("Configure Management")})

s = m:section(TypedSection, "servers")
s.anonymous = true
s.addremove = true
s.sortable = true
s.template = "cbi/tblsection"
s.extedit = luci.dispatcher.build_url("admin/services/udpspeeder/servers/%s")
function s.create(...)
	local sid = TypedSection.create(...)
	if sid then
		luci.http.redirect(s.extedit % sid)
		return
	end
end

o = s:option(DummyValue, "alias", translate("Alias"))
function o.cfgvalue(...)
	return Value.cfgvalue(...) or translate("None")
end

o = s:option(DummyValue, "_server", translate("Server"))
function o.cfgvalue(self, section)
	local server_ip = m.uci:get("udpspeeder", section, "server_ip") or "127.0.0.1"
	local server_port = m.uci:get("udpspeeder", section, "server_port")
	return "%s:%s" %{server_ip, server_port}
end

o = s:option(DummyValue, "_local_listening", translate("Local Listening"))
function o.cfgvalue(self, section)
	local listen_ip = m.uci:get("udpspeeder", section, "listen_ip") or "0.0.0.0"
	local listen_port = m.uci:get("udpspeeder", section, "listen_port")
	return "%s:%s" %{listen_ip, listen_port}
end

o = s:option(DummyValue, "mode", translate("Mode"))
function o.cfgvalue(...)
	local v = Value.cfgvalue(...)
	return v and v:lower() or "0"
end

o = s:option(DummyValue, "fec", translate("FEC"))
function o.cfgvalue(...)
	local v = Value.cfgvalue(...)
	return v and v:lower() or "20:10"
end

o = s:option(DummyValue, "mtu", translate("MTU"))
function o.cfgvalue(...)
	local v = Value.cfgvalue(...)
	return v and v:lower() or "1250"
end

o = s:option(DummyValue, "jitter", translate("Jitter"))
function o.cfgvalue(...)
	local v = Value.cfgvalue(...)
	return v and v:lower() or "0"
end

o = s:option(DummyValue, "interval", translate("Interval"))
function o.cfgvalue(...)
	local v = Value.cfgvalue(...)
	return v and v:lower() or "0"
end

return m
