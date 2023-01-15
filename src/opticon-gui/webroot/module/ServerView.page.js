ServerView = new Module.Page("ServerView","/Server/%", 2);

ServerView.create = function() {
    var self = ServerView;
    self.createView({
        empty:true,
        data:{
        },
        tab:"Overview",
        log:[]
    });
    self.apires = {};
    self.id = null;
    self.tenantid = null;
    self.graph = {};
    self.canvasbuilt = {};
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
            Module.backgroundInterval = 30000;
            Module.setBackground (self.refresh);
            self.refresh();
            self.refreshGraph ("cpu","usage","Usage","%");
            self.refreshGraph ("link","rtt","RTT","ms");
            self.refreshGraph ("net","input","Traffic","Kb/s");
            self.refreshGraph ("net","output","Traffic","Kb/s");
            self.refreshGraph ("io","read","i/o","ops/s");
            self.refreshGraph ("io","write","i/o","ops/s");
        }
    });
}

ServerView.refresh = function() {
    var self = ServerView;
    API.Opticon.Host.getCurrent (self.tenantid, self.id, function (res) {
        self.apires = res;
        self.View.data = self.apires;
        API.Opticon.Host.getLog (self.tenantid, self.id, function (res) {
            if (res) {
                self.View.empty = false;
                self.View.log = res.log;
            }
            else self.View.empty = true;
        });
    });
}

ServerView.refreshGraph = function(graph,datum,title,unit) {
    var self = ServerView;
    API.Opticon.Host.getGraph (self.tenantid, self.id, graph, datum,
                               86400, 1000, function (res) {
        if (res) {
            console.log ("getgraph:",res);
            if (self.graph[graph] === undefined) {
                self.graph[graph] = {};
            }
            if (self.graph[graph][datum] === undefined) {
                let id = "graph-"+graph+"-"+datum;
                const canvas = document.getElementById (id);
                if (! canvas) return;
                self.graph[graph][datum] = new Graph (canvas, title, 530, 200);
                self.graph[graph][datum].initialize();
            }

            self.graph[graph][datum].set (res.data, res.max, 86400, unit);
            self.graph[graph][datum].drawGraph();
        }
    });
}

ServerView.back = function() {
    var self = ServerView;
    self.hide();
    App.go ("/Server");
}

ServerView.statusClass = function (st) {
    if (st == "OK") return "status small green";
    if (st == "WARN") return "status small orange";
    if (st == "ALERT") return "status small red";
    return "status small grey";
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
            if (kb > 1023) {
                kb = kb / 1024;
                res.size = kb;
                res.unit = "TB";
            }
        }
    }
    res.size = parseFloat(res.size).toFixed(2);
    if (isbps) res.unit = res.unit[0] + 'b/s';
    return res;
}

ServerView.devName = function (dev) {
    if (dev[0] != '@') return "★ " + dev;
    if (dev.indexOf('/')>=0) return "⇨ " + dev.replace(/@/,'');
    return "⇨ " + dev.replace(/@/,'').replace(/--/g,'@').
                      replace(/-/g,'/').replace(/@/g,'-');
}

ServerView.devClass = function (dev) {
    if (dev[0] != '@') return "devtype-device";
    return "devtype-volume";
}

ServerView.translateTimestamp = function (ts) {
    var dt = new Date(ts * 1000);
    return dt.toLocaleString();
}

ServerView.translateUser = function (u) {
  var lu = (""+u).toLowerCase();
  if (/([a-f\d]{8}(-[a-f\d]{4}){3}-[a-f\d]{12}?)/.test(lu)) {
    return lu.substring(0,12)+"...";
  }
  return lu;
}

ServerView.translateUptime = function (u) {
    let uptime = parseInt (u);
    let u_days = Math.floor (uptime / 86400);
    let u_hours = Math.floor ((uptime - (86400 * u_days)) / 3600);
    let u_mins = Math.floor (
        (uptime - (86400 * u_days) - (3600 * u_hours)) / 60);
    let u_sec = uptime % 60;
    let res = "Unknown";
    
    if (u_days) {
        res = "" + u_days + " day";
        if (u_days != 1) res += "s";
        res += ", ";
        res += ("" + u_hours).padStart(2, '0');
        res += ":";
        res += ("" + u_mins).padStart(2, '0');
    }
    else if (u_hours) {
        res = "" + u_hours + " hour";
        if (u_hours != 1) res += "s";
        res += ", " + u_minute + " minute";
        if (u_minute != 1) res += "s";
    }
    else {
        res = "" + u_minutes + " minute";
        if (u_minute != 1) res += "s";
        res += ", " + u_sec + " second";
        if (u_sec != 1) res += "s";
    }
    return res;
}