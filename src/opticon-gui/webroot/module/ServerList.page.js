// ==========================================================================
// Implementation of the CustomerOrder Page module.
// ==========================================================================
ServerList = new Module.Page("ServerList", "/Server");

// --------------------------------------------------------------------------
// Constructor called when module is loaded.
// --------------------------------------------------------------------------
ServerList.create = function () {
    var self = ServerList;
    self.createView({
        overview: [],
        haveselection: false,
        selected: "",
        showextra: false,
        empty: false,
        server_status: "ALL",
    });
}

// --------------------------------------------------------------------------
// Stores the loaded parent customer's id.
// --------------------------------------------------------------------------
ServerList.selectedObject = null;

// --------------------------------------------------------------------------
// Activate method. Loads customer from backend API.
// --------------------------------------------------------------------------
ServerList.activate = function (argv) {
    var self = ServerList;
    self.View.haveselection = false;
    self.View.selected = "";
    self.selectedObject = null;
    self.loaded = false;
    self.refresh();
    Module.backgroundInterval = 30000;
    Module.setBackground (self.refresh);
    self.show();

    $("#"+self.id+" .uSearchInput input").focus();

    self.checkTimer = setTimeout (function() {
        $("#honking-checkmark").fadeIn(5000);
    }, 8000);
}

ServerList.queryHost = function (q, host) {
    let terms = q.split(' ');
    for (let term of terms) {
        let w = [];
        let comp = '>';
        
        if (term.indexOf('!=')>=0) {
            w = term.split('!=');
            comp = '!';
        }
        else if (term.indexOf('=')>=0) {
            w = term.split('=');
            comp = '=';
        }
        else if (term.indexOf('<')>=0) {
            w = term.split('<');
            comp = '<';
        }
        else if (term.indexOf('>')>=0) {
            w = term.split('>');
            comp = '>';
        }
        else if (term.indexOf('~')>=0) {
            w = term.split('~');
            comp = '~';
        }
        else return false;

        let param = w[0];
        let val = w[1];
        let crsr = host;
        let path = param.split(' ');
        for (let pathelm of path) {
            crsr = crsr[pathelm];
        }
        
        console.log ("comp "+param+" "+comp+" "+val);
        console.log (crsr);
        
        switch (comp) {
            case '!=':
                if (crsr == val) return false;
                break;
                
            case '=':
                if (crsr === undefined) return false;
                if (crsr != val) return false;
                break;
            
            case '<':
                if (crsr === undefined) return false;
                if (parseFloat(crsr) >= parseFloat (val)) return false;
                break;
            
            case '>':
                if (crsr === undefined) return false;
                if (parseFloat(crsr) <= parseFloat (val)) return false;
                break;
            
            case '~':
                if (crsr === undefined) return false;
                if (String(crsr).toLowerCase().indexOf(val)<0) return false;
                break;
            
            default:
                return false;
        }
    }
    return true;
}

ServerList.matchHost = function (q, host) {
    let lq = String(q).toLowerCase();
    
    if (lq[0] == ':') return ServerList.queryHost (lq.substring(1), host);
    
    if (host.hostname) {
        let lh = String(host.hostname).toLowerCase();
        if (lh.indexOf (lq) >= 0) return true;
    }
    if (host.external && host.external.description) {
        let ld = String(host.external.description).toLowerCase();
        if (ld.indexOf (lq) >= 0) return true;
    }
    return false;
}

// --------------------------------------------------------------------------
// Refresh task.
// --------------------------------------------------------------------------
ServerList.refresh = function () {
    let self = ServerList;
    let nwlist = [];
    let count = 0;
    
    if (! self.loaded) App.busy();
    
    API.Opticon.Host.getOverview (function (err, res) {
        if (! self.loaded) {
            App.done();
            self.loaded = true;
        }
        if (! err) {
            for (var i in res.overview) {
                let srv = res.overview[i];
                if (srv.hostname === undefined) continue;
                if (srv.status == "OK") {
                    if (self.View.server_status != "ALL") continue;
                }
                else if (srv.status == "WARN") {
                    if (self.View.server_status == "ALERT") continue;
                }
                if (self.View.query) {
                    if (! self.matchHost (self.View.query, srv)) continue;
                }
                srv.id = i;
                nwlist.push (srv);
                if (srv.external && srv.external.description) {
                    self.View.showextra = true;
                }
                count++;
            }
            
            let stval = {
                "OK":0,
                "WARN":1,
                "ALERT":2,
                "CRIT":3,
                "DEAD":4,
                "STALE":4
            }
            
            nwlist = nwlist.sort (function (left, right) {
                if (stval[left.status] < stval[right.status]) return 1;
                if (stval[left.status] > stval[right.status]) return -1;
                if (left.pcpu < right.pcpu) return 1;
                if (left.pcpu > right.pcpu) return -1;
                if (left.loadavg < right.loadavg) return 1;
                if (left.loadavg > right.loadavg) return -1;
                return 0;
            });
            
            self.View.empty = (count == 0);
            self.View.overview = nwlist;
        }
    });    
}

ServerList.search = function () {
    ServerList.refresh();
}

ServerList.clearSearch = function () {
    var self = ServerList;
    
    self.View.query = "";
    self.refresh();
}

// --------------------------------------------------------------------------
// Handler for switching between filter tab
// --------------------------------------------------------------------------
ServerList.switchTab = function (status) {
    $("#honking-checkmark").hide();
    let self = ServerList;
    if (self.checkTimer) clearTimeout (self.checkTimer);
    self.checkTimer = null;
    self.View.selected = "";
    self.View.haveselection = false;
    self.View.server_status = status;
    self.refresh();
    self.checkTimer = setTimeout (function() {
        $("#honking-checkmark").fadeIn(5000);
    }, 8000);
}

// --------------------------------------------------------------------------
// Translate server status to a css class for the status badge
// --------------------------------------------------------------------------
ServerList.statusClass = function (server) {
    if (server.status == "OK") return "status green";
    if (server.status == "WARN") return "status orange";
    if (server.status == "ALERT") return "status red";
    return "status grey";
}

// --------------------------------------------------------------------------
// Click handler for result row.
// --------------------------------------------------------------------------
ServerList.rowClick = function (data, event) {
    App.go ("/Server/"+data);
    if (event) event.stopPropagation();
    return false;
}

// --------------------------------------------------------------------------
// Handler for a double click on the row.
// --------------------------------------------------------------------------
ServerList.rowDoubleClick = function(order) {
    let self = ServerList;
}

// --------------------------------------------------------------------------
// Click handler for empty space.
// --------------------------------------------------------------------------
ServerList.deSelect = function () {
    var self = ServerList;
    self.View.selected = "";
    self.View.selectedObject = null;
    self.View.haveselection = false;
    return false;
}

// --------------------------------------------------------------------------
// Display string for amounts. Examples:
// translateUnit (4230, "", "iops") -> "4.23 Kiops"
// translateUnit (832, "K", "B") -> 823.00 KB
// translateUnit (4096 "M", "B") -> 4.00 GB
// --------------------------------------------------------------------------
ServerList.translateUnit = function (val, base, unit) {
    while ((base != "G") && (val >= 1000)) {
        val = val / 1024.0;
        switch (base) {
            case "":
                base = "K";
                break;
            
            case "K":
                base = "M";
                break;
            
            case "M":
                base = "G";
                
            case "G":
                base = "T";
                break;
        }
    }
    
    unit = '<span class="unit">' + base + unit + '</span>';
    
    if (base == "") return parseInt(val) + " " + unit;
    return parseFloat(val).toFixed(2) + " " + unit;
}

ServerList.osMargin = function(obj) {
    let self = ServerList;
    if (self.osIcon(obj) == "icon/apple.png") return "margin-bottom:-2px;";
    return "margin-bottom:-4px;";
}

ServerList.osIcon = function(obj) {
    let kernel = obj["os/kernel"];
    let kvers = obj["os/version"];
    let distro = obj["os/distro"];
    
    if (kernel == "Windows") return "icon/windows.png";
    if (kernel == "Darwin") return "icon/apple.png";

    if (/Cumulus/.test (distro)) return "icon/nvidia.png";
    if (/^CentOS/.test (distro)) return "icon/centos.png";
    if (/UBNT/.test (kvers)) return "icon/ubnt.png";
    if (/el[0-9]/.test (kvers) && /^Alma/.test (distro)) {
        return "icon/alma.png";
    }
    if (/generic/.test (kvers) && /^Ubuntu/.test (distro)) {
        return "icon/ubuntu.png";
    }
    if (/^Debian/.test (distro)) return "icon/debian.png";
    return "icon/linux.png";
}

ServerList.pkgDotClass = function(q, r) {
    if (r) return "dot red";
    if (q == 0) return "";
    if (q < 10) return "dot yellow";
    return "dot red";
}
