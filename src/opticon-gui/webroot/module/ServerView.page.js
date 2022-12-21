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
            self.refreshGraph ("cpu","usage");
            self.refreshGraph ("link","rtt");
            self.refreshGraph ("net","input");
            self.refreshGraph ("net","output");
            self.refreshGraph ("io","read");
            self.refreshGraph ("io","write");
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

ServerView.refreshGraph = function(graph,datum) {
    var self = ServerView;
    API.Opticon.Host.getGraph (self.tenantid, self.id, graph, datum,
                               86400, 1000, function (res) {
        if (res) {
            console.log ("getgraph:",res);
            if (self.graph[graph] === undefined) {
                self.graph[graph] = {};
            }
            delete self.graph[graph][datum];

            let numsamples = res.data.length;
            if (numsamples < 2) return;
            
            let step = 1060 / (numsamples-1);
            let xcoords = [];
            let ycoords = [];
            for (let i=0;i<numsamples;++i) {
                xcoords.push (i * step);
                ycoords.push (res.data[i]);
            }

            self.graph[graph][datum] = {
                max: res.max,
                data: new Spline (xcoords,ycoords)
            }
            self.drawGraph (graph, datum);
        }
    });
}

ServerView.drawGraph = function (graphid,datumid) {
    let self = ServerView;
    let id = "graph-"+graphid+"-"+datumid;
    const canvas = document.getElementById (id);
    if (! canvas) return;
    
    const ctx = canvas.getContext("2d");
    var gradient = ctx.createLinearGradient(0,0,0,200);
    gradient.addColorStop(0.00, '#305090c0');
    gradient.addColorStop(1.00, '#50c0c0c0');
    var gradient2 = ctx.createLinearGradient(0,0,0,200);
    gradient2.addColorStop(0.00, '#30509060');
    gradient2.addColorStop(1.00, '#50c0c060');

    if (! self.canvasbuilt[id]) {    
        ctx.transform(1,0,0,-1,0,canvas.height);
        ctx.scale (0.5,0.5);
        ctx.width = 1060;
        ctx.height = 400;
    }
    
    let width = 1;
    if (self.graph[graphid] === undefined) return;
    let obj = self.graph[graphid][datumid];
    if (! obj) return;
    if (! obj.max) obj.max = 1;

    ctx.clearRect(0,0,ctx.width, ctx.height);
    
    
    for (let i=0; i<1060; ++i) {
        let x = i;
        let y = 200 * (obj.data.at(i)/obj.max);
        ctx.fillStyle = gradient;
        ctx.fillRect(x,0,1,2*y);
        if (i<1059) {
            ctx.fillStyle = gradient2;
            ctx.fillRect(x+1,0,2,2*y);
        }
    }
    
    ctx.scale (1,1);
    self.canvasbuilt[id] = true;
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
