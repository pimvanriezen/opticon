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
        query:"",
        sort:"status",
        sortorder:"descending"
    });
}

// --------------------------------------------------------------------------
// Validate the search field if it has a query string.
// --------------------------------------------------------------------------
ServerList.checkquery = function() {
    var self = ServerList;
    let qstr = String(self.View.query);
    if (qstr[0] != ':') return true;
    if (! self.queryObj) return false;
    if (self.queryObj.tree.length) return true;
    return false;
}

// --------------------------------------------------------------------------
// Stores the loaded parent customer's id.
// --------------------------------------------------------------------------
ServerList.selectedObject = null;

ServerList.setSort = function (label) {
    var self = ServerList;
    
    if (self.View.sort != label) {
        self.View.sort = label;
        self.reSort();
        return;
    }
    
    if (self.View.sortorder == "descending") {
        self.View.sortorder = "ascending";
    }
    else {
        self.View.sortorder = "descending";
    }
    
    self.reSort();
}

ServerList.sortArrow = function() {
    let self = ServerList;
    if (self.View.sortorder == "ascending") {
        return "▲";
    }
    else {
        return "▼";
    }
}

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

// --------------------------------------------------------------------------
// Match an overview row against the search field.
// --------------------------------------------------------------------------
ServerList.matchHost = function (q, host) {
    let lq = String(q).toLowerCase();
    let self = ServerList;
    
    if (self.queryObj) {
        return self.queryObj.match (host);
    }
    
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
            let qstr = String(self.View.query);
            if (qstr[0] == ':') {
                self.queryObj = mkquery (qstr.substring(1));
            }
            
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
                    if (! self.matchHost (qstr, srv)) continue;
                }
                srv.id = i;
                nwlist.push (srv);
                if (srv.external && srv.external.description) {
                    self.View.showextra = true;
                }
                count++;
            }
            
            self.rawdata = Vidi.clone (nwlist);
            nwlist = nwlist.sort (self.sortFunc);
            
            self.View.empty = ((count == 0) && (! self.queryObj));
            self.View.overview = nwlist;
        }
    });    
}

ServerList.sortFunc = function (left, right) {
    let self = ServerList;
    let sortby = self.View.sort;
    let sortord = self.View.sortorder;
    let stval = {
        "OK":0,
        "WARN":1,
        "ALERT":2,
        "CRIT":3,
        "DEAD":4,
        "STALE":4
    };
        
    let SRTLESS = 1;
    let SRTMORE = -1;
    
    if (sortord == 'ascending') {
        SRTLESS = -1;
        SRTMORE = 1;
    }
    
    if (sortby == "status") {
        if (stval[left.status] < stval[right.status]) return SRTLESS;
        if (stval[left.status] > stval[right.status]) return SRTMORE;
        if (left.pcpu < right.pcpu) return SRTLESS;
        if (left.pcpu > right.pcpu) return SRTMORE;
        if (left.loadavg < right.loadavg) return SRTLESS;
        if (left.loadavg > right.loadavg) return SRTMORE;
        return 0;
    }
    
    let lval = "";
    let rval = "";
    
    try {
        switch (sortby) {
            case "hostname":
                lval = String(left.hostname).toLowerCase();
                rval = String(right.hostname).toLowerCase();
                break;
        
            case "label":
                if (left.external) lval = left.external.description;
                if (right.external) rval = right.external.description;
                break;
        
            case "ipaddress":
                lval = left["link/ip"] + " " + left["agent/ip"];
                rval = right["link/ip"] + " " + right["link/ip"];
                break;
            
            case "cpu":
                lval = left["pcpu"];
                rval = right["pcpu"];
                break;
        
            case "loadavg":
                lval = parseFloat(left["loadavg"]);
                rval = parseFloat(right["loadavg"]);
                break;
                
            case "diskio":
                lval = left["io/rdops"] + left["io/wrops"];
                rval = right["io/rdops"] + right["io/wrops"];
                break;
            
            case "netio":
                lval = left["net/in_kbs"] + left["net/out_kbs"];
                rval = right["net/in_kbs"] + right["net/out_kbs"];
                break;
            
            case "rtt":
                lval = left["link/rtt"];
                rval = right["link/rtt"];
                break;
            
            case "freeram":
                lval = left["mem/free"];
                rval = right["mem/free"];
                break;
        }
    
        if (lval < rval) return SRTLESS;
        if (lval > rval) return SRTMORE;
    }
    catch (e) {
        console.log ("sort exception", e);
    }
    return 0;
}

ServerList.reSort = function() {
    let self = ServerList;
    let nwlist = self.rawdata;
    nwlist = nwlist.sort(self.sortFunc);
    self.View.overview = nwlist;
}

// --------------------------------------------------------------------------
// No-op callback for the search bar
// --------------------------------------------------------------------------
ServerList.search = function () {
    ServerList.refresh();
}

// --------------------------------------------------------------------------
// Clear the search/query
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// Create icon-specific offsets for the os/distro icon
// --------------------------------------------------------------------------
ServerList.osMargin = function(obj) {
    let self = ServerList;
    if (self.osIcon(obj) == "icon/apple.png") return "margin-bottom:-2px;";
    return "margin-bottom:-4px;";
}

// --------------------------------------------------------------------------
// Generate a distro icon
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// Generate update indicators.
// --------------------------------------------------------------------------
ServerList.pkgDotClass = function(q, r) {
    if (r) return "dot red";
    if (q == 0) return "";
    if (q < 10) return "dot yellow";
    return "dot red";
}
