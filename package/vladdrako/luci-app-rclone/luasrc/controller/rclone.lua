module("luci.controller.rclone", package.seeall)

function index()
	if not nixio.fs.access("/etc/config/rclone") then
		return
	end
	
	entry({"admin", "services"}, firstchild(), _("Services") , 45).dependent = false
	entry({"admin", "services", "rclone"}, cbi("rclone"), _("Rclone"), 100).dependent = false
end
