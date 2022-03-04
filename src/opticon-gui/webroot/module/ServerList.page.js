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
        server_status: "ALERT",
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
    
    /*    
    
    API.Order.list({status: self.View.order_status}, function (err, res) {
        if (err) res = [];
        self.setOrders(res);
    });
    
    */
    
    self.show();
}

// --------------------------------------------------------------------------
// Refresh task.
// --------------------------------------------------------------------------
ServerList.refresh = function () {
    let self = ServerList;
    let nwlist = {};
    let count = 0;
    
    API.Opticon.hostOverview (function (err, res) {
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
    let self = ServerList;
    self.View.selected = "";
    self.View.haveselection = false;
    self.View.server_status = status;
    self.refresh();
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
    $(".uOrderList li.selected").removeClass("selected");
    $(".uOrderList li#orderList-" + data.id).addClass("selected");
    var self = ServerList;
    setTimeout(function () {
        self.Vidi.lock();
        self.View.selected = data.id;
        self.View.haveselection = true;
        self.selectedObject = data;
        self.Vidi.unlock();
    }, 50);
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

