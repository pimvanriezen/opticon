ServerView = new Module.Page("ServerView","/Server/%", 2);

ServerView.create = function() {
    var self = ServerView;
    self.createView({
        data:{}
    });
    self.apires = {};
    self.id = null;
    self.tenantid = null;
}

ServerView.activate = function(argv) {
    var self = ServerView;
    self.id = argv[0];
    self.show();
    API.Opticon.Host.resolveTenant (self.id, function (tenantid) {
        self.tenantid = tenantid;
        if (self.tenantid) {
            API.Opticon.Tenant.getMeta (self.tenantid, function (res) {
                if (res) self.View.meta = res.metadata;
            });
            API.Opticon.Host.getCurrent (self.tenantid, self.id, function (res) {
                self.apires = res;
                self.View.data = self.apires;
            });
        }
    });
}

ServerView.back = function() {
    var self = ServerView;
    self.hide();
    App.go ("/Server");
}

ServerView.statusClass = function (st) {
    if (st == "OK") return "status green";
    if (st == "WARN") return "status orange";
    if (st == "ALERT") return "status red";
    return "status grey";
}

ServerView.ktoh = function (kb,isbps) {
    res = {size:kb, unit:"KB"};
    if (kb > 1024) {
        kb = kb/1024;
        res.size = kb;
        res.unit = "MB";
        if (kb > 1024) {
            kb = kb / 1024;
            res.size = kb;
            res.unit = "GB";
        }
    }
    res.size = parseFloat(res.size).toFixed(2);
    if (isbps) res.unit = res.unit[0] + 'b/s';
    return res;
}

ServerView.devName = function (dev) {
    if (dev[0] != '@') return "★ " + dev;
    if (dev.indexOf('/')>=0) return "⇨ " + dev.replace(/@/,'');
    return "⇨ " + dev.replace(/@/,'').replace(/--/g,'@').replace(/-/g,'/').replace(/@/g,'-');
}

ServerView.devClass = function (dev) {
    if (dev[0] != '@') return "devtype-device";
    return "devtype-volume";
}
