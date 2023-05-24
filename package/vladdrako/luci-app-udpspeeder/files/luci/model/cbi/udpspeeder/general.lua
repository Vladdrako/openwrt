local m, s, o
local uci = luci.model.uci.cursor()
local servers = {}

local function has_bin(name)
	return luci.sys.call("command -v %s >/dev/null" %{name}) == 0
end

if has_bin("udpspeeder") then
    uci:foreach("udpspeeder", "servers", function(s)
        if s.server_port and s.listen_port then
            servers[#servers+1] = {name = s[".name"], alias = s.alias or "%s:%s" %{s.server_port, s.listen_port}}
        end
    end)

    m = Map("udpspeeder", "%s - %s" %{translate("UDPspeeder"), translate("Status")})
    --Running Status
    m:append(Template("udpspeeder/status"))

    --General Settings
    s = m:section(NamedSection, "udpspeeder", "general", translate("Settings"))
    s.anonymous = true
    s.addremove = false

    o = s:option(DynamicList, "server", translate("Server"))
    o.template = "udpspeeder/dynamiclist"
    o:value("nil", translate("Disable"))
    for _, s in ipairs(servers) do o:value(s.name, s.alias) end
    o.default = "nil"
    o.rmempty = false

    o = s:option(ListValue, "daemon_user", translate("Run Daemon as User"))
    for u in luci.util.execi("cat /etc/passwd | cut -d ':' -f1") do o:value(u) end
    o.default = "root"
    o.rmempty = false
else
	return Map("udpspeeder", "%s - %s" %{translate("UDPspeeder"),
		translate("Settings")}, '<b style="color:red">UDPspeeder binary file (/usr/bin/udpspeeder) not found. </b>')
end

return m
