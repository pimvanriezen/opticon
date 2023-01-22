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
    self.refresh();
    Module.backgroundInterval = 30000;
    Module.setBackground (self.refresh);
    self.show();
    self.checkTimer = setTimeout (function() {
        $("#honking-checkmark").fadeIn(5000);
    }, 8000);
}

// --------------------------------------------------------------------------
// Refresh task.
// --------------------------------------------------------------------------
ServerList.refresh = function () {
    let self = ServerList;
    let nwlist = {};
    let count = 0;
    
    API.Opticon.Host.getOverview (function (err, res) {
        if (! err) {
            for (var i in res.overview) {
                let srv = res.overview[i];
                if (srv.status == "OK") {
                    if (self.View.server_status != "ALL") continue;
                }
                else if (srv.status == "WARN") {
                    if (self.View.server_status == "ALERT") continue;
                }
                nwlist[i] = srv;
                count++;
            }
            self.View.empty = (count == 0);
            self.View.overview = nwlist;
        }
    });    
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

ServerList.osIcon = function(obj) {
    let hack = {
        "VM01216":"icon/windows.png",
        "opticon":"icon/ubuntu.png",
        "smokeping":"icon/ubuntu.png",
        "jumphost.newvm.com":"icon/ubuntu.png",
        "opticon-ubuntu":"icon/ubuntu.png",
        "ns2.office.midilab.nl":"icon/alma.png",
        "gitlab.office.midilab.nl":"icon/alma.png",
        "http-01.midilab.nl":"icon/alma.png",
        "rt-lkk2-core01":"icon/ubnt.png",
        "rt-ams3-core01":"icon/alma.png",
        "ns1.office.midilab.nl":"icon/alma.png",
        "ns2.office.midilab.nl":"icon/alma.png",
        "mx.midilab.nl":"icon/ubuntu.png",
        "jumphost.midilab.nl":"icon/alma.png",
        "vm.midilab.nl":"icon/alma.png",
        "uisp.office.midilab.nl":"icon/ubuntu.png",
        "dump.midilab.nl":"icon/alma.png",
        "deepmind.office.midilab.nl":"icon/apple.png"
    }
    
    let t = hack[obj.hostname];
    if (t) return t;   

    let kernel = obj["os/kernel"];
    let distro = obj["os/distro"];
    
    if (kernel == "Windows") return "icon/windows.png";
    if (/UBNT/.test (kernel)) return "icon/ubnt.png";
    if (/el[0-9]/.test (kernel) && /^Alma/.test (distro)) {
        return "icon/alma.png";
    }
    if (/generic/.test (kernel) && /^Ubuntu/.test (distro)) {
        return "icon/ubuntu.png";
    }
    return "icon/linux.png";
}
