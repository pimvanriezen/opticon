ServerView = new Module.Page("ServerView","/Server/%", 2);

ServerView.create = function() {
    var self = ServerView;
    self.createView({
        empty:true,
        data:{
        },
        tab:"Overview",
        scale:86400,
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
    self.View.id = self.id;
    self.show();
    self.Vidi.onRender = function() {
        self.fixLayout();
    }
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
        /* translate pre-0.9.26 layout */
        if (res.version && (res.agent === undefined || res.agent.v === undefined)) {
            if (res.agent === undefined) res.agent = {};
            res.agent.v = res.version;
            var tmp = res.link.ip;
            res.link.ip = res.agent.ip;
            res.agent.ip = tmp;
            delete res.version;
            if (res.uptimea) {
                res.agent.up = res.uptimea;
                delete res.uptimea;
            }
        }
        self.apires = res;
        $("#serverjsonraw").html (self.jsonPrint (res));
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
                               self.View.scale, 1000, function (res) {
        if (res) {
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

            self.graph[graph][datum].set (res.data, res.max, self.View.scale, unit);
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

ServerView.jsonReplace = function (match, pIndent, pKey, pVal, pEnd) {
    var key = '<span class=json-key>';
    var val = '<span class=json-value>';
    var str = '<span class=json-string>';
    var r = pIndent || '';
    if (pKey)
        r = r + key + pKey.replace(/[": ]/g, '') + '</span>: ';
    if (pVal)
        r = r + (pVal[0] == '"' ? str : val) + pVal + '</span>';

    return r + (pEnd || '');
}

ServerView.jsonPrint = function (obj) {
    var jsonLine = /^( *)("[\w]+": )?("[^"]*"|[\w.+-]*)?([,[{])?$/mg;
    return JSON.stringify(obj, null, 3)
               .replace(/\[\]/g, '"---x-EMPTY_ARRAY-x---"')
               .replace(/&/g, '&amp;').replace(/\\"/g, '&quot;')
               .replace(/</g, '&lt;').replace(/>/g, '&gt;')
               .replace(jsonLine, ServerView.jsonReplace)
               .replace(/"---x-EMPTY_ARRAY-x---"/g, '</span><span>[]');
}

ServerView.setScale = function(e) {
    var self = ServerView;
    self.View.scale = e;
    self.refreshGraph ("cpu","usage","Usage","%");
    self.refreshGraph ("link","rtt","RTT","ms");
    self.refreshGraph ("net","input","Traffic","Kb/s");
    self.refreshGraph ("net","output","Traffic","Kb/s");
    self.refreshGraph ("io","read","i/o","ops/s");
    self.refreshGraph ("io","write","i/o","ops/s");
}

ServerView.makeLayout = function(q, portWidth) {
    var objtable= {};
    var columnpositions = {};
    var widths= [];
    var outcolumns = [];
    var idx = 0;
    var leftmost = 1000;
    var rightmost = 0;
    
    for (obj of q) {
        if (! obj.offsetWidth) continue;
        let w = obj.offsetWidth;
        if (objtable[w] === undefined) {
            objtable[w] = [];
            columnpositions[w] = [];
            widths.push (w);
        }
        objtable[w].push ({
            idx: idx++,
            height: obj.offsetHeight,
            left: obj.offsetLeft,
            obj: obj
        });
        
        if (obj.offsetLeft < leftmost) leftmost = obj.offsetLeft;
        if (obj.offsetLeft+obj.offsetWidth > rightmost) {
            rightmost = obj.offsetLeft+obj.offsetWidth;
        }
        
        if (! columnpositions[w].includes (obj.offsetLeft)) {
            columnpositions[w].push (obj.offsetLeft);
            outcolumns.push ([]);
        }
    }
    
    let cpos = 0;
    let yoffs = 20;
    let xoffs = (((portWidth-rightmost)/2)-leftmost) - 10/*margin/2*/;
    
    let columnh = [0,0,0,0,0,0];
    let numc = 0;
    let numw = widths.length;
    if (numw != 2) {
        console.log ("Unknown layout scheme", widths.length, widths);
        return;
    }
    
    var width = widths[0];
    var owidth = widths[1];
    var oheight = objtable[owidth][0].height;
    let wcolumnh = JSON.parse (JSON.stringify (columnh));
    var thirdwassorted = false;
    
    objtable[width].sort (function (a,b) {
        if (a.idx <2) return 0;
        if (b.idx <2) return 0;
        if (a.height < b.height) {
            if (a.idx == 2 || b.idx == 2) {
                if (thirdwassorted) return 0;
                thirdwassorted = true;
            }
            return 1;
        }
        return -1;
    });
    
    console.log ("sorted", objtable[width]);
    
    wcolumnh[0] = oheight;
    wcolumnh[1] = oheight;

    numc = columnpositions[width].length;

    for (var item of objtable[width]) {
        let lowestcolumn = 0;
        let lowestval = -1;
        for (let i=0; i<numc; ++i) {
            if (columnh[i] == 0) {
                lowestval = 0;
                lowestcolumn = i;
                break;
            }
            if ((lowestval<0) || (wcolumnh[i]<lowestval)) {
                lowestval = wcolumnh[i];
                lowestcolumn = i;
            }
        }
        
        lowestval = columnh[lowestcolumn];

        outcolumns[lowestcolumn].push ({
            top: lowestval+yoffs,
            left: columnpositions[width][lowestcolumn],
            idx: item.idx,
            obj: item.obj
        });

        columnh[lowestcolumn] += item.height;
        wcolumnh[lowestcolumn] += item.height;
    }
    
    let plisty = columnh[0];
    if (columnh[1] > plisty) plisty = columnh[1];
    let pslist = objtable[owidth][0].obj;
    
    if (numc > 2) {
        columnh[0] = plisty + pslist.offsetHeight;
        columnh[1] = plisty + pslist.offsetHeight;
    }
    
    let maxh = 0;
    for (let col=0; col<numc; ++col) {
        if (columnh[col] > maxh) maxh = columnh[col];
    }
    
    for (let c of outcolumns) {
        for (let j=0; j<c.length-1;++j) {
            for (let i=0; i<c.length-1;++i) {
                if (c[i].idx > c[i+1].idx) {
            
                    let tmp = {
                        top:c[i].top+c[i+1].obj.offsetHeight,
                        left:c[i].left,
                        idx:c[i].idx,
                        obj:c[i].obj
                    }
                
                    let tmp2 = {
                        top:c[i].top,
                        left:c[i+1].left,
                        idx:c[i+1].idx,
                        obj:c[i+1].obj
                    }
                
                    c[i] = tmp2;
                    c[i+1] = tmp;
                }
            }
        }
    }        

    for (let col=0; col<numc; ++col) {
        if (columnh[col] < maxh) {
            if (outcolumns[col].length<2) continue;
            
            let diff = maxh - columnh[col];
            let delta = diff;
            let cdelta = diff / (outcolumns[col].length-1);
            
            for (let i = outcolumns[col].length-1; i>0; --i) {
                let o = outcolumns[col][i];
                o.top += delta;
                delta -= cdelta;
            }
        }
    }
    
    if (numc <= 2) {
        plisty = maxh;
    }
    else {
        if (plisty + pslist.offsetHeight < maxh) {
            plisty = (maxh - pslist.offsetHeight);
        }
    }

    pslist.style.top = plisty + yoffs;
    pslist.style.left = leftmost + xoffs;
    pslist.style.position = "absolute";

    for (let c of outcolumns) {
        for (let item of c) {
            item.obj.style.top = item.top;
            item.obj.style.left = item.left + xoffs;
            item.obj.style.position = "absolute";
        }
    }
}

ServerView.fixLayout = function() {
    var positions = {};
    var counts = {};
    var q = $("#ServerView #ServerOverview .magicLayout");
    ServerView.makeLayout (q, $("#ServerView").width());
}

ServerView.linuxIcon = function(kernel) {
    if (/UBNT/.test (kernel)) return "icon/ubnt.png"
    if (/qnap/.test (kernel)) return "icon/qnap.png"
    return "icon/linux.png"
}
