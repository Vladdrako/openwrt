module("luci.controller.unlocker", package.seeall)
function index()
		if not nixio.fs.access("/etc/config/unlocker") then
		return
	end
	local page
	page = entry({"admin", "services", "unlocker"}, cbi("unlocker"), _("Unlocker"))
	page.dependent = true
end
