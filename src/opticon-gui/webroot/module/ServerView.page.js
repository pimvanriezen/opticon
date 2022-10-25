ServerView = new Module.Page("ServerView","/Server/%", 2);

ServerView.create = function() {
    var self = ServerView;
    self.createView({
        data:{
        },
        graph:{
            cpu:[]
        },
        tab:"Overview"
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
            Module.backgroundInterval = 30000;
            Module.setBackground (self.refresh);
            self.refresh();
        }
    });
}

ServerView.refresh = function() {
    var self = ServerView;
    API.Opticon.Host.getCurrent (self.tenantid, self.id, function (res) {
        self.apires = res;
        self.View.data = self.apires;
        if (self.View.graph.cpu.length == 0) {
            var ngraphy = [];
            var ngraphx = [];
            var i = 0;
            for (i=0; i<107; ++i) {
                ngraphy.push (30 + Math.random() * 50);
                ngraphx.push (10*i);
            }
            console.log (ngraphy);
            const spline = new Spline (ngraphx, ngraphy);
            var ngraph = [];
            for (i=0; i<1060; ++i) {
                ngraph.push (spline.at(i));
            }
            self.View.graph.cpu = ngraph;
            self.drawGraph ("cpu");
        }
    });
}

ServerView.drawGraph = function (graphid) {
    let self = ServerView;
    let id = "graph-"+graphid;
    const canvas = document.getElementById (id);
    if (! canvas) return;
    const ctx = canvas.getContext("2d");
    var gradient = ctx.createLinearGradient(0,0,0,200);
    gradient.addColorStop(0.00, '#305090c0');
    gradient.addColorStop(1.00, '#50c0c0c0');
    var gradient2 = ctx.createLinearGradient(0,0,0,200);
    gradient2.addColorStop(0.00, '#30509060');
    gradient2.addColorStop(1.00, '#50c0c060');
    
    ctx.transform(1,0,0,-1,0,canvas.height);
    
    let width = 1;
    let arr = self.View.graph[graphid];
    if (! arr) return;
    ctx.clearRect(0,0,530,200);
    ctx.width = 1060;
    ctx.height = 400;
    ctx.scale (0.5,0.5);
    
    for (let i=0; i<1060; ++i) {
        let x = i;
        let y = (arr[i]*2);
        ctx.fillStyle = gradient;
        ctx.fillRect(x,0,1,2*y);
        if (i<1059) {
            ctx.fillStyle = gradient2;
            ctx.fillRect(x+1,0,2,2*y);
        }
    }
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
